#include "Generation/HutongActorSpawn.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongDetail.h"
#include "Generation/HutongStarterMaterials.h"

#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSourceData.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialParameters.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "DynamicMeshToMeshDescription.h"
#include "DynamicMesh/MeshNormals.h"
#include "PhysicsEngine/BodySetup.h"
#include "Generation/HutongCollision.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Misc/AutomationTest.h"
#include "Hash/xxhash.h"
#include "HAL/IConsoleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/UObjectIterator.h"

using UE::Geometry::FDynamicMesh3;

namespace
{
	UMaterialInterface* GetCachedDefaultMaterial()
	{
		static TWeakObjectPtr<UMaterialInterface> Cached;
		if (UMaterialInterface* Mat = Cached.Get())
		{
			return Mat;
		}
		UMaterialInterface* Loaded = LoadObject<UMaterialInterface>(
			nullptr,
			TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		Cached = Loaded;
		return Loaded;
	}

	// BasicShapeMaterial is white and uneditable → tint via an instance. Instances save in the
	// actor's package, so reuse one by colour rather than leak one per rebuild. Scans everything
	// outered to the actor, not the material list: a demotion (精 → 塊) drops slots from the list
	// but leaves their instances in the package.
	UMaterialInterface* FindReusableTint(UObject* Outer, const FLinearColor& Color, UMaterialInterface* Base,
		UTexture* Albedo = nullptr)
	{
		if (!Outer || !Base) return nullptr;

		UMaterialInterface* Found = nullptr;
		ForEachObjectWithOuter(Outer, [&](UObject* Obj)
		{
			if (Found) return;

			UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Obj);
			if (!MIC || MIC->Parent != Base) return;

			FLinearColor Existing;
			UTexture* ExistingAlbedo = nullptr;
			const bool bOwnAlbedo = MIC->TextureParameterValues.ContainsByPredicate(
				[](const FTextureParameterValue& V) { return V.ParameterInfo.Name == TEXT("Albedo"); });
			if (bOwnAlbedo) MIC->GetTextureParameterValue(FMaterialParameterInfo(TEXT("Albedo")), ExistingAlbedo);
			if (MIC->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Color")), Existing)
				&& Existing.Equals(Color, 1.0e-4f) && ExistingAlbedo == Albedo)
			{
				Found = MIC;
			}
		}, EGetObjectsFlags::None);

		return Found;
	}

	UMaterialInterface* CreateTintedMaterial(UObject* Outer, const FLinearColor& Color)
	{
		UMaterialInterface* Base = GetCachedDefaultMaterial();
		if (!Base) return nullptr;

		if (UMaterialInterface* Existing = FindReusableTint(Outer, Color, Base))
		{
			return Existing;
		}

		UMaterialInstanceConstant* Instance = NewObject<UMaterialInstanceConstant>(
			Outer, NAME_None, RF_Public | RF_Transactional);
		Instance->SetParentEditorOnly(Base);
		Instance->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Color")), Color);
		Instance->PostEditChange();
		return Instance;
	}

	// An instance of Base with its own colour, and its own Albedo where given (a sign's art), reused by
	// both like the tint.
	UMaterialInterface* CreateInstanceOf(UObject* Outer, UMaterialInterface* Base, const FLinearColor& Color, UTexture* Albedo)
	{
		if (UMaterialInterface* Existing = FindReusableTint(Outer, Color, Base, Albedo)) return Existing;
		UMaterialInstanceConstant* Instance = NewObject<UMaterialInstanceConstant>(
			Outer, NAME_None, RF_Public | RF_Transactional);
		Instance->SetParentEditorOnly(Base);
		Instance->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Color")), Color);
		if (Albedo) Instance->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(TEXT("Albedo")), Albedo);
		Instance->PostEditChange();
		return Instance;
	}

	// Priority: assigned material; a sign's texture on the starter material; the starter material,
	// tinted where the palette's colour is not the slot's default; the tinted default material.
	UMaterialInterface* ResolveSlotMaterial(UObject* Outer, const FHutongPalette& Palette, int32 Slot)
	{
		if (UMaterialInterface* Assigned = Palette.GetSlotMaterial(Slot)) return Assigned;
		UMaterialInterface* Starter = HutongGen::FindStarterMaterial(Slot);
		if (UTexture* Art = Palette.GetSlotTexture(Slot))
		{
			if (Starter) return CreateInstanceOf(Outer, Starter, FLinearColor::White, Art);
			UE_LOG(LogTemp, Warning, TEXT("Hutong: a sign texture needs the starter materials (Scene tab, Create Starter Materials); showing its colour."));
		}
		const FLinearColor Color = Palette.GetSlotColor(Slot);
		if (Starter)
		{
			return Color.Equals(FHutongPalette().GetSlotColor(Slot), 1.0e-4f) ? Starter : CreateInstanceOf(Outer, Starter, Color, nullptr);
		}
		return CreateTintedMaterial(Outer, Color);
	}

	TAutoConsoleVariable<bool> CVarShareMeshes(
		TEXT("hutong.ShareMeshes"), true,
		TEXT("Buildings with identical geometry share one mesh asset under /Game/HutongLayout/Generated."));

	// Bump when the bake itself changes (normals, build settings, collision fit): same geometry, new asset.
	constexpr uint32 BakeVersion = 1;
	const TCHAR* LibraryRoot = TEXT("/Game/HutongLayout/Generated");

	// Key off the geometry as built: identical output shares, any change in the generators or the
	// placement makes a new asset, so a shared mesh never goes stale. Which corners are shared (vertex and
	// UV element IDs) counts too: the baked normals and tangents smooth across shared ones, so a weld alone
	// changes the asset.
	FString LibraryKey(const TArray<FDynamicMesh3>& LODs, int32 CollisionLOD)
	{
		FXxHash64Builder H;
		const uint32 Header[3] = { BakeVersion, (uint32)LODs.Num(), (uint32)CollisionLOD };
		H.Update(Header, sizeof(Header));
		for (const FDynamicMesh3& M : LODs)
		{
			const UE::Geometry::FDynamicMeshMaterialAttribute* Mat = M.HasAttributes() ? M.Attributes()->GetMaterialID() : nullptr;
			const UE::Geometry::FDynamicMeshUVOverlay* UV = M.HasAttributes() ? M.Attributes()->PrimaryUV() : nullptr;
			for (const int32 Tid : M.TriangleIndicesItr())
			{
				const UE::Geometry::FIndex3i T = M.GetTriangle(Tid);
				H.Update(&T, sizeof(T));
				for (int32 k = 0; k < 3; ++k)
				{
					const FVector3d P = M.GetVertex(T[k]);
					H.Update(&P, sizeof(P));
				}
				const int32 Slot = Mat ? Mat->GetValue(Tid) : 0;
				H.Update(&Slot, sizeof(Slot));
				if (UV && UV->IsSetTriangle(Tid))
				{
					const UE::Geometry::FIndex3i E = UV->GetTriangle(Tid);
					H.Update(&E, sizeof(E));
					for (int32 k = 0; k < 3; ++k)
					{
						const FVector2f U = UV->GetElement(E[k]);
						H.Update(&U, sizeof(U));
					}
				}
			}
		}
		return FString::Printf(TEXT("HM_%016llx"), H.Finalize().Hash);
	}

	UStaticMesh* FindLibraryMesh(const FString& Name)
	{
		const FString PackagePath = FString(LibraryRoot) / Name;
		const FString ObjectPath = PackagePath + TEXT(".") + Name;
		if (UStaticMesh* Loaded = FindObject<UStaticMesh>(nullptr, *ObjectPath)) return Loaded;
		if (FPackageName::DoesPackageExist(PackagePath)) return LoadObject<UStaticMesh>(nullptr, *ObjectPath);
		return nullptr;
	}

	// Slot list of a baked mesh, read back off its material slot names.
	TArray<int32> SlotsOf(const UStaticMesh* Mesh)
	{
		TArray<int32> Slots;
		for (const FStaticMaterial& M : Mesh->GetStaticMaterials())
		{
			for (int32 Slot = 0; Slot < HutongGen::MatSlot_Count; ++Slot)
			{
				if (HutongGen::MaterialSlotName(Slot) == M.MaterialSlotName) { Slots.Add(Slot); break; }
			}
		}
		return Slots;
	}
}

namespace HutongGen
{
	const double SmoothingAngleDeg = 32.0;

	const FLinearColor DefaultBrickColor = FLinearColor::FromSRGBColor(FColor(133, 136, 134));
	const FLinearColor DefaultRoofColor = FLinearColor::FromSRGBColor(FColor(94, 97, 96));
	const FLinearColor DefaultWoodColor = FLinearColor::FromSRGBColor(FColor(116, 56, 42));
	const FLinearColor DefaultStoneColor = FLinearColor::FromSRGBColor(FColor(163, 167, 168));
	const FLinearColor DefaultPaintColor = FLinearColor::FromSRGBColor(FColor(46, 92, 110));
	const FLinearColor DefaultBaseCourseColor = FLinearColor::FromSRGBColor(FColor(104, 108, 110));
	const FLinearColor DefaultDoorPaintColor = FLinearColor::FromSRGBColor(FColor(48, 34, 30));
	const FLinearColor DefaultLatticeColor = FLinearColor::FromSRGBColor(FColor(96, 62, 48));
	const FLinearColor DefaultPaperColor = FLinearColor::FromSRGBColor(FColor(226, 214, 186));
	const FLinearColor DefaultPlasterColor = FLinearColor::FromSRGBColor(FColor(232, 228, 216));
	// Swept loess: a Beijing courtyard floor between its paving.
	const FLinearColor DefaultEarthColor = FLinearColor::FromSRGBColor(FColor(168, 146, 112));
	// Waxed pine boarding, not column lacquer.
	const FLinearColor DefaultPartitionColor = FLinearColor::FromSRGBColor(FColor(158, 130, 96));
	const FLinearColor DefaultFloorColor = FLinearColor::FromSRGBColor(FColor(146, 149, 146));
	const FLinearColor DefaultPlaqueColor = FLinearColor::FromSRGBColor(FColor(30, 28, 27));
	const FLinearColor DefaultSignboardColor = FLinearColor::FromSRGBColor(FColor(226, 220, 204));
	const FLinearColor DefaultTradeSignColor = FLinearColor::FromSRGBColor(FColor(168, 42, 34));
	const FLinearColor DefaultCityBrickColor = FLinearColor::FromSRGBColor(FColor(122, 122, 116));

	AStaticMeshActor* SpawnEmptyActor(
		UWorld* World,
		const FTransform& Transform,
		const FString& NameBase)
	{
		if (!World) return nullptr;
		if (ULevel* Level = World->GetCurrentLevel())
		{
			Level->Modify();
		}
		FActorSpawnParameters Params;
		Params.ObjectFlags = RF_Transactional;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), Transform, Params);
		if (!Actor) return nullptr;
		Actor->SetMobility(EComponentMobility::Static);
		Actor->SetActorLabel(FString::Printf(TEXT("%s_%s"),
			*NameBase, *FGuid::NewGuid().ToString().Left(6)));
		return Actor;
	}

	AStaticMeshActor* SpawnStaticMeshActor(
		UWorld* World,
		FDynamicMesh3& Mesh,
		const FTransform& Transform,
		const FString& NameBase,
		const FHutongPalette& Palette)
	{
		if (!World) return nullptr;

		// Record the level in any open transaction so the spawn can be undone.
		if (ULevel* Level = World->GetCurrentLevel())
		{
			Level->Modify();
		}

		FActorSpawnParameters Params;
		Params.ObjectFlags = RF_Transactional;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), Transform, Params);
		if (!Actor) return nullptr;

		// Static: level geometry that never moves, and VSM caches its shadow pages.
		Actor->SetMobility(EComponentMobility::Static);
		Actor->SetActorLabel(FString::Printf(TEXT("%s_%s"),
			*NameBase, *FGuid::NewGuid().ToString().Left(6)));

		BuildAndAssignStaticMesh(Actor, Mesh, Palette);
		return Actor;
	}

	AStaticMeshActor* SpawnStaticMeshActor(
		UWorld* World,
		TArray<FDynamicMesh3>& LODs,
		const FTransform& Transform,
		const FString& NameBase,
		const FHutongPalette& Palette,
		int32 CollisionLOD)
	{
		if (LODs.Num() == 0) return nullptr;
		if (LODs.Num() == 1)
		{
			return SpawnStaticMeshActor(World, LODs[0], Transform, NameBase, Palette);
		}

		// Actor must exist before the mesh (mesh is outered to it).
		if (!World) return nullptr;
		if (ULevel* Level = World->GetCurrentLevel())
		{
			Level->Modify();
		}

		FActorSpawnParameters Params;
		Params.ObjectFlags = RF_Transactional;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(
			AStaticMeshActor::StaticClass(), Transform, Params);
		if (!Actor) return nullptr;

		Actor->SetMobility(EComponentMobility::Static);
		Actor->SetActorLabel(FString::Printf(TEXT("%s_%s"),
			*NameBase, *FGuid::NewGuid().ToString().Left(6)));

		BuildAndAssignStaticMesh(Actor, LODs, Palette, CollisionLOD);
		return Actor;
	}

	TArray<UStaticMesh*> FindUnusedLibraryMeshes()
	{
		TArray<UStaticMesh*> Out;
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPath(FName(LibraryRoot), Assets, /*bRecursive*/ true);
		if (Assets.Num() == 0) return Out;

		// What loaded buildings wear, saved or not.
		TSet<const UStaticMesh*> Worn;
		for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
		{
			if (It->IsTemplate() || !IsValid(*It)) continue;
			if (const UStaticMesh* M = It->GetStaticMesh()) Worn.Add(M);
		}
		for (const FAssetData& A : Assets)
		{
			TArray<FName> Referencers;
			Registry.GetReferencers(A.PackageName, Referencers);
			Referencers.Remove(A.PackageName);
			if (Referencers.Num() > 0) continue;
			UStaticMesh* Mesh = Cast<UStaticMesh>(A.GetAsset());
			if (Mesh && !Worn.Contains(Mesh)) Out.Add(Mesh);
		}
		return Out;
	}

	TArray<int32> CompactMaterialSlots(FDynamicMesh3& Mesh)
	{
		TArray<int32> Used;

		UE::Geometry::FDynamicMeshMaterialAttribute* MatIDs =
			Mesh.HasAttributes() ? Mesh.Attributes()->GetMaterialID() : nullptr;
		if (MatIDs)
		{
			bool bSeen[MatSlot_Count] = {};
			for (int32 tid : Mesh.TriangleIndicesItr())
			{
				bSeen[FMath::Clamp(MatIDs->GetValue(tid), 0, MatSlot_Count - 1)] = true;
			}
			for (int32 Slot = 0; Slot < MatSlot_Count; ++Slot)
			{
				if (bSeen[Slot]) Used.Add(Slot);
			}

			// Slot order kept, so sections still follow enum order.
			int32 Compact[MatSlot_Count] = {};
			for (int32 i = 0; i < Used.Num(); ++i) Compact[Used[i]] = i;
			for (int32 tid : Mesh.TriangleIndicesItr())
			{
				MatIDs->SetValue(tid,
					Compact[FMath::Clamp(MatIDs->GetValue(tid), 0, MatSlot_Count - 1)]);
			}
		}

		if (Used.Num() == 0) Used.Add(MatSlot_Body);
		return Used;
	}

	void AssignPaletteMaterials(AStaticMeshActor* Actor, const FHutongPalette& Palette)
	{
		UStaticMeshComponent* Comp = Actor ? Actor->GetStaticMeshComponent() : nullptr;
		const UStaticMesh* Mesh = Comp ? Comp->GetStaticMesh() : nullptr;
		if (!Mesh) return;
		Comp->Modify();
		Comp->EmptyOverrideMaterials();
		const TArray<int32> Slots = SlotsOf(Mesh);
		for (int32 i = 0; i < Slots.Num(); ++i)
		{
			if (UMaterialInterface* M = ResolveSlotMaterial(Actor, Palette, Slots[i])) Comp->SetMaterial(i, M);
		}
	}

	void BuildAndAssignStaticMesh(
		AStaticMeshActor* Actor,
		FDynamicMesh3& Mesh,
		const FHutongPalette& Palette)
	{
		// A lone mesh is a chain of one.
		TArray<FDynamicMesh3> LODs;
		LODs.Emplace(MoveTemp(Mesh));
		BuildAndAssignStaticMesh(Actor, LODs, Palette);
	}

	void BuildAndAssignStaticMesh(
		AStaticMeshActor* Actor,
		TArray<FDynamicMesh3>& LODs,
		const FHutongPalette& Palette,
		int32 CollisionLOD)
	{
		if (!Actor || LODs.Num() == 0) return;

		// Priority: assigned material, then the project's starter material (palette fields are
		// session-only), then the tinted default. Worn as the component's overrides, so buildings
		// sharing a mesh keep their own palettes.
		auto PaletteMaterials = [&](const TArray<int32>& Slots)
		{
			TArray<UMaterialInterface*> Mats;
			for (const int32 Slot : Slots) Mats.Add(ResolveSlotMaterial(Actor, Palette, Slot));
			return Mats;
		};
		auto Assign = [&](UStaticMesh* StaticMesh, const TArray<UMaterialInterface*>& SlotMats)
		{
			UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent();
			if (!Comp) return;
			// Modify before reassigning so undo can restore the previous mesh.
			Comp->Modify();
			Comp->SetStaticMesh(StaticMesh);
			// SetStaticMesh keeps stale override entries; a build with fewer slots would serialise
			// unreachable references to the previous build.
			Comp->EmptyOverrideMaterials();
			for (int32 i = 0; i < SlotMats.Num(); ++i)
			{
				if (SlotMats[i]) Comp->SetMaterial(i, SlotMats[i]);
			}
			Comp->SetCollisionProfileName(TEXT("BlockAll"));
			// Query only: nothing simulates against a building.
			Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		};

		// The library: a building whose geometry is already baked takes that mesh, no build.
		const bool bShare = CVarShareMeshes.GetValueOnGameThread();
		const FString Key = bShare ? LibraryKey(LODs, CollisionLOD) : FString();
		if (bShare)
		{
			if (UStaticMesh* Shared = FindLibraryMesh(Key))
			{
				Assign(Shared, PaletteMaterials(SlotsOf(Shared)));
				return;
			}
		}

		// A library mesh is outside undo: undoing a placement must not revert an asset other buildings
		// share (an undone creation left one with no source data). The actor's reference is undoable.
		TOptional<TGuardValue<ITransaction*>> NoUndo;
		if (bShare) NoUndo.Emplace(GUndo, nullptr);

		// From the collision LOD as built, before bake winding: the fit does not care which way faces point.
		FKAggregateGeom CollisionGeom;
		BuildSimpleCollision(LODs[FMath::Clamp(CollisionLOD, 0, LODs.Num() - 1)], CollisionGeom);

		// Prepare every LOD before touching the asset.
		TArray<TArray<int32>> LODSlots;
		TArray<FMeshDescription> LODDescs;
		LODSlots.Reserve(LODs.Num());
		LODDescs.Reserve(LODs.Num());
		for (FDynamicMesh3& Mesh : LODs)
		{
			// Reverse first: normals must come from the final winding.
			// Hard above the angle, smooth below: 舉架 creases and column facets smooth, boxes crisp.
			// Kept under 36° (hip panels on a 五舉 eave) so a hip stays hard.
			Mesh.ReverseOrientation();
			Mesh.EnableAttributes();
			UE::Geometry::FMeshNormals::InitializeOverlayTopologyFromOpeningAngle(
				&Mesh, Mesh.Attributes()->PrimaryNormals(), SmoothingAngleDeg);
			UE::Geometry::FMeshNormals::QuickRecomputeOverlayNormals(Mesh);

			// UVs last: roofs wrote their own; ReverseOrientation keeps them.
			HutongMeshUtils::FillUnsetUVsBoxProjected(Mesh, 100.0);

			// Before conversion: it packs material IDs densely into polygon groups → this LOD's sections.
			LODSlots.Add(CompactMaterialSlots(Mesh));

			FMeshDescription& Desc = LODDescs.AddDefaulted_GetRef();
			FStaticMeshAttributes(Desc).Register();
			::FDynamicMeshToMeshDescription Converter;
			Converter.Convert(&Mesh, Desc);
		}

		// The asset's one material list: the union of what the chain wears, in enum order.
		TArray<int32> UsedSlots;
		{
			bool bSeen[MatSlot_Count] = {};
			for (const TArray<int32>& Slots : LODSlots)
			{
				for (const int32 Slot : Slots)
				{
					if (Slot >= 0 && Slot < MatSlot_Count) bSeen[Slot] = true;
				}
			}
			for (int32 Slot = 0; Slot < MatSlot_Count; ++Slot)
			{
				if (bSeen[Slot]) UsedSlots.Add(Slot);
			}
			if (UsedSlots.Num() == 0) UsedSlots.Add(MatSlot_Body);
		}

		const TArray<UMaterialInterface*> SlotMats = PaletteMaterials(UsedSlots);

		// Shared: its own package in the library, its slots wearing only what any building may (the
		// palette rides on each component). Bespoke: outered to the actor, saved in the actor's package.
		UStaticMesh* StaticMesh = nullptr;
		UPackage* LibraryPackage = nullptr;
		if (bShare)
		{
			LibraryPackage = CreatePackage(*(FString(LibraryRoot) / Key));
			StaticMesh = NewObject<UStaticMesh>(LibraryPackage, FName(*Key), RF_Public | RF_Standalone);
		}
		else
		{
			StaticMesh = NewObject<UStaticMesh>(Actor, NAME_None, RF_Public | RF_Transactional);
		}
		// Defaults were on; wrong for a city's worth of meshes.
		StaticMesh->bAllowCPUAccess = false;
		StaticMesh->NeverStream = false;
		// The chain's screen sizes are chosen, not measured off the bounds.
		StaticMesh->SetAutoComputeLODScreenSize(false);
		// Nanite renders LOD0 and bins by material; the chain is the non-Nanite fallback and
		// collision source. Fallback at 100%: LOD0 is a chosen level, not a scan.
		{
			FMeshNaniteSettings Nanite = StaticMesh->GetNaniteSettings();
			Nanite.bEnabled = true;
			Nanite.FallbackTarget = ENaniteFallbackTarget::PercentTriangles;
			Nanite.FallbackPercentTriangles = 1.0f;
			StaticMesh->SetNaniteSettings(Nanite);
		}
		// 遠 is the collision representation: no 勾頭 or 椽頭 in the physics triangles.
		StaticMesh->LODForCollision = FMath::Clamp(CollisionLOD, 0, LODDescs.Num() - 1);
		StaticMesh->InitResources();
		StaticMesh->SetLightingGuid();
		// Only the slots the mesh wears, in enum order.
		for (int32 i = 0; i < UsedSlots.Num(); ++i)
		{
			const FName Name = MaterialSlotName(UsedSlots[i]);
			UMaterialInterface* Own = bShare ? FindStarterMaterial(UsedSlots[i]) : SlotMats[i];
			if (bShare && !Own) Own = GetCachedDefaultMaterial();
			StaticMesh->GetStaticMaterials().Add(FStaticMaterial(Own, Name, Name));
		}

		for (int32 LOD = 0; LOD < LODDescs.Num(); ++LOD)
		{
			FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
			SrcModel.BuildSettings.bRecomputeNormals = false;   // keep the baked normals
			SrcModel.BuildSettings.bRecomputeTangents = true;
			// MikkTSpace needs UVs; the mesh has them.
			SrcModel.BuildSettings.bUseMikkTSpace = true;
			SrcModel.BuildSettings.bGenerateLightmapUVs = false;
			SrcModel.BuildSettings.bBuildReversedIndexBuffer = false;
			// No reduction: each LOD is its own build.
			SrcModel.ReductionSettings.PercentTriangles = 1.0f;
			SrcModel.ScreenSize.Default = Detail::LODScreenSize(LOD);

			if (FMeshDescription* Dest = StaticMesh->CreateMeshDescription(LOD))
			{
				*Dest = MoveTemp(LODDescs[LOD]);
				StaticMesh->CommitMeshDescription(LOD);
			}
			for (int32 i = 0; i < LODSlots[LOD].Num(); ++i)
			{
				const int32 SlotIndex = UsedSlots.IndexOfByKey(LODSlots[LOD][i]);
				StaticMesh->GetSectionInfoMap().Set(LOD, i,
					FMeshSectionInfo(FMath::Max(SlotIndex, 0)));
			}
		}

		// Collision flags must be set before Build so the cooked collision data matches them.
		// Simple shapes fitted to the collision LOD's pieces answer every query, complex ones included.
		StaticMesh->CreateBodySetup();
		if (UBodySetup* BodySetup = StaticMesh->GetBodySetup())
		{
			BodySetup->AggGeom = CollisionGeom;
			BodySetup->CollisionTraceFlag = CollisionGeom.GetElementCount() > 0 ? CTF_UseSimpleAsComplex : CTF_UseComplexAsSimple;
		}

		// No PostEditChange after Build: it runs a second Build (900 ms vs 7 ms on a 正房 chain).
		// Everything it would refresh (section info, body setup, materials) is already set.
		StaticMesh->Build(false);

		if (LibraryPackage)
		{
			FAssetRegistryModule::AssetCreated(StaticMesh);
			LibraryPackage->MarkPackageDirty();
			// On disk at once: a level saved without it would reload with its buildings missing their mesh.
			// Not under automation, which would litter the host project.
			if (!GIsAutomationTesting)
			{
				FSavePackageArgs Args;
				Args.TopLevelFlags = RF_Public | RF_Standalone;
				const FString File = FPackageName::LongPackageNameToFilename(
					LibraryPackage->GetName(), FPackageName::GetAssetPackageExtension());
				UPackage::SavePackage(LibraryPackage, StaticMesh, *File, Args);
			}
		}
		NoUndo.Reset();
		Assign(StaticMesh, SlotMats);
	}
}
