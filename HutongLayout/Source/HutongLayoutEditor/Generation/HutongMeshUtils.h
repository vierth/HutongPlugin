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

	// Convex solid of six planar quad faces: Corners[0..3] the bottom loop, Corners[4..7] the top, each above
	// its partner. Faces wound outward by the solid's centroid, so the loops may run either way. For a
	// battered or sloped block (a ramp on a battered face, a bastion) a box cannot express.
	void AppendHexahedron(UE::Geometry::FDynamicMesh3& Mesh, const FVector3d Corners[8]);

	// Prism with a ridge along its length.
	void AppendTriPrism(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector3d& BaseMin,
		double Length,
		double Width,
		double ApexHeight,
		EAxis2D AlongAxis,
		double ApexFraction = 0.5);

	// AppendCurvedGableRoof's profile: (cross, height above eave) at each 步架 crease, front eave to
	// the far slope's cut. Anything laid along a gable edge follows this.
	TArray<FVector2d> GableRoofProfile(
		double Width,
		double ApexHeight,
		const HutongGen::FHutongRoofSection& Section,
		int32 SlopeSegments,
		double FarEaveTrim = 0.0);

	// Where a gable roof's underside leaves the eave line: flat between the column lines (the rooms'
	// ceiling), rising from under each eave course to them (the soffit). All zero: flat throughout.
	struct FGableUnderside
	{
		double InnerStart = 0.0;   // cross of the front column line
		double InnerEnd = -1.0;    // cross of the rear column line; negative: the far edge
		double FrontLap = 0.0;     // cross of the front eave course's back face
		double RearLap = 0.0;      // the rear eave course's depth, from the far edge; zero: no rear soffit
		double Drop = 0.0;         // how far below the eave line the soffit starts
		double InnerRise = 0.0;    // the ceiling between the column lines, above the eave line
		// 徹上明造: between the column lines the underside follows the slope this far below it (plumb),
		// the roof a shell on its rafters, instead of a flat ceiling. Zero: flat.
		double ShellCover = 0.0;
		// Behind a 前廊, the cross of the wall face on the 金柱 line: from the front column line to here the
		// underside rises with the slope (the 檐椽 open over the 廊, up to the 金檁), starting from the level
		// ceiling's height; the rooms keep the level ceiling. Negative: none.
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

	// Straight-sloped hip prism: no sag, no corner sweep.
	void AppendHipRoof(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector3d& EaveMin,
		const FVector3d& EaveMax,
		double RidgeHeightAboveEave);

	// Yaws vertices since FirstVertexID about a vertical axis through PivotXY.
	void YawVerticesFrom(
		UE::Geometry::FDynamicMesh3& Mesh,
		int32 FirstVertexID,
		const FVector2d& PivotXY,
		double YawDegrees);

	// Applies HutongFootprint::Map in XY. Box-projects unset UVs first, in the unwarped frame. Ends first
	// splits at each zone seam so long boxes bend there. Zero offsets are free; folding offsets leave
	// the mesh untouched and return false.
	bool WarpFootprint(UE::Geometry::FDynamicMesh3& Mesh, double Width, double Depth, const FHutongFootprintSkew& Skew);

	// General form of YawVerticesFrom.
	void TransformVerticesFrom(
		UE::Geometry::FDynamicMesh3& Mesh,
		int32 FirstVertexID,
		const FTransform& Transform);

	void EnsureUVLayer(UE::Geometry::FDynamicMesh3& Mesh);

	// 方磚 laid on the diagonal: UVs in metres on the plane of U and V, turned 45°, for every triangle from
	// FirstTri, a whole brick centred on Centre (the pattern's joints fall on whole bricks) so a field is symmetric.
	void SetDiagonalPaverUVs(UE::Geometry::FDynamicMesh3& Mesh, int32 FirstTri, const FVector3d& U, const FVector3d& V,
		const FVector3d& Centre);

	// Box-projected UVs for triangles without them. Roof-slot ones use RoofWorldPerUV (one tile row
	// per unit, as authored roofs) so tiles read at tile size.
	void FillUnsetUVsBoxProjected(UE::Geometry::FDynamicMesh3& Mesh, double WorldPerUV = 100.0,
		double RoofWorldPerUV = HutongGen::RoofTile::DefaultRowSpacing);

	// A part's material stated where it is built: every triangle appended while this is the most recently
	// opened, still open scope on its mesh takes Slot. Close() (or the destructor) ends it; scopes may
	// close in any order. A range tag applies after the open scope's pending triangles are tagged, so it
	// wins over them.
	class FSlotScope
	{
	public:
		FSlotScope(UE::Geometry::FDynamicMesh3& InMesh, int32 InSlot);
		~FSlotScope() { Close(); }
		FSlotScope(const FSlotScope&) = delete;
		FSlotScope& operator=(const FSlotScope&) = delete;

		void Close();

		// Tags what the mesh's current scope has pending; the range tags call it first.
		static void FlushCurrent(const UE::Geometry::FDynamicMesh3& Mesh);

	private:
		void Flush();
		static FSlotScope* Current(const UE::Geometry::FDynamicMesh3& Mesh);

		UE::Geometry::FDynamicMesh3& Mesh;
		int32 Slot;
		int32 Flushed;
		bool bOpen = true;
	};

	// Tags the downward-facing triangles of [FirstTri, EndTri) as Slot: a box's underside under an eave.
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

	// Closed 2D profile swept through Stations: ring per station, side strips, both ends capped.
	void AppendSweptProfile(
		UE::Geometry::FDynamicMesh3& Mesh,
		const TArray<FVector2d>& Profile,
		const TArray<FTransform>& Stations);

	// UVs for what AppendSweptProfile just appended (vertices from FirstVertex, triangles from FirstTri,
	// through Stations): each vertex's UV from its station and its point in that station's profile plane.
	void SetSweptUVs(
		UE::Geometry::FDynamicMesh3& Mesh, int32 FirstVertex, int32 FirstTri,
		const TArray<FTransform>& Stations,
		TFunctionRef<FVector2f(int32 Station, const FVector2d& Local)> UVAt);

	// A sign's face mapped once: triangles of [FirstTri, EndTri) facing Outward get u from U0 to U1 along
	// UAxis over ULength from Origin and v from 0 to 1 along VAxis over VLength (v down the image).
	// Pick UAxis as the viewer's right, so the image reads unmirrored.
	void SetFaceUVs(UE::Geometry::FDynamicMesh3& Mesh, int32 FirstTri, int32 EndTri, const FVector3d& Outward,
		const FVector3d& Origin, const FVector3d& UAxis, double ULength, const FVector3d& VAxis, double VLength,
		double U0 = 0.0, double U1 = 1.0);

	// Solid of one (Y, Z) section from X0 to X1.
	void AppendYZPrism(UE::Geometry::FDynamicMesh3& Mesh, const TArray<FVector2d>& ProfileYZ, double X0, double X1);

	// A CCW circle profile matching AppendCylinder's vertex layout.
	TArray<FVector2d> MakeCircleProfile(double Radius, int32 NumSides);

	// Small solid strip along roof-surface points (正脊, 垂脊, 戧脊): HalfWidth across, Below/Above the path.
	void AppendRidgeStrip(
		UE::Geometry::FDynamicMesh3& Mesh,
		const TArray<FVector3d>& Path,
		const FVector3d& AcrossHint,
		double HalfWidth, double Below, double Above);

	// One slope of a roof, for what is laid on it (tile courses, 椽頭, ridges): its surface and how a
	// course square to its eave finds its way up it. Plan XY in the mesh's frame; eaves run CCW.
	struct FRoofPanel
	{
		FVector2d EaveA = FVector2d::ZeroVector;
		FVector2d EaveB = FVector2d::ZeroVector;
		// Outward corner diagonals at each end, which 椽頭 fan toward under a 翼角.
		FVector2d DiagA = FVector2d::ZeroVector;
		FVector2d DiagB = FVector2d::ZeroVector;
		double FlareLength = 0.0;
		// Top of the slope parameter: 1 at a ridge, less where the panel stops short (歇山 end panels).
		double VMax = 1.0;
		// Point on the slope; U across the panel, V from eave (0) up.
		TFunction<FVector3d(double U, double V)> Sample;
		// U of the point at distance A along the eave from EaveA, at height parameter V. Outside [0, 1] = past a hip.
		TFunction<double(double A, double V)> SolveU;
	};

	// 廡殿 (with ridge) or 攢尖 (without), sagging on the 舉折, flared corners.
	struct FHipRoofSpec
	{
		double Width = 300.0;         // eave rectangle, X
		double Depth = 200.0;         // eave rectangle, Y
		// Ridge along X.
		double RidgeLength = 0.0;
		double Rise = 100.0;          // ridge above eave; scales the section

		// 舉架 shape: z depends only on normalized distance from the ridge, so the hips close.
		HutongGen::FHutongRoofSection Section;

		// 翼角起翹. Run clamped so the eave outline stays simple.
		double FlareRun = 0.0;        // corner sweep out along the diagonal
		double FlareRise = 0.0;       // corner lift
		double FlareLength = 0.0;     // sweep reach back along each eave

		// Eave course: solid continues below the eave line to a flat base.
		double FasciaDrop = 0.0;
		// Eave to column line: the soffit rises from the eave course's base to it. Zero: flat soffit.
		double EaveOverhang = 0.0;
		// The ceiling at the column line above the eave line (HutongGen::Proportions::UndersideRise).
		double UndersideRise = 0.0;
		// 徹上明造: the roof a shell this thick (plumb) inside the column line; zero = flat ceiling.
		double ShellCover = 0.0;

		// 勾頭 round the eave ring.
		bool bEaveCaps = false;
		double TileRowSpacing = 20.0;

		int32 SlopeSegments = 4;      // samples up each slope
		int32 EaveSegments = 6;       // samples along each panel's eave
	};

	// Where a hip ridge starts: V along the hip whose plan distance from the corner (V = 0) is Inset.
	// Run to the flared tip, the ridge's end hung below the eave as a block.
	double HipStartV(TFunctionRef<FVector3d(double V)> Hip, double Inset, double VMax = 1.0);

	// EaveMin: eave rectangle min corner, at eave height.
	void AppendHippedRoof(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector3d& EaveMin,
		const FHipRoofSpec& Spec,
		TArray<FRoofPanel>* OutPanels = nullptr);   // the soffit tags itself wood

	// 歇山: gabled upper roof on a hipped skirt.
	struct FXieshanRoofSpec
	{
		double Width = 500.0;         // eave rectangle, X; ridge runs along X
		double Depth = 350.0;         // eave rectangle, Y
		double Rise = 150.0;          // ridge above eave; scales the section

		// 舉架 shape, as on the hipped roof.
		HutongGen::FHutongRoofSection Section;

		// 收山.
		double ShouInset = 90.0;

		// 翼角起翹, clamped as on the hipped roof.
		double FlareRun = 0.0;
		double FlareRise = 0.0;
		double FlareLength = 0.0;

		double FasciaDrop = 0.0;
		double EaveOverhang = 0.0;    // as on the hipped roof
		double UndersideRise = 0.0;
		double ShellCover = 0.0;

		// 勾頭 round the eave ring. 筒瓦 only.
		bool bEaveCaps = false;
		double TileRowSpacing = 20.0;

		// 正脊 along the top, 垂脊 down each rake, 戧脊 out along each corner hip.
		double RidgeWidth = 0.0;
		double RidgeHeight = 0.0;

		// 博風板 down each 山花's rakes; zero = off.
		double BargeThickness = 0.0;
		double BargeDepth = 0.0;

		int32 SlopeSegments = 6;      // samples up the full slope, eave to ridge
		int32 EaveSegments = 6;       // samples along each panel's eave
	};

	// The 正脊 tags itself ridge and the soffit wood; OutTimber (山花板, 博風板) is the caller's to tag.
	void AppendXieshanRoof(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector3d& EaveMin,
		const FXieshanRoofSpec& Spec,
		TArray<FRoofPanel>* OutPanels = nullptr,
		TArray<UE::Geometry::FIndex2i>* OutTimber = nullptr);

	// A chain in (r, z) turned about the vertical axis through Centre: each point a ring, a point on the
	// axis (r = 0) a single pole. Open chains run axis to axis; bClosed joins the last point to the first
	// (a ring solid). CCW from outside whichever way the chain runs. OutEdgeFirstTri: the first triangle of
	// each edge, then the end, so a caller can tag by edge.
	void AppendRevolvedProfile(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector2d& Centre,
		const TArray<FVector2d>& RZ,
		int32 Segments,
		bool bClosed = false,
		TArray<int32>* OutEdgeFirstTri = nullptr);

	// A closed 2D profile swept along a horizontal arc about Centre, CCW from angle A0 to A1 (degrees):
	// profile X up, Y outward from the centre.
	void AppendArcSweep(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector2d& Centre, double Radius, double Z,
		double A0, double A1,
		const TArray<FVector2d>& Profile,
		int32 Segments);

	// 圓攢尖: a roof of revolution on the section, no hips. Eave circle at EaveZ.
	struct FRoundRoofSpec
	{
		double Radius = 200.0;        // eave, in plan
		double Rise = 100.0;          // apex above the eave; scales the section
		HutongGen::FHutongRoofSection Section;
		double FasciaDrop = 0.0;
		double EaveOverhang = 0.0;    // eave to column line, where the soffit meets the ceiling
		double UndersideRise = 0.0;
		double ShellCover = 0.0;      // 徹上明造; zero = flat ceiling
		// 壟 round the eave, Panels × CoursesPerPanel of them; the slope's UVs put a course at each half-unit.
		int32 Panels = 72;
		int32 CoursesPerPanel = 1;
		int32 Segments = 96;          // round the axis
		int32 SlopeSegments = 12;
	};
	// Slope and eave course take the caller's tag; soffit and underside are OutUnderside, the caller's to
	// tag wood. Slope UVs: u one 壟 a unit round the eave (converging, as tapered 筒瓦 do), v arc up it.
	void AppendRoundRoof(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FVector3d& Centre,
		const FRoundRoofSpec& Spec,
		UE::Geometry::FIndex2i* OutUnderside = nullptr);
	// The eave pitch the courses land on: the panel chord over its courses.
	double RoundRoofPitch(const FRoundRoofSpec& Spec);
	// Sector panels, eave to apex, for what the dressing lays along the eave (椽頭).
	TArray<FRoofPanel> MakeRoundRoofPanels(const FVector3d& Centre, const FRoundRoofSpec& Spec);
	// Slope height above the eave line at a plan radius.
	double RoundRoofHeight(const FRoundRoofSpec& Spec, double Radius);
}
