#include "Tools/MeasureTool.h"
#include "InteractiveToolManager.h"
#include "SceneManagement.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "GameFramework/Actor.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "HutongMeasureTool"

void UHutongMeasureToolProperties::ApplyScaleToSelected()
{
#if WITH_EDITOR
	if (!GEditor || ScaleCorrection <= 0.0) return;

	USelection* Selected = GEditor->GetSelectedActors();
	if (!Selected) return;

	// Scaled about the measurement's own first point, not about the actor's pivot.
	const FScopedTransaction Transaction(
		LOCTEXT("CalibrateMap", "Calibrate Hutong Map Scale"));

	for (FSelectionIterator It(*Selected); It; ++It)
	{
		AActor* Actor = Cast<AActor>(*It);
		if (!Actor) continue;

		Actor->Modify();
		const FTransform X = Actor->GetActorTransform();
		const FVector Offset = X.GetLocation() - AnchorWorld;

		Actor->SetActorScale3D(X.GetScale3D() * ScaleCorrection);
		Actor->SetActorLocation(AnchorWorld + Offset * ScaleCorrection);
	}
#endif
}

UInteractiveTool* UHutongMeasureToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongMeasureTool>(SceneState.ToolManager);
}

void UHutongMeasureTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongMeasureToolProperties>(this);
	// Not persisted: a measurement belongs to the thing that was measured, and a stale reading restored into the panel next session would be a number with nothing behind it.
	RegisterSettings(Settings, /*bPersist*/ false);
}

void UHutongMeasureTool::OnPlacementStarted(const FVector& HitWorld)
{
	if (Settings) Settings->AnchorWorld = HitWorld;
	RefreshReading();
}

void UHutongMeasureTool::OnPlacementHover(const FVector& HitWorld)
{
	RefreshReading();
}

bool UHutongMeasureTool::OnRectCommitted(const FVector& HitWorld)
{
	// False, so the reading freezes.
	RefreshReading();
	return false;
}

void UHutongMeasureTool::RefreshReading()
{
	if (!Settings) return;

	const double Dist = FVector::Dist2D(StartWorld, CurrentWorld);
	Settings->DistanceCm = Dist;
	Settings->DistanceBu = Dist / HutongGen::Urban::BuCm;
	Settings->StreetReading = (Dist > 1.0)
		? HutongGen::Urban::DescribeWidth(Dist) : FString();
	Settings->ScaleCorrection = (Dist > 1.0 && Settings->KnownRealDistanceCm > 0.0)
		? Settings->KnownRealDistanceCm / Dist : 0.0;
}

FString UHutongMeasureTool::GetPlacementDetail() const
{
	if (!Settings || Settings->DistanceCm <= 1.0) return FString();

	FString Out = HutongGen::Urban::DescribeDistance(Settings->DistanceCm);
	if (!Settings->StreetReading.IsEmpty())
	{
		Out += FString::Printf(TEXT(" · as a street: %s"), *Settings->StreetReading);
	}
	if (Settings->ScaleCorrection > 0.0)
	{
		Out += FString::Printf(TEXT(" · scale x%.4f"), Settings->ScaleCorrection);
	}
	return Out;
}

TArray<FText> UHutongMeasureTool::GetStageNames() const
{
	return { NSLOCTEXT("MeasureTool", "StageStart", "Start"),
		NSLOCTEXT("MeasureTool", "StageEnd", "End"),
		NSLOCTEXT("MeasureTool", "StageRead", "Read") };
}

FText UHutongMeasureTool::GetStagePromptText() const
{
	if (bIsDragging && !bRectCommitted)
	{
		return LOCTEXT("PromptSecond",
			"Click the far end of the span. Nothing is placed — this only measures.");
	}
	if (bIsDragging && bRectCommitted)
	{
		return LOCTEXT("PromptFrozen",
			"Reading frozen. To calibrate: type the span's true length into Known Real Distance, "
			"select the map actor, then Apply Scale To Selected. Click again to measure something else.");
	}
	return LOCTEXT("PromptFirst", "Click one end of the span you want to measure.");
}

void UHutongMeasureTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	// Deliberately not Super: the base draws the drag rectangle and its corner posts, and this tool measures a line.
	if (!bIsDragging || RenderAPI == nullptr) return;
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	if (!PDI) return;

	const FLinearColor Line(1.0f, 0.85f, 0.2f);
	DrawPreviewLine(PDI, StartWorld, CurrentWorld, Line, 3.0f);

	// End ticks across the span, so the two ends are visible against a busy map texture.
	const FVector Along = (CurrentWorld - StartWorld).GetSafeNormal();
	const FVector Across = FVector::CrossProduct(Along, FVector::UpVector).GetSafeNormal();
	const double Tick = FMath::Max(0.03 * FVector::Dist2D(StartWorld, CurrentWorld), 25.0);
	DrawPreviewLine(PDI, StartWorld - Across * Tick, StartWorld + Across * Tick, Line, 3.0f);
	DrawPreviewLine(PDI, CurrentWorld - Across * Tick, CurrentWorld + Across * Tick, Line, 3.0f);

	// The module, drawn on the span.
	const double Dist = FVector::Dist2D(StartWorld, CurrentWorld);
	const int32 Paces = FMath::Clamp(FMath::FloorToInt32(Dist / HutongGen::Urban::BuCm), 0, 120);
	const FLinearColor Pace(0.4f, 0.8f, 1.0f, 0.9f);
	for (int32 i = 1; i <= Paces; ++i)
	{
		const FVector P = StartWorld + Along * (i * HutongGen::Urban::BuCm);
		DrawPreviewLine(PDI, P - Across * (0.4 * Tick), P + Across * (0.4 * Tick), Pace, 1.5f);
	}
}

TArray<FText> UHutongMeasureTool::GetToolHelpLines() const
{
	TArray<FText> Lines;
	Lines.Add(LOCTEXT("HelpDrag",
		"Click, move, click to measure a span. Nothing is ever placed by this tool."));
	Lines.Add(LOCTEXT("HelpRead",
		"The reading gives metres, paces (步, the Yuan pace the grid is set in), and what the span would be if it were a street width — a lane (胡同) is 6 paces, a minor street (小街) 12, an avenue (大街) 24."));
	Lines.Add(LOCTEXT("HelpCalibrate",
		"To calibrate a map image: measure a span whose true length you know, type that into Known Real Distance, select the map actor and press Apply Scale To Selected."));
	Lines.Add(LOCTEXT("HelpEsc", "Esc clears the measurement."));
	return Lines;
}

#undef LOCTEXT_NAMESPACE
