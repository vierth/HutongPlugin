#include "Tools/HutongImportTool.h"
#include "Tools/HutongOverlaps.h"
#include "Tools/HutongImportTypes.h"

#include "HutongLayoutModeSettings.h"

#include "InteractiveToolManager.h"
#include "ContextObjectStore.h"
#include "ToolContextInterfaces.h"

#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "HutongImportTool"

// --- property set buttons, forwarding only ---

void UHutongImportToolProperties::Browse()
{
	if (UHutongImportTool* Tool = Owner.Get()) Tool->BrowseForFile();
}

void UHutongImportToolProperties::Reload()
{
	if (UHutongImportTool* Tool = Owner.Get()) Tool->LoadFile(FileName);
}

void UHutongImportToolProperties::PlaceAtRecordedCoordinates()
{
	if (UHutongImportTool* Tool = Owner.Get()) Tool->PlaceAtRecordedCoordinates();
}

// --- the tool ---

void UHutongImportTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongImportToolProperties>(this);
	Settings->Owner = this;
	RegisterSettings(Settings);

	// Seeded from the mode's config.
	const UHutongLayoutModeSettings* Mode = GetDefault<UHutongLayoutModeSettings>();
	if (Mode && !Mode->LastSceneFile.IsEmpty() && FPaths::FileExists(Mode->LastSceneFile))
	{
		LoadFile(Mode->LastSceneFile, /*bPrompt*/ false);
	}
}

void UHutongImportTool::BrowseForFile()
{
	IDesktopPlatform* Platform = FDesktopPlatformModule::Get();
	if (!Platform) return;

	const void* Parent = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
	TArray<FString> Files;
	if (!Platform->OpenFileDialog(Parent, TEXT("Import a Hutong scene"),
		FPaths::ProjectSavedDir(), FString(), HutongExchange::SceneFileTypes, EFileDialogFlags::None, Files))
	{
		return;
	}
	if (Files.Num() == 0) return;

	LoadFile(Files[0]);
}

void UHutongImportTool::LoadFile(const FString& InPath, bool bPrompt)
{
	FilePath = InPath;
	Loaded = HutongExchange::FSceneFile();
	PreviewCorners.Reset();

	HutongExchange::FResult Result;
	bool bOk = HutongExchange::Read(FilePath, Loaded, Result);
	// Resolved once at load: both placement paths use the resolved file, and the preview draws
	// what will be laid down.
	if (bOk && bPrompt && !HutongImportTypes::ResolveUnknownTypes(Loaded))
	{
		bOk = false;
		Result.Problems.Add(TEXT("Import cancelled."));
	}
	if (!bOk)
	{
		Loaded = HutongExchange::FSceneFile();
	}
	// Per-record problems on success are worth reporting; a failure louder.
	Result.bSucceeded = bOk;
	if (!bOk || Result.Problems.Num() > 0)
	{
		HutongExchange::Report(Result, TEXT("import"));
	}

	RebuildPreview();

	if (Settings)
	{
		Settings->FileName = FilePath;
		Settings->RecordCount = Loaded.Records.Num();
		Settings->SourceLevel = Loaded.LevelName;
		// A layout-only file builds from current type defaults; say so, lest the shipped parameters
		// be read as lost work.
		Settings->Contents = !bOk ? FString()
			: (Loaded.bLayoutOnly
				? TEXT("Layout only — each building is rebuilt from its type's current defaults")
				: TEXT("Full parameters"));
		Settings->Extent = bOk
			? FString::Printf(TEXT("%.0f x %.0f m"),
				Loaded.BoundsSize.X * 0.01, Loaded.BoundsSize.Y * 0.01)
			: FString();
	}

	if (bOk)
	{
		// Remembered across sessions; shared with the mode panel's import button.
		if (UHutongLayoutModeSettings* Mode = GetMutableDefault<UHutongLayoutModeSettings>())
		{
			Mode->LastSceneFile = FilePath;
			Mode->SaveConfig();
		}
	}
}

void UHutongImportTool::RebuildPreview()
{
	PreviewCorners.Reset();
	if (Loaded.Records.Num() == 0 || Loaded.Records.Num() > MaxPreviewFootprints) return;

	PreviewCorners.Reserve(Loaded.Records.Num() * 4);
	for (const HutongExchange::FRecord& R : Loaded.Records)
	{
		FVector2D Corners[4];
		HutongExchange::FootprintCornersInSetFrame(R, Corners);
		for (const FVector2D& C : Corners) PreviewCorners.Add(C);
	}
}

void UHutongImportTool::GetEffectiveRectBounds(
	double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	Super::GetEffectiveRectBounds(OutMinX, OutMinY, OutMaxX, OutMaxY);
	if (Loaded.Records.Num() == 0) return;

	// The set's size is the file's, not the drag's.
	HoldExtentAtCursor(OutMinX, OutMaxX, FMath::Max(Loaded.BoundsSize.X, 1.0));
	HoldExtentAtCursor(OutMinY, OutMaxY, FMath::Max(Loaded.BoundsSize.Y, 1.0));
}

void UHutongImportTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	Super::Render(RenderAPI);
	if (!bIsDragging || RenderAPI == nullptr || Loaded.Records.Num() == 0) return;
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	if (!PDI) return;

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);

	const FLinearColor Plan(0.35f, 0.75f, 1.0f, 1.0f);

	// Corner to corner in order, not a min/max box.
	for (int32 i = 0; i + 3 < PreviewCorners.Num(); i += 4)
	{
		FVector W[4];
		for (int32 k = 0; k < 4; ++k)
		{
			const FVector2D& C = PreviewCorners[i + k];
			W[k] = LocalRectToWorld(MinX + C.X, MinY + C.Y);
		}
		for (int32 k = 0; k < 4; ++k)
		{
			DrawPreviewLine(PDI, W[k], W[(k + 1) % 4], Plan, 3.0f);
		}
	}
}

FString UHutongImportTool::GetPlacementDetail() const
{
	if (Loaded.Records.Num() == 0) return FString();

	FString Detail = FString::Printf(TEXT("%d buildings · %.0f x %.0f m"),
		Loaded.Records.Num(), Loaded.BoundsSize.X * 0.01, Loaded.BoundsSize.Y * 0.01);
	if (PreviewCorners.Num() == 0)
	{
		Detail += TEXT(" · outline only");
	}
	return Detail;
}

FText UHutongImportTool::GetStagePromptText() const
{
	if (Loaded.Records.Num() == 0)
	{
		return LOCTEXT("PromptNoFile", "Choose a scene file in the panel below to import.");
	}
	if (!bIsDragging)
	{
		return LOCTEXT("PromptAnchorSet",
			"Click the ground to put the set down. Hold R to turn it, Esc to cancel.");
	}
	return Super::GetStagePromptText();
}

TArray<FText> UHutongImportTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = LOCTEXT("HelpImport",
		"Browse for a scene file below, then click to anchor and click to place.");
	Lines.Insert(LOCTEXT("HelpImportSet",
		"The drag moves and turns the set; its size comes from the file."), 1);
	Lines.Insert(LOCTEXT("HelpImportSync",
		"Update Matching Placements reshapes buildings the file names instead of adding copies; it never deletes."), 2);
	Lines.Insert(LOCTEXT("HelpImportRecorded",
		"Place At Recorded Coordinates puts the set where it was exported; use it only in that level."), 3);
	return Lines;
}

void UHutongImportTool::PlaceAtRecordedCoordinates()
{
	if (Loaded.Records.Num() == 0 || FilePath.IsEmpty()) return;

	HutongExchange::FResult Result;
	HutongExchange::ImportAtRecordedTransforms(
		GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld(), FilePath,
		(Settings && Settings->bUpdateMatchingPlacements)
			? HutongExchange::EMode::Sync : HutongExchange::EMode::Additive,
		Settings ? Settings->OutlinerFolder : NAME_None, Result,
		// Resolved at load; this path re-reads from disk, so reuse the answer. The dialog appears
		// only if the file changed and names something new.
		[this](HutongExchange::FSceneFile& File)
		{
			File.TypeRemap = Loaded.TypeRemap;
			return HutongImportTypes::ResolveUnknownTypes(File);
		});
	HutongExchange::Report(Result, TEXT("import"));
	HutongOverlaps::CheckAfterImport(Result.Placed);
}

void UHutongImportTool::SpawnFinalActor()
{
	if (Loaded.Records.Num() == 0) return;

	UWorld* World = GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld();
	if (!World) return;

	// Unbuildable types the file names, answered before placement rather than reported after.
	// Already-answered types cost nothing.
	if (!HutongImportTypes::ResolveUnknownTypes(Loaded)) return;

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);

	// The set frame's origin is its bounding rectangle's min corner.
	const FVector Origin = LocalRectToWorld(MinX, MinY);
	const FTransform SetToWorld(FRotator(0.0, PlacementYawDeg, 0.0),
		FVector(Origin.X, Origin.Y, StartWorld.Z));

	UInteractiveToolManager* ToolManager = GetToolManager();
	ToolManager->BeginUndoTransaction(LOCTEXT("ImportScene", "Import Hutong Scene"));

	HutongExchange::FResult Result;
	HutongExchange::Place(World, Loaded, SetToWorld,
		(Settings && Settings->bUpdateMatchingPlacements)
			? HutongExchange::EMode::Sync : HutongExchange::EMode::Additive,
		Settings ? Settings->OutlinerFolder : NAME_None, Result);

	ToolManager->EndUndoTransaction();

	HutongExchange::Report(Result, TEXT("import"));
	HutongOverlaps::CheckAfterImport(Result.Placed);
}

UInteractiveTool* UHutongImportToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongImportTool>(SceneState.ToolManager);
}

#undef LOCTEXT_NAMESPACE
