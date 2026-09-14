#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongActorSpawn.h"
#include "Generation/HutongMetadata.h"
#include "Tools/HutongExchange.h"
#include "Tools/HutongDetailOps.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"

#if WITH_DEV_AUTOMATION_TESTS

using UE::Geometry::FDynamicMesh3;

namespace
{
	UHutongBuildingComponent* PlaceHouse(UWorld* World, const FTransform& Xform,
		TFunctionRef<void(UHutongBuildingComponent*)> Setup)
	{
		UHutongSiheyuanBuildingComponent* Template =
			NewObject<UHutongSiheyuanBuildingComponent>(GetTransientPackage());
		Template->DetailLevel = EHutongDetail::Massing;
		Template->bBuildLODChain = false;
		TArray<FDynamicMesh3> LODs;
		Template->BuildLODs(LODs);

		AStaticMeshActor* Actor = HutongGen::SpawnStaticMeshActor(World, LODs, Xform,
			TEXT("Metadata"), FHutongPalette());
		if (!Actor) return nullptr;

		UHutongSiheyuanBuildingComponent* B = NewObject<UHutongSiheyuanBuildingComponent>(
			Actor, UHutongSiheyuanBuildingComponent::StaticClass(), NAME_None, RF_Transactional);
		B->DetailLevel = EHutongDetail::Massing;
		B->bBuildLODChain = false;
		Setup(B);
		B->EnsureBuildingId();
		Actor->AddInstanceComponent(B);
		B->RegisterComponent();
		return B;
	}
}

// A polygon traced off the map and one put there to close a gap in a street are the same
// rectangle, and only the placement can say which it is. So the confidence and the note are facts
// about the placement, travel with it, and default to the honest answer for the common case.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongConfidenceTest, "HutongLayout.Metadata.Confidence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongConfidenceTest::RunTest(const FString& Parameters)
{
	// The numbers are the scale, and they are what anyone reading a scene file or a spreadsheet
	// sees. 5 is attested, 1 is nothing but the need to fill the block.
	TestEqual(TEXT("attested is 5"), (int32)EHutongConfidence::Attested, 5);
	TestEqual(TEXT("not present is 1"), (int32)EHutongConfidence::Absent, 1);
	TestTrue(TEXT("and the scale is ordered"),
		EHutongConfidence::Absent < EHutongConfidence::Inferred
			&& EHutongConfidence::Inferred < EHutongConfidence::Attested);

	const UEnum* Enum = StaticEnum<EHutongConfidence>();
	for (int32 Value = 1; Value <= 5; ++Value)
	{
		const FString Label = Enum->GetDisplayNameTextByValue(Value).ToString();
		TestTrue(FString::Printf(TEXT("level %d names its own number"), Value),
			Label.StartsWith(FString::FromInt(Value)));
	}

	// A placement nobody has said anything about is attested: the ordinary act is tracing.
	const UHutongSiheyuanBuildingComponent* Fresh =
		NewObject<UHutongSiheyuanBuildingComponent>(GetTransientPackage());
	TestEqual(TEXT("a new placement is attested"), Fresh->Confidence, EHutongConfidence::Attested);
	TestTrue(TEXT("and carries no note"), Fresh->Notes.IsEmpty());

	UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
	if (!TestNotNull(TEXT("a world to place into"), World)) return false;

	const FString Note = TEXT("Read off sheet 12; the east end is a guess.");
	PlaceHouse(World, FTransform::Identity, [&Note](UHutongBuildingComponent* B)
	{
		B->Confidence = EHutongConfidence::Inferred;
		B->Notes = Note;
	});
	// One nobody touched, to hold the delta rule: what was not decided is not in the file.
	PlaceHouse(World, FTransform(FRotator::ZeroRotator, FVector(6000.0, 0.0, 0.0)),
		[](UHutongBuildingComponent*) {});

	const FString Path = FPaths::Combine(FPaths::AutomationTransientDir(),
		TEXT("HutongMetadata.hutong.json"));

	HutongExchange::FResult Out;
	HutongExchange::ExportLoaded(World, Path, Out);
	if (!TestTrue(TEXT("the export succeeds"), Out.bSucceeded)) { World->DestroyWorld(false); return false; }

	FString Text;
	if (TestTrue(TEXT("the file is on disk"), FFileHelper::LoadFileToString(Text, *Path)))
	{
		TestTrue(TEXT("the confidence travels"), Text.Contains(TEXT("Inferred")));
		TestTrue(TEXT("and so does the note"), Text.Contains(TEXT("sheet 12")));
		// The untouched one says nothing: a default confidence is not a decision.
		int32 Count = 0, From = 0;
		while (true)
		{
			const int32 At = Text.Find(TEXT("\"confidence\""), ESearchCase::CaseSensitive,
				ESearchDir::FromStart, From);
			if (At == INDEX_NONE) break;
			++Count;
			From = At + 1;
		}
		TestEqual(TEXT("only the placement that was decided about carries one"), Count, 1);
	}

	for (UHutongBuildingComponent* B : HutongDetailOps::CollectLoaded(World))
	{
		B->GetOwner()->Destroy();
	}

	HutongExchange::FResult In;
	HutongExchange::ImportAtRecordedTransforms(World, Path, HutongExchange::EMode::Sync,
		NAME_None, In);
	TestTrue(TEXT("the import succeeds"), In.bSucceeded);

	int32 Inferred = 0, Attested = 0;
	for (const UHutongBuildingComponent* B : HutongDetailOps::CollectLoaded(World))
	{
		if (B->Confidence == EHutongConfidence::Inferred)
		{
			++Inferred;
			TestEqual(TEXT("with its note"), B->Notes, Note);
		}
		else if (B->Confidence == EHutongConfidence::Attested)
		{
			++Attested;
			TestTrue(TEXT("and the untouched one is still attested and silent"), B->Notes.IsEmpty());
		}
	}
	TestEqual(TEXT("the decided placement comes back decided"), Inferred, 1);
	TestEqual(TEXT("and the untouched one comes back untouched"), Attested, 1);

	World->DestroyWorld(false);
	return true;
}

#endif
