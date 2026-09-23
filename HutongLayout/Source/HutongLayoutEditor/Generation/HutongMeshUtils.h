#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongRoofTile.h"
#include "Generation/HutongFootprint.h"
#include "DynamicMesh/DynamicMesh3.h"

namespace HutongMeshUtils
{
	enum class EAxis2D : uint8 { X, Y };

	void AppendBox(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector3d& Min,
		const FVector3d& Max);

	// A prism with a ridge running along its length.
	void AppendTriPrism(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector3d& BaseMin,
		double Length,
		double Width,
		double ApexHeight,
		EAxis2D AlongAxis,
		double ApexFraction = 0.5);

	// Gable roof on a 舉架 section: one straight slope per 步架, creasing on the section's own breakpoints.
	// The profile AppendCurvedGableRoof builds on: (cross, height above the eave) at every crease,
	// front eave to wherever the far slope is cut. Anything laid along a gable edge follows this.
	TArray<FVector2d> GableRoofProfile(
		double Width,
		double ApexHeight,
		const HutongGen::FHutongRoofSection& Section,
		int32 SlopeSegments,
		double FarEaveTrim = 0.0);

	void AppendCurvedGableRoof(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector3d& BaseMin,
		double Length,
		double Width,
		double ApexHeight,
		const HutongGen::FHutongRoofSection& Section,
		int32 SlopeSegments,
		EAxis2D AlongAxis,
		double FarEaveTrim = 0.0,
		double UVTileSize = 20.0,
		// The triangle range [first, end) of the two gable end faces, for a caller that wants the
		// 山牆 pediment tagged as the brick it is rather than as roof.
		UE::Geometry::FIndex2i* OutGableFaceRange = nullptr);

	// A straight-sloped hip prism with no sag and no corner sweep.
	void AppendHipRoof(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector3d& EaveMin,
		const FVector3d& EaveMax,
		double RidgeHeightAboveEave);

	// Yaws every vertex appended since FirstVertexID about a vertical axis through PivotXY.
	void YawVerticesFrom(
		UE::Geometry::FDynamicMesh3& Mesh,
		int32 FirstVertexID,
		const FVector2d& PivotXY,
		double YawDegrees);

	// The footprint off square, by HutongFootprint::Map; z is untouched. Under Ends the mesh is
	// first split along the plane where each end zone begins, so a box that ran the whole length
	// bends there rather than tilting whole. Zero offsets cost nothing. Offsets that fold are
	// refused, the mesh left as built, and false returned.
	bool WarpFootprint(UE::Geometry::FDynamicMesh3& Mesh, double Width, double Depth, const FHutongFootprintSkew& Skew);

	// The general case of YawVerticesFrom.
	void TransformVerticesFrom(
		UE::Geometry::FDynamicMesh3& Mesh,
		int32 FirstVertexID,
		const FTransform& Transform);

	void EnsureUVLayer(UE::Geometry::FDynamicMesh3& Mesh);

	// Planar UVs from each triangle's dominant face axis, for every triangle without them.
	// Roof-slot triangles without authored UVs (corbels, ridge tails, skirts, wall caps) are
	// projected at one tile row per unit, the scale every authored roof uses, so the tile pattern
	// reads at tile size on them rather than a metre wide.
	void FillUnsetUVsBoxProjected(UE::Geometry::FDynamicMesh3& Mesh, double WorldPerUV = 100.0,
		double RoofWorldPerUV = HutongGen::RoofTile::DefaultRowSpacing);

	// Tags every triangle appended since FirstTriangleID.
	void SetMaterialIDForTrianglesFrom(
		UE::Geometry::FDynamicMesh3& Mesh,
		int32 FirstTriangleID,
		int32 MaterialID);

	// Tags [FirstTriangleID, EndTriangleID). Capture both marks with Mesh.MaxTriangleID().
	// Every triangle in FromSlot whose face looks down goes to ToSlot: the underside of an eave is
	// 望板 and rafters, not a picture of tiles from below. In this mesh's winding — CCW from
	// outside, reversed at bake — GetTriNormal points into the solid, so a face that looks down
	// reads as a normal pointing up; MinNormalZ is read against that.
	void RetagDownwardFaces(UE::Geometry::FDynamicMesh3& Mesh, int32 FromSlot, int32 ToSlot, double MinNormalZ = 0.3);

	void SetMaterialIDForTriangleRange(
		UE::Geometry::FDynamicMesh3& Mesh,
		int32 FirstTriangleID,
		int32 EndTriangleID,
		int32 MaterialID);

	void AppendCylinder(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector3d& BaseCenter,
		double Radius,
		double Height,
		int32 NumSides);

	// A closed 2D profile carried along a path: one ring per station, side strips between, both ends capped.
	void AppendSweptProfile(
		UE::Geometry::FDynamicMesh3& Mesh,
		const TArray<FVector2d>& Profile,
		const TArray<FTransform>& Stations);

	// A CCW circle profile matching AppendCylinder's vertex layout.
	TArray<FVector2d> MakeCircleProfile(double Radius, int32 NumSides);

	// 廡殿 with a ridge, 攢尖 without one, sagging on the 舉折 and flaring at the corners.
	struct FHipRoofSpec
	{
		double Width = 300.0;         // eave rectangle, X
		double Depth = 200.0;         // eave rectangle, Y
		// Ridge along X.
		double RidgeLength = 0.0;
		double Rise = 100.0;          // ridge above the eave line: the scale on the section below

		// 舉架 as a shape function — every panel takes its height from the section at its own normalized distance from the ridge, which keeps z a function of that parameter alone and so closes the hips.
		HutongGen::FHutongRoofSection Section;

		// 翼角起翹. Run is clamped against the length so the eave outline stays simple.
		double FlareRun = 0.0;        // how far the corner sweeps out along its diagonal
		double FlareRise = 0.0;       // how far it lifts
		double FlareLength = 0.0;     // how far back along each eave the sweep reaches

		// The eave course, built in: the solid continues below the eave line to a flat base.
		double FasciaDrop = 0.0;

		// 勾頭 round the whole eave ring.
		bool bEaveCaps = false;
		double TileRowSpacing = 20.0;

		int32 SlopeSegments = 4;      // samples up each slope
		int32 EaveSegments = 6;       // samples along each panel's eave
	};

	// EaveMin is the min corner of the eave rectangle, at the eave line.
	void AppendHippedRoof(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector3d& EaveMin,
		const FHipRoofSpec& Spec);

	// 歇山: a gabled upper roof on a hipped skirt, and the only roof the other two cannot express between them.
	struct FXieshanRoofSpec
	{
		double Width = 500.0;         // eave rectangle, X. The ridge runs this way.
		double Depth = 350.0;         // eave rectangle, Y
		double Rise = 150.0;          // ridge above the eave line: the scale on the section below

		// 舉架 as a shape function, as on the hipped roof.
		HutongGen::FHutongRoofSection Section;

		// 收山.
		double ShouInset = 90.0;

		// 翼角起翹, clamped as the hipped roof clamps them.
		double FlareRun = 0.0;
		double FlareRise = 0.0;
		double FlareLength = 0.0;

		double FasciaDrop = 0.0;

		// 勾頭 round the eave ring. 筒瓦 only.
		bool bEaveCaps = false;
		double TileRowSpacing = 20.0;

		// 正脊 along the top, 垂脊 down each rake, 戧脊 out along each corner hip.
		double RidgeWidth = 0.0;
		double RidgeHeight = 0.0;

		// 博風板 down the two rakes of each 山花. Zero thickness turns them off.
		double BargeThickness = 0.0;
		double BargeDepth = 0.0;

		int32 SlopeSegments = 6;      // samples up the full slope, eave to ridge
		int32 EaveSegments = 6;       // samples along each panel's eave
	};

	// OutMainRidge: the triangle range [A, B) of the 正脊, for the caller to tag after the roof.
	void AppendXieshanRoof(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector3d& EaveMin,
		const FXieshanRoofSpec& Spec,
		UE::Geometry::FIndex2i* OutMainRidge = nullptr);
}
