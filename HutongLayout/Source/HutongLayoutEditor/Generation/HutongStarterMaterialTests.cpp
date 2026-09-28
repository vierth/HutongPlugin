#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#include "Generation/HutongPalette.h"
#include "Generation/HutongStarterMaterials.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Generation/HutongActorSpawn.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/SiheyuanGenerator.h"
#include "Generation/HutongMeshInspect.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "ShaderCompiler.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongStarterMaterialNamingTest,
	"HutongLayout.Appearance.StarterMaterials",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongStarterMaterialNamingTest::RunTest(const FString& Parameters)
{
	// Each slot has its own, uniquely named starter asset.
	TSet<FString> Names;
	for (int32 Slot = 0; Slot < HutongGen::MatSlot_Count; ++Slot)
	{
		const FString Name = HutongGen::StarterMaterialAssetName(Slot);
		TestFalse(FString::Printf(TEXT("slot %d has a starter asset name"), Slot), Name.IsEmpty());
		TestTrue(FString::Printf(TEXT("slot %d's asset is named as a material"), Slot),
			Name.StartsWith(TEXT("M_Hutong_")));
		bool bAlready = false;
		Names.Add(Name, &bAlready);
		TestFalse(FString::Printf(TEXT("slot %d's asset name is its own"), Slot), bAlready);
	}
	TestTrue(TEXT("a slot outside the enum has no asset"),
		HutongGen::StarterMaterialAssetName(HutongGen::MatSlot_Count).IsEmpty());

	// A missing asset is an ordinary answer, not an error: every spawn asks.
	TestNull(TEXT("an out-of-range slot finds nothing"),
		HutongGen::FindStarterMaterial(HutongGen::MatSlot_Count));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongStarterMaterialCreateTest,
	"HutongLayout.Appearance.StarterMaterialsCreate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongStarterMaterialCreateTest::RunTest(const FString& Parameters)
{
	// Project temp mount: the test leaves nothing the project keeps.
	const FString Path = FString::Printf(TEXT("/Temp/HutongTests/Materials_%s"), *FGuid::NewGuid().ToString().Left(8));
	TArray<FString> Created, Skipped;
	const int32 Count = HutongGen::CreateStarterMaterialsAt(Path, Created, Skipped);
	TestEqual(TEXT("one material per slot is created"), Count, (int32)HutongGen::MatSlot_Count);
	TestEqual(TEXT("nothing was skipped on a fresh path"), Skipped.Num(), 0);

	// Second run finds all, writes none.
	HutongGen::CreateStarterMaterialsAt(Path, Created, Skipped);
	TestEqual(TEXT("a second run creates nothing"), Created.Num(), 0);
	TestEqual(TEXT("a second run skips every slot"), Skipped.Num(), (int32)HutongGen::MatSlot_Count);

	// Each material wired: base colour from the Color parameter, a roughness, and the palette's Color
	// default for its slot (roof darker than body).
	auto Loaded = [&](int32 Slot) -> UMaterial*
	{
		const FString Name = HutongGen::StarterMaterialAssetName(Slot);
		return FindObject<UMaterial>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Path, *Name, *Name));
	};
	for (int32 Slot = 0; Slot < HutongGen::MatSlot_Count; ++Slot)
	{
		UMaterial* M = Loaded(Slot);
		if (!TestNotNull(*FString::Printf(TEXT("slot %d's material is loaded"), Slot), M)) continue;
		const UMaterialEditorOnlyData* EO = M->GetEditorOnlyData();
		TestNotNull(*FString::Printf(TEXT("slot %d base colour is connected"), Slot),
			EO ? (UMaterialExpression*)EO->BaseColor.Expression : nullptr);
		TestNotNull(*FString::Printf(TEXT("slot %d roughness is connected"), Slot),
			EO ? (UMaterialExpression*)EO->Roughness.Expression : nullptr);
		// Patterned slot: normal from the pattern; roughness runs through the multiply carrying its figure.
		if (HutongGen::StarterMaterialHasRelief(Slot))
		{
			TestNotNull(*FString::Printf(TEXT("slot %d's normal is the pattern's"), Slot),
				EO ? (UMaterialExpression*)EO->Normal.Expression : nullptr);
			TestNotNull(*FString::Printf(TEXT("slot %d's roughness is scaled by the pattern"), Slot),
				EO ? Cast<UMaterialExpressionMultiply>(EO->Roughness.Expression) : nullptr);
		}
		else
		{
			TestNull(*FString::Printf(TEXT("a flat slot (%d) leaves the normal alone"), Slot),
				EO ? (UMaterialExpression*)EO->Normal.Expression : nullptr);
		}
		FLinearColor Colour;
		TestTrue(*FString::Printf(TEXT("slot %d carries a Color parameter"), Slot),
			M->GetVectorParameterDefaultValue(FMaterialParameterInfo(TEXT("Color")), Colour));
		TestTrue(*FString::Printf(TEXT("slot %d's Color is the palette's"), Slot),
			Colour.Equals(FHutongPalette().GetSlotColor(Slot), 1.0e-4f));
	}

	// Custom nodes are HLSL: with a real RHI they compile here and errors fail the test; under the null
	// RHI nothing compiles and nothing is claimed.
	if (GShaderCompilingManager)
	{
		GShaderCompilingManager->FinishAllCompilation();
	}
	for (int32 Slot = 0; Slot < HutongGen::MatSlot_Count; ++Slot)
	{
		UMaterial* M = Loaded(Slot);
		FMaterialResource* Resource = M ? M->GetMaterialResource(GMaxRHIShaderPlatform) : nullptr;
		if (GUsingNullRHI) continue;
		const FString Name = HutongGen::StarterMaterialAssetName(Slot);
		if (!TestNotNull(*FString::Printf(TEXT("%s has a material resource"), *Name), Resource)) continue;
		for (const FString& Error : Resource->GetCompileErrors())
		{
			AddError(FString::Printf(TEXT("%s: %s"), *Name, *Error));
		}
		TestNotNull(*FString::Printf(TEXT("%s compiled to a shader map"), *Name),
			Resource->GetGameThreadShaderMap());
	}

	// Files are deleted; loaded packages persist for the session, hence a fresh folder per run.
	IFileManager::Get().DeleteDirectory(*(FPaths::ProjectSavedDir() / TEXT("HutongTests")), false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongStarterMaterialSlotNamesTest,
	"HutongLayout.Appearance.SlotNamesOnMesh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongStarterMaterialSlotNamesTest::RunTest(const FString& Parameters)
{
	// Whatever material a slot wears, the baked mesh names the slot.
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	UHutongSiheyuanBuildingComponent* Template =
		NewObject<UHutongSiheyuanBuildingComponent>(GetTransientPackage());
	Template->DetailLevel = EHutongDetail::Near;
	Template->bBuildLODChain = false;
	TArray<UE::Geometry::FDynamicMesh3> LODs;
	Template->BuildLODs(LODs);

	AStaticMeshActor* Actor = HutongGen::SpawnStaticMeshActor(World, LODs, FTransform::Identity,
		TEXT("SlotNames"), FHutongPalette());
	if (!TestNotNull(TEXT("the house is spawned"), Actor)) { World->DestroyWorld(false); return false; }

	UStaticMesh* Mesh = Actor->GetStaticMeshComponent()->GetStaticMesh();
	if (TestNotNull(TEXT("the actor carries a mesh"), Mesh))
	{
		const TArray<FStaticMaterial>& Mats = Mesh->GetStaticMaterials();
		TestTrue(TEXT("the mesh wears several slots"), Mats.Num() >= 5);
		for (int32 i = 0; i < Mats.Num(); ++i)
		{
			TestFalse(*FString::Printf(TEXT("slot %d is named"), i), Mats[i].MaterialSlotName.IsNone());
			TestNotNull(*FString::Printf(TEXT("slot %d has a material"), i), Mats[i].MaterialInterface.Get());
		}
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongGablePedimentSlotTest,
	"HutongLayout.Appearance.GablePediment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongGablePedimentSlotTest::RunTest(const FString& Parameters)
{
	// On a 硬山 house the face above the eave line on the gable is brick, not tile.
	FHutongSiheyuanParams P;
	P.Width = 1040.0; P.Depth = 600.0;

	UE::Geometry::FDynamicMesh3 Mesh;
	HutongGen::BuildSiheyuan(Mesh, P);
	const UE::Geometry::FDynamicMeshMaterialAttribute* Mat =
		Mesh.HasAttributes() ? Mesh.Attributes()->GetMaterialID() : nullptr;
	if (!TestNotNull(TEXT("the mesh carries material IDs"), (void*)Mat)) return false;

	// Faces on the +X gable plane, above the eave and below the apex ridge-strip cap. Primitives are
	// CCW-from-outside (the bake reverses), so an outward face's dynamic-mesh normal points inward.
	const double Eave = P.GetRoofBaseHeight();
	const double Top = Eave + 0.6 * P.GetRoofRise();
	int32 Brick = 0, Tile = 0;
	for (int32 tid : Mesh.TriangleIndicesItr())
	{
		if (Mesh.GetTriNormal(tid).X > -0.99) continue;
		const FVector3d C = Mesh.GetTriCentroid(tid);
		// Within the gable wall's depth: behind it is the 封護檐 drip course, whose end is tile.
		if (FMath::Abs(C.X - P.Width) > 1.0 || C.Z < Eave + 5.0 || C.Z > Top || C.Y < 0.0 || C.Y > P.Depth) continue;
		(Mat->GetValue(tid) == HutongGen::MatSlot_Body ? Brick : Tile)++;
	}
	TestTrue(TEXT("the pediment has brick faces"), Brick > 0);
	TestEqual(TEXT("no tile on the gable plane above the eave"), Tile, 0);
	return true;
}
