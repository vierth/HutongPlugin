#include "Generation/HutongActorSpawn.h"
#include "Components/InstancedStaticMeshComponent.h"
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
#include "StaticMeshCompiler.h"
#include "Containers/Ticker.h"
#include "Editor.h"
#include "UObject/ObjectSaveContext.h"

using UE::Geometry::FDynamicMesh3;

namespace
{
	const FName BuiltMeshTag(TEXT("HutongBuiltMesh"));
	const FName MeshInstancesTag(TEXT("HutongMeshInstances"));

	UStaticMeshComponent* TurnedMeshComponent(AStaticMeshActor* Actor, const HutongGen::FMeshTurn& Turn)
	{
		UStaticMeshComponent* Root = Actor->GetStaticMeshComponent();
		UStaticMeshComponent* Child = HutongGen::GetBuiltMeshComponent(Actor);
		if (!Child || Child == Root)
		{
			Child = NewObject<UStaticMeshComponent>(Actor, MakeUniqueObjectName(Actor, UStaticMeshComponent::StaticClass(), TEXT("BuiltMesh")), RF_Transactional);
			Child->ComponentTags.Add(BuiltMeshTag);
			Child->SetMobility(EComponentMobility::Static);
			Child->SetupAttachment(Actor->GetRootComponent());
			Actor->AddInstanceComponent(Child);
			Child->RegisterComponent();
		}
		const FTransform Placed = Turn.ToTransform();
		if (!Child->GetRelativeTransform().Equals(Placed, 1.0e-6))
		{
			Child->Modify();
			Child->SetRelativeTransform(Placed);
		}
		if (Root && Root != Child && Root->GetStaticMesh())
		{
			Root->Modify();
			Root->SetStaticMesh(nullptr);
			Root->EmptyOverrideMaterials();
		}
		return Child;
	}

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

	// BasicShapeMaterial is uneditable: tint via an instance, reused per colour from the actor's package.
	// Scans everything outered to the actor: a demotion (精 → 塊) leaves instances the material list dropped.
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

	TAutoConsoleVariable<bool> CVarLogBakeTiming(
		TEXT("hutong.LogBakeTiming"), false,
		TEXT("Logs how long each stage of a building's bake takes."));

	struct FBakeTimer
	{
		const TCHAR* Stage;
		double Start;
		explicit FBakeTimer(const TCHAR* InStage) : Stage(InStage), Start(FPlatformTime::Seconds()) {}
		~FBakeTimer()
		{
			if (CVarLogBakeTiming.GetValueOnGameThread())
			{
				UE_LOG(LogTemp, Display, TEXT("HutongBake %s: %.1f ms"), Stage, 1000.0 * (FPlatformTime::Seconds() - Start));
			}
		}
	};

	// Saved once their background build is done so placements carry on; flushed before a level save.
	TArray<TWeakObjectPtr<UStaticMesh>>& PendingLibrarySaves()
	{
		static TArray<TWeakObjectPtr<UStaticMesh>> Pending;
		return Pending;
	}

	void SaveLibraryMeshNow(UStaticMesh* Mesh)
	{
		UPackage* Package = Mesh ? Mesh->GetPackage() : nullptr;
		if (!Package) return;
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		UPackage::SavePackage(Package, Mesh, *File, Args);
	}

	void SavePendingLibraryMeshes(bool bWait)
	{
		TArray<TWeakObjectPtr<UStaticMesh>>& Pending = PendingLibrarySaves();
		if (Pending.Num() == 0) return;
		if (bWait)
		{
			TArray<UStaticMesh*> Building;
			for (const TWeakObjectPtr<UStaticMesh>& M : Pending) if (M.IsValid() && M->IsCompiling()) Building.Add(M.Get());
			if (Building.Num() > 0) FStaticMeshCompilingManager::Get().FinishCompilation(Building);
		}
		for (int32 i = Pending.Num() - 1; i >= 0; --i)
		{
			UStaticMesh* M = Pending[i].Get();
			if (M && M->IsCompiling()) continue;
			SaveLibraryMeshNow(M);
			Pending.RemoveAtSwap(i);
		}
	}

	FTSTicker::FDelegateHandle LibrarySaveTicker;
	FDelegateHandle LibraryPreSaveWorld;

	TAutoConsoleVariable<bool> CVarShareMeshes(
		TEXT("hutong.ShareMeshes"), true,
		TEXT("Buildings with identical geometry share one mesh asset under /Game/HutongLayout/Generated."));

	// Bump when the bake itself changes (normals, build settings, collision fit): same geometry, new asset.
	constexpr uint32 BakeVersion = 1;
	const TCHAR* LibraryRoot = TEXT("/Game/HutongLayout/Generated");

	// Shared vertex and UV element IDs count: baked normals smooth across them, so a weld alone changes the asset.
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

	// Cached because reading a mesh's materials waits for its background build.
	TMap<TWeakObjectPtr<const UStaticMesh>, TArray<int32>>& SlotCache()
	{
		static TMap<TWeakObjectPtr<const UStaticMesh>, TArray<int32>> Cache;
		return Cache;
	}

	TArray<int32> SlotsOf(const UStaticMesh* Mesh)
	{
		if (const TArray<int32>* Known = SlotCache().Find(Mesh)) return *Known;
		TArray<int32> Slots;
		for (const FStaticMaterial& M : Mesh->GetStaticMaterials())
		{
			for (int32 Slot = 0; Slot < HutongGen::MatSlot_Count; ++Slot)
			{
				if (HutongGen::MaterialSlotName(Slot) == M.MaterialSlotName) { Slots.Add(Slot); break; }
			}
		}
		SlotCache().Add(Mesh, Slots);
		return Slots;
	}
}

namespace HutongGen
{
	const double SmoothingAngleDeg = 32.0;

	FString LibraryKeyFor(const TArray<FDynamicMesh3>& LODs, int32 CollisionLOD)
	{
		return LibraryKey(LODs, CollisionLOD);
	}

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
	const FLinearColor DefaultEarthColor = FLinearColor::FromSRGBColor(FColor(168, 146, 112));
	const FLinearColor DefaultPartitionColor = FLinearColor::FromSRGBColor(FColor(158, 130, 96));
	const FLinearColor DefaultFloorColor = FLinearColor::FromSRGBColor(FColor(146, 149, 146));
	const FLinearColor DefaultPlaqueColor = FLinearColor::FromSRGBColor(FColor(30, 28, 27));
	const FLinearColor DefaultSignboardColor = FLinearColor::FromSRGBColor(FColor(226, 220, 204));
	const FLinearColor DefaultTradeSignColor = FLinearColor::FromSRGBColor(FColor(168, 42, 34));
	const FLinearColor DefaultCityBrickColor = FLinearColor::FromSRGBColor(FColor(122, 122, 116));
	const FLinearColor DefaultRedWallColor = FLinearColor::FromSRGBColor(FColor(168, 62, 46));
	const FLinearColor DefaultGlazedTileColor = FLinearColor::FromSRGBColor(FColor(214, 160, 44));

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

		// Static: fixed level geometry, and VSM caches its shadow pages.
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
		int32 CollisionLOD,
		const FMeshTurn& Turn)
	{
		if (LODs.Num() == 0) return nullptr;

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

		BuildAndAssignStaticMesh(Actor, LODs, Palette, CollisionLOD, false, Turn);
		return Actor;
	}

	UStaticMeshComponent* GetBuiltMeshComponent(const AActor* Actor)
	{
		if (!Actor) return nullptr;
		TArray<UStaticMeshComponent*> Meshes;
		Actor->GetComponents(Meshes);
		for (UStaticMeshComponent* C : Meshes)
		{
			if (C->ComponentHasTag(BuiltMeshTag)) return C;
		}
		return Cast<UStaticMeshComponent>(Actor->GetRootComponent());
	}

	TArray<FAssetData> FindUnusedLibraryMeshes()
	{
		TArray<FAssetData> Out;
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPath(FName(LibraryRoot), Assets, /*bRecursive*/ true);
		if (Assets.Num() == 0) return Out;

		TSet<FName> Worn;
		for (TObjectIterator<UStaticMeshComponent> It; It; ++It)
		{
			if (It->IsTemplate() || !IsValid(*It)) continue;
			if (const UStaticMesh* M = It->GetStaticMesh()) Worn.Add(M->GetPackage()->GetFName());
		}
		for (const FAssetData& A : Assets)
		{
			if (Worn.Contains(A.PackageName) || !A.IsInstanceOf<UStaticMesh>()) continue;
			TArray<FName> Referencers;
			Registry.GetReferencers(A.PackageName, Referencers);
			Referencers.Remove(A.PackageName);
			if (Referencers.Num() == 0) Out.Add(A);
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
		TArray<UStaticMeshComponent*> Wearers;
		Actor->GetComponents(Wearers);
		for (UStaticMeshComponent* Comp : Wearers)
		{
			const bool bWorn = Comp == GetBuiltMeshComponent(Actor) || Comp->ComponentHasTag(MeshInstancesTag);
			const UStaticMesh* Mesh = bWorn ? Comp->GetStaticMesh() : nullptr;
			if (!Mesh) continue;
			Comp->Modify();
			Comp->EmptyOverrideMaterials();
			const TArray<int32> Slots = SlotsOf(Mesh);
			for (int32 i = 0; i < Slots.Num(); ++i)
			{
				if (UMaterialInterface* M = ResolveSlotMaterial(Actor, Palette, Slots[i])) Comp->SetMaterial(i, M);
			}
		}
	}

	void BuildAndAssignStaticMesh(
		AStaticMeshActor* Actor,
		FDynamicMesh3& Mesh,
		const FHutongPalette& Palette)
	{
		TArray<FDynamicMesh3> LODs;
		LODs.Emplace(MoveTemp(Mesh));
		BuildAndAssignStaticMesh(Actor, LODs, Palette);
	}

	using FAssignBaked = TFunctionRef<void(UStaticMesh*, const TArray<UMaterialInterface*>&)>;

	static void FindOrBakeMesh(
		AStaticMeshActor* Actor,
		TArray<FDynamicMesh3>& LODs,
		const FHutongPalette& Palette,
		int32 CollisionLOD,
		bool bBespoke,
		FAssignBaked Assign)
	{
		// Worn as the component's overrides, so buildings sharing a mesh keep their own palettes.
		auto PaletteMaterials = [&](const TArray<int32>& Slots)
		{
			TArray<UMaterialInterface*> Mats;
			for (const int32 Slot : Slots) Mats.Add(ResolveSlotMaterial(Actor, Palette, Slot));
			return Mats;
		};
		const bool bShare = CVarShareMeshes.GetValueOnGameThread() && !bBespoke;
		FString Key;
		{
			FBakeTimer T(TEXT("library key"));
			Key = bShare ? LibraryKey(LODs, CollisionLOD) : FString();
		}
		if (bShare)
		{
			FBakeTimer T(TEXT("library hit: find and dress"));
			if (UStaticMesh* Shared = FindLibraryMesh(Key))
			{
				Assign(Shared, PaletteMaterials(SlotsOf(Shared)));
				return;
			}
		}

		// A library mesh is outside undo, so undoing a placement leaves an asset other buildings share intact.
		TOptional<TGuardValue<ITransaction*>> NoUndo;
		if (bShare) NoUndo.Emplace(GUndo, nullptr);

		FKAggregateGeom CollisionGeom;
		{
			FBakeTimer T(TEXT("collision fit"));
			BuildSimpleCollision(LODs[FMath::Clamp(CollisionLOD, 0, LODs.Num() - 1)], CollisionGeom);
		}
		TOptional<FBakeTimer> PrepTimer(InPlace, TEXT("normals, UVs, conversion"));

		TArray<TArray<int32>> LODSlots;
		TArray<FMeshDescription> LODDescs;
		LODSlots.Reserve(LODs.Num());
		LODDescs.Reserve(LODs.Num());
		for (FDynamicMesh3& Mesh : LODs)
		{
			// Reverse first: normals must come from the final winding.
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

		PrepTimer.Reset();
		TArray<UMaterialInterface*> SlotMats;
		{
			FBakeTimer T(TEXT("materials"));
			SlotMats = PaletteMaterials(UsedSlots);
		}

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
		StaticMesh->bAllowCPUAccess = false;
		StaticMesh->NeverStream = false;
		StaticMesh->SetAutoComputeLODScreenSize(false);
		// Nanite fallback at 100%: LOD0 is a chosen level.
		{
			FMeshNaniteSettings Nanite = StaticMesh->GetNaniteSettings();
			Nanite.bEnabled = true;
			Nanite.FallbackTarget = ENaniteFallbackTarget::PercentTriangles;
			Nanite.FallbackPercentTriangles = 1.0f;
			StaticMesh->SetNaniteSettings(Nanite);
		}
		// 遠 is the collision representation, keeping 勾頭 and 椽頭 out of the physics triangles.
		StaticMesh->LODForCollision = FMath::Clamp(CollisionLOD, 0, LODDescs.Num() - 1);
		StaticMesh->InitResources();
		StaticMesh->SetLightingGuid();
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
			SrcModel.BuildSettings.bRecomputeNormals = false;
			SrcModel.BuildSettings.bRecomputeTangents = true;
			SrcModel.BuildSettings.bUseMikkTSpace = true;
			SrcModel.BuildSettings.bGenerateLightmapUVs = false;
			SrcModel.BuildSettings.bBuildReversedIndexBuffer = false;
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
		StaticMesh->CreateBodySetup();
		if (UBodySetup* BodySetup = StaticMesh->GetBodySetup())
		{
			BodySetup->AggGeom = CollisionGeom;
			BodySetup->CollisionTraceFlag = CollisionGeom.GetElementCount() > 0 ? CTF_UseSimpleAsComplex : CTF_UseComplexAsSimple;
		}

		// Build alone: PostEditChange runs a second Build (900 ms vs 7 ms on a 正房 chain).
		{
			FBakeTimer T(TEXT("static mesh build (Nanite, LODs, collision cook)"));
			StaticMesh->Build(false);
		}

		if (LibraryPackage)
		{
			FAssetRegistryModule::AssetCreated(StaticMesh);
			LibraryPackage->MarkPackageDirty();
			// Skipped under automation, which would litter the host project.
			if (!GIsAutomationTesting) PendingLibrarySaves().AddUnique(StaticMesh);
		}
		SlotCache().Add(StaticMesh, UsedSlots);
		NoUndo.Reset();
		Assign(StaticMesh, SlotMats);
	}

	static void WearBakedMesh(UStaticMeshComponent* Comp, UStaticMesh* StaticMesh, const TArray<UMaterialInterface*>& SlotMats)
	{
		Comp->SetStaticMesh(StaticMesh);
		// SetStaticMesh keeps stale override entries past the new slot count.
		Comp->EmptyOverrideMaterials();
		for (int32 i = 0; i < SlotMats.Num(); ++i)
		{
			if (SlotMats[i]) Comp->SetMaterial(i, SlotMats[i]);
		}
		Comp->SetCollisionProfileName(TEXT("BlockAll"));
		Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	static TArray<UInstancedStaticMeshComponent*> TaggedInstances(const AActor* Actor)
	{
		TArray<UInstancedStaticMeshComponent*> Instances;
		if (Actor) Actor->GetComponents(Instances);
		Instances.RemoveAll([](const UInstancedStaticMeshComponent* Comp) { return !Comp->ComponentHasTag(MeshInstancesTag); });
		return Instances;
	}

	void BuildAndAssignStaticMesh(
		AStaticMeshActor* Actor,
		TArray<FDynamicMesh3>& LODs,
		const FHutongPalette& Palette,
		int32 CollisionLOD,
		bool bBespoke,
		const FMeshTurn& Turn)
	{
		if (!Actor || LODs.Num() == 0) return;
		ClearMeshInstances(Actor);
		auto Assign = [&](UStaticMesh* StaticMesh, const TArray<UMaterialInterface*>& SlotMats)
		{
			UStaticMeshComponent* Comp = TurnedMeshComponent(Actor, Turn);
			if (!Comp) return;
			// Modify before reassigning so undo can restore the previous mesh.
			Comp->Modify();
			WearBakedMesh(Comp, StaticMesh, SlotMats);
		};
		FindOrBakeMesh(Actor, LODs, Palette, CollisionLOD, bBespoke, Assign);
	}

	void ClearMeshInstances(AStaticMeshActor* Actor)
	{
		for (UInstancedStaticMeshComponent* Comp : TaggedInstances(Actor))
		{
			Comp->Modify();
			Actor->RemoveInstanceComponent(Comp);
			Comp->DestroyComponent();
		}
	}

	void AssignMeshInstances(AStaticMeshActor* Actor, TArray<FMeshInstances>& Groups, const FHutongPalette& Palette)
	{
		if (!Actor) return;
		ClearMeshInstances(Actor);
		if (UStaticMeshComponent* Built = GetBuiltMeshComponent(Actor))
		{
			Built->Modify();
			Built->SetStaticMesh(nullptr);
		}
		for (FMeshInstances& Group : Groups)
		{
			if (Group.LODs.Num() == 0 || Group.Placed.Num() == 0) continue;
			FindOrBakeMesh(Actor, Group.LODs, Palette, Group.CollisionLOD, /*bBespoke*/ false,
				[Actor, &Group](UStaticMesh* Mesh, const TArray<UMaterialInterface*>& SlotMats)
				{
					UInstancedStaticMeshComponent* Comp = NewObject<UInstancedStaticMeshComponent>(Actor,
						MakeUniqueObjectName(Actor, UInstancedStaticMeshComponent::StaticClass()), RF_Transactional);
					Comp->ComponentTags.Add(MeshInstancesTag);
					Comp->SetMobility(EComponentMobility::Static);
					Comp->SetupAttachment(Actor->GetRootComponent());
					Actor->AddInstanceComponent(Comp);
					WearBakedMesh(Comp, Mesh, SlotMats);
					Comp->RegisterComponent();
					Comp->AddInstances(Group.Placed, /*bShouldReturnIndices*/ false);
				});
		}
	}

	int64 CountBuiltTriangles(const AActor* Actor, bool bWaitForBuild)
	{
		int64 Triangles = 0;
		const UStaticMeshComponent* Built = GetBuiltMeshComponent(Actor);
		// Reading a mesh still building in the background waits for it ("Waiting for Static Meshes"): say so instead.
		if (const UStaticMesh* Mesh = Built ? Built->GetStaticMesh() : nullptr)
		{
			if (!bWaitForBuild && Mesh->IsCompiling()) return -1;
			if (Mesh->GetNumLODs() > 0) Triangles += Mesh->GetNumTriangles(0);
		}
		for (const UInstancedStaticMeshComponent* Comp : TaggedInstances(Actor))
		{
			const UStaticMesh* Mesh = Comp->GetStaticMesh();
			if (!bWaitForBuild && Mesh && Mesh->IsCompiling()) return -1;
			if (Mesh && Mesh->GetNumLODs() > 0)
			{
				Triangles += int64(Mesh->GetNumTriangles(0)) * Comp->GetInstanceCount();
			}
		}
		return Triangles;
	}

	void CountMeshInstances(const AActor* Actor, int32& OutPlaced, int32& OutMeshes)
	{
		OutPlaced = OutMeshes = 0;
		for (const UInstancedStaticMeshComponent* Comp : TaggedInstances(Actor))
		{
			OutPlaced += Comp->GetInstanceCount();
			++OutMeshes;
		}
	}

	void StartLibrarySaver()
	{
		LibrarySaveTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			SavePendingLibraryMeshes(false);
			return true;
		}), 0.5f);
		LibraryPreSaveWorld = FEditorDelegates::PreSaveWorldWithContext.AddLambda([](UWorld*, FObjectPreSaveContext)
		{
			SavePendingLibraryMeshes(true);
		});
	}

	void StopLibrarySaver()
	{
		SavePendingLibraryMeshes(true);
		FTSTicker::GetCoreTicker().RemoveTicker(LibrarySaveTicker);
		FEditorDelegates::PreSaveWorldWithContext.Remove(LibraryPreSaveWorld);
	}

	bool MakeMeshUnique(AStaticMeshActor* Actor)
	{
		UStaticMeshComponent* Comp = GetBuiltMeshComponent(Actor);
		UStaticMesh* Shared = Comp ? Comp->GetStaticMesh() : nullptr;
		if (!Shared || Shared->GetOuter() == Actor) return false;
		Actor->Modify();
		Comp->Modify();
		UStaticMesh* Own = DuplicateObject<UStaticMesh>(Shared, Actor);
		Own->ClearFlags(RF_Standalone);
		Own->SetFlags(RF_Public | RF_Transactional);
		Own->Build(false);
		Comp->SetStaticMesh(Own);
		SlotCache().Add(Own, SlotsOf(Shared));
		return true;
	}
}
