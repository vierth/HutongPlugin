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

	// Corners[0..3] the bottom loop, Corners[4..7] the top above them; either winding.
	void AppendHexahedron(UE::Geometry::FDynamicMesh3& Mesh, const FVector3d Corners[8]);

	void AppendTriPrism(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector3d& BaseMin,
		double Length,
		double Width,
		double ApexHeight,
		EAxis2D AlongAxis,
		double ApexFraction = 0.5);

	// (cross, height above eave) at each 步架 crease, front eave to the far slope's cut.
	TArray<FVector2d> GableRoofProfile(
		double Width,
		double ApexHeight,
		const HutongGen::FHutongRoofSection& Section,
		int32 SlopeSegments,
		double FarEaveTrim = 0.0);

	struct FGableUnderside
	{
		double InnerStart = 0.0;
		double InnerEnd = -1.0;    // cross of the rear column line; negative: the far edge
		double FrontLap = 0.0;
		double RearLap = 0.0;      // the rear eave course's depth, from the far edge; zero: no rear soffit
		double Drop = 0.0;
		double InnerRise = 0.0;
		double ShellCover = 0.0;
		// Cross of the 金柱-line wall behind a 前廊; negative: none.
		double VerandaEnd = -1.0;
	};

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
		// [first, end) of the gable end faces, so the caller can tag the 山牆 pediment as brick.
		UE::Geometry::FIndex2i* OutGableFaceRange = nullptr,
		// The underside tags itself wood.
		const FGableUnderside& Underside = FGableUnderside());

	void AppendHipRoof(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector3d& EaveMin,
		const FVector3d& EaveMax,
		double RidgeHeightAboveEave);

	void YawVerticesFrom(
		UE::Geometry::FDynamicMesh3& Mesh,
		int32 FirstVertexID,
		const FVector2d& PivotXY,
		double YawDegrees);

	// Box-projects unset UVs first, in the unwarped frame; folding offsets return false, mesh as it was.
	bool WarpFootprint(UE::Geometry::FDynamicMesh3& Mesh, double Width, double Depth, const FHutongFootprintSkew& Skew);

	void TransformVerticesFrom(
		UE::Geometry::FDynamicMesh3& Mesh,
		int32 FirstVertexID,
		const FTransform& Transform);

	void EnsureUVLayer(UE::Geometry::FDynamicMesh3& Mesh);

	// UVs in metres on the U/V plane, turned 45°, a whole brick centred on Centre.
	void SetDiagonalPaverUVs(UE::Geometry::FDynamicMesh3& Mesh, int32 FirstTri, const FVector3d& U, const FVector3d& V,
		const FVector3d& Centre);

	// Roof-slot triangles use RoofWorldPerUV so tiles read at tile size.
	void FillUnsetUVsBoxProjected(UE::Geometry::FDynamicMesh3& Mesh, double WorldPerUV = 100.0,
		double RoofWorldPerUV = HutongGen::RoofTile::DefaultRowSpacing);

	// Scopes may close in any order; a range tag flushes the open scope first, so it wins.
	class FSlotScope
	{
	public:
		FSlotScope(UE::Geometry::FDynamicMesh3& InMesh, int32 InSlot);
		~FSlotScope() { Close(); }
		FSlotScope(const FSlotScope&) = delete;
		FSlotScope& operator=(const FSlotScope&) = delete;

		void Close();

		static void FlushCurrent(const UE::Geometry::FDynamicMesh3& Mesh);

	private:
		void Flush();
		static FSlotScope* Current(const UE::Geometry::FDynamicMesh3& Mesh);

		UE::Geometry::FDynamicMesh3& Mesh;
		int32 Slot;
		int32 Flushed;
		bool bOpen = true;
	};

	// Pre-bake winding makes GetTriNormal point inward, so "down" means normal Z ≥ MinNormalZ.
	void TagUndersides(UE::Geometry::FDynamicMesh3& Mesh, int32 FirstTri, int32 EndTri, int32 Slot, double MinNormalZ = 0.3);

	// Tags [FirstTriangleID, EndTriangleID); capture both with Mesh.MaxTriangleID().
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

	void AppendSweptProfile(
		UE::Geometry::FDynamicMesh3& Mesh,
		const TArray<FVector2d>& Profile,
		const TArray<FTransform>& Stations);

	void SetSweptUVs(
		UE::Geometry::FDynamicMesh3& Mesh, int32 FirstVertex, int32 FirstTri,
		const TArray<FTransform>& Stations,
		TFunctionRef<FVector2f(int32 Station, const FVector2d& Local)> UVAt);

	// v runs down the image; pick UAxis as the viewer's right so the image reads unmirrored.
	void SetFaceUVs(UE::Geometry::FDynamicMesh3& Mesh, int32 FirstTri, int32 EndTri, const FVector3d& Outward,
		const FVector3d& Origin, const FVector3d& UAxis, double ULength, const FVector3d& VAxis, double VLength,
		double U0 = 0.0, double U1 = 1.0);

	void AppendYZPrism(UE::Geometry::FDynamicMesh3& Mesh, const TArray<FVector2d>& ProfileYZ, double X0, double X1);

	// A solid between two horizontal rings of equal count, each CCW from above, the bottom below the top. A ring may
	// repeat a point (an edge closing to nothing at one level): the side there is one triangle. Untriangulable: nothing.
	void AppendLoft(UE::Geometry::FDynamicMesh3& Mesh, const TArray<FVector3d>& Bottom, const TArray<FVector3d>& Top);

	// A CCW circle profile matching AppendCylinder's vertex layout.
	TArray<FVector2d> MakeCircleProfile(double Radius, int32 NumSides);

	void AppendRidgeStrip(
		UE::Geometry::FDynamicMesh3& Mesh,
		const TArray<FVector3d>& Path,
		const FVector3d& AcrossHint,
		double HalfWidth, double Below, double Above);

	struct FRoofPanel
	{
		FVector2d EaveA = FVector2d::ZeroVector;
		FVector2d EaveB = FVector2d::ZeroVector;
		FVector2d DiagA = FVector2d::ZeroVector;
		FVector2d DiagB = FVector2d::ZeroVector;
		double FlareLength = 0.0;
		double VMax = 1.0;
		TFunction<FVector3d(double U, double V)> Sample;
		// U of the point at distance A along the eave from EaveA, at height parameter V. Outside [0, 1] = past a hip.
		TFunction<double(double A, double V)> SolveU;
	};

	struct FHipRoofSpec
	{
		double Width = 300.0;
		double Depth = 200.0;
		// Ridge along X.
		double RidgeLength = 0.0;
		double Rise = 100.0;

		// 舉架 shape: z depends only on normalized distance from the ridge, so the hips close.
		HutongGen::FHutongRoofSection Section;

		// Run clamped so the eave outline stays simple.
		double FlareRun = 0.0;
		double FlareRise = 0.0;
		double FlareLength = 0.0;

		double FasciaDrop = 0.0;
		double EaveOverhang = 0.0;
		double UndersideRise = 0.0;
		double ShellCover = 0.0;

		bool bEaveCaps = false;
		double TileRowSpacing = 20.0;

		int32 SlopeSegments = 4;
		int32 EaveSegments = 6;
	};

	// V along the hip whose plan distance from the corner (V = 0) is Inset.
	double HipStartV(TFunctionRef<FVector3d(double V)> Hip, double Inset, double VMax = 1.0);

	// EaveMin: eave rectangle min corner, at eave height.
	void AppendHippedRoof(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector3d& EaveMin,
		const FHipRoofSpec& Spec,
		TArray<FRoofPanel>* OutPanels = nullptr);

	struct FXieshanRoofSpec
	{
		double Width = 500.0;   // ridge along X
		double Depth = 350.0;
		double Rise = 150.0;

		HutongGen::FHutongRoofSection Section;

		double ShouInset = 90.0;

		double FlareRun = 0.0;
		double FlareRise = 0.0;
		double FlareLength = 0.0;

		double FasciaDrop = 0.0;
		double EaveOverhang = 0.0;
		double UndersideRise = 0.0;
		double ShellCover = 0.0;

		bool bEaveCaps = false;
		double TileRowSpacing = 20.0;

		double RidgeWidth = 0.0;
		double RidgeHeight = 0.0;

		double BargeThickness = 0.0;
		double BargeDepth = 0.0;

		// An end hipped to the ridge instead of carrying a 山花: X = 0 (start) or X = Width (end). The L of a 角樓.
		bool bHipStart = false;
		bool bHipEnd = false;

		int32 SlopeSegments = 6;
		int32 EaveSegments = 6;
	};

	// The 正脊 tags itself ridge and the soffit wood; OutTimber (山花板, 博風板) is the caller's to tag.
	void AppendXieshanRoof(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector3d& EaveMin,
		const FXieshanRoofSpec& Spec,
		TArray<FRoofPanel>* OutPanels = nullptr,
		TArray<UE::Geometry::FIndex2i>* OutTimber = nullptr);

	// Open chains run axis to axis; CCW from outside either way. OutEdgeFirstTri: each edge's first triangle, then the end.
	void AppendRevolvedProfile(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector2d& Centre,
		const TArray<FVector2d>& RZ,
		int32 Segments,
		bool bClosed = false,
		TArray<int32>* OutEdgeFirstTri = nullptr);

	// Angles in degrees, CCW A0 to A1; profile X up, Y outward.
	void AppendArcSweep(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector2d& Centre, double Radius, double Z,
		double A0, double A1,
		const TArray<FVector2d>& Profile,
		int32 Segments);

	struct FRoundRoofSpec
	{
		double Radius = 200.0;
		double Rise = 100.0;
		HutongGen::FHutongRoofSection Section;
		double FasciaDrop = 0.0;
		double EaveOverhang = 0.0;
		double UndersideRise = 0.0;
		double ShellCover = 0.0;
		int32 Panels = 72;
		int32 CoursesPerPanel = 1;
		int32 Segments = 96;
		int32 SlopeSegments = 12;
	};
	// Soffit and underside are OutUnderside, the caller's to tag wood; slope u is one 壟 per unit round the eave.
	void AppendRoundRoof(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector3d& Centre,
		const FRoundRoofSpec& Spec,
		UE::Geometry::FIndex2i* OutUnderside = nullptr);
	double RoundRoofPitch(const FRoundRoofSpec& Spec);
	TArray<FRoofPanel> MakeRoundRoofPanels(const FVector3d& Centre, const FRoundRoofSpec& Spec);
	double RoundRoofHeight(const FRoundRoofSpec& Spec, double Radius);
}
