#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongJiajia.h"
#include "Templates/Function.h"
#include "HutongDetail.generated.h"

UENUM(BlueprintType)
enum class EHutongDetail : uint8
{
	Massing UMETA(DisplayName = "Block (塊)", ToolTip="Platform, one block to the eave and the plain roof, with no ornament."),

	Far     UMETA(DisplayName = "Far (遠)", ToolTip="The full generator with ornament off; walls, doorways, platforms and steps stay."),

	Near    UMETA(DisplayName = "Near (近)", ToolTip="The full building at standard detail."),

	Hero    UMETA(DisplayName = "Fine (精)", ToolTip="Near detail with the roof segment counts raised."),
};

struct FHutongWallParams;
struct FHutongSiheyuanParams;
struct FHutongGateHouseParams;
struct FHutongPaifangParams;
struct FHutongScreenWallParams;
struct FHutongCorridorParams;
struct FHutongInnerGateParams;
struct FHutongShopfrontParams;
struct FHutongStoreyParams;
struct FHutongPavilionParams;
struct FHutongPathParams;
struct FHutongPassageParams;
struct FHutongFlowerBedParams;
struct FHutongWaterJarParams;
struct FHutongHallParams;

namespace HutongGen
{
	namespace Detail
	{
		inline bool IsMassing(EHutongDetail D) { return D == EHutongDetail::Massing; }

		void Apply(EHutongDetail D, FHutongWallParams& P);
		void Apply(EHutongDetail D, FHutongSiheyuanParams& P);
		void Apply(EHutongDetail D, FHutongGateHouseParams& P);
		void Apply(EHutongDetail D, FHutongPaifangParams& P);
		void Apply(EHutongDetail D, FHutongScreenWallParams& P);
		void Apply(EHutongDetail D, FHutongCorridorParams& P);
		void Apply(EHutongDetail D, FHutongInnerGateParams& P);
		void Apply(EHutongDetail D, FHutongShopfrontParams& P);
		void Apply(EHutongDetail D, FHutongStoreyParams& P);
		void Apply(EHutongDetail D, FHutongPavilionParams& P);
		void Apply(EHutongDetail D, FHutongPathParams& P);
		void Apply(EHutongDetail D, FHutongPassageParams& P);
		void Apply(EHutongDetail D, FHutongFlowerBedParams& P);
		void Apply(EHutongDetail D, FHutongWaterJarParams& P);
		void Apply(EHutongDetail D, FHutongHallParams& P);

		TArray<EHutongDetail> LODChain(EHutongDetail Placed);

		float LODScreenSize(int32 LODIndex);

		void BuildLODChain(
			EHutongDetail Placed,
			bool bChain,
			TFunctionRef<void(UE::Geometry::FDynamicMesh3&, EHutongDetail)> Build,
			TArray<UE::Geometry::FDynamicMesh3>& OutLODs);

		void BuildPlacementLODs(
			bool bPlanOnly,
			double SizeX,
			double SizeY,
			EHutongDetail Placed,
			bool bChain,
			TFunctionRef<void(UE::Geometry::FDynamicMesh3&, EHutongDetail)> Build,
			TArray<UE::Geometry::FDynamicMesh3>& OutLODs);
	}

	namespace Massing
	{
		enum class ERoof : uint8 { None, Gable, Hipped, Xieshan };

		struct FBlock
		{
			double Width = 100.0;
			double Depth = 100.0;

			double OriginY = 0.0;

			// 臺基. Zero for a piece that stands on the ground.
			double FloorHeight = 0.0;
			double PlatformOverhang = 0.0;

			// Top of the body: the eave line for a roofed piece, the whole height for a wall.
			double EaveHeight = 100.0;

			bool bOpen = false;
			double ColumnSize = 0.0;
			int32 ColumnsAlongWidth = 2;
			int32 ColumnRows = 2;

			ERoof Roof = ERoof::Gable;

			// 舉架 as the real roofs take it, so the rise and the creases are the building's own.
			HutongGen::FHutongRoofSection Section;
			double Rise = 0.0;            // zero takes the section's own
			double FrontOverhang = 0.0;
			double RearOverhang = 0.0;
			double GableOverhang = 0.0;
			double RearSlopeTrim = 0.0;

			// Hipped family only.
			double RidgeLength = 0.0;
			double ShouInset = 0.0;
		};

		void AppendBlock(UE::Geometry::FDynamicMesh3& Mesh, const FBlock& B);

		FBlock From(const FHutongSiheyuanParams& P);
		FBlock From(const FHutongGateHouseParams& P);
		FBlock From(const FHutongPaifangParams& P);
		FBlock From(const FHutongScreenWallParams& P);
		FBlock From(const FHutongCorridorParams& P);
		FBlock From(const FHutongInnerGateParams& P);
		FBlock From(const FHutongShopfrontParams& P);
		FBlock From(const FHutongStoreyParams& P);
		FBlock From(const FHutongPavilionParams& P);
		FBlock From(const FHutongPassageParams& P);
		FBlock From(const FHutongHallParams& P);
	}
}
