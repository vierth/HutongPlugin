#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/SiheyuanGenerator.h"
#include "FrameGenerator.generated.h"

// 構架: a house's bare timber frame on its 臺明 (四合院建築及其構造 圖5-3-1), from the house params.
USTRUCT(BlueprintType)
struct FHutongFrameParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frame", meta=(HutongBasic, DisplayName="Platform (臺明)", ToolTip="Builds the platform with its column bases, edge stones and steps."))
	bool bHasPlatform = true;

	// Off by default: a roof hides the members the frame is placed to show.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frame", meta=(HutongBasic, DisplayName="Rafters (椽)", ToolTip="Lays the rafters from purlin to purlin and out over the eaves."))
	bool bHasRafters = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frame", meta=(DisplayName="Flying Rafters (飛椽)", EditCondition="bHasRafters", ToolTip="Adds the flying rafters over the eave rafters."))
	bool bHasFlyingRafters = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frame", meta=(DisplayName="Eave Boards (連檐)", EditCondition="bHasRafters", ToolTip="Adds the boards across the rafter ends."))
	bool bHasEaveBoards = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frame", meta=(HutongBasic, DisplayName="Roof Boards (望板)", EditCondition="bHasRafters", ToolTip="Covers the rafters with sheathing boards."))
	bool bHasRoofBoards = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frame", meta=(DisplayName="Ridge Braces (角背)", ToolTip="Adds the braces either side of the ridge post."))
	bool bHasRidgeBraces = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Building", meta=(DisplayName="House (房)", ToolTip="The house whose frame is shown."))
	FHutongSiheyuanParams House;
};

namespace HutongGen
{
	// Purlin line positions; shared by the generator, the ridge estimate and the tests.
	namespace FrameLayout
	{
		struct FLayout
		{
			// 柱徑 of the 檐柱 and 金柱.
			double D = 30.0;
			double GoldD = 33.0;
			double Floor = 0.0;
			// Purlin lines front to back: position across the depth, top of their support.
			TArray<double> Y;
			TArray<double> Support;
			double Step = 100.0;
			// Rows the main beams span: 1 and N-2 under 前後廊, 0 and N-1 with no 廊.
			int32 Front = 0;
			int32 Rear = 0;
			bool bFrontVeranda = false;
			bool bRearVeranda = false;

			int32 Num() const { return Y.Num(); }
			int32 Ridge() const { return Y.Num() / 2; }
			double PurlinCentre(int32 i) const;
			double PurlinTop(int32 i) const { return PurlinCentre(i) + 0.5 * D; }
		};

		// Requires the house's Width and Depth filled in.
		FLayout Make(const FHutongSiheyuanParams& House);

		// Any building's: 檐柱 diameter and top, depth column line to column line, 檁數, 廊.
		FLayout Make(double ColumnDiameter, double ColumnTop, double Floor, double Depth, EHutongPurlins Purlins,
			bool bFrontVeranda, bool bRearVeranda);
	}

	// The 梁架 over the column tops, 圖5-3-1: a cross frame (抱頭梁, main beams, 瓜柱, 角背) on each of
	// FrameX, and 檁 with 枋 and 墊板 under it on each purlin line from X0 to X1. YOffset slides the layout's
	// depth (its first column line at 0). The eave lines' 檁 are left to AppendEaveStack when bSkipEaveLines.
	struct FRoofFrameOptions
	{
		bool bSkipEaveLines = false;
		bool bRidgeBraces = true;
		// 隨梁枋 under the outermost main beam, column to column (a 卷棚 walk's under its 四架梁, 圖18).
		bool bTieUnderMainBeam = false;
		double YOffset = 0.0;
		// No member reaches past these (layout depth): beam heads and 出頭 stop inside a wall there.
		double FrontLimit = -BIG_NUMBER;
		double RearLimit = BIG_NUMBER;
		// Per purlin line, where its 檁 starts and stops (a hipped roof's lines end at the hips); unset = X0..X1.
		TFunction<void(int32 Line, double& X0, double& X1)> LineExtent;
		// The roof's underside at a depth (layout coordinates): each line's 檁 is held under it by a rafter, so
		// a 捲棚's crown, rounded below the fold, still clears its members. Unset = the layout's own heights.
		TFunction<double(double Y)> Underside;
		// 中柱式 (四合院建築及其構造 圖5-1-4.2): a 中柱 of this diameter on each frame line, floor to 脊檁, with 步梁
		// from each side's outer line into it and a 瓜柱 on each under the next line, in place of the stacked
		// beams. 0 = the stack.
		double CentreColumnDiameter = 0.0;
		// 金柱 of a 廊 (L.GoldD) on each frame line, floor to its purlin's support: where a building's walls
		// show them (金柱大門).
		bool bGoldColumns = false;
		// 穿插枋 under a 廊's 抱頭梁 (and a 鑽金柱 frame's 插梁). Off where a wall holds it out of sight (圖5-1-3).
		bool bVerandaTies = true;
		// 懸山: every 檁 runs this far past X0 and X1, a 燕尾枋 under each end.
		double GableReach = 0.0;
	};
	void AppendRoofFrame(UE::Geometry::FDynamicMesh3& Mesh, const FrameLayout::FLayout& L,
		const TArray<double>& FrameX, double X0, double X1, const FRoofFrameOptions& Options);

	void BuildFrame(UE::Geometry::FDynamicMesh3& Mesh, const FHutongFrameParams& P);
}
