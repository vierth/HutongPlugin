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
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

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

	// BasicShapeMaterial is white and has no asset we may edit, so tint it through an instance.
	// A rebuild used to mint a fresh instance for every slot every time. They are outered to the
	// actor and saved with it, so each rebuild left the discarded ones in the actor's own package —
	// and a building edited a dozen times carried a dozen dead instances. Reused by colour, since
	// the tint is the only thing that varies.
	// Over everything outered to the actor, not the component's current material list. A build that
	// wears fewer slots than the last one — a demotion, 塊 against 精 — leaves its predecessors in
	// the actor's package where that list cannot reach them, so scanning it minted a second
	// instance for a colour the package already carried, and a promotion back paid for it again.
	UMaterialInterface* FindReusableTint(UObject* Outer, const FLinearColor& Color, UMaterialInterface* Base)
	{
		if (!Outer || !Base) return nullptr;

		UMaterialInterface* Found = nullptr;
		ForEachObjectWithOuter(Outer, [&](UObject* Obj)
		{
			if (Found) return;

			UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Obj);
			if (!MIC || MIC->Parent != Base) return;

			FLinearColor Existing;
			if (MIC->GetVectorParameterValue(FMaterialParameterInfo(TEXT("Color")), Existing)
				&& Existing.Equals(Color, 1.0e-4f))
			{
				Found = MIC;
			}
		}, /*bIncludeNestedObjects*/ false);

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
}

namespace HutongGen
{
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
	// Dry swept loess, which is what a Beijing courtyard floor is between its paving.
	const FLinearColor DefaultEarthColor = FLinearColor::FromSRGBColor(FColor(168, 146, 112));
	// Plain boarding — waxed pine rather than the lacquer on a column. Warm, light, and quiet.
	const FLinearColor DefaultPartitionColor = FLinearColor::FromSRGBColor(FColor(158, 130, 96));

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
		const FHutongPalette& Palette)
	{
		if (LODs.Num() == 0) return nullptr;
		if (LODs.Num() == 1)
		{
			return SpawnStaticMeshActor(World, LODs[0], Transform, NameBase, Palette);
		}

		// The single-mesh path spawns and bakes in one call, and the actor has to exist before the mesh either way.
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

		BuildAndAssignStaticMesh(Actor, LODs, Palette);
		return Actor;
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

			// In slot order, so the sections still come out in the order the enum declares even though the numbering is now the mesh's own.
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

	void BuildAndAssignStaticMesh(
		AStaticMeshActor* Actor,
		FDynamicMesh3& Mesh,
		const FHutongPalette& Palette)
	{
		// One implementation, and it takes the chain: a lone mesh is a chain of one.
		TArray<FDynamicMesh3> LODs;
		LODs.Emplace(MoveTemp(Mesh));
		BuildAndAssignStaticMesh(Actor, LODs, Palette);
	}

	void BuildAndAssignStaticMesh(
		AStaticMeshActor* Actor,
		TArray<FDynamicMesh3>& LODs,
		const FHutongPalette& Palette)
	{
		if (!Actor || LODs.Num() == 0) return;

		// Every LOD is prepared the same way, and all of them before the asset is touched.
		TArray<TArray<int32>> LODSlots;
		TArray<FMeshDescription> LODDescs;
		LODSlots.Reserve(LODs.Num());
		LODDescs.Reserve(LODs.Num());
		for (FDynamicMesh3& Mesh : LODs)
		{
			// Reverse first, then bake normals: per-triangle normals must be derived from the final winding.
			Mesh.ReverseOrientation();
			Mesh.EnableAttributes();
			UE::Geometry::FMeshNormals::InitializeOverlayToPerTriangleNormals(
				Mesh.Attributes()->PrimaryNormals());

			// UVs last: the roofs wrote their own during the build and ReverseOrientation carried them through the flip.
			HutongMeshUtils::FillUnsetUVsBoxProjected(Mesh, 100.0);

			// Before the conversion: it packs material IDs into polygon groups densely up to the highest one used, and those become this LOD's sections.
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

		// An assigned material is used as-is and replaces the tint. With nothing assigned, the
		// project's starter material for the slot, if it has been created: the palette's material
		// fields live only for the session, and setting twelve of them on every tool is not how a
		// street gets its brick. Only a slot with neither gets the tinted default.
		TArray<UMaterialInterface*> SlotMats;
		SlotMats.Reserve(UsedSlots.Num());
		for (const int32 Slot : UsedSlots)
		{
			UMaterialInterface* Assigned = Palette.GetSlotMaterial(Slot);
			if (!Assigned) Assigned = FindStarterMaterial(Slot);
			SlotMats.Add(Assigned ? Assigned
								  : CreateTintedMaterial(Actor, Palette.GetSlotColor(Slot)));
		}

		// Outered to the actor so it saves into the actor's own package.
		UStaticMesh* StaticMesh = NewObject<UStaticMesh>(
			Actor, NAME_None, RF_Public | RF_Transactional);
		// Both were on, and both are wrong for a city.
		StaticMesh->bAllowCPUAccess = false;
		StaticMesh->NeverStream = false;
		// The chain's screen sizes are chosen, not measured off the bounds.
		StaticMesh->bAutoComputeLODScreenSize = false;
		StaticMesh->InitResources();
		StaticMesh->SetLightingGuid();
		// Exactly the slots the mesh wears, in enum order, each named for what it is.
		for (int32 i = 0; i < UsedSlots.Num(); ++i)
		{
			const FName Name = MaterialSlotName(UsedSlots[i]);
			StaticMesh->GetStaticMaterials().Add(FStaticMaterial(SlotMats[i], Name, Name));
		}

		for (int32 LOD = 0; LOD < LODDescs.Num(); ++LOD)
		{
			// Build through a SourceModel.
			FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
			SrcModel.BuildSettings.bRecomputeNormals = false;   // keep the baked per-triangle normals
			SrcModel.BuildSettings.bRecomputeTangents = true;
			// MikkTSpace derives the tangent frame from UVs, which there now are.
			SrcModel.BuildSettings.bUseMikkTSpace = true;
			SrcModel.BuildSettings.bGenerateLightmapUVs = false;
			SrcModel.BuildSettings.bBuildReversedIndexBuffer = false;
			// No reduction. Every LOD here is its own build of the building at a coarser level.
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
			BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple;
		}

		// Build, and **not** PostEditChange after it: UStaticMesh::PostEditChangeProperty ends in a
		// second Build of its own, with UV channel data and lightmap restrictions on top. Measured
		// on a 正房 chain it is 900 ms against 7 — the whole of the frozen editor a placement costs,
		// and a dozen seconds of it on a compound. Everything it would refresh (the section info
		// map, the body setup, the material list) is set before the Build above.
		StaticMesh->Build(false);

		if (UStaticMeshComponent* Comp = Actor->GetStaticMeshComponent())
		{
			// Modify before reassigning: on a rebuild this swaps a mesh reference that undo has to be able to put back.
			Comp->Modify();
			Comp->SetStaticMesh(StaticMesh);
			// SetStaticMesh does not prune the override array, so a build wearing fewer slots than
			// the last one left entries for slots this mesh has not got — live references into the
			// previous build that serialise with the actor and can never be reached again.
			Comp->EmptyOverrideMaterials();
			for (int32 i = 0; i < SlotMats.Num(); ++i)
			{
				if (SlotMats[i]) Comp->SetMaterial(i, SlotMats[i]);
			}
			Comp->SetCollisionProfileName(TEXT("BlockAll"));
			// Query only: nothing simulates against a building.
			Comp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}
	}
}
