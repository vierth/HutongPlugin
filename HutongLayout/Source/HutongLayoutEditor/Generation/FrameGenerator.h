#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/SiheyuanGenerator.h"
#include "FrameGenerator.generated.h"

// 構架: a house's timber frame with nothing hung on it, standing on its 臺明 — 四合院建築及其構造
// 圖5-3-1 drawn whole. The house's own params decide every column, purlin and slope, so the frame
// is the one the house tool would build around.
USTRUCT(BlueprintType)
struct FHutongFrameParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frame", meta=(DisplayName="Platform (臺明)", ToolTip="Builds the platform with its column bases, edge stones and steps."))
	bool bHasPlatform = true;

	// Off by default: the frame is here to be looked at, and a roof over it hides the very members
	// it is placed to show. The purlins are the top of it until this is turned on.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frame", meta=(DisplayName="Rafters (椽)", ToolTip="Lays the rafters from purlin to purlin and out over the eaves."))
	bool bHasRafters = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frame", meta=(DisplayName="Flying Rafters (飛椽)", EditCondition="bHasRafters", ToolTip="Adds the flying rafters over the eave rafters."))
	bool bHasFlyingRafters = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frame", meta=(DisplayName="Eave Boards (連檐)", EditCondition="bHasRafters", ToolTip="Adds the boards across the rafter ends."))
	bool bHasEaveBoards = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frame", meta=(DisplayName="Roof Boards (望板)", EditCondition="bHasRafters", ToolTip="Covers the rafters with sheathing boards."))
	bool bHasRoofBoards = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frame", meta=(DisplayName="Ridge Braces (角背)", ToolTip="Adds the braces either side of the ridge post."))
	bool bHasRidgeBraces = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building", meta=(DisplayName="House (房)", ToolTip="The house whose frame this is; its bays, column heights and roof section set every member."))
	FHutongSiheyuanParams House;
};

namespace HutongGen
{
	// Where each purlin line of a frame stands. Shared by the generator, the ridge estimate and the tests.
	namespace FrameLayout
	{
		struct FLayout
		{
			// 柱徑 of the 檐柱, and of the 金柱.
			double D = 30.0;
			double GoldD = 33.0;
			double Floor = 0.0;
			// Purlin lines front to back: where each stands across the depth, and the top of what carries it.
			TArray<double> Y;
			TArray<double> Support;
			double Step = 100.0;
			// The rows the main beams span between: 1 and N-2 under 前後廊, 0 and N-1 with no 廊.
			int32 Front = 0;
			int32 Rear = 0;
			bool bFrontVeranda = false;
			bool bRearVeranda = false;

			int32 Num() const { return Y.Num(); }
			int32 Ridge() const { return Y.Num() / 2; }
			double PurlinCentre(int32 i) const;
			double PurlinTop(int32 i) const { return PurlinCentre(i) + 0.5 * D; }
		};

		// The house's Width and Depth must be filled in first.
		FLayout Make(const FHutongSiheyuanParams& House);
	}

	void BuildFrame(UE::Geometry::FDynamicMesh3& Mesh, const FHutongFrameParams& P);
}
