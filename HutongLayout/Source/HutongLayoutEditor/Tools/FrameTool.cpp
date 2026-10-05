#include "Tools/FrameTool.h"
#include "Tools/HutongPresetDefaults.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"
#include "SceneManagement.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongFrameToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongFrameTool>(SceneState.ToolManager);
}

void UHutongFrameTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongFrameToolProperties>(this);

	// Picker first: which house's frame is the first question.
	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("Frame"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongFrameToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	RegisterSettings(Presets);
	RegisterSettings(Settings);

	ApplyDefaultPreset(Presets, HutongPresets::DefaultFrameName());
}

FVector2D UHutongFrameTool::StampSize(EHutongBaySide Side) const
{
	const FHutongSiheyuanParams H = Settings ? Settings->Params.House : FHutongSiheyuanParams();
	const double Frontage = (H.SuggestedFrontage > 0.0) ? H.SuggestedFrontage : HutongCanon::House::MainHall.FrontageCm;
	const double Depth = FMath::Max(H.GetSuggestedDepth(), 100.0);
	return HutongGen::BaySide::IsAlongX(Side) ? FVector2D(Frontage, Depth) : FVector2D(Depth, Frontage);
}

void UHutongFrameTool::GetEffectiveRectBounds(
	double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	// Not dragged: the preset's footprint, centred on the first click.
	const FVector2D Size = StampSize(BaySide);
	OutMinX = -0.5 * Size.X; OutMaxX = 0.5 * Size.X;
	OutMinY = -0.5 * Size.Y; OutMaxY = 0.5 * Size.Y;
}

void UHutongFrameTool::OnPlacementStarted(const FVector& HitWorld)
{
	BaySide = ComputeDefaultBaySide();
}

bool UHutongFrameTool::OnRectCommitted(const FVector& HitWorld)
{
	return true;
}

void UHutongFrameTool::OnPlacementHover(const FVector& HitWorld)
{
	if (bRotateModeActive) return;
	// Front is the side the cursor moved toward; undecided near the centre.
	const FVector2D Local = WorldXYToLocalRect(HitWorld);
	if (Local.Size() < 50.0) return;
	BaySide = (FMath::Abs(Local.X) > FMath::Abs(Local.Y))
		? (Local.X > 0.0 ? EHutongBaySide::PlusX : EHutongBaySide::MinusX)
		: (Local.Y > 0.0 ? EHutongBaySide::PlusY : EHutongBaySide::MinusY);
}

void UHutongFrameTool::RenderIdlePreview(FPrimitiveDrawInterface* PDI, const FVector& CursorGround)
{
	// Before the first click the footprint follows the cursor, front toward the camera.
	const EHutongBaySide Side = ComputeDefaultBaySide();
	const FVector2D Size = StampSize(Side);
	auto At = [&](double X, double Y) { return LocalRectToWorldFrom(CursorGround, X, Y); };
	const FLinearColor Plan(1.0f, 0.9f, 0.15f);
	const double HX = 0.5 * Size.X, HY = 0.5 * Size.Y;
	DrawPreviewLine(PDI, At(-HX, -HY), At(HX, -HY), Plan, 3.0f);
	DrawPreviewLine(PDI, At(HX, -HY), At(HX, HY), Plan, 3.0f);
	DrawPreviewLine(PDI, At(HX, HY), At(-HX, HY), Plan, 3.0f);
	DrawPreviewLine(PDI, At(-HX, HY), At(-HX, -HY), Plan, 3.0f);
	DrawFront(PDI, CursorGround, Side);
}

void UHutongFrameTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Level)
{
	if (IsFullHouse())
	{
		UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(
			Settings->Params.House, BaySide, 0, SizeX, SizeY, OutMesh, Level);
		return;
	}
	UHutongFrameBuildingComponent::BuildFrameMesh(
		Settings ? Settings->Params : FHutongFrameParams(),
		BaySide, 0, SizeX, SizeY, OutMesh, Level);
}

void UHutongFrameTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor) return;

	if (IsFullHouse())
	{
		UHutongSiheyuanBuildingComponent* House = NewObject<UHutongSiheyuanBuildingComponent>(Actor, TEXT("House"));
		House->Params = Settings->Params.House;
		House->FootprintX = SizeX;
		House->FootprintY = SizeY;
		House->BaySide = BaySide;
		if (Appearance) House->Palette = Appearance->Palette;
		StampDetail(House);
		Actor->AddInstanceComponent(House);
		House->RegisterComponent();
		House->ApplyPlacementAttachments();
		return;
	}

	UHutongFrameBuildingComponent* Building =
		NewObject<UHutongFrameBuildingComponent>(Actor, TEXT("Frame"));
	if (!Building) return;

	if (Settings) Building->Params = Settings->Params;
	Building->FootprintX = SizeX;
	Building->FootprintY = SizeY;
	Building->BaySide = BaySide;
	if (Appearance) Building->Palette = Appearance->Palette;

	StampDetail(Building);

	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	// After registration: the plan outline attaches to the actor's root, which needs the component live.
	Building->ApplyPlacementAttachments();
}

int32 UHutongFrameTool::BayCount() const
{
	const FHutongSiheyuanParams H = Settings ? Settings->Params.House : FHutongSiheyuanParams();
	return HutongGen::ComputeBayCount(StampSize(EHutongBaySide::MinusY).X, H.MinBayWidth, H.MaxBayWidth);
}

double UHutongFrameTool::GetPreviewHeight() const
{
	if (!Settings) return 0.0;
	FHutongSiheyuanParams H = Settings->Params.House;
	H.Width = FMath::Max(H.SuggestedFrontage, 1.0);
	H.Depth = FMath::Max(H.GetSuggestedDepth(), 1.0);
	return H.GetEaveHeight();
}

FString UHutongFrameTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	const FHutongSiheyuanParams& H = Settings->Params.House;
	const int32 Purlins = HutongGen::Jiajia::PurlinCount(H.Purlins);
	const TCHAR* Form = !H.bHasFrontVeranda ? TEXT("no veranda (無廊)")
		: H.bHasRearVeranda ? TEXT("front and rear verandas (前後廊)") : TEXT("front veranda only (前廊後無廊)");
	const FVector2D Size = StampSize(EHutongBaySide::MinusY);
	return FString::Printf(TEXT("%s · %.0f x %.0f cm · %d bays (間) · %d purlins (檁) · %s"),
		IsFullHouse() ? TEXT("House (房)") : TEXT("Timber frame (構架)"), Size.X, Size.Y,
		BayCount(), Purlins, Form);
}

TArray<FText> UHutongFrameTool::GetStageNames() const
{
	return { NSLOCTEXT("HutongFrameTool", "StagePosition", "Position"), NSLOCTEXT("HutongFrameTool", "StageFront", "Front") };
}

FText UHutongFrameTool::GetStagePromptText() const
{
	if (!bIsDragging)
	{
		return NSLOCTEXT("HutongFrameTool", "PromptPosition",
			"Click to set the frame's centre.");
	}
	if (bRotateModeActive) return Super::GetStagePromptText();
	return NSLOCTEXT("HutongFrameTool", "PromptFront",
		"Move toward the front (green), then click to place. Hold R to rotate, Esc to cancel.");
}

void UHutongFrameTool::DrawFront(FPrimitiveDrawInterface* PDI, const FVector& Origin, EHutongBaySide Side) const
{
	const FVector2D Size = StampSize(Side);
	const double MinX = -0.5 * Size.X, MaxX = 0.5 * Size.X, MinY = -0.5 * Size.Y, MaxY = 0.5 * Size.Y;
	const HutongGen::BaySide::FEdge Edge = HutongGen::BaySide::GetEdge(Side, MinX, MinY, MaxX, MaxY);
	const double SpanMin = Edge.bAlongX ? MinX : MinY;
	const double SpanMax = Edge.bAlongX ? MaxX : MaxY;
	auto EdgePoint = [&](double A)
	{
		return Edge.bAlongX ? LocalRectToWorldFrom(Origin, A, Edge.FixedCoord)
							: LocalRectToWorldFrom(Origin, Edge.FixedCoord, A);
	};

	const double Span = SpanMax - SpanMin;
	const double Across = Edge.bAlongX ? MaxY - MinY : MaxX - MinX;
	const double InSign = Edge.bAlongX ? -Edge.OutDir.Y : -Edge.OutDir.X;
	TArray<double> Bounds;
	int32 DoorBay = INDEX_NONE;
	if (Settings)
	{
		const FHutongSiheyuanParams& H = Settings->Params.House;
		const int32 N = BayCount();
		for (int32 i = 0; i <= N; ++i) Bounds.Add(H.GetBayBoundary(i, N, Span, H.GetColumnRadius()));
		DoorBay = N / 2;
	}
	DrawBaysAndFacing(PDI, [&](double Along, double Depth)
	{
		const double Fixed = Edge.FixedCoord + InSign * Depth;
		return Edge.bAlongX ? LocalRectToWorldFrom(Origin, SpanMin + Along, Fixed) : LocalRectToWorldFrom(Origin, Fixed, SpanMin + Along);
	}, Span, Across, Bounds, DoorBay);
}

void UHutongFrameTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	Super::Render(RenderAPI);
	if (!bIsDragging || RenderAPI == nullptr) return;
	if (FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface())
	{
		DrawFront(PDI, StartWorld, BaySide);
	}
}

TArray<FText> UHutongFrameTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongFrameTool", "HelpStamp",
		"Click where the frame stands, move toward its front, click to place; the preset sets the size.");
	Lines.Insert(NSLOCTEXT("HutongFrameTool", "HelpWhat",
		"The bare timber frame (構架) of a main hall (正房) on its platform (臺明). Full House builds the whole house."), 1);
	return Lines;
}
