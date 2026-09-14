#include "Tools/HutongExchange.h"
#include "Tools/HutongDetailOps.h"
#include "Tools/HutongImportTypes.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongActorSpawn.h"
#include "Generation/HutongPalette.h"
#include "Tools/HutongPresetDefaults.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "JsonObjectConverter.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

// What a scene file has to survive. The exchange has no per-type code.

using UE::Geometry::FDynamicMesh3;

namespace
{
	// Something distinct in every field the exchange claims to carry, reached by reflection so a property added next year is perturbed the day it is added.
	void PerturbStruct(UStruct* Type, void* Container, int32& Counter)
	{
		for (TFieldIterator<FProperty> It(Type); It; ++It)
		{
			FProperty* Prop = *It;
			if (!Prop->HasAnyPropertyFlags(CPF_Edit)) continue;
			if (Prop->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated)) continue;
			// This plugin's own properties only.
			if (const UClass* Owner = Prop->GetOwnerClass())
			{
				if (!Owner->IsChildOf(UHutongBuildingComponent::StaticClass())) continue;
			}
			// An object reference is an asset path, covered by its own test; perturbing it here would mean inventing an asset.
			if (Prop->IsA<FObjectPropertyBase>()) continue;

			void* Value = Prop->ContainerPtrToValuePtr<void>(Container);
			++Counter;

			if (FBoolProperty* B = CastField<FBoolProperty>(Prop))
			{
				B->SetPropertyValue(Value, !B->GetPropertyValue(Value));
			}
			else if (FEnumProperty* E = CastField<FEnumProperty>(Prop))
			{
				const UEnum* Enum = E->GetEnum();
				FNumericProperty* Underlying = E->GetUnderlyingProperty();
				if (Enum && Underlying && Enum->NumEnums() > 2)
				{
					// NumEnums() counts the generated _MAX, so the last real entry is at -2.
					const int64 Pick = Enum->GetValueByIndex((Counter % (Enum->NumEnums() - 1)));
					Underlying->SetIntPropertyValue(Value, Pick);
				}
			}
			else if (FNumericProperty* N = CastField<FNumericProperty>(Prop))
			{
				if (N->IsFloatingPoint()) { N->SetFloatingPointPropertyValue(Value, 3.0 + Counter); }
				else { N->SetIntPropertyValue(Value, (int64)(1 + (Counter % 7))); }
			}
			else if (FStructProperty* S = CastField<FStructProperty>(Prop))
			{
				PerturbStruct(S->Struct, Value, Counter);
			}
		}
	}

	FString Serialise(const UHutongBuildingComponent* C)
	{
		FString Out;
		FJsonObjectConverter::UStructToJsonObjectString(C->GetClass(), C, Out,
			CPF_Edit, CPF_Transient | CPF_Deprecated);
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongExchangeRoundTripTest,
	"HutongLayout.Exchange.ComponentRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongExchangeRoundTripTest::RunTest(const FString& Parameters)
{
	TMap<FName, UClass*> Classes;
	HutongExchange::GatherBuildingComponentClasses(Classes);
	TestTrue(TEXT("every building type is discoverable"), Classes.Num() >= 14);

	for (const TPair<FName, UClass*>& Pair : Classes)
	{
		UHutongBuildingComponent* Source = NewObject<UHutongBuildingComponent>(
			GetTransientPackage(), Pair.Value);
		if (!TestNotNull(*FString::Printf(TEXT("%s constructs"), *Pair.Key.ToString()), Source))
		{
			continue;
		}

		int32 Counter = 0;
		PerturbStruct(Source->GetClass(), Source, Counter);
		TestTrue(*FString::Printf(TEXT("%s has parameters to carry"), *Pair.Key.ToString()),
			Counter > 3);

		TSharedPtr<FJsonObject> Blob = HutongExchange::WriteComponent(Source);
		if (!TestTrue(*FString::Printf(TEXT("%s writes a blob"), *Pair.Key.ToString()),
			Blob.IsValid()))
		{
			continue;
		}

		UHutongBuildingComponent* Target = NewObject<UHutongBuildingComponent>(
			GetTransientPackage(), Pair.Value);
		FString Problem;
		TestTrue(*FString::Printf(TEXT("%s reads back: %s"), *Pair.Key.ToString(), *Problem),
			HutongExchange::ReadComponent(Blob.ToSharedRef(), Target, Problem));

		// Compared as serialised strings.
		TestEqual(*FString::Printf(TEXT("%s round-trips unchanged"), *Pair.Key.ToString()),
			Serialise(Target), Serialise(Source));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongExchangeExportableTest,
	"HutongLayout.Exchange.EveryPropertyIsExportable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongExchangeExportableTest::RunTest(const FString& Parameters)
{
	// CPF_Edit is the whole serialisation contract, and its failure mode is silence. Nothing on a
	// building component is exempt any more: the two that were are gone with the lights.

	TMap<FName, UClass*> Classes;
	HutongExchange::GatherBuildingComponentClasses(Classes);

	for (const TPair<FName, UClass*>& Pair : Classes)
	{
		for (TFieldIterator<FProperty> It(Pair.Value); It; ++It)
		{
			FProperty* Prop = *It;
			// Only this module's own properties.
			if (Prop->GetOwnerClass() &&
				!Prop->GetOwnerClass()->IsChildOf(UHutongBuildingComponent::StaticClass()))
			{
				continue;
			}
			TestTrue(*FString::Printf(TEXT("%s::%s is exportable (CPF_Edit)"),
				*Pair.Key.ToString(), *Prop->GetName()),
				Prop->HasAnyPropertyFlags(CPF_Edit));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongExchangeLightPointersTest,
	"HutongLayout.Exchange.EngineFieldsExcluded",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongExchangeLightPointersTest::RunTest(const FString& Parameters)
{
	UHutongSiheyuanBuildingComponent* C =
		NewObject<UHutongSiheyuanBuildingComponent>(GetTransientPackage());
	// A record carries what was changed, so something has to have been.
	C->Params.EaveHeight += 17.0;
	C->Palette.Body = FLinearColor(0.1f, 0.2f, 0.3f, 1.0f);
	TSharedPtr<FJsonObject> Blob = HutongExchange::WriteComponent(C);
	if (!TestTrue(TEXT("a house writes a blob"), Blob.IsValid())) return false;

	// Nothing UActorComponent owns travels. Case-insensitively, because the converter lowercases
	// the first character on the way out.
	for (const auto& Field : Blob->Values)
	{
		const FString Key = FString(Field.Key).ToLower();
		TestTrue(*FString::Printf(TEXT("no engine field travels (%s)"), *Key),
			Key != TEXT("primarycomponenttick") && Key != TEXT("componenttags")
			&& Key != TEXT("bautoactivate") && Key != TEXT("breplicates"));
	}

	// And the things that must travel do.
	TestTrue(TEXT("a tuned parameter travels"), Blob->HasField(TEXT("params")));
	TestTrue(TEXT("a tuned palette travels"), Blob->HasField(TEXT("palette")));
	// Identity always, whether or not anything about the building was touched.
	TestTrue(TEXT("the id travels"), Blob->HasField(TEXT("buildingId")));

	UHutongSiheyuanBuildingComponent* Plain =
		NewObject<UHutongSiheyuanBuildingComponent>(GetTransientPackage());
	TSharedPtr<FJsonObject> PlainBlob = HutongExchange::WriteComponent(Plain);
	if (TestTrue(TEXT("an untouched building writes a blob"), PlainBlob.IsValid()))
	{
		TestFalse(TEXT("with no parameters in it"), PlainBlob->HasField(TEXT("params")));
		TestFalse(TEXT("and no palette"), PlainBlob->HasField(TEXT("palette")));
		TestTrue(TEXT("but still its id"), PlainBlob->HasField(TEXT("buildingId")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongExchangePaletteMaterialTest,
	"HutongLayout.Exchange.PaletteMaterialByReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongExchangePaletteMaterialTest::RunTest(const FString& Parameters)
{
	UMaterialInterface* Asset = UMaterial::GetDefaultMaterial(MD_Surface);
	if (!TestNotNull(TEXT("a material to reference"), Asset)) return false;

	UHutongWallBuildingComponent* Source =
		NewObject<UHutongWallBuildingComponent>(GetTransientPackage());
	Source->Palette.BodyMaterial = Asset;

	TSharedPtr<FJsonObject> Blob = HutongExchange::WriteComponent(Source);
	if (!TestTrue(TEXT("the wall writes a blob"), Blob.IsValid())) return false;

	const TSharedPtr<FJsonObject>* PaletteObj = nullptr;
	if (!TestTrue(TEXT("the palette is an object"),
		Blob->TryGetObjectField(TEXT("palette"), PaletteObj) && PaletteObj))
	{
		return false;
	}

	// A string, not a nested object.
	FString Path;
	TestTrue(TEXT("a material slot is written as a path"),
		(*PaletteObj)->TryGetStringField(TEXT("bodyMaterial"), Path));
	TestTrue(TEXT("the path names the asset"), Path.Contains(Asset->GetName()));

	UHutongWallBuildingComponent* Target =
		NewObject<UHutongWallBuildingComponent>(GetTransientPackage());
	FString Problem;
	TestTrue(TEXT("it reads back"),
		HutongExchange::ReadComponent(Blob.ToSharedRef(), Target, Problem));
	TestTrue(TEXT("and resolves to the same asset"), Target->Palette.BodyMaterial == Asset);

	// The recipient without the asset: a null slot and the palette tint, not a failed record.
	(*PaletteObj)->SetStringField(TEXT("bodyMaterial"),
		TEXT("/Script/Engine.Material'/Game/NotHere/M_Missing.M_Missing'"));
	UHutongWallBuildingComponent* Stranger =
		NewObject<UHutongWallBuildingComponent>(GetTransientPackage());
	TestTrue(TEXT("a missing asset still reads"),
		HutongExchange::ReadComponent(Blob.ToSharedRef(), Stranger, Problem));
	TestNull(TEXT("and leaves the slot empty"), Stranger->Palette.BodyMaterial.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongExchangePlacementTest,
	"HutongLayout.Exchange.PlacementComposition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongExchangePlacementTest::RunTest(const FString& Parameters)
{
	// A record standing at an angle to the set.
	HutongExchange::FRecord R;
	R.Offset = FVector2D(100.0, 200.0);
	R.RelativeYawDeg = 90.0;
	R.Footprint = FVector2D(400.0, 100.0);

	FVector2D Corners[4];
	HutongExchange::FootprintCornersInSetFrame(R, Corners);
	TestTrue(TEXT("the near corner is the offset"), Corners[0].Equals(R.Offset, 0.01));
	// Turned a quarter, (400, 0) becomes (0, 400).
	TestTrue(TEXT("a quarter turn swings the long axis onto Y"),
		Corners[1].Equals(FVector2D(100.0, 600.0), 0.01));
	TestTrue(TEXT("and the far corner follows"),
		Corners[2].Equals(FVector2D(0.0, 600.0), 0.01));

	// Composition at three set bearings, against transforms worked out by hand.
	const double SetYaws[] = { 0.0, 37.0, -90.0 };
	for (double SetYaw : SetYaws)
	{
		const FVector Anchor(1000.0, -500.0, 25.0);
		const FTransform SetToWorld(FRotator(0.0, SetYaw, 0.0), Anchor);
		const FTransform Xf = HutongExchange::ComposeRecordTransform(R, SetToWorld);

		const FRotator Rot(0.0, SetYaw, 0.0);
		const FVector Want = Anchor + Rot.RotateVector(FVector(R.Offset.X, R.Offset.Y, R.OffsetZ));
		TestTrue(*FString::Printf(TEXT("the offset turns with the set at %.0f"), SetYaw),
			Xf.GetLocation().Equals(Want, 0.01));
		TestTrue(*FString::Printf(TEXT("the yaws add at %.0f"), SetYaw),
			FMath::IsNearlyEqual(FRotator::NormalizeAxis(Xf.Rotator().Yaw),
				FRotator::NormalizeAxis(SetYaw + R.RelativeYawDeg), 0.001));
	}

	// And the export arithmetic is its inverse.
	{
		const double SetYaw = 37.0;
		const FVector Anchor(1000.0, -500.0, 0.0);
		const FTransform SetToWorld(FRotator(0.0, SetYaw, 0.0), Anchor);

		HutongExchange::FRecord B;
		B.Offset = FVector2D(-250.0, 640.0);
		B.RelativeYawDeg = -45.0;

		for (const HutongExchange::FRecord* Rec : { &R, &B })
		{
			const FTransform Xf = HutongExchange::ComposeRecordTransform(*Rec, SetToWorld);
			const FRotator Rot(0.0, SetYaw, 0.0);
			const FVector Rel = Rot.UnrotateVector(Xf.GetLocation() - Anchor);
			TestTrue(TEXT("the offset survives a round trip"),
				FVector2D(Rel.X, Rel.Y).Equals(Rec->Offset, 0.01));
			TestTrue(TEXT("the relative yaw survives a round trip"),
				FMath::IsNearlyEqual(FRotator::NormalizeAxis(Xf.Rotator().Yaw - SetYaw),
					Rec->RelativeYawDeg, 0.001));
		}
	}

	// NormaliseToBounds slides the set onto its own min corner.
	{
		TArray<HutongExchange::FRecord> Records;
		HutongExchange::FRecord A;
		A.Offset = FVector2D(-300.0, 100.0);
		A.Footprint = FVector2D(200.0, 200.0);
		Records.Add(A);
		HutongExchange::FRecord C;
		C.Offset = FVector2D(500.0, -400.0);
		C.Footprint = FVector2D(100.0, 100.0);
		Records.Add(C);

		FVector2D Size;
		const FVector2D Shift = HutongExchange::NormaliseToBounds(Records, Size);
		TestTrue(TEXT("the min corner is what was removed"),
			Shift.Equals(FVector2D(-300.0, -400.0), 0.01));
		TestTrue(TEXT("the first record now sits on the origin in X"),
			FMath::IsNearlyEqual(Records[0].Offset.X, 0.0, 0.01));
		TestTrue(TEXT("the size spans both footprints"),
			Size.Equals(FVector2D(900.0, 700.0), 0.01));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongExchangeLayoutOnlyTest,
	"HutongLayout.Exchange.LayoutOnlyRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongExchangeLayoutOnlyTest::RunTest(const FString& Parameters)
{
	// A layout-only file carries the arrangement and nothing about how a piece is built, so a
	// re-import puts the same street down built from the types' current defaults.
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	const FTransform Xform(FRotator(0.0, 23.0, 0.0), FVector(800.0, -200.0, 0.0));
	const FVector2D Footprint(1120.0, 640.0);
	const double TunedEave = 512.0;

	{
		UHutongSiheyuanBuildingComponent* Template =
			NewObject<UHutongSiheyuanBuildingComponent>(GetTransientPackage());
		Template->DetailLevel = EHutongDetail::Massing;
		Template->bBuildLODChain = false;
		TArray<FDynamicMesh3> LODs;
		Template->BuildLODs(LODs);

		AStaticMeshActor* Actor = HutongGen::SpawnStaticMeshActor(World, LODs, Xform,
			TEXT("LayoutOnly"), FHutongPalette());
		if (!TestNotNull(TEXT("the actor spawns"), Actor)) { World->DestroyWorld(false); return false; }

		UHutongSiheyuanBuildingComponent* B = NewObject<UHutongSiheyuanBuildingComponent>(
			Actor, UHutongSiheyuanBuildingComponent::StaticClass(), NAME_None, RF_Transactional);
		B->DetailLevel = EHutongDetail::Massing;
		B->bBuildLODChain = false;
		B->SetFootprintSize(Footprint);
		B->BaySide = EHutongBaySide::PlusX;
		// Tuned away from the shipped default, which is exactly what must not travel.
		B->Params.EaveHeight = TunedEave;
		Actor->AddInstanceComponent(B);
		B->RegisterComponent();
	}

	const FString Path = FPaths::Combine(FPaths::AutomationTransientDir(),
		TEXT("HutongLayoutOnly.hutong.json"));

	HutongExchange::FResult Out;
	HutongExchange::ExportLoaded(World, Path, Out, /*bLayoutOnly*/ true);
	TestTrue(TEXT("the layout export succeeds"), Out.bSucceeded);
	TestEqual(TEXT("the building is exported"), Out.Exported, 1);

	// The file itself: it says what it is, and it carries no parameters at all.
	FString Text;
	if (TestTrue(TEXT("the file is on disk"), FFileHelper::LoadFileToString(Text, *Path)))
	{
		TestTrue(TEXT("the file says it is layout only"), Text.Contains(TEXT("\"layoutOnly\": true")));
		TestFalse(TEXT("no parameters travel"), Text.Contains(TEXT("\"component\"")));
		TestTrue(TEXT("the facing travels"), Text.Contains(TEXT("\"facing\"")));
	}

	for (UHutongBuildingComponent* B : HutongDetailOps::CollectLoaded(World))
	{
		B->GetOwner()->Destroy();
	}

	HutongExchange::FResult In;
	HutongExchange::ImportAtRecordedTransforms(World, Path, HutongExchange::EMode::Additive,
		NAME_None, In);
	TestTrue(TEXT("the layout import succeeds"), In.bSucceeded);
	TestEqual(TEXT("the building comes back"), In.Created, 1);
	TestEqual(TEXT("and nothing is skipped"), In.Skipped, 0);

	const TArray<UHutongBuildingComponent*> Back = HutongDetailOps::CollectLoaded(World);
	if (!TestEqual(TEXT("one building is in the level"), Back.Num(), 1))
	{
		World->DestroyWorld(false);
		return false;
	}

	UHutongSiheyuanBuildingComponent* House = Cast<UHutongSiheyuanBuildingComponent>(Back[0]);
	if (!TestNotNull(TEXT("it came back as a house"), House))
	{
		World->DestroyWorld(false);
		return false;
	}

	// The arrangement is the file's.
	const FTransform Placed = House->GetOwner()->GetActorTransform();
	TestTrue(TEXT("it stands where it stood"),
		Placed.GetLocation().Equals(Xform.GetLocation(), 1.0));
	TestTrue(TEXT("it faces the way it faced"),
		FMath::Abs(FRotator::NormalizeAxis(Placed.Rotator().Yaw - Xform.Rotator().Yaw)) < 0.1);
	TestTrue(TEXT("its footprint is the recorded one"),
		House->GetFootprintSize().Equals(Footprint, 1.0));
	TestEqual(TEXT("its facade is the recorded side"), House->BaySide, EHutongBaySide::PlusX);
	TestEqual(TEXT("its detail level travels"), House->DetailLevel, EHutongDetail::Massing);

	// And the parameters are the type's own, not the ones the export was tuned to.
	const FHutongSiheyuanParams Defaults;
	TestNotEqual(TEXT("the tuned parameter did not travel"), House->Params.EaveHeight, TunedEave);
	TestEqual(TEXT("the parameters are the shipped defaults"),
		House->Params.EaveHeight, Defaults.EaveHeight);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongExchangeFileRoundTripTest,
	"HutongLayout.Exchange.FileRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongExchangeFileRoundTripTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	struct FPlaced { UClass* Class; FTransform Xform; };
	const FPlaced Wanted[] = {
		{ UHutongWallBuildingComponent::StaticClass(),
		  FTransform(FRotator(0.0, 0.0, 0.0), FVector(0.0, 0.0, 0.0)) },
		{ UHutongSiheyuanBuildingComponent::StaticClass(),
		  FTransform(FRotator(0.0, 37.0, 0.0), FVector(1200.0, 400.0, 0.0)) },
		{ UHutongFlowerBedBuildingComponent::StaticClass(),
		  FTransform(FRotator(0.0, -45.0, 0.0), FVector(300.0, 900.0, 0.0)) },
	};

	TArray<FGuid> Ids;
	for (const FPlaced& P : Wanted)
	{
		// Massing with no chain: three bakes.
		UHutongBuildingComponent* Template = NewObject<UHutongBuildingComponent>(
			GetTransientPackage(), P.Class);
		Template->DetailLevel = EHutongDetail::Massing;
		Template->bBuildLODChain = false;

		TArray<FDynamicMesh3> LODs;
		Template->BuildLODs(LODs);
		if (!TestTrue(TEXT("the piece builds"), LODs.Num() > 0 && LODs[0].TriangleCount() > 0))
		{
			World->DestroyWorld(false);
			return false;
		}

		AStaticMeshActor* Actor = HutongGen::SpawnStaticMeshActor(World, LODs, P.Xform,
			TEXT("Test"), FHutongPalette());
		if (!TestNotNull(TEXT("the actor spawns"), Actor)) { World->DestroyWorld(false); return false; }

		UHutongBuildingComponent* B = NewObject<UHutongBuildingComponent>(
			Actor, P.Class, NAME_None, RF_Transactional);
		B->DetailLevel = EHutongDetail::Massing;
		B->bBuildLODChain = false;
		Actor->AddInstanceComponent(B);
		B->RegisterComponent();
		TestTrue(TEXT("registration mints an id"), B->BuildingId.IsValid());
		Ids.Add(B->BuildingId);
	}

	const FString Path = FPaths::Combine(FPaths::AutomationTransientDir(),
		TEXT("HutongExchangeRoundTrip.hutong.json"));

	HutongExchange::FResult Out;
	HutongExchange::ExportLoaded(World, Path, Out);
	TestTrue(TEXT("the export succeeds"), Out.bSucceeded);
	TestEqual(TEXT("all three are exported"), Out.Exported, 3);

	// Read it back as a file rather than trusting the in-memory structure: the JSON is what a collaborator receives.
	HutongExchange::FSceneFile File;
	HutongExchange::FResult ReadResult;
	if (!TestTrue(TEXT("the file reads"), HutongExchange::Read(Path, File, ReadResult)))
	{
		World->DestroyWorld(false);
		return false;
	}
	TestEqual(TEXT("it describes three buildings"), File.Records.Num(), 3);

	// Where each actor stood, so the placement can be checked against it once they are gone.
	TMap<FGuid, FTransform> Before;
	for (UHutongBuildingComponent* B : HutongDetailOps::CollectLoaded(World))
	{
		Before.Add(B->BuildingId, B->GetOwner()->GetActorTransform());
	}

	for (UHutongBuildingComponent* B : HutongDetailOps::CollectLoaded(World))
	{
		B->GetOwner()->Destroy();
	}
	TestEqual(TEXT("the level is empty again"),
		HutongDetailOps::CollectLoaded(World).Num(), 0);

	HutongExchange::FResult In;
	HutongExchange::ImportAtRecordedTransforms(World, Path, HutongExchange::EMode::Sync,
		NAME_None, In);
	TestTrue(TEXT("the import succeeds"), In.bSucceeded);
	TestEqual(TEXT("all three are placed"), In.Created, 3);
	TestEqual(TEXT("and none is skipped"), In.Skipped, 0);

	TArray<UHutongBuildingComponent*> After = HutongDetailOps::CollectLoaded(World);
	TestEqual(TEXT("three buildings came back"), After.Num(), 3);

	for (UHutongBuildingComponent* B : After)
	{
		const FTransform* Was = Before.Find(B->BuildingId);
		if (!TestNotNull(TEXT("each keeps the id it was exported under"), (void*)Was)) continue;

		const FTransform Now = B->GetOwner()->GetActorTransform();
		TestTrue(TEXT("and lands where it stood"),
			Now.GetLocation().Equals(Was->GetLocation(), 0.01));
		TestTrue(TEXT("at the bearing it stood at"),
			FMath::IsNearlyEqual(FRotator::NormalizeAxis(Now.Rotator().Yaw),
				FRotator::NormalizeAxis(Was->Rotator().Yaw), 0.001));
		TestEqual(TEXT("built at the level it was placed at"),
			(int32)B->DetailLevel, (int32)EHutongDetail::Massing);
	}

	// Sync means what it says: the same file again reshapes what is there rather than doubling it.
	HutongExchange::FResult Again;
	HutongExchange::ImportAtRecordedTransforms(World, Path, HutongExchange::EMode::Sync,
		NAME_None, Again);
	TestEqual(TEXT("a second import creates nothing"), Again.Created, 0);
	TestEqual(TEXT("and updates all three"), Again.Updated, 3);
	TestEqual(TEXT("the level still holds three"),
		HutongDetailOps::CollectLoaded(World).Num(), 3);

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongExchangeRunAxisTest,
	"HutongLayout.Exchange.LineLikeRunAxis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongExchangeRunAxisTest::RunTest(const FString& Parameters)
{
	// A footprint is two numbers and says nothing about which of them is the run, so a line-like
	// piece drawn down Y came back from a layout-only file with its own thickness for a length —
	// a stub where a wall had been. Every type that has the flag is checked, both ways round.
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	struct FCase { UClass* Class; double Length; };
	const FCase Cases[] = {
		{ UHutongWallBuildingComponent::StaticClass(), 1400.0 },
		{ UHutongCorridorBuildingComponent::StaticClass(), 900.0 },
		{ UHutongPathBuildingComponent::StaticClass(), 700.0 },
		{ UHutongScreenWallBuildingComponent::StaticClass(), 500.0 },
		{ UHutongPaifangBuildingComponent::StaticClass(), 800.0 },
		{ UHutongPassageBuildingComponent::StaticClass(), 400.0 },
	};

	const FString Path = FPaths::Combine(FPaths::AutomationTransientDir(),
		TEXT("HutongRunAxis.hutong.json"));

	for (bool bLayoutOnly : { false, true })
	{
		for (const FCase& C : Cases)
		{
			const FString What = FString::Printf(TEXT("%s %s"), *C.Class->GetName(),
				bLayoutOnly ? TEXT("(layout only)") : TEXT("(full)"));

			for (UHutongBuildingComponent* Old : HutongDetailOps::CollectLoaded(World))
			{
				Old->GetOwner()->Destroy();
			}

			FVector2D Wanted;
			{
				UHutongBuildingComponent* Template = NewObject<UHutongBuildingComponent>(
					GetTransientPackage(), C.Class);
				Template->DetailLevel = EHutongDetail::Massing;
				Template->bBuildLODChain = false;
				// Down Y, which is the case the run axis is the whole of.
				Template->SetRunAlongY(true);
				Template->SetFootprintSize(FVector2D(Template->GetFootprintSize().X, C.Length));

				TArray<FDynamicMesh3> LODs;
				Template->BuildLODs(LODs);
				AStaticMeshActor* Actor = HutongGen::SpawnStaticMeshActor(World, LODs,
					FTransform::Identity, TEXT("RunAxis"), FHutongPalette());
				if (!TestNotNull(*FString::Printf(TEXT("%s spawns"), *What), Actor)) continue;

				UHutongBuildingComponent* B = NewObject<UHutongBuildingComponent>(
					Actor, C.Class, NAME_None, RF_Transactional);
				B->DetailLevel = EHutongDetail::Massing;
				B->bBuildLODChain = false;
				B->SetRunAlongY(true);
				B->SetFootprintSize(FVector2D(B->GetFootprintSize().X, C.Length));
				Actor->AddInstanceComponent(B);
				B->RegisterComponent();
				Wanted = B->GetFootprintSize();
			}

			HutongExchange::FResult Out;
			HutongExchange::ExportLoaded(World, Path, Out, bLayoutOnly);
			if (!TestTrue(*FString::Printf(TEXT("%s exports"), *What), Out.bSucceeded)) continue;

			for (UHutongBuildingComponent* Old : HutongDetailOps::CollectLoaded(World))
			{
				Old->GetOwner()->Destroy();
			}

			HutongExchange::FResult In;
			HutongExchange::ImportAtRecordedTransforms(World, Path,
				HutongExchange::EMode::Additive, NAME_None, In);
			if (!TestTrue(*FString::Printf(TEXT("%s imports"), *What), In.bSucceeded)) continue;

			const TArray<UHutongBuildingComponent*> Back = HutongDetailOps::CollectLoaded(World);
			if (!TestEqual(*FString::Printf(TEXT("%s comes back"), *What), Back.Num(), 1)) continue;

			TestTrue(*FString::Printf(TEXT("%s still runs along Y"), *What), Back[0]->IsRunAlongY());
			TestTrue(*FString::Printf(TEXT("%s keeps its footprint (%s against %s)"), *What,
				*Back[0]->GetFootprintSize().ToString(), *Wanted.ToString()),
				Back[0]->GetFootprintSize().Equals(Wanted, 1.0));
		}
	}

	// And the parameters a full file carries reach the mesh: a lane wall's gate is the case that
	// showed, since a wall with its gate lost is a wall with no way through it.
	{
		for (UHutongBuildingComponent* Old : HutongDetailOps::CollectLoaded(World))
		{
			Old->GetOwner()->Destroy();
		}

		UHutongWallBuildingComponent* Template =
			NewObject<UHutongWallBuildingComponent>(GetTransientPackage());
		Template->bLengthAlongY = true;
		Template->Length = 1600.0;
		Template->Params.bHasGate = true;
		Template->Params.GatePosition = 0.35;
		TArray<FDynamicMesh3> LODs;
		Template->BuildLODs(LODs);

		AStaticMeshActor* Actor = HutongGen::SpawnStaticMeshActor(World, LODs,
			FTransform::Identity, TEXT("GatedWall"), FHutongPalette());
		if (TestNotNull(TEXT("the gated wall spawns"), Actor))
		{
			UHutongWallBuildingComponent* B = NewObject<UHutongWallBuildingComponent>(
				Actor, UHutongWallBuildingComponent::StaticClass(), NAME_None, RF_Transactional);
			B->bLengthAlongY = true;
			B->Length = 1600.0;
			B->Params.bHasGate = true;
			B->Params.GatePosition = 0.35;
			Actor->AddInstanceComponent(B);
			B->RegisterComponent();

			HutongExchange::FResult Out;
			HutongExchange::ExportLoaded(World, Path, Out, /*bLayoutOnly*/ false);
			Actor->Destroy();

			HutongExchange::FResult In;
			HutongExchange::ImportAtRecordedTransforms(World, Path,
				HutongExchange::EMode::Additive, NAME_None, In);

			const TArray<UHutongBuildingComponent*> Back = HutongDetailOps::CollectLoaded(World);
			if (TestEqual(TEXT("the gated wall comes back"), Back.Num(), 1))
			{
				UHutongWallBuildingComponent* Wall = Cast<UHutongWallBuildingComponent>(Back[0]);
				if (TestNotNull(TEXT("as a wall"), Wall))
				{
					TestTrue(TEXT("it still carries its gate"), Wall->Params.bHasGate);
					TestTrue(TEXT("at the position it was placed at"),
						FMath::IsNearlyEqual(Wall->Params.GatePosition, 0.35, 0.001));
					TestTrue(TEXT("and its run is 16 m"),
						FMath::IsNearlyEqual(Wall->Length, 1600.0, 1.0));
				}
			}
		}
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongExchangeRemapTest,
	"HutongLayout.Exchange.UnknownTypeRemap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongExchangeRemapTest::RunTest(const FString& Parameters)
{
	// A file naming a type this build has not got — written before a rename or a split. Unanswered
	// it is skipped and said so; answered it lands as the type the user chose, at the footprint the
	// file recorded, since the old type's parameters are not this one's.
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	const FTransform Xform(FRotator(0.0, 15.0, 0.0), FVector(400.0, 100.0, 0.0));
	const double Length = 1250.0;

	HutongExchange::FSceneFile File;
	{
		HutongExchange::FRecord R;
		R.Id = FGuid::NewGuid();
		R.ClassName = TEXT("UHutongRetiredBuildingComponent");
		R.Label = TEXT("OldWall");
		R.Footprint = FVector2D(37.0, Length);
		R.bRunAlongY = true;
		R.Detail = EHutongDetail::Massing;
		File.Records.Add(R);
		File.SetOriginWorld = Xform.GetLocation();
		File.SetYawDeg = Xform.Rotator().Yaw;
		File.bLayoutOnly = true;
	}

	TMap<FName, int32> Unknown;
	HutongExchange::FindUnknownTypes(File, Unknown);
	TestEqual(TEXT("the unknown type is found once"), Unknown.Num(), 1);
	TestEqual(TEXT("with its record counted"),
		Unknown.FindRef(FName(TEXT("UHutongRetiredBuildingComponent"))), 1);

	// Unanswered.
	{
		HutongExchange::FResult Out;
		HutongExchange::Place(World, File, FTransform(FRotator(0.0, File.SetYawDeg, 0.0),
			File.SetOriginWorld), HutongExchange::EMode::Additive, NAME_None, Out);
		TestEqual(TEXT("an unknown type places nothing"), Out.Created, 0);
		TestEqual(TEXT("and is reported"), Out.Problems.Num(), 1);
	}

	// Deliberately dropped: skipped, and not reported as a problem.
	{
		HutongExchange::FSceneFile Dropped = File;
		HutongImportTypes::SkipUnknownTypes(Dropped);
		HutongExchange::FResult Out;
		HutongExchange::Place(World, Dropped, FTransform(FRotator(0.0, File.SetYawDeg, 0.0),
			File.SetOriginWorld), HutongExchange::EMode::Additive, NAME_None, Out);
		TestEqual(TEXT("a dropped type places nothing"), Out.Created, 0);
		TestEqual(TEXT("and says nothing about it"), Out.Problems.Num(), 0);
	}

	// Answered.
	{
		HutongExchange::FSceneFile Mapped = File;
		Mapped.TypeRemap.Add(FName(TEXT("UHutongRetiredBuildingComponent")),
			UHutongWallBuildingComponent::StaticClass()->GetFName());

		HutongExchange::FResult Out;
		HutongExchange::Place(World, Mapped, FTransform(FRotator(0.0, File.SetYawDeg, 0.0),
			File.SetOriginWorld), HutongExchange::EMode::Additive, NAME_None, Out);
		TestEqual(TEXT("the remapped record places one building"), Out.Created, 1);

		const TArray<UHutongBuildingComponent*> Back = HutongDetailOps::CollectLoaded(World);
		if (TestEqual(TEXT("one building is in the level"), Back.Num(), 1))
		{
			UHutongWallBuildingComponent* Wall = Cast<UHutongWallBuildingComponent>(Back[0]);
			if (TestNotNull(TEXT("it is the type that was chosen"), Wall))
			{
				TestTrue(TEXT("it runs the way the record said"), Wall->IsRunAlongY());
				TestTrue(TEXT("at the recorded length"),
					FMath::IsNearlyEqual(Wall->Length, Length, 1.0));
			}
		}
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongExchangeLayoutFactsTest,
	"HutongLayout.Exchange.LayoutOnlyPlacementFacts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongExchangeLayoutFactsTest::RunTest(const FString& Parameters)
{
	// A layout-only file drops the parameters on purpose. What it must not drop is anything the
	// *placement* decided: the kind of wall a run is, which way a corridor opens, how many bays a
	// house was given, where its facade is. Each of those used to come back as the class default.
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	auto Spawn = [World](UClass* Class, const FTransform& Xform,
		TFunctionRef<void(UHutongBuildingComponent*)> Setup) -> UHutongBuildingComponent*
	{
		UHutongBuildingComponent* Template = NewObject<UHutongBuildingComponent>(
			GetTransientPackage(), Class);
		Template->DetailLevel = EHutongDetail::Massing;
		Template->bBuildLODChain = false;
		Setup(Template);
		TArray<FDynamicMesh3> LODs;
		Template->BuildLODs(LODs);

		AStaticMeshActor* Actor = HutongGen::SpawnStaticMeshActor(World, LODs, Xform,
			TEXT("LayoutFacts"), FHutongPalette());
		if (!Actor) return nullptr;

		UHutongBuildingComponent* B = NewObject<UHutongBuildingComponent>(
			Actor, Class, NAME_None, RF_Transactional);
		B->DetailLevel = EHutongDetail::Massing;
		B->bBuildLODChain = false;
		Setup(B);
		Actor->AddInstanceComponent(B);
		B->RegisterComponent();
		return B;
	};

	Spawn(UHutongWallBuildingComponent::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(0.0, 0.0, 0.0)),
		[](UHutongBuildingComponent* B)
		{
			UHutongWallBuildingComponent* W = CastChecked<UHutongWallBuildingComponent>(B);
			W->Params.Role = EHutongWallRole::Courtyard;
			W->Length = 1100.0;
			W->bLengthAlongY = true;
		});

	Spawn(UHutongCorridorBuildingComponent::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(3000.0, 0.0, 0.0)),
		[](UHutongBuildingComponent* B)
		{
			UHutongCorridorBuildingComponent* C = CastChecked<UHutongCorridorBuildingComponent>(B);
			C->Length = 950.0;
			C->bFlipOpenSide = true;
			C->BenchGapAt = 0.4;
		});

	Spawn(UHutongSiheyuanBuildingComponent::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(6000.0, 0.0, 0.0)),
		[](UHutongBuildingComponent* B)
		{
			UHutongSiheyuanBuildingComponent* H = CastChecked<UHutongSiheyuanBuildingComponent>(B);
			H->FootprintX = 1300.0;
			H->FootprintY = 700.0;
			H->BaySide = EHutongBaySide::PlusX;
			H->BayCountOverride = 5;
		});

	const FString Path = FPaths::Combine(FPaths::AutomationTransientDir(),
		TEXT("HutongLayoutFacts.hutong.json"));

	HutongExchange::FResult Out;
	HutongExchange::ExportLoaded(World, Path, Out, /*bLayoutOnly*/ true);
	if (!TestTrue(TEXT("the layout export succeeds"), Out.bSucceeded))
	{
		World->DestroyWorld(false);
		return false;
	}

	for (UHutongBuildingComponent* B : HutongDetailOps::CollectLoaded(World))
	{
		B->GetOwner()->Destroy();
	}

	HutongExchange::FResult In;
	HutongExchange::ImportAtRecordedTransforms(World, Path, HutongExchange::EMode::Additive,
		NAME_None, In);
	TestTrue(TEXT("the layout import succeeds"), In.bSucceeded);
	TestEqual(TEXT("all three come back"), In.Created, 3);

	for (UHutongBuildingComponent* B : HutongDetailOps::CollectLoaded(World))
	{
		if (UHutongWallBuildingComponent* W = Cast<UHutongWallBuildingComponent>(B))
		{
			TestEqual(TEXT("the wall is still a 隔牆"),
				(int32)W->Params.Role, (int32)EHutongWallRole::Courtyard);
			TestTrue(TEXT("at the length it was drawn"), FMath::IsNearlyEqual(W->Length, 1100.0, 1.0));
			TestTrue(TEXT("down the axis it was drawn"), W->bLengthAlongY);
		}
		else if (UHutongCorridorBuildingComponent* C = Cast<UHutongCorridorBuildingComponent>(B))
		{
			TestTrue(TEXT("the corridor still opens the way it did"), C->bFlipOpenSide);
			TestTrue(TEXT("and keeps the gap in its bench"),
				FMath::IsNearlyEqual(C->BenchGapAt, 0.4, 0.001));
			TestTrue(TEXT("at its own length"), FMath::IsNearlyEqual(C->Length, 950.0, 1.0));
		}
		else if (UHutongSiheyuanBuildingComponent* H = Cast<UHutongSiheyuanBuildingComponent>(B))
		{
			TestEqual(TEXT("the house keeps its forced bay count"), H->BayCountOverride, 5);
			TestEqual(TEXT("and its facade side"), H->BaySide, EHutongBaySide::PlusX);
			TestTrue(TEXT("and its footprint"),
				H->GetFootprintSize().Equals(FVector2D(1300.0, 700.0), 1.0));
		}
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongExchangeDeltaTest,
	"HutongLayout.Exchange.DeltaRecords",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongExchangeDeltaTest::RunTest(const FString& Parameters)
{
	// A record carries what was decided, not what a building is: everything at its type's default
	// is left out, and what comes back is byte-for-byte what went in.
	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	auto Spawn = [World](UClass* Class, const FTransform& Xform,
		TFunctionRef<void(UHutongBuildingComponent*)> Setup) -> UHutongBuildingComponent*
	{
		UHutongBuildingComponent* Template = NewObject<UHutongBuildingComponent>(
			GetTransientPackage(), Class);
		Template->DetailLevel = EHutongDetail::Massing;
		Template->bBuildLODChain = false;
		Setup(Template);
		TArray<FDynamicMesh3> LODs;
		Template->BuildLODs(LODs);

		AStaticMeshActor* Actor = HutongGen::SpawnStaticMeshActor(World, LODs, Xform,
			TEXT("Delta"), FHutongPalette());
		if (!Actor) return nullptr;

		UHutongBuildingComponent* B = NewObject<UHutongBuildingComponent>(
			Actor, Class, NAME_None, RF_Transactional);
		B->DetailLevel = EHutongDetail::Massing;
		B->bBuildLODChain = false;
		Setup(B);
		B->EnsureBuildingId();
		Actor->AddInstanceComponent(B);
		B->RegisterComponent();
		return B;
	};

	// A gated lane wall, a 隔牆 with a doorway, and a house nobody has touched.
	Spawn(UHutongWallBuildingComponent::StaticClass(), FTransform::Identity,
		[](UHutongBuildingComponent* B)
		{
			UHutongWallBuildingComponent* W = CastChecked<UHutongWallBuildingComponent>(B);
			W->Length = 1800.0;
			W->Params.bHasGate = true;
			W->Params.GatePosition = 0.28;
		});
	Spawn(UHutongWallBuildingComponent::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(4000.0, 0.0, 0.0)),
		[](UHutongBuildingComponent* B)
		{
			UHutongWallBuildingComponent* W = CastChecked<UHutongWallBuildingComponent>(B);
			W->Params.Role = EHutongWallRole::Courtyard;
			W->Params.Doorway = EHutongWallDoorway::Moon;
		});
	Spawn(UHutongSiheyuanBuildingComponent::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(8000.0, 0.0, 0.0)),
		[](UHutongBuildingComponent*) {});

	// What each of them is, before the trip.
	TMap<FGuid, FString> Before;
	for (UHutongBuildingComponent* B : HutongDetailOps::CollectLoaded(World))
	{
		Before.Add(B->BuildingId, Serialise(B));
	}
	TestEqual(TEXT("three buildings to export"), Before.Num(), 3);

	const FString Path = FPaths::Combine(FPaths::AutomationTransientDir(),
		TEXT("HutongDelta.hutong.json"));

	HutongExchange::FResult Out;
	HutongExchange::ExportLoaded(World, Path, Out);
	if (!TestTrue(TEXT("the export succeeds"), Out.bSucceeded))
	{
		World->DestroyWorld(false);
		return false;
	}

	FString Text;
	if (TestTrue(TEXT("the file is on disk"), FFileHelper::LoadFileToString(Text, *Path)))
	{
		// What was decided is in it.
		TestTrue(TEXT("the gate travels"), Text.Contains(TEXT("\"bHasGate\": true")));
		TestTrue(TEXT("where the gate sits travels"), Text.Contains(TEXT("\"gatePosition\"")));
		TestTrue(TEXT("the wall's role travels"), Text.Contains(TEXT("Courtyard")));
		TestTrue(TEXT("the doorway travels"), Text.Contains(TEXT("Moon")));
		// And what was not is not: three fields nobody touched, one per piece.
		TestFalse(TEXT("an untouched cap does not"), Text.Contains(TEXT("capRidgeRoll")));
		TestFalse(TEXT("an untouched window spacing does not"), Text.Contains(TEXT("windowSpacing")));
		TestFalse(TEXT("an untouched eave does not"), Text.Contains(TEXT("eaveHeight")));
		// A whole untouched house is its identity and nothing else.
		TestFalse(TEXT("no palette travels untouched"), Text.Contains(TEXT("\"palette\"")));
	}

	for (UHutongBuildingComponent* B : HutongDetailOps::CollectLoaded(World))
	{
		B->GetOwner()->Destroy();
	}

	HutongExchange::FResult In;
	HutongExchange::ImportAtRecordedTransforms(World, Path, HutongExchange::EMode::Sync,
		NAME_None, In);
	TestTrue(TEXT("the import succeeds"), In.bSucceeded);
	TestEqual(TEXT("all three come back"), In.Created, 3);
	TestEqual(TEXT("with nothing to report"), In.Problems.Num(), 0);

	// Exactly, field for field, from a file that carried a handful of them.
	for (UHutongBuildingComponent* B : HutongDetailOps::CollectLoaded(World))
	{
		const FString* Was = Before.Find(B->BuildingId);
		if (!TestNotNull(TEXT("each comes back under its own id"), (void*)Was)) continue;
		TestEqual(TEXT("and is the building it was"), Serialise(B), *Was);
	}

	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongExchangePresetReferenceTest,
	"HutongLayout.Exchange.PresetReference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongExchangePresetReferenceTest::RunTest(const FString& Parameters)
{
	// **The preset is the reference a record is measured against**, and until a placement recorded
	// which one it used that branch never ran: every delta was taken against the shipped class
	// defaults, so everything a preset sets was written into the file as though somebody had typed
	// it. Lossless, and the wrong reference — a re-import could not pick up a canon that had moved
	// for any field the preset touched.
	const TArray<FString>& Names = HutongPresets::BuiltInSiheyuanNames();
	if (!TestTrue(TEXT("there are built-in house presets"), Names.Num() > 0)) return false;
	const FString PresetName = Names[0];

	// The premise: this preset moves a field off the struct's own default. Without that the test
	// below would pass on a preset that changes nothing.
	UHutongSiheyuanBuildingComponent* Plain = NewObject<UHutongSiheyuanBuildingComponent>(
		GetTransientPackage());
	UHutongSiheyuanBuildingComponent* Preset = NewObject<UHutongSiheyuanBuildingComponent>(
		GetTransientPackage());
	if (!TestTrue(TEXT("the preset resolves"), Preset->ApplyPresetParams(PresetName))) return false;
	if (!TestNotEqual(TEXT("and moves the eave off the shipped default"),
		Preset->Params.EaveHeight, Plain->Params.EaveHeight)) return false;

	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	// A house placed from that preset, with one field tuned away from it afterwards.
	UHutongSiheyuanBuildingComponent* Template = NewObject<UHutongSiheyuanBuildingComponent>(
		GetTransientPackage());
	Template->DetailLevel = EHutongDetail::Massing;
	Template->bBuildLODChain = false;
	Template->ApplyPresetParams(PresetName);
	TArray<FDynamicMesh3> LODs;
	Template->BuildLODs(LODs);

	AStaticMeshActor* Actor = HutongGen::SpawnStaticMeshActor(World, LODs, FTransform::Identity,
		TEXT("PresetRef"), FHutongPalette());
	if (!TestNotNull(TEXT("the actor spawns"), Actor)) { World->DestroyWorld(false); return false; }

	UHutongSiheyuanBuildingComponent* B = NewObject<UHutongSiheyuanBuildingComponent>(
		Actor, NAME_None, RF_Transactional);
	B->DetailLevel = EHutongDetail::Massing;
	B->bBuildLODChain = false;
	B->Preset = PresetName;
	B->ApplyPresetParams(PresetName);
	B->Params.BaseCourseHeight = Plain->Params.BaseCourseHeight + 37.0;
	B->EnsureBuildingId();
	Actor->AddInstanceComponent(B);
	B->RegisterComponent();

	const FString Was = Serialise(B);

	const FString Path = FPaths::Combine(FPaths::AutomationTransientDir(),
		TEXT("HutongPresetRef.hutong.json"));
	HutongExchange::FResult Out;
	HutongExchange::ExportLoaded(World, Path, Out);
	if (!TestTrue(TEXT("the export succeeds"), Out.bSucceeded))
	{
		World->DestroyWorld(false);
		return false;
	}

	FString Text;
	if (TestTrue(TEXT("the file is on disk"), FFileHelper::LoadFileToString(Text, *Path)))
	{
		TestTrue(TEXT("the preset travels, since it is what the rest is measured against"),
			Text.Contains(PresetName));
		// The point of the whole exercise: a field the preset sets is the preset's to answer for.
		TestFalse(TEXT("a field the preset sets is not written out"),
			Text.Contains(TEXT("eaveHeight")));
		// And a field tuned away from the preset still is.
		TestTrue(TEXT("what was tuned off the preset is"),
			Text.Contains(TEXT("baseCourseHeight")));
	}

	Actor->Destroy();

	HutongExchange::FResult In;
	HutongExchange::ImportAtRecordedTransforms(World, Path, HutongExchange::EMode::Sync,
		NAME_None, In);
	TestTrue(TEXT("the import succeeds"), In.bSucceeded);
	TestEqual(TEXT("with nothing to report"), In.Problems.Num(), 0);

	const TArray<UHutongBuildingComponent*> Back = HutongDetailOps::CollectLoaded(World);
	if (TestEqual(TEXT("one building comes back"), Back.Num(), 1))
	{
		TestEqual(TEXT("and is the building it was, field for field"), Serialise(Back[0]), Was);
	}

	World->DestroyWorld(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
