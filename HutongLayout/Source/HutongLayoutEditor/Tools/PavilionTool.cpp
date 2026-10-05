#include "Tools/PavilionTool.h"
#include "Generation/HutongBuildingComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"
#include "ToolContextInterfaces.h"
#include "SceneManagement.h"

using UE::Geometry::FDynamicMesh3;

UInteractiveTool* UHutongPavilionToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongPavilionTool>(SceneState.ToolManager);
}

void UHutongPavilionTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongPavilionToolProperties>(this);

	Presets = NewObject<UHutongPresetProperties>(this);
	Presets->Initialize(TEXT("Pavilion"), Settings,
		GET_MEMBER_NAME_CHECKED(UHutongPavilionToolProperties, Params));
	Presets->OnPresetLoaded = [this]() { NotifyOfPropertyChangeByTool(Settings); };
	// Presets after the parameters they save.
	RegisterSettings(Settings);
	RegisterSettings(Presets);
}

void UHutongPavilionTool::GetEffectiveRectBounds(
	double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	Super::GetEffectiveRectBounds(OutMinX, OutMinY, OutMaxX, OutMaxY);

	// Held roughly square.
	const double SX = OutMaxX - OutMinX;
	const double SY = OutMaxY - OutMinY;
	if (SX < 1.0 || SY < 1.0) return;

	auto Hold = [](double& Lo, double& Hi, double Want)
	{
		if (Hi > 0.0) Hi = Lo + Want;
		else          Lo = Hi - Want;
	};

	// A round plan is exactly square: its circle fits the smaller side.
	if (Settings && Settings->Params.IsRound())
	{
		const double Side = FMath::Min(SX, SY);
		Hold(OutMinX, OutMaxX, Side);
		Hold(OutMinY, OutMaxY, Side);
		return;
	}
	if (SX > SY * 1.34)      Hold(OutMinX, OutMaxX, SY * 1.34);
	else if (SY > SX * 1.34) Hold(OutMinY, OutMaxY, SX * 1.34);
}

void UHutongPavilionTool::BuildMeshForRect(double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Level)
{
	UHutongPavilionBuildingComponent::BuildPavilionMesh(
		Settings ? Settings->Params : FHutongPavilionParams(), SizeX, SizeY, OutMesh,
		Level);
}

void UHutongPavilionTool::AttachBuildingComponent(AStaticMeshActor* Actor, double SizeX, double SizeY)
{
	if (!Actor) return;

	UHutongPavilionBuildingComponent* Building =
		NewObject<UHutongPavilionBuildingComponent>(Actor, TEXT("Pavilion"));
	if (!Building) return;

	if (Settings) Building->Params = Settings->Params;
	Building->Width = SizeX;
	Building->Depth = SizeY;
	if (Appearance) Building->Palette = Appearance->Palette;

	StampDetail(Building);

	Actor->AddInstanceComponent(Building);
	Building->RegisterComponent();
	// After registration: plan outline and lights attach to the actor root, which needs the component live.
	Building->ApplyPlacementAttachments();
}

// The params sized to the rect being drawn (or the figure's own, before the first click).
FHutongPavilionParams UHutongPavilionTool::GetSizedParams() const
{
	FHutongPavilionParams P = Settings ? Settings->Params : FHutongPavilionParams();
	if (bIsDragging)
	{
		double MinX, MinY, MaxX, MaxY;
		GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
		if (MaxX - MinX > 1.0 && MaxY - MinY > 1.0)
		{
			P.Width = MaxX - MinX;
			P.Depth = MaxY - MinY;
		}
	}
	return P;
}

void UHutongPavilionTool::AdjustHeight(double DeltaCm)
{
	if (!Settings) return;
	// Starts from the height on screen: every size is frozen at the figure's, then the eave moves.
	const FHutongPavilionParams Sized = GetSizedParams();
	FHutongPavilionParams& P = Settings->Params;
	if (P.bDeriveProportions)
	{
		FHutongPavilionParams Frozen = Sized;
		Frozen.FreezeProportions();
		Frozen.Width = P.Width;
		Frozen.Depth = P.Depth;
		P = Frozen;
	}
	P.EaveHeight = FMath::Clamp(P.EaveHeight + DeltaCm, 120.0, 600.0);
}

double UHutongPavilionTool::GetPreviewHeight() const
{
	return Settings ? GetSizedParams().GetEaveHeight() : 0.0;
}

FString UHutongPavilionTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	const FHutongPavilionParams P = GetSizedParams();
	return FString::Printf(TEXT("Pavilion (亭) · column spacing (面闊) %.0f cm · column %.0f cm · eave %.0f cm%s"),
		P.GetBay(), P.GetColumnDiameter(), P.GetEaveHeight(), P.bDeriveProportions ? TEXT(" · Qing regulations (則例)") : TEXT(""));
}

TArray<FText> UHutongPavilionTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongPavilionTool", "HelpDrag",
		"Click to anchor, move, click to place; the plan stays roughly square.");
	Lines.Insert(NSLOCTEXT("HutongPavilionTool", "HelpSides",
		"Plan: square (方亭) or round six-column (六柱圓亭) pavilion of the Qing regulations (則例); - and = set the height."), 1);
	Lines.Insert(NSLOCTEXT("HutongPavilionTool", "HelpRound",
		"Steps show in orange with an arrow; hold R while placing to turn them."), 2);
	return Lines;
}

// A round plan draws as it will stand: the columns' circle (the footprint), the 台基's circle dashed, the
// six columns with posts to the eave, and the 垂帶踏跺 before the entry bay with an arrow in.
void UHutongPavilionTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	Super::Render(RenderAPI);
	if (!bIsDragging || !RenderAPI || !Settings || !Settings->Params.IsRound()) return;
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	if (!PDI) return;
	namespace C = HutongCanon::Pavilion;

	const FHutongPavilionParams P = GetSizedParams();
	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);
	const double CX = 0.5 * (MinX + MaxX), CY = 0.5 * (MinY + MaxY);
	const double Side = FMath::Min(MaxX - MinX, MaxY - MinY);
	const double ColR = P.GetPlanColumnRadius();
	const double R = FMath::Max(0.5 * Side - ColR, 10.0);
	const double Rp = R + ColR + P.GetPlatformOverhang();
	const double Bay = P.GetBay();
	const FVector Lift(0.0, 0.0, 2.0);
	auto At = [&](double X, double Y) { return LocalRectToWorld(X, Y) + Lift; };

	const FLinearColor Footprint(1.0f, 0.9f, 0.15f, 1.0f);
	const FLinearColor Platform(1.0f, 0.9f, 0.15f, 0.6f);
	const FLinearColor Steps(1.0f, 0.55f, 0.1f, 1.0f);
	const FLinearColor HeightColor(0.45f, 0.8f, 1.0f, 1.0f);
	auto Circle = [&](double Radius, const FLinearColor& Colour, float Thick, bool bDashed, double Z = 0.0)
	{
		constexpr int32 N = 64;
		for (int32 i = 0; i < N; ++i)
		{
			const double A0 = 2.0 * PI * i / N, A1 = 2.0 * PI * (i + 1) / N;
			const FVector P0 = At(CX + Radius * FMath::Cos(A0), CY + Radius * FMath::Sin(A0)) + FVector(0, 0, Z);
			const FVector P1 = At(CX + Radius * FMath::Cos(A1), CY + Radius * FMath::Sin(A1)) + FVector(0, 0, Z);
			if (bDashed) { if (i % 2 == 0) DrawPreviewLine(PDI, P0, P1, Colour, Thick); }
			else DrawPreviewLine(PDI, P0, P1, Colour, Thick);
		}
	};
	Circle(R + ColR, Footprint, 5.0f, false);
	Circle(Rp, Platform, 3.0f, true);

	const double Height = GetPreviewHeight();
	for (int32 i = 0; i < C::RoundColumns; ++i)
	{
		const double A = 2.0 * PI * i / C::RoundColumns;
		const double X = CX + R * FMath::Cos(A), Y = CY + R * FMath::Sin(A);
		for (int32 k = 0; k < 8; ++k)
		{
			const double B0 = 2.0 * PI * k / 8, B1 = 2.0 * PI * (k + 1) / 8;
			DrawPreviewLine(PDI, At(X + ColR * FMath::Cos(B0), Y + ColR * FMath::Sin(B0)), At(X + ColR * FMath::Cos(B1), Y + ColR * FMath::Sin(B1)), Footprint, 3.0f);
		}
		if (Height > 0.0) DrawDashedPreviewLine(PDI, At(X, Y), At(X, Y) + FVector(0, 0, Height), HeightColor, 2.5f);
	}
	if (Height > 0.0) Circle(R, HeightColor, 3.0f, true, Height);

	// Before the entry bay, on the plan's -Y: the steps' outline, then an arrow walking in.
	if (P.bHasSteps && P.GetFloorHeight() > 0.0)
	{
		const double Half = 0.5 * C::RoundStepWidthPerBay * Bay + C::StringerWidth * P.GetColumnDiameter();
		const double Y0 = CY - Rp, Y1 = Y0 - 3.0 * C::RoundStepTreadPerBay * Bay;
		const double Back = CY - FMath::Sqrt(FMath::Max(Rp * Rp - Half * Half, 0.0));
		DrawPreviewLine(PDI, At(CX - Half, Back), At(CX - Half, Y1), Steps, 4.0f);
		DrawPreviewLine(PDI, At(CX - Half, Y1), At(CX + Half, Y1), Steps, 4.0f);
		DrawPreviewLine(PDI, At(CX + Half, Y1), At(CX + Half, Back), Steps, 4.0f);
		for (int32 k = 1; k <= 2; ++k)
		{
			const double Y = Y0 - k * C::RoundStepTreadPerBay * Bay;
			DrawDashedPreviewLine(PDI, At(CX - Half, Y), At(CX + Half, Y), Steps, 2.0f);
		}
		const double Tail = Y1 - 0.25 * Bay, Tip = CY - R + ColR;
		const double Head = 0.06 * Bay;
		DrawPreviewLine(PDI, At(CX, Tail), At(CX, Tip), Steps, 4.0f);
		DrawPreviewLine(PDI, At(CX, Tip), At(CX - Head, Tip - Head), Steps, 4.0f);
		DrawPreviewLine(PDI, At(CX, Tip), At(CX + Head, Tip - Head), Steps, 4.0f);
	}
}
