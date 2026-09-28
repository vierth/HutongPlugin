#include "Tools/HallTool.h"
#include "Generation/SiheyuanGenerator.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"
#include "LevelEditorViewport.h"
#include "SceneManagement.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongHallToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongHallTool>(SceneState.ToolManager);
}

void UHutongHallTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongHallToolProperties>(this);

	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("Hall"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongHallToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	// Panel order is registration order: presets after the params they save.
	RegisterSettings(Settings);
	RegisterSettings(Presets);
}

void UHutongHallTool::OnPlacementStarted(const FVector& HitWorld)
{
	BaySide = ComputeDefaultBaySide();
	// Bay count depends on the frontage drawn, so it never carries between placements.
	BayCountOverride = 0;
}

void UHutongHallTool::CancelPlacement()
{
	Super::CancelPlacement();
	BayCountOverride = 0;
}

bool UHutongHallTool::OnRectCommitted(const FVector& HitWorld)
{
	// Defer to a third click so the facing side is picked by hovering.
	return false;
}

void UHutongHallTool::OnPlacementHover(const FVector& HitWorld)
{
	if (bRotateModeActive) return;
	if (!bRectCommitted)
	{
		BaySide = ComputeDefaultBaySide();
		return;
	}
	TGuardValue<bool> AsDragged(bPickingSide, true);
	BaySide = ComputeClosestSide(HitWorld.X, HitWorld.Y);
}

void UHutongHallTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Level)
{
	UHutongHallBuildingComponent::BuildHallMesh(
		Settings ? Settings->Params : FHutongHallParams(),
		BaySide, BayCountOverride, SizeX, SizeY, OutMesh, Level);
}

void UHutongHallTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor) return;

	UHutongHallBuildingComponent* Building =
		NewObject<UHutongHallBuildingComponent>(Actor, TEXT("Hall"));
	if (!Building) return;

	if (Settings) Building->Params = Settings->Params;
	// The previewed count, mirrored onto the component.
	Building->BayCountOverride = BayCountOverride;
	Building->FootprintX = SizeX;
	Building->FootprintY = SizeY;
	Building->BaySide = BaySide;
	if (Appearance) Building->Palette = Appearance->Palette;

	StampDetail(Building);

	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	// After registration: the plan outline and lights attach to the actor's root, which needs the component live.
	Building->ApplyPlacementAttachments();
}

void UHutongHallTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	FHutongHallParams& P = Settings->Params;
	// The eave only: the 舉架 is tuned, and raising a hall must not flatten its roof.
	P.EaveHeight = FMath::Clamp(P.EaveHeight + DeltaCm, P.FloorHeight + 150.0, 900.0);
}

FHutongHallParams UHutongHallTool::GetSizedParams() const
{
	FHutongHallParams P = Settings ? Settings->Params : FHutongHallParams();
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	if (MaxX - MinX > 1.0 && MaxY - MinY > 1.0)
	{
		const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
		P.Width = bAlongX ? MaxX - MinX : MaxY - MinY;
		P.Depth = bAlongX ? MaxY - MinY : MaxX - MinX;
	}
	return P;
}

// The 大式 hall keeps the figure's plan: the rect holds its proportion, the frontage on the longer drag until the
// facing is picked, then on the facing's side (GetSizedParams takes Width along it).
void UHutongHallTool::GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	Super::GetEffectiveRectBounds(OutMinX, OutMinY, OutMaxX, OutMaxY);
	if (bPickingSide || !Settings || !Settings->Params.IsGrand()) return;
	namespace G = HutongCanon::GrandHall;
	const double Ratio = (G::Depth + G::EaveColumn) / (G::Frontage + G::EaveColumn);
	auto Hold = [](double& Lo, double& Hi, double Want)
	{
		if (Hi > 0.0) Hi = Lo + Want;
		else          Lo = Hi - Want;
	};
	const double SX = OutMaxX - OutMinX, SY = OutMaxY - OutMinY;
	const bool bFrontAlongX = bRectCommitted ? HutongGen::BaySide::IsAlongX(BaySide) : SX >= SY;
	if (bFrontAlongX) Hold(OutMinY, OutMaxY, SX * Ratio);
	else              Hold(OutMinX, OutMaxX, SY * Ratio);
}

double UHutongHallTool::GetPreviewHeight() const
{
	return Settings ? GetSizedParams().GetEaveHeight() : 0.0;
}

FString UHutongHallTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	const FHutongHallParams P = GetSizedParams();
	if (P.IsGrand())
	{
		return FString::Printf(TEXT("Grand hall (大式 九檁歇山殿) · 5 bays (間) · frontage (面闊) %.1f chi (尺, %.1f m) · bracket module (斗口) %.1f cm"),
			HutongCanon::GrandHall::Frontage, HutongCanon::GrandHall::Frontage * P.GetGrandScale() / 100.0, HutongCanon::GrandHall::Doukou * P.GetGrandScale());
	}
	const TCHAR* Roof =
		(P.RoofType == EHutongRoofType::Xieshan) ? TEXT("hip-and-gable roof (歇山)") :
		(P.RoofType == EHutongRoofType::Wudian)  ? TEXT("hipped roof (廡殿)") : TEXT("pyramidal roof (攢尖)");
	return FString::Printf(TEXT("Temple hall (殿) · %d bays (間) · %s"), ComputeBayCountForSide(), Roof);
}

TArray<FText> UHutongHallTool::GetStageNames() const
{
	TArray<FText> Names = Super::GetStageNames();
	Names.Add(NSLOCTEXT("HallTool", "StageFacing", "Facing"));
	return Names;
}

FText UHutongHallTool::GetStagePromptText() const
{
	if (bIsDragging && bRectCommitted && !bRotateModeActive)
	{
		return NSLOCTEXT("HutongHallTool", "PromptSide",
			"Move to pick which side the facade faces (green ticks), then click to place. "
			"[ and ] change the bay count.");
	}
	return Super::GetStagePromptText();
}

void UHutongHallTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	Super::Render(RenderAPI);
	if (!bIsDragging || RenderAPI == nullptr) return;
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	if (!PDI) return;

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	if (MaxX - MinX < 1.0 || MaxY - MinY < 1.0) return;

	const HutongGen::BaySide::FEdge Edge =
		HutongGen::BaySide::GetEdge(BaySide, MinX, MinY, MaxX, MaxY);
	const double SpanMin = Edge.bAlongX ? MinX : MinY;
	const double SpanMax = Edge.bAlongX ? MaxX : MaxY;

	auto EdgePoint = [&](double A)
	{
		return Edge.bAlongX ? LocalRectToWorld(A, Edge.FixedCoord)
							: LocalRectToWorld(Edge.FixedCoord, A);
	};

	// Facade ticks, with the bay boundaries marked.
	const FLinearColor Green(0.25f, 1.0f, 0.45f);
	const int32 Ticks = 9;
	for (int32 i = 0; i < Ticks; ++i)
	{
		const double A0 = FMath::Lerp(SpanMin, SpanMax, (i + 0.15) / Ticks);
		const double A1 = FMath::Lerp(SpanMin, SpanMax, (i + 0.85) / Ticks);
		DrawPreviewLine(PDI, EdgePoint(A0), EdgePoint(A1), Green, 7.0f);
	}

	if (Settings)
	{
		const int32 N = ComputeBayCountForSide();
		const double ColR = Settings->Params.GetColumnRadius();
		const double Span = SpanMax - SpanMin;
		const FLinearColor Orange(1.0f, 0.55f, 0.15f);
		// The 大式 hall's bays are the figure's, scaled to the frontage.
		TArray<double> Grand;
		if (Settings->Params.IsGrand())
		{
			const double K = GetSizedParams().GetGrandScale();
			double X = 0.5 * (Span - HutongCanon::GrandHall::Frontage * K);
			Grand.Add(X);
			for (const double Bay : HutongCanon::GrandHall::Bays) Grand.Add(X += Bay * K);
		}
		for (int32 i = 0; i <= N; ++i)
		{
			const double A = SpanMin + (Grand.IsValidIndex(i) ? Grand[i] : Settings->Params.GetBayBoundary(i, N, Span, ColR));
			// Door bay, marked as the siheyuan marks it.
			const bool bDoorEdge = (i == N / 2 || i == N / 2 + 1);
			DrawPreviewLine(PDI, EdgePoint(A), EdgePoint(A), Green, 1.0f);
			if (bDoorEdge && i <= N)
			{
				const double A1 = FMath::Min(A + 0.02 * Span, SpanMax);
				DrawPreviewLine(PDI, EdgePoint(A), EdgePoint(A1), Orange, 9.0f);
			}
		}
	}
}

FText UHutongHallTool::GetKeyHintText() const
{
	return FText::Format(NSLOCTEXT("HallTool", "KeyHint", "[ ] bays · {0}"), Super::GetKeyHintText());
}

TArray<FText> UHutongHallTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongHallTool", "HelpDrag",
		"Click to anchor, move, click to fix the footprint, move toward the front, click to place.");
	Lines.Insert(NSLOCTEXT("HutongHallTool", "HelpGrand",
		"Style Grand builds the nine-purlin hall of the Qing building regulations (則例) at the size you drag, five bays with bracket sets (斗栱); the footprint keeps its proportion."), 1);
	Lines.Insert(NSLOCTEXT("HutongHallTool", "HelpRoof",
		"Roof Type: hip-and-gable (歇山) is usual, hipped (廡殿) for a grander hall, pyramidal (攢尖) for a near-square plan."), 1);
	return Lines;
}

int32 UHutongHallTool::ComputeBayCountForSide() const
{
	if (!Settings) return 1;
	if (Settings->Params.IsGrand()) return UE_ARRAY_COUNT(HutongCanon::GrandHall::Bays);
	if (BayCountOverride > 0) return BayCountOverride;

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
	const double Frontage = bAlongX ? (MaxX - MinX) : (MaxY - MinY);
	return HutongGen::ComputeBayCount(
		Frontage, Settings->Params.MinBayWidth, Settings->Params.MaxBayWidth);
}

void UHutongHallTool::AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse)
{
	if (!Settings || Delta == 0) return;
	// Seeded from the count on screen, so the first press steps from the preview.
	BayCountOverride = FMath::Clamp(ComputeBayCountForSide() + Delta, 1, 24);
}
