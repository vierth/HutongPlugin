#include "PlaceLabelsExchange.h"
#include "PlaceRegionActor.h"
#include "PlaceRegionComponent.h"

#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

// The metadata is only worth carrying if it survives the trip out to a spreadsheet and back, and
// the confidence is the half that can silently go wrong: it travels as its own number, and a
// display name or an empty cell must not be read as one.

namespace
{
	// A world of its own, so the test neither depends on nor disturbs whatever level is open.
	UWorld* MakeTestWorld()
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Editor, /*bInformEngineOfWorld*/ false);
		if (World)
		{
			World->InitializeActorsForPlay(FURL());
		}
		return World;
	}

	UPlaceRegionComponent* SpawnSquareRegion(UWorld* World, double X, double Size)
	{
		APlaceRegionActor* Actor = World->SpawnActor<APlaceRegionActor>(
			APlaceRegionActor::StaticClass(), FTransform(FVector(X, 0.0, 0.0)));
		if (!Actor || !Actor->Region)
		{
			return nullptr;
		}

		UPlaceRegionComponent* Region = Actor->Region;
		Region->LocalPoints = { FVector2D(0.0, 0.0), FVector2D(Size, 0.0),
								FVector2D(Size, Size), FVector2D(0.0, Size) };
		Region->EnsureRegionId();
		Region->RebuildCache();
		return Region;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlaceLabelsExchangeMetadataTest,
	"PlaceLabels.Exchange.Metadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPlaceLabelsExchangeMetadataTest::RunTest(const FString& Parameters)
{
	UWorld* World = MakeTestWorld();
	if (!TestNotNull(TEXT("test world"), World))
	{
		return false;
	}

	UPlaceRegionComponent* Attested = SpawnSquareRegion(World, 0.0, 100.0);
	UPlaceRegionComponent* Guessed = SpawnSquareRegion(World, 500.0, 100.0);
	if (!TestNotNull(TEXT("first region"), Attested) || !TestNotNull(TEXT("second region"), Guessed))
	{
		World->DestroyWorld(/*bInformEngineOfWorld*/ false);
		return false;
	}

	Attested->Name.Chinese = FText::FromString(TEXT("大柵欄"));
	Attested->Source = FText::FromString(TEXT("乾隆京城全圖, sheet 12"));
	Attested->Confidence = EPlaceConfidence::Attested;

	Guessed->Name.Pinyin = FText::FromString(TEXT("unnamed lane"));
	Guessed->Source = FText::FromString(TEXT("nothing; drawn to close the block"));
	Guessed->Confidence = EPlaceConfidence::Conjectural;

	const FString Path = FPaths::Combine(FPaths::AutomationTransientDir(),
		TEXT("PlaceLabelsExchangeMetadata.csv"));

	PlaceLabelsExchange::FResult Export;
	PlaceLabelsExchange::ExportCsv(World, Path, Export);
	TestTrue(TEXT("the export succeeded"), Export.bSucceeded);
	TestEqual(TEXT("both regions exported"), Export.Exported, 2);

	FString Written;
	TestTrue(TEXT("the file is readable"), FFileHelper::LoadFileToString(Written, *Path));
	TestTrue(TEXT("the header names the metadata columns"),
		Written.Contains(TEXT("source,confidence")));

	// The scale travels as its own number: a display name would be a translation of it.
	TestTrue(TEXT("the conjectural region wrote a 2"), Written.Contains(TEXT(",2,")));

	// Wipe what the file carries, and one thing it does not, before reading it back.
	Attested->Source = FText::GetEmpty();
	Attested->Confidence = EPlaceConfidence::Unknown;
	Guessed->Confidence = EPlaceConfidence::Unknown;

	PlaceLabelsExchange::FResult Import;
	PlaceLabelsExchange::ImportCsv(World, Path, Import);
	TestTrue(TEXT("the import succeeded"), Import.bSucceeded);
	TestEqual(TEXT("nothing was skipped"), Import.Skipped, 0);

	TestEqual(TEXT("the source came back"), Attested->Source.ToString(),
		FString(TEXT("乾隆京城全圖, sheet 12")));
	TestTrue(TEXT("5 came back as attested"), Attested->Confidence == EPlaceConfidence::Attested);
	TestTrue(TEXT("2 came back as conjectural"), Guessed->Confidence == EPlaceConfidence::Conjectural);

	// A blank cell is "not my column", not "no evidence": the region keeps what it had.
	Guessed->Confidence = EPlaceConfidence::Probable;
	FString Blanked = Written.Replace(TEXT(",2,"), TEXT(",,"));
	TestTrue(TEXT("the blanked file is writable"), FFileHelper::SaveStringToFile(Blanked, *Path,
		FFileHelper::EEncodingOptions::ForceUTF8));

	PlaceLabelsExchange::FResult Reimport;
	PlaceLabelsExchange::ImportCsv(World, Path, Reimport);
	TestTrue(TEXT("an empty confidence leaves the region alone"),
		Guessed->Confidence == EPlaceConfidence::Probable);

	World->DestroyWorld(/*bInformEngineOfWorld*/ false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
