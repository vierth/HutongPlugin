#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongDoor.h"
#include "Generation/HutongApron.h"
#include "Generation/HutongDoorStone.h"
#include "WallGenerator.generated.h"

UENUM(BlueprintType)
enum class EHutongWallRole : uint8
{
	Perimeter UMETA(DisplayName = "Perimeter Wall (院牆) onto the Lane", ToolTip = "A boundary wall facing the lane: tall, thick and blank."),
	Courtyard UMETA(DisplayName = "Partition Wall (隔牆) inside the Compound", ToolTip = "A dividing wall inside the compound."),
};

UENUM(BlueprintType)
enum class EHutongWallDoorway : uint8
{
	None      UMETA(DisplayName = "None", ToolTip = "No garden doorway."),

	Rect      UMETA(DisplayName = "Plain Square-Headed Doorway (隨牆門)", ToolTip = "A plain square-headed opening."),

	Moon      UMETA(DisplayName = "Moon Gate (月亮門)", ToolTip = "A full circular opening whose width equals its height."),

	Arch      UMETA(DisplayName = "Arched Doorway (拱門)", ToolTip = "Square jambs under a semicircular head."),

	Octagon   UMETA(DisplayName = "Octagonal Doorway (八角門)", ToolTip = "An eight-sided opening."),
	Hexagon   UMETA(DisplayName = "Hexagonal Doorway (六角門)", ToolTip = "A six-sided opening."),
};

UENUM(BlueprintType)
enum class EHutongWindowShape : uint8
{
	Round    UMETA(DisplayName = "Round Window (圓窗)", ToolTip = "A circular window opening."),
	Octagon  UMETA(DisplayName = "Octagonal Window (八角窗)", ToolTip = "An eight-sided window opening."),
	Hexagon  UMETA(DisplayName = "Hexagonal Window (六角窗)", ToolTip = "A six-sided window opening."),
};

USTRUCT(BlueprintType)
struct FHutongWallParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Gate (牆垣門)", ToolTip="Cuts a gate through the wall under its own raised hood."))
	bool bHasGate = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Garden Doorway", ToolTip="Shape of the garden doorway cut through the wall, if any."))
	EHutongWallDoorway Doorway = EHutongWallDoorway::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Openings", meta=(DisplayName="Decorative Windows (什錦窗)", ToolTip="Adds a row of decorative windows (什錦窗) along the wall."))
	bool bHasWindows = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall", meta=(DisplayName="Role", ToolTip="Role of the wall."))
	EHutongWallRole Role = EHutongWallRole::Perimeter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall", meta=(DisplayName="Derive From Role", ToolTip="Takes height, thickness and cap from the role."))
	bool bDeriveFromRole = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall", meta=(EditCondition="!bDeriveFromRole", UIMin="30", UIMax="600", ClampMin="10", Units="cm", ToolTip="Height of the wall body to the underside of the cap, in cm."))
	double Height = HutongCanon::Wall::PerimeterHeightCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall", meta=(EditCondition="!bDeriveFromRole", UIMin="5", UIMax="120", ClampMin="1", Units="cm", ToolTip="Thickness of the wall across the run, in cm."))
	double Thickness = HutongCanon::Wall::PerimeterThicknessCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall", meta=(DisplayName="Base Course Height", UIMin="0", UIMax="300", Units="cm", ToolTip="Height of the base course (下鹼) from the ground, in cm; zero derives it."))
	double BaseCourseHeight = 0.0;

	double GetBaseCourseHeight() const
	{
		if (BaseCourseHeight > 0.0) return BaseCourseHeight;
		const double H = GetHeight();
		return FMath::Min(Role == EHutongWallRole::Courtyard ? H / 3.0 : HutongCanon::BaseCourse::TopCm, 0.6 * H);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall", meta=(DisplayName="Base Course Projection", UIMin="0", UIMax="15", Units="cm", ToolTip="How far the base course stands proud of each wall face, in cm."))
	double BaseCourseProjection = 3.0;

	// How far a run continuing along a neighbour's face holds its own face inside that face:
	// its 下鹼 then stands flush with the house and the body a shadow line behind it, so the
	// two read as two pieces and the cap dies into a step rather than into mid-plane.
	double GetAbuttingSetback() const { return FMath::Max(BaseCourseProjection, 0.0); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cap", meta=(EditCondition="!bDeriveFromRole", UIMin="0", UIMax="40", Units="cm", ToolTip="How far the top cornice course projects past each wall face, in cm."))
	double CapOverhang = 9.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cap", meta=(DisplayName="Cap Corbel Courses", EditCondition="!bDeriveFromRole", UIMin="1", UIMax="4", ClampMin="1", ClampMax="8", ToolTip="Number of brick courses stepping out under the tiled cap."))
	int32 CapCorbelCourses = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cap", meta=(DisplayName="Cap Cornice Height", UIMin="0", UIMax="40", Units="cm", ToolTip="Total height of the corbelled cornice, in cm."))
	double CapSlabHeight = 16.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cap", meta=(UIMin="0", UIMax="40", Units="cm", ToolTip="Rise of the tiled cap above the cornice, in cm; zero gives a flat top."))
	double CapRidgeHeight = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cap", meta=(DisplayName="Cap Ridge Roll", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1", ToolTip="How rounded the cap's crown is, 0 for a peaked cap to 1 for a full roll."))
	double CapRidgeRoll = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Window Details", meta=(DisplayName="Shape", EditCondition="bHasWindows", ToolTip="Outline of each decorative window (什錦窗) opening."))
	EHutongWindowShape WindowShape = EHutongWindowShape::Round;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Window Details", meta=(DisplayName="Opening Size", EditCondition="bHasWindows", UIMin="40", UIMax="120", ClampMin="20", Units="cm", ToolTip="Width and height of each window opening, in cm."))
	double WindowSize = 62.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Window Details", meta=(DisplayName="Centre Height", EditCondition="bHasWindows", UIMin="120", UIMax="220", ClampMin="60", Units="cm", ToolTip="Height of each window's centre above the ground, in cm."))
	double WindowCentreHeight = 165.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Window Details", meta=(DisplayName="Spacing", EditCondition="bHasWindows", UIMin="150", UIMax="600", ClampMin="80", Units="cm", ToolTip="Distance from one window centre to the next along the run, in cm."))
	double WindowSpacing = 330.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Window Details", meta=(DisplayName="Surround Width", EditCondition="bHasWindows", UIMin="0", UIMax="30", ClampMin="0", Units="cm", ToolTip="Width of the moulded surround round each window, in cm; zero omits it."))
	double WindowSurroundWidth = 11.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Window Details", meta=(DisplayName="Surround Projection", EditCondition="bHasWindows", UIMin="0", UIMax="12", ClampMin="0", Units="cm", ToolTip="How far the window surround stands proud of each wall face, in cm."))
	double WindowSurroundProjection = 4.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Window Details", meta=(DisplayName="Lattice Bars (欞條)", EditCondition="bHasWindows", UIMin="0", UIMax="5", ClampMin="0", ClampMax="8", ToolTip="Number of lattice bars across each window opening; zero leaves it open."))
	int32 WindowLatticeBars = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Window Details", meta=(DisplayName="Lattice Bar Thickness", EditCondition="bHasWindows", UIMin="2", UIMax="10", ClampMin="1", Units="cm", ToolTip="Thickness of each lattice bar, in cm."))
	double WindowLatticeThickness = 4.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Window Details", meta=(DisplayName="Outline Steps", EditCondition="bHasWindows", UIMin="4", UIMax="20", ClampMin="2", ClampMax="32", ToolTip="Number of masonry rows used to step round each window's outline."))
	int32 WindowOutlineSteps = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Garden Doorway Details", meta=(EditCondition="Doorway != EHutongWallDoorway::None", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1", ToolTip="Where the doorway sits along the run, 0 to 1."))
	double DoorwayPosition = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Garden Doorway Details", meta=(DisplayName="Doorway Width", EditCondition="Doorway != EHutongWallDoorway::None && Doorway != EHutongWallDoorway::Moon", UIMin="80", UIMax="220", ClampMin="60", Units="cm", ToolTip="Clear width of the garden doorway, in cm."))
	double DoorwayWidth = 120.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Garden Doorway Details", meta=(DisplayName="Doorway Height", EditCondition="Doorway != EHutongWallDoorway::None", UIMin="150", UIMax="260", ClampMin="120", Units="cm", ToolTip="Clear height of the garden doorway above the sill, in cm."))
	double DoorwayHeight = 200.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Garden Doorway Details", meta=(DisplayName="Doorway Sill", EditCondition="Doorway != EHutongWallDoorway::None", UIMin="0", UIMax="30", ClampMin="0", Units="cm", ToolTip="Height of the stone sill the doorway's shape is cut flat at, in cm."))
	double DoorwaySillHeight = 12.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Garden Doorway Details", meta=(DisplayName="Surround Width", EditCondition="Doorway != EHutongWallDoorway::None", UIMin="0", UIMax="35", ClampMin="0", Units="cm", ToolTip="Width of the moulded surround round the doorway, in cm; zero omits it."))
	double DoorwaySurroundWidth = 14.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Garden Doorway Details", meta=(DisplayName="Surround Projection", EditCondition="Doorway != EHutongWallDoorway::None", UIMin="0", UIMax="15", ClampMin="0", Units="cm", ToolTip="How far the doorway surround stands proud of each wall face, in cm."))
	double DoorwaySurroundProjection = 5.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Garden Doorway Details", meta=(DisplayName="Outline Steps", EditCondition="Doorway != EHutongWallDoorway::None", UIMin="6", UIMax="32", ClampMin="3", ClampMax="48", ToolTip="Number of masonry rows used to step round the doorway's outline."))
	int32 DoorwayOutlineSteps = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Garden Doorway Details", meta=(DisplayName="Hanging-Flower Dressing (垂花)", EditCondition="Doorway != EHutongWallDoorway::None", ToolTip="Dresses the doorway with a beam, fretwork band (花板) and hanging posts (垂蓮柱)."))
	bool bDoorwayChuihua = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Garden Doorway Details", meta=(DisplayName="Hanging-Flower Dressing (垂花) Projection", EditCondition="Doorway != EHutongWallDoorway::None && bDoorwayChuihua", UIMin="4", UIMax="20", ClampMin="1", Units="cm", ToolTip="Projection of the dressing's beam and band from each wall face, in cm."))
	double DoorwayDressProjection = 9.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Garden Doorway Details", meta=(DisplayName="Hanging Lotus Post (垂蓮柱) Drop", EditCondition="Doorway != EHutongWallDoorway::None && bDoorwayChuihua", UIMin="20", UIMax="70", ClampMin="10", Units="cm", ToolTip="How far each hanging lotus post (垂蓮柱) drops below the beam, in cm."))
	double DoorwayPostDrop = 38.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Garden Doorway Details", meta=(DisplayName="Hanging Lotus Post (垂蓮柱) Diameter", EditCondition="Doorway != EHutongWallDoorway::None && bDoorwayChuihua", UIMin="8", UIMax="22", ClampMin="4", Units="cm", ToolTip="Diameter of each hanging lotus post (垂蓮柱), in cm."))
	double DoorwayPostDiameter = 13.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Details", meta=(EditCondition="bHasGate", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1", ToolTip="Where the gate sits along the run, 0 to 1."))
	double GatePosition = 0.5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Details", meta=(EditCondition="bHasGate", UIMin="80", UIMax="300", ClampMin="40", Units="cm", ToolTip="Clear width of the gate opening, in cm."))
	double GateWidth = 130.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Details", meta=(EditCondition="bHasGate", UIMin="150", UIMax="300", ClampMin="80", Units="cm", ToolTip="Height of the gate opening's head above the ground, in cm."))
	double GateHeadHeight = 200.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Details", meta=(DisplayName="Gate Roof Rise", EditCondition="bHasGate", UIMin="0", UIMax="150", ClampMin="0", Units="cm", ToolTip="How far the gate's hood rises above the wall cap, in cm."))
	double GateRise = 45.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Details", meta=(DisplayName="Gate Pier Wing", EditCondition="bHasGate", UIMin="0", UIMax="150", ClampMin="0", Units="cm", ToolTip="Extent of the gate pier past the opening on each side, in cm."))
	double GateWingWidth = 40.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Details", meta=(DisplayName="Gate Leaves Open", EditCondition="bHasGate", ToolTip="Builds the gate leaves swung open rather than shut."))
	bool bGateLeavesOpen = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Details", meta=(DisplayName="Gate Leaf Swing", EditCondition="bHasGate && bGateLeavesOpen", UIMin="45", UIMax="95", ClampMin="0", ClampMax="120", Units="deg", ToolTip="How far each gate leaf swings inward from shut, in degrees."))
	double GateLeafAngleDeg = 85.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Details", meta=(DisplayName="Gate Frame Thickness", EditCondition="bHasGate", UIMin="4", UIMax="25", ClampMin="2", Units="cm", ToolTip="Thickness of the gate's jambs and head, in cm."))
	double GateFrameThickness = 10.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Details", meta=(DisplayName="Gate Threshold Height", EditCondition="bHasGate", UIMin="0", UIMax="40", ClampMin="0", Units="cm", ToolTip="Height of the threshold (門檻) across the gate opening, in cm."))
	double GateThresholdHeight = 15.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Details", meta=(DisplayName="Door Pegs (門簪)", EditCondition="bHasGate", UIMin="0", UIMax="4", ClampMin="0", ClampMax="6", ToolTip="Number of door pegs (門簪) through the head above the gate leaves."))
	int32 GatePegCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Details", meta=(DisplayName="Has Door Piers (門垛)", EditCondition="bHasGate", ToolTip="Adds door pier (門垛) pilasters either side of the gate."))
	bool bHasDoorPiers = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Details", meta=(DisplayName="Door Pier Width", EditCondition="bHasGate && bHasDoorPiers", UIMin="15", UIMax="80", ClampMin="5", Units="cm", ToolTip="Width of each door pier along the run, in cm."))
	double GatePierWidth = 34.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Details", meta=(DisplayName="Door Pier Projection", EditCondition="bHasGate && bHasDoorPiers", UIMin="0", UIMax="25", ClampMin="0", Units="cm", ToolTip="How far each door pier stands proud of each wall face, in cm."))
	double GatePierProjection = 7.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Details", meta=(DisplayName="Door Stones (門墩)", EditCondition="bHasGate", ToolTip="The door stones (門墩) at the foot of the gate jambs."))
	FHutongDoorStoneParams DoorStones;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Details", meta=(DisplayName="Door Step Depth", EditCondition="bHasGate", UIMin="0", UIMax="90", ClampMin="0", Units="cm", ToolTip="Depth of the stone step slab in front of the gate, in cm; zero omits it."))
	double GateStepDepth = 48.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Details", meta=(DisplayName="Door Step Height", EditCondition="bHasGate", UIMin="0", UIMax="35", ClampMin="0", Units="cm", ToolTip="Height of the stone step slab in front of the gate, in cm."))
	double GateStepHeight = 13.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Details", meta=(DisplayName="Hood Rafter Section (椽頭)", EditCondition="bHasGate", UIMin="0", UIMax="16", ClampMin="0", Units="cm", ToolTip="Size of the square rafter ends under the gate hood, in cm; zero omits them."))
	double GateRafterSection = 6.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Details", meta=(DisplayName="Hood Rafter Spacing", EditCondition="bHasGate", UIMin="8", UIMax="50", ClampMin="4", Units="cm", ToolTip="Distance between rafter ends under the gate hood, in cm."))
	double GateRafterSpacing = 18.0;

	// A floor under the head the user typed: the opening has to admit the player, same rule as every other doorway here.
	double GetGateHeadHeight() const
	{
		return FMath::Max(GateHeadHeight, HutongGen::Passage::MinHeadZ(0.0, GateThresholdHeight));
	}

	// BuildWall holds the head 10 cm below the wall top with the jambs a frame's thickness above it.
	double GetMinHeight() const
	{
		const double GateNeed = bHasGate
			? GetGateHeadHeight() + FMath::Max(GateFrameThickness, 2.0) + 10.0
			: 0.0;
		// A garden doorway asks the same of the wall it is cut into.
		const double DoorNeed = (Doorway != EHutongWallDoorway::None)
			? GetDoorwayHeight() + FMath::Max(DoorwaySurroundWidth, 0.0)
				+ (bDoorwayChuihua ? 26.0 : 10.0)
			: 0.0;
		return FMath::Max(GateNeed, DoorNeed);
	}

	// The fields as asked, before the walker is fitted: a 月亮門 is a circle, so its width is its
	// height whatever the width field says, and the height keeps the crouch floor every doorway does.
	double RawDoorwayHeight() const
	{
		const double Sill = FMath::Clamp(DoorwaySillHeight, 0.0, 40.0);
		return FMath::Max(DoorwayHeight, Sill + HutongGen::Passage::MinClearHeight);
	}
	double RawDoorwayWidth() const
	{
		return (Doorway == EHutongWallDoorway::Moon) ? RawDoorwayHeight() : FMath::Max(DoorwayWidth, 20.0);
	}

	// The opening is fitted round a standing walker: scaled up, keeping its shape, and its
	// outline continued below the sill (buried) so the cut at the sill is wide enough at the
	// foot. A shaped doorway narrows at its head and at its foot — a 六角門 at its sill was a
	// hand's breadth wide, and no scale mends that since the sill stays at the foot of the
	// shape; carrying the shape on down does. One scale for width and height, so a circle
	// stays a circle and the arch's springing stays where the fields put it. Cached on the
	// fields it reads: the accessors below are asked per course while the wall is built.
	void FitWalker(double& OutScale, double& OutBury) const
	{
		const double W0 = RawDoorwayWidth(), H0 = RawDoorwayHeight();
		const double Sill = FMath::Clamp(DoorwaySillHeight, 0.0, 40.0);
		if (WalkFitShape == (int32)Doorway && WalkFitW == W0 && WalkFitH == H0 && WalkFitSill == Sill)
		{
			OutScale = WalkFitScale;
			OutBury = WalkFitBury;
			return;
		}
		const double Rad = HutongCanon::Openings::WalkerRadiusCm;
		const double WalkH = HutongCanon::Openings::WalkerHeightCm;
		auto Fits = [&](double K, double Bury)
		{
			const double Head = K * H0;
			const double Span = Head + Bury;
			const double W = (Doorway == EHutongWallDoorway::Moon) ? Span : K * W0;
			for (double Y = 0.0; Y <= WalkH; Y += 5.0)
			{
				const double Z = Sill + Y;
				if (Z >= Head) return false;
				// The capsule's own half-width at this height: rounded at both ends, full between.
				double Need = Rad;
				if (Y < Rad) Need = FMath::Sqrt(FMath::Max(Rad * Rad - (Rad - Y) * (Rad - Y), 0.0));
				else if (Y > WalkH - Rad) Need = FMath::Sqrt(FMath::Max(Rad * Rad - (Y - (WalkH - Rad)) * (Y - (WalkH - Rad)), 0.0));
				const double t = 2.0 * (Z + Bury) / Span - 1.0;
				if (ShapeHalfWidthFraction(Doorway, t, W / Span) * 0.5 * W < Need) return false;
			}
			return true;
		};
		// The least burying that fits at a scale, or the span's half when none does.
		auto BuryFor = [&](double K)
		{
			if (Fits(K, 0.0)) return 0.0;
			double Lo = 0.0, Hi = 0.5 * K * H0;
			if (!Fits(K, Hi)) return -1.0;
			for (int32 i = 0; i < 12; ++i)
			{
				const double Mid = 0.5 * (Lo + Hi);
				if (Fits(K, Mid)) Hi = Mid; else Lo = Mid;
			}
			return Hi;
		};
		double Scale = 1.0, Bury = BuryFor(1.0);
		if (Bury < 0.0)
		{
			double Lo = 1.0, Hi = 4.0;
			if (BuryFor(Hi) < 0.0) { Scale = Hi; Bury = 0.0; }
			else
			{
				for (int32 i = 0; i < 12; ++i)
				{
					const double Mid = 0.5 * (Lo + Hi);
					if (BuryFor(Mid) >= 0.0) Hi = Mid; else Lo = Mid;
				}
				Scale = Hi;
				Bury = FMath::Max(BuryFor(Hi), 0.0);
			}
		}
		WalkFitShape = (int32)Doorway; WalkFitW = W0; WalkFitH = H0; WalkFitSill = Sill;
		WalkFitScale = Scale; WalkFitBury = Bury;
		OutScale = Scale;
		OutBury = Bury;
	}
	mutable int32 WalkFitShape = -1;
	mutable double WalkFitW = -1.0, WalkFitH = -1.0, WalkFitSill = -1.0, WalkFitScale = 1.0, WalkFitBury = 0.0;

	// How far below the ground the outline carries on before the sill cuts it, in cm.
	double GetDoorwayBury() const { double K, B; FitWalker(K, B); return B; }
	// The head of the opening above the ground, in cm.
	double GetDoorwayHeight() const { double K, B; FitWalker(K, B); return K * RawDoorwayHeight(); }
	// The whole outline's height, buried part included: a 月亮門's diameter.
	double GetDoorwaySpan() const { return GetDoorwayHeight() + GetDoorwayBury(); }
	double GetDoorwayWidth() const
	{
		double K, B; FitWalker(K, B);
		return (Doorway == EHutongWallDoorway::Moon) ? GetDoorwaySpan() : K * RawDoorwayWidth();
	}

	// Half-width at height t, normalized to [-1, 1] over the opening.
	double GetDoorwayHalfWidthFraction(double t) const
	{
		return ShapeHalfWidthFraction(Doorway, t, GetDoorwayWidth() / FMath::Max(GetDoorwaySpan(), 1.0));
	}

	// The outline of each shape, as a half-width fraction of its full width at height t in
	// [-1, 1]; the arch alone needs the opening's width-over-height, which a uniform scale keeps.
	static double ShapeHalfWidthFraction(EHutongWallDoorway Shape, double t, double WidthOverHeight)
	{
		const double a = FMath::Abs(FMath::Clamp(t, -1.0, 1.0));
		switch (Shape)
		{
		case EHutongWallDoorway::Rect:
			// Square-headed, so the outline is the same width all the way up and every course round it is one box.
			return 1.0;

		case EHutongWallDoorway::Arch:
		{
			// Square jambs to the springing, then a semicircular head of the opening's own half-width.
			const double Head = FMath::Clamp(WidthOverHeight, 0.05, 1.0);
			const double Spring = 1.0 - Head;
			if (t <= Spring) return 1.0;
			const double u = FMath::Clamp((t - Spring) / Head, 0.0, 1.0);
			return FMath::Sqrt(FMath::Max(1.0 - u * u, 0.0));
		}
		case EHutongWallDoorway::Hexagon:
		{
			const double h = 0.5;
			return (a <= h) ? 1.0 : (1.0 - a) / (1.0 - h);
		}
		case EHutongWallDoorway::Octagon:
		{
			const double s = UE_SQRT_2 - 1.0;
			return (a <= s) ? 1.0 : FMath::Max(1.0 - (a - s), 0.0);
		}
		default:   // 月亮門
			return FMath::Sqrt(FMath::Max(1.0 - a * a, 0.0));
		}
	}

	// Masonry the wall generator keeps beside the surround, both sides together, before it cuts a doorway.
	static constexpr double DoorwayMasonryCm = 60.0;

	// Where the doorway's centre lands along a run: DoorwayPosition held in from each end by the
	// opening's own half-width and surround, the one clamp the generator and every preview share.
	double GetDoorwayCentre(double RunLength) const
	{
		const double Margin = 0.5 * GetDoorwayWidth() + FMath::Max(DoorwaySurroundWidth, 0.0);
		return FMath::Clamp(DoorwayPosition * RunLength, Margin, RunLength - Margin);
	}

	// The shortest run the doorway as fitted passes HasDecorativeDoorway on.
	double GetDoorwayRunNeed() const
	{
		return GetDoorwayWidth() + 2.0 * FMath::Max(DoorwaySurroundWidth, 0.0) + DoorwayMasonryCm + 1.0;
	}

	// Whether this run carries one.
	bool HasDecorativeDoorway(double RunLength) const
	{
		if (Doorway == EHutongWallDoorway::None) return false;
		if (bDeriveFromRole && Role != EHutongWallRole::Courtyard) return false;

		// It has to fit the run with masonry left either side, and stand clear of the wall's top.
		const double Outer = GetDoorwayWidth() + 2.0 * FMath::Max(DoorwaySurroundWidth, 0.0);
		return Outer + DoorwayMasonryCm < RunLength
			&& GetDoorwayHeight() + FMath::Max(DoorwaySurroundWidth, 0.0) + 10.0 <= GetHeight();
	}

	// The band the 垂花 dressing has to live in: from the top of the surround to the underside of the cap.
	double GetDoorwayDressBand() const
	{
		const double Top = GetDoorwayHeight() + FMath::Max(DoorwaySurroundWidth, 0.0);
		const double Band = GetHeight() - Top;
		return (Band >= 22.0) ? Band : 0.0;
	}

	bool HasDoorwayChuihua(double RunLength) const
	{
		return bDoorwayChuihua && HasDecorativeDoorway(RunLength) && GetDoorwayDressBand() > 0.0;
	}

	// What the role asks for.
	struct FRoleDefaults
	{
		double Height = HutongCanon::Wall::PerimeterHeightCm;
		double Thickness = HutongCanon::Wall::PerimeterThicknessCm;
		int32 CapCorbelCourses = HutongCanon::Wall::PerimeterCapCourses;
		double CapOverhang = HutongCanon::Wall::PerimeterCapOverhangCm;
	};

	FRoleDefaults GetRoleDefaults() const
	{
		FRoleDefaults R;
		if (Role == EHutongWallRole::Courtyard)
		{
			R.Height = HutongCanon::Wall::CourtyardHeightCm;
			R.Thickness = HutongCanon::Wall::CourtyardThicknessCm;
			R.CapCorbelCourses = HutongCanon::Wall::CourtyardCapCourses;
			R.CapOverhang = HutongCanon::Wall::CourtyardCapOverhangCm;
		}
		return R;
	}

	double GetHeight() const
	{
		const double Asked = bDeriveFromRole ? GetRoleDefaults().Height : Height;
		return FMath::Max(Asked, GetMinHeight());
	}

	// Holds the body at a height another part sets, keeping everything else the role gave it.
	void PinHeight(double H)
	{
		if (bDeriveFromRole)
		{
			const FRoleDefaults R = GetRoleDefaults();
			Thickness = R.Thickness;
			CapCorbelCourses = R.CapCorbelCourses;
			CapOverhang = R.CapOverhang;
			if (Role != EHutongWallRole::Courtyard) { Doorway = EHutongWallDoorway::None; bHasWindows = false; }
			bDeriveFromRole = false;
		}
		Height = FMath::Max(H, 10.0);
	}

	// Capped by the footprint when a caller has one.
	double GetThickness() const
	{
		const double Asked = FMath::Max(bDeriveFromRole ? GetRoleDefaults().Thickness : Thickness, 1.0);
		return (FootprintThickness > 0.0) ? FMath::Min(Asked, FootprintThickness) : Asked;
	}

	// Zero on anything facing the lane: a 院牆 exists so the household is not seen, and an opening in one defeats it.
	int32 GetWindowCount(double RunLength) const
	{
		if (!bHasWindows) return 0;
		if (bDeriveFromRole && Role != EHutongWallRole::Courtyard) return 0;

		const double Pitch = FMath::Max(WindowSpacing, 80.0);
		// Half a pitch of masonry at each end.
		return FMath::Clamp(FMath::FloorToInt32(RunLength / Pitch), 0, 64);
	}

	// Half-width at height t, normalized to [-1, 1].
	double GetWindowHalfWidthFraction(double t) const
	{
		const double a = FMath::Abs(FMath::Clamp(t, -1.0, 1.0));
		switch (WindowShape)
		{
		case EHutongWindowShape::Hexagon:
		{
			// Flat sides to half height, then straight to a point top and bottom.
			const double h = 0.5;
			return (a <= h) ? 1.0 : (1.0 - a) / (1.0 - h);
		}
		case EHutongWindowShape::Octagon:
		{
			// Corners cut at 45°, which puts the break at sqrt(2) - 1 of the half height.
			const double s = UE_SQRT_2 - 1.0;
			return (a <= s) ? 1.0 : FMath::Max(1.0 - (a - s), 0.0);
		}
		default:
			return FMath::Sqrt(FMath::Max(1.0 - a * a, 0.0));
		}
	}

	int32 GetCapCorbelCourses() const
	{
		return bDeriveFromRole ? GetRoleDefaults().CapCorbelCourses : CapCorbelCourses;
	}

	double GetCapOverhang() const
	{
		return bDeriveFromRole ? GetRoleDefaults().CapOverhang : CapOverhang;
	}

	// Set by the tool from the drag rect; not user-editable.
	double Length = 500.0;

	// The footprint's own cross extent, likewise tool-driven. Zero means no footprint to answer to.
	double FootprintThickness = 0.0;

	// 轉角: how far each end runs past the drawn rect, filling the corner between two runs.
	double StartExtend = 0.0;
	double EndExtend = 0.0;

};

namespace HutongGen
{
	void BuildWall(UE::Geometry::FDynamicMesh3& Mesh, const FHutongWallParams& P);
}
