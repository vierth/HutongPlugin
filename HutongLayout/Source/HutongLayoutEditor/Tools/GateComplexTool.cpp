#include "Tools/GateComplexTool.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongActorSpawn.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"
#include "SceneManagement.h"
#include "Misc/ScopedSlowTask.h"

UInteractiveTool* UHutongGateComplexToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongGateComplexTool>(SceneState.ToolManager);
}

void UHutongGateComplexTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongGateComplexToolProperties>(this);
	RegisterSettings(Settings);
}

void UHutongGateComplexTool::ApplyCompass()
{
	if (!Settings) return;
	// North 0, east a quarter turn on from it (the compound's frame: east is local -X with north +Y).
	const double OutsideYaw = Settings->NorthYawDeg + 90.0 * (double)(int32)Settings->OutsideFaces;
	// Local -Y toward the outside.
	PlacementYawDeg = OutsideYaw + 90.0;
}

FHutongGateComplexParams UHutongGateComplexTool::FacedParams() const
{
	FHutongGateComplexParams P = Settings ? Settings->Params : FHutongGateComplexParams();
	if (!Settings) return P;
	// The layout puts an "east" gate on local +X; facing south (or west) that hand is the other.
	const int32 Faces = (int32)Settings->OutsideFaces;
	if (Faces == (int32)EHutongCompassSide::South || Faces == (int32)EHutongCompassSide::West)
	{
		P.SideGate = P.SideGate == EHutongBarbicanGateSide::East ? EHutongBarbicanGateSide::West : EHutongBarbicanGateSide::East;
	}
	return P;
}

void UHutongGateComplexTool::OnPlacementStarted(const FVector& HitWorld)
{
	ApplyCompass();
	// One click places it: the preview has followed the cursor already.
	FinishPlacement();
}

void UHutongGateComplexTool::GetEffectiveRectBounds(double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	// Render draws the pieces; the base's rectangle is a point.
	OutMinX = OutMinY = 0.0;
	OutMaxX = OutMaxY = 1.0;
}

void UHutongGateComplexTool::DrawPieces(FPrimitiveDrawInterface* PDI, const FVector& Origin) const
{
	TArray<HutongGen::FGateComplexPiece> Pieces;
	HutongGen::LayOutGateComplex(FacedParams(), Pieces);
	auto At = [&](const FVector2D& P) { return LocalRectToWorldFrom(Origin, P.X, P.Y); };
	for (const HutongGen::FGateComplexPiece& Piece : Pieces)
	{
		FVector2D C[4];
		HutongGen::GateComplexPieceCorners(Piece, C);
		const bool bWall = Piece.Kind == HutongGen::FGateComplexPiece::EKind::Wall;
		const FLinearColor Colour = bWall ? FLinearColor(1.0f, 0.9f, 0.15f) : FLinearColor(1.0f, 0.45f, 0.1f);
		for (int32 k = 0; k < 4; ++k) DrawPreviewLine(PDI, At(C[k]), At(C[(k + 1) % 4]), Colour, bWall ? 4.0f : 6.0f);
	}
	// An arrow out of the city from the gate.
	const FLinearColor Arrow(0.2f, 0.8f, 1.0f);
	DrawPreviewLine(PDI, At(FVector2D(0.0, 0.0)), At(FVector2D(0.0, -1500.0)), Arrow, 6.0f);
	DrawPreviewLine(PDI, At(FVector2D(-400.0, -1100.0)), At(FVector2D(0.0, -1500.0)), Arrow, 6.0f);
	DrawPreviewLine(PDI, At(FVector2D(400.0, -1100.0)), At(FVector2D(0.0, -1500.0)), Arrow, 6.0f);
}

void UHutongGateComplexTool::RenderIdlePreview(FPrimitiveDrawInterface* PDI, const FVector& CursorGround)
{
	ApplyCompass();
	DrawPieces(PDI, CursorGround);
}

FText UHutongGateComplexTool::GetStagePromptText() const
{
	if (bIsDragging) return Super::GetStagePromptText();
	return NSLOCTEXT("GateComplexTool", "Prompt", "Click the middle of the gate's outer face on the line of the city wall to place the gate. Set which way it faces in the panel.");
}

void UHutongGateComplexTool::SpawnFinalActor()
{
	UWorld* World = GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld();
	if (!World || !Settings) return;
	TArray<HutongGen::FGateComplexPiece> Pieces;
	HutongGen::LayOutGateComplex(FacedParams(), Pieces);

	FScopedSlowTask Task((float)Pieces.Num(), NSLOCTEXT("GateComplexTool", "Placing", "Laying out the gate…"));
	Task.MakeDialog();
	UInteractiveToolManager* ToolManager = GetToolManager();
	ToolManager->BeginUndoTransaction(NSLOCTEXT("GateComplexTool", "Place", "Place Hutong Gate Complex"));
	for (const HutongGen::FGateComplexPiece& Piece : Pieces)
	{
		Task.EnterProgressFrame(1.0f);
		const FVector At = LocalRectToWorldFrom(StartWorld, Piece.Origin.X, Piece.Origin.Y);
		const FTransform Xform(FRotator(0.0, PlacementYawDeg + Piece.YawDeg, 0.0), FVector(At.X, At.Y, StartWorld.Z));
		using EKind = HutongGen::FGateComplexPiece::EKind;
		const TCHAR* Name = Piece.Kind == EKind::Gate ? TEXT("Hutong_CityGate") : Piece.Kind == EKind::ArrowTower ? TEXT("Hutong_ArrowTower")
			: Piece.Kind == EKind::Corner ? TEXT("Hutong_WallCorner") : TEXT("Hutong_CityWall");
		AStaticMeshActor* Actor = HutongGen::SpawnEmptyActor(World, Xform, Name);
		if (!Actor) continue;

		UHutongBuildingComponent* Building = nullptr;
		switch (Piece.Kind)
		{
		case EKind::Gate:
		{
			UHutongCityGateBuildingComponent* B = NewObject<UHutongCityGateBuildingComponent>(Actor, NAME_None, RF_Transactional);
			B->Params = Piece.Gate;
			B->Facade = Piece.Facade;
			Building = B;
			break;
		}
		case EKind::ArrowTower:
		{
			UHutongArrowTowerBuildingComponent* B = NewObject<UHutongArrowTowerBuildingComponent>(Actor, NAME_None, RF_Transactional);
			B->Params = Piece.Tower;
			B->Facade = Piece.Facade;
			Building = B;
			break;
		}
		case EKind::Corner:
		{
			UHutongWallCornerBuildingComponent* B = NewObject<UHutongWallCornerBuildingComponent>(Actor, NAME_None, RF_Transactional);
			B->Params = Piece.Wall;
			B->Shape = EHutongWallCornerShape::Rounded;
			B->PlatformSize = Piece.CornerSize;
			B->OuterCorner = Piece.OuterCorner;
			Building = B;
			break;
		}
		default:
		{
			UHutongCityWallBuildingComponent* B = NewObject<UHutongCityWallBuildingComponent>(Actor, NAME_None, RF_Transactional);
			B->Params = Piece.Wall;
			B->Length = Piece.Length;
			B->OuterSide = Piece.OuterSide;
			B->FootprintSkew = Piece.Skew;
			Building = B;
			break;
		}
		}
		if (Appearance) Building->Palette = Appearance->Palette;
		StampDetail(Building);
		Actor->AddInstanceComponent(Building);
		Building->RegisterComponent();
		Building->Rebuild();
		Building->ApplyPlacementAttachments();
	}
	ToolManager->EndUndoTransaction();
}

FString UHutongGateComplexTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	const FHutongGateComplexParams& P = Settings->Params;
	return FString::Printf(TEXT("Gate complex (德勝門) · barbican %.0f × %.0f m · gate tower, arrow tower, barbican gate on the %s · the blue arrow points out of the city"),
		P.BarbicanWidth / 100.0, P.BarbicanDepth / 100.0, P.SideGate == EHutongBarbicanGateSide::East ? TEXT("east") : TEXT("west"));
}

TArray<FText> UHutongGateComplexTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongGateComplexTool", "HelpPlace",
		"The whole gate follows the cursor; click the middle of the gate's outer face on the line of the city wall to place it. Outside Faces and North Direction in the panel turn it.");
	Lines.Insert(NSLOCTEXT("HutongGateComplexTool", "HelpWhat",
		"Lays out a whole gate as at Deshengmen (德勝門): the gate tower (城樓), the barbican (甕城) with its rounded corners, the arrow tower (箭樓), the barbican's gate (閘樓) and a stretch of city wall either side. Each piece can be changed afterwards; carry the city wall on from the stretches' open ends."), 1);
	return Lines;
}
