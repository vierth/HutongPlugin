#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongRearEave.h"

// Every figure the plugin's proportions rest on, in one file, so what the buildings claim can be
// checked against the sources without reading the generators.
//
// Each entry is marked:
//   CANON     — a source states it. The source is named on the line above.
//   DERIVED   — arithmetic over other entries here; not independently chosen.
//   JUDGEMENT — proportioned to look right. No source states it, and it is the first thing to
//               argue with when a building looks wrong.
//
// Sources, cited in short form below:
//   則例    《工程做法則例》, 1734 (Qing, 工部).
//   營造    《清式營造則例》, 梁思成, 1934.
//   元大都  the Yuan grid as surveyed and as 《乾隆京城全圖》 (1745-50) still shows it.
//
// These are defaults, not laws: every one of them reaches the panel through a params struct the
// user can edit. What lives here is the value that ships, and where it came from.
namespace HutongCanon
{
	// 小式 module: 柱徑 is the unit and everything on a dwelling is a multiple of it.
	namespace Module
	{
		// CANON 則例 — 小式 columns run 11 柱徑 tall.
		inline constexpr double ColumnHeightInDiameters = 11.0;

		// CANON 則例 — 臺明高 is 2 柱徑, measured off the same eave as the column.
		inline constexpr double PlatformHeightInDiameters = 2.0;

		// CANON 營造 — 上檐出 is about three tenths of 柱高 on the courtyard side.
		inline constexpr double EaveOverhangRatio = 0.30;

		// CANON 則例 — 封護檐: a back to the lane projects nothing at all.
		inline constexpr double LaneEaveOverhangRatio = 0.0;

		// CANON 營造 — 檐柱高 = 8/10 明間面闊, the rule tying the two halves of a facade together.
		inline constexpr double ColumnHeightPerCentralBay = 0.8;

		// JUDGEMENT — the 8/10 rule gives a 耳房 a 1.99 m column, and small vernacular buildings are
		// proportionally taller than the system says. 2.35 m is habitable room height, not a figure
		// from 則例; it is a floor on 柱高 rather than on the doorway, so the opening stack above it
		// is undistorted.
		inline constexpr double MinColumnHeightCm = 235.0;

		// CANON 營造 — 明間 is the widest bay; each 次間 is a fixed fraction of it.
		inline constexpr double SideBayWidthRatio = 0.85;

		// CANON 則例 — 收分: a column narrows toward its head by a hundredth of 柱高.
		inline constexpr double ColumnTaperRatio = 0.01;

		// CANON 營造 — 柱徑 runs between a tenth and an eleventh of 柱高. The derived path takes the
		// eleventh through ColumnHeightInDiameters above; this is the absolute fallback, which
		// ships at the tenth.
		inline constexpr double ColumnDiameterRatio = 0.1;
	}

	// 舉架 and 檁數: the roof section, which is a polyline and whose depth is not free.
	namespace Roof
	{
		// CANON 則例 — the 舉 sequence, eave first: 檐步五舉, then steepening to the ridge. Each
		// entry is the rise of one 步架 as a fraction of that 步架's own run.
		inline constexpr double JuThree[] = { 0.5 };
		inline constexpr double JuFive[]  = { 0.5, 0.7 };
		inline constexpr double JuSeven[] = { 0.5, 0.7, 0.9 };
		inline constexpr double JuNine[]  = { 0.5, 0.65, 0.75, 0.9 };

		// CANON 則例 — 進深 = (檁數 - 1) × 步架. The purlin count is the decision; the depth follows.
		// 檁數 by type: 三檁 游廊/影壁/垂花門, 五檁 houses and shops, 七檁 a 正房 with 前廊, 九檁 a hall.

		// 收山: how far in from each end eave the 山花 stands, which is the one number that makes a
		// roof 歇山 rather than 廡殿.
		// CANON 營造 — 清式 sets the gable face one 檩徑 inside the 山面檐檩 centre.
		// JUDGEMENT — what that comes to in centimetres, proportioned to each type's own scale:
		// a 亭's roof is small and a 殿's is not.
		inline constexpr double PavilionShouInsetCm = 88.0;
		inline constexpr double HallShouInsetCm = 150.0;
	}

	// The 檻框 stack. 則例 gives the members' sections in 柱徑 and where the body wants the sill;
	// the heights fall out of those rather than being fractions of 柱高.
	namespace Openings
	{
		// CANON 則例 — 額枋高 is one 柱徑, so on an 11-柱徑 column its underside sits at 0.909 柱高.
		inline constexpr double ArchitraveInDiameters = 1.0;

		// CANON 營造 — 檻牆 stands two and a half to three 營造尺 above the floor. Absolute, not a
		// ratio: a sill is set by the body leaning on it, so a 耳房 and a 正房 share one.
		inline constexpr double SillHeightAboveFloorCm = 85.0;

		// JUDGEMENT — what the 橫披窗 takes of the opening below the 額枋. The 中檻 is one member
		// across the bay, so this sets the window head and the leaf head together.
		inline constexpr double TransomBandFraction = 0.12;

		// JUDGEMENT — a crouch floor (an 88 cm crouched capsule with room over it), not a standing
		// guarantee: real 耳房 are low, and a standing clamp flattens the hierarchy between the
		// small types. The mesh is its own collision, so a doorway under this is a wall with a
		// picture of a door on it.
		inline constexpr double MinClearHeightCm = 110.0;

		// JUDGEMENT — what a garden doorway in a wall has to pass, since walking through it is
		// the whole point of one: a standing capsule with a little room round it (the mannequin
		// is 34 by 176). A shaped opening — 月亮門, 八角門 — narrows at its head and its sill, so
		// the shape as a whole is scaled up until this fits inside it.
		inline constexpr double WalkerRadiusCm = 40.0;
		inline constexpr double WalkerHeightCm = 190.0;

		// 高窗: the row of small windows under the eave of a 封護檐 back wall. The height is what
		// makes them allowable — the head is measured down from the eave, so the sill stands above
		// head height on a 279 cm 倒座房 as well as on a 364 cm 正房, and the lane sees nothing.
		// JUDGEMENT — the three figures; the argument for them is the sill height they produce.
		inline constexpr double RearWindowWidthCm = 70.0;
		inline constexpr double RearWindowHeightCm = 45.0;
		inline constexpr double RearWindowHeadBelowEaveCm = 45.0;

		// DERIVED — a leaf equals its flanking panel at half the bay, which is the width a two-leaf
		// door folds flat at; the leaf must also clear the jamb and the column radius, so the
		// shipped fraction sits just under a half.
		inline constexpr double DoorWidthFraction = 0.44;
	}

	// 院牆 faces the lane and 隔牆 divides one household's own courts. The ordering is solid; the
	// centimetres are proportioned so the pieces clear each other.
	namespace BaseCourse
	{
		// JUDGEMENT — where the 下鹼 tops out above the ground, one line for every piece of a
		// frontage: the 倒座房's rear wall, the 大門 and the walls between were built together and
		// their band runs through. 營造算例 gives a house's 下鹼 as 3/10 of its 檐柱高 above the 臺基,
		// which for the shipped presets lands near this; a wall has no rule of its own. Each type
		// subtracts its own floor or plinth to get the band's height on its wall.
		inline constexpr double TopCm = 110.0;
	}

	namespace Wall
	{
		// JUDGEMENT — 300 to the underside of the cap is about 一丈 overall, level with the wings'
		// eaves, and the 正房's roof rises clear of it.
		inline constexpr double PerimeterHeightCm = 300.0;

		// CANON 營造 — 一磚半 laid in 青磚.
		inline constexpr double PerimeterThicknessCm = 37.0;

		// JUDGEMENT — corbelled 磚檐 courses under the cap.
		inline constexpr int32 PerimeterCapCourses = 3;
		inline constexpr double PerimeterCapOverhangCm = 9.0;

		// JUDGEMENT — the cross wall stays under the 垂花門's 310 eave so the gate steps clear of
		// the run it stands in.
		inline constexpr double CourtyardHeightCm = 240.0;

		// CANON 營造 — 一磚.
		inline constexpr double CourtyardThicknessCm = 24.0;

		// JUDGEMENT — one course fewer than the perimeter's, in proportion to the lower wall.
		inline constexpr int32 CourtyardCapCourses = 2;
		inline constexpr double CourtyardCapOverhangCm = 6.0;

		// JUDGEMENT — a 墀頭 wraps the corner 檐柱 standing on the wall plane, so it projects at
		// least the column's base radius plus this, or the column's foot bulges through its face.
		inline constexpr double ChitouColumnClearanceCm = 1.0;
	}

	// The four ordinary courtyard gates. All are one bay of a small 硬山 building; what separates
	// them is how far the door plane stands behind the 檐柱 line, and the rank that goes with it.
	namespace Gate
	{
		// A gate's plan band: 面闊, 進深 and eave, in cm. Both gate params structs use this as their
		// own FSizeRange rather than declaring the same six doubles again; all zero is the
		// unconstrained state the tools already test for.
		struct FSizeBand
		{
			double FrontageMin = 0.0, FrontageMax = 0.0;
			double DepthMin = 0.0, DepthMax = 0.0;
			double EaveMin = 0.0, EaveMax = 0.0;
		};

		// CANON 營造 — the door plane of a 廣亮大門 stands on the 中柱, half the depth back.
		inline constexpr double GuangliangDoorPlane = 0.5;

		// CANON 營造 — a 金柱大門 hangs its doors on the 前金柱, so the recess is shallower.
		// JUDGEMENT — that column's position as a fraction of the depth.
		inline constexpr double JinzhuDoorPlane = 0.3;

		// CANON 營造 — 蠻子門 and 如意門 hang their doors on the 檐柱 line, flush with the facade.
		inline constexpr double FlushDoorPlane = 0.0;

		// CANON 營造 — the rank order: 廣亮 and 金柱 are 五檁 and carry 筒瓦; 如意門 is three purlins
		// and a doorway in a brick screen. The sumptuary rule behind the tile is that 筒瓦 on a
		// commoner's house is 逾制.
		// JUDGEMENT — the bands themselves. The ordering is the claim; the centimetres are a reading.
		inline constexpr FSizeBand GuangliangSize = { 330.0, 420.0, 400.0, 520.0, 320.0, 400.0 };
		inline constexpr FSizeBand JinzhuSize     = { 320.0, 400.0, 350.0, 460.0, 300.0, 375.0 };
		inline constexpr FSizeBand ManziSize      = { 300.0, 380.0, 300.0, 400.0, 285.0, 350.0 };
		inline constexpr FSizeBand RuyiSize       = { 260.0, 350.0, 250.0, 350.0, 265.0, 330.0 };

		// 垂花門, the inner gate, sized the same way and as unverified. Its depth is the 擔梁's full
		// run with the columns halfway along it, so the form is one 步架 deep and the eaves project
		// further than the frame — which is what a 垂花門 looks like.
		inline constexpr FSizeBand InnerGateSize  = { 280.0, 400.0, 105.0, 215.0, 265.0, 365.0 };

		// JUDGEMENT — how much of a house's 上檐出 each rank's gate carries over the street. A 廣亮大門
		// is a columned bay and takes a full eave; a 如意門 is a doorway in a brick screen and takes
		// little. The ordering is the claim; the fractions are a reading.
		inline constexpr double GuangliangEaveShare = 1.0;
		inline constexpr double JinzhuEaveShare = 0.95;
		inline constexpr double ManziEaveShare = 0.8;
		inline constexpr double RuyiEaveShare = 0.5;

		// JUDGEMENT — a 大門 set into a street row is the tallest thing on that face: its ridge
		// stands this much above the row's. One rule for the compound, the street row and a gate
		// placed by hand against a traced 倒座房, whose own band was written for a gate standing alone.
		inline constexpr double RidgeAboveRowCm = 35.0;
	}

	// 門墩 / 門枕石 at the foot of each gate jamb. The form is rank: 抱鼓石 belongs to the two gates
	// that announce an official, and within a rank the choice is real (文官 block, 武官 drum).
	namespace Stone
	{
		// JUDGEMENT — a 方門墩 is knee height, in the 50-60 cm band the photographs show.
		inline constexpr double BlockHeightCm = 54.0;

		// JUDGEMENT — a 抱鼓石 with its plinth stands 90-110; a gate's own is a little under that.
		inline constexpr double DrumHeightCm = 82.0;

		// JUDGEMENT — the disc against the whole stone, so the drum overhangs its plinth the way a
		// real one does rather than reading as a knob.
		inline constexpr double DrumFraction = 0.62;

		// JUDGEMENT — the stone stands in front of the doorway, roughly as deep as it is tall.
		inline constexpr double ProjectionCm = 32.0;

		// JUDGEMENT — small and close in at an inner gate, where the stone is a bearing for the
		// 門軸 rather than an announcement. A 垂花門's 板門 turn on pivots and need one whether or
		// not anybody is meant to look at it.
		inline constexpr double InnerGateHeightCm = 40.0;
		inline constexpr double InnerGateProjectionCm = 20.0;
	}

	// The 元大都 grid, in 步 so the numbers stay checkable.
	namespace Urban
	{
		// CANON 元大都 — 1 步 = 5 尺 at a Yuan 尺 of ~31.6 cm.
		inline constexpr double PaceCm = 158.0;

		// CANON 元大都 — the street classes, in 步.
		inline constexpr double HutongPaces = 6.0;       // 胡同, ~9.5 m
		inline constexpr double MinorStreetPaces = 12.0; // 小街, ~19 m
		inline constexpr double MajorStreetPaces = 24.0; // 大街, ~38 m

		// CANON 元大都 — lane centre to lane centre, which leaves the ~70 m plot band between them.
		inline constexpr double LanePitchPaces = 50.0;

		// JUDGEMENT — how far off a nominal width a run may be and still read as a good example of
		// its class. The Outer City was never regularised and the Inner City has 500 years of
		// encroachment, so off-band is a note rather than an error.
		inline constexpr double InBandToleranceCm = 120.0;
	}

	// The five house types, which are one 硬山 hall at five footprints. The eave each builds at is
	// 檐柱高 = 8/10 明間面闊 off the frontage; the eave figure below is the fallback for when the
	// derivation is turned off, and is what the preset was measured at.
	namespace House
	{
		struct FHouse
		{
			double EaveCm;
			double MinBayCm;
			double MaxBayCm;
			bool bVeranda;
			EHutongRearEave RearEave;
			double FrontageCm;
			EHutongPurlins Purlins;
			double StepRunCm;
			bool bRearHighWindows;
			bool bRearVeranda = false;
			double SideBayRatio = Module::SideBayWidthRatio;
			double ColumnPerCentralBay = Module::ColumnHeightPerCentralBay;
		};

		// JUDGEMENT unless marked — these are the sizes the types are ordinarily built at, read off
		// the plans rather than stated anywhere. What is CANON is the shape of the table: the hall
		// is the tallest and the only one with a 前廊, the rows facing the lane carry 高窗, and
		// 進深 is never given because it is (檁數 - 1) × 步架.

		// CANON 四合院建築及其構造 p.84 — 正房: 七檁前後廊, four rows of columns (前檐柱, 前檐金柱,
		// 後檐金柱, 後檐柱), 門窗 on the 前金柱 line and the 後檐牆 on the 後檐柱 line, so the 後廊
		// is enclosed in the room. 進深 7 m and more with the 廊, 明間 3.9-4.2 m, 次間 about 3.3 m,
		// 檐柱 3.3-3.5 m: a large or medium compound's hall.
		// DERIVED — 1060 frontage at 0.82 gives 明間 402 and 次間 329; 6 步架 of 117 give 702; 0.84 of
		// the 明間 gives a 337 column. 8/10 would give 321, under the source's range at any 明間 in it.
		inline constexpr FHouse MainHall =
			{ 399.0, 320.0, 420.0, true,  EHutongRearEave::Lane, 1060.0, EHutongPurlins::Seven, 117.0, true,
			  true, 0.82, 0.84 };

		// 五間正房: the same 七檁前後廊 hall two 次間 wider, for a large compound's main court.
		// JUDGEMENT — frontage = 明間 + four 次間 at MainHall's own widths (402 + 4 × 330), so every
		// bay, the 檐柱 and the section match the three-bay hall and only the count changes.
		inline constexpr FHouse MainHallFiveBay =
			{ 399.0, 320.0, 420.0, true,  EHutongRearEave::Lane, 1720.0, EHutongPurlins::Seven, 117.0, true,
			  true, 0.82, 0.84 };

		// CANON same work, p.85 and 圖5-3-2 — 前廊後無廊: a smaller court's 正房 drops the 後檐柱. Framed
		// with a 鑽金柱 so the ridge stays over the middle: 五檁, four 步架 across the whole depth with
		// the 廊 as the first of them, both eaves level, the roof visibly lower than the 七檁 hall's.
		// JUDGEMENT — 150 步架 and 600 進深 scaled off the figure, whose two sections are near one depth;
		// 1040 frontage because the page says 面寬要酌減 and gives no figure.
		inline constexpr FHouse MainHallSmall =
			{ 365.0, 300.0, 380.0, true,  EHutongRearEave::Lane, 1040.0, EHutongPurlins::Five, 150.0, true };

		// 廂房: the side houses down the east and west of the courtyard.
		inline constexpr FHouse SideHouse =
			{ 340.0, 280.0, 340.0, false, EHutongRearEave::Lane,  900.0, EHutongPurlins::Five, 112.0, false };

		// 倒座房: the row along the street, low and long, its back the lane wall.
		inline constexpr FHouse FrontRow =
			{ 322.0, 250.0, 300.0, false, EHutongRearEave::Lane, 1300.0, EHutongPurlins::Five,  90.0, true };

		// 後罩房: the row closing the back of a 三進 plot.
		inline constexpr FHouse RearRow =
			{ 328.0, 260.0, 320.0, false, EHutongRearEave::Courtyard, 1300.0, EHutongPurlins::Five, 100.0, true };

		// 耳房: the low rooms tucked against the hall's flanks, and the type the 柱高 floor bites on.
		// CANON p.83 — 耳房面寬 3.0 m in a 大型 court, two rooms to a flank (三正四耳); 2.4 m and one
		// room in a 小型 one, which the compound's own size table carries.
		inline constexpr FHouse EarRoom =
			{ 316.0, 240.0, 320.0, false, EHutongRearEave::Lane,  600.0, EHutongPurlins::Five,  85.0, false };
	}

	// 構架: the members of a house's timber frame, in 柱徑 (D) of its 檐柱.
	// JUDGEMENT throughout — the usual 小式 sections as the secondary literature tabulates them from
	// 則例, not checked against its text. The first place to look when a member reads too heavy.
	namespace Frame
	{
		inline constexpr double GoldColumnExtraCm = 3.2;      // 金柱徑 = 檐柱徑 + 1 寸
		inline constexpr double PurlinDiameter = 0.9;          // 檁徑
		inline constexpr double BoardHeight = 0.65;            // 墊板
		inline constexpr double BoardThickness = 0.25;
		inline constexpr double TieHeight = 1.0;               // 檐枋, 金枋, 脊枋, 穿插枋, 隨梁枋
		inline constexpr double TieThickness = 0.68;
		inline constexpr double HeadBeamHeight = 1.4;          // 抱頭梁, 插梁
		inline constexpr double HeadBeamWidth = 1.1;
		inline constexpr double LongBeamHeight = 1.5;          // 五架梁, 七架梁
		inline constexpr double LongBeamWidth = 1.25;
		inline constexpr double ShortBeamHeight = 1.25;        // 三架梁
		inline constexpr double ShortBeamWidth = 1.1;
		// 瓜柱: the stoutest thing at its joint — wider than the 檁 it carries (0.9) and the 枋 that
		// tenons into it (0.68), narrower than the beam it stands on (1.1), so no two of their faces
		// are coplanar. Its head bevels in to seat the 檁, and runs up into it rather than stopping
		// at its underside.
		inline constexpr double StrutSection = 1.0;
		inline constexpr double StrutHeadWidth = 0.6;          // of the section, where it meets the 檁
		inline constexpr double StrutHeadBevel = 0.35;         // of the section, the bevel's own height
		inline constexpr double StrutIntoPurlin = 0.33;        // of 檁徑, how far the head rises into it

		// 角背: the brace the 脊瓜柱 stands in. 圖5-3-1 draws it flat on top with an angled edge down
		// to the beam, reaching about half a 步架 either side of the post and a little thinner than
		// the 三架梁 it sits on.
		inline constexpr double BraceWidth = 0.85;             // of the 三架梁's own width
		inline constexpr double BraceReach = 0.5;              // 步架, each side of the post
		inline constexpr double BraceCut = 0.12;               // 步架, the corner cut's own run
		// Low enough that the post it braces is read all the way down to the beam, not a head rising
		// out of a lump: 圖5-3-1's 角背 covers the post's foot, no more.
		inline constexpr double BraceHeight = 0.38;            // of the post's exposed height
		inline constexpr double BraceStraight = 0.45;          // of its height, standing before the cut

		// 出頭: what a 穿插枋 shows where it passes through a column. 圖5-3-1 draws a smaller square
		// than the member itself, sitting on its underside — the tenon, not the whole timber.
		inline constexpr double TenonHeight = 0.5;             // of the member's own height
		inline constexpr double TenonWidth = 0.75;             // of its thickness
		inline constexpr double TenonReach = 0.9;              // 柱徑, past the column's axis
		// How far every member running along the frontage passes the last column it crosses. 圖5-3-1
		// carries 檐枋, 墊板 and 檁 — and the 金 and 脊 sets above them — out to one line past the
		// gable frame, so the nine of them end together; ending on the column's axis reads as a
		// timber buried in it.
		inline constexpr double RunProjection = 0.8;           // 檁, 墊板 and 枋 alike
		inline constexpr double RafterDiameter = 1.0 / 3.0;    // 椽徑, laid one 椽徑 apart
		inline constexpr double FlyingShareOfEave = 1.0 / 3.0; // 飛椽出 of 上檐出
		inline constexpr double FlyingTailRatio = 2.5;         // 飛椽 tail against its head
		inline constexpr double PlatformReachOfEave = 0.8;     // 臺明 下出 inside 上出, the 回水
		inline constexpr double PlatformSide = 2.0;            // 臺明 past the gable columns
		inline constexpr double EdgeStoneWidth = 1.3;          // 階條
		inline constexpr double BaseStoneSide = 2.0;           // 柱頂石
		inline constexpr double StringerWidth = 1.3;           // 垂帶
	}

	// 院落尺度: 北京四合院有小型、中型、大型之分. The plot's width decides the buildings in it —
	// 院落寬大, 房子也隨之高大 — so a size is a row of figures, not a scale factor.
	namespace Courtyard
	{
		struct FSize
		{
			double PlotWidthCm;
			double HallCentralBayCm;   // 正房明間面寬
			double HallSideBayCm;      // 次間面寬
			double EarRoomBayCm;       // 耳房面寬, per room
			int32 EarRoomsPerFlank;    // 三正兩耳 or 三正四耳
			double WingStepRunCm;      // 廂房's 步架; its 進深 is (檁數 - 1) × this, plus a 廊 if it has one
			bool bWingVeranda;         // 廂房's 外廊: the 大型 court's 5.5 m 進深 includes one, the 小型's does not
			bool bHallRearVeranda;     // 七檁前後廊, against 前廊後無廊 in a small court
		};

		// CANON 四合院建築及其構造 p.83 — 小型: 占地寬 16 m, 三正兩耳共五間, 明間 3.3, 次間 3.0,
		// 耳房 2.4, 廂房進深 3.5-4 m 無外廊, 院當寬度 7-8 m.
		inline constexpr FSize Small = { 1600.0, 330.0, 300.0, 240.0, 1, 100.0, false, false };

		// JUDGEMENT — 中型 between the two the page gives figures for, which names only 小型 and 大型.
		inline constexpr FSize Medium = { 2000.0, 360.0, 315.0, 300.0, 1, 110.0, false, false };

		// CANON same page — 大型: 占地寬 25 m, 三正四耳共七間, 明間 3.9-4.2, 次間 3.3, 耳房 3.0,
		// 廂房進深 5.5 m 含外廊, 院當寬度 13 m 左右. The house table's 正房 is this hall.
		inline constexpr FSize Large = { 2500.0, 405.0, 330.0, 300.0, 2, 110.0, true, true };
	}

	// 影壁, the screen facing the gate.
	namespace Screen
	{
		// JUDGEMENT — how far the 須彌座 stands proud of the screen's body on each face. The compound
		// reads this to push a 座山影壁's footprint into the wall it backs onto, so the body lands on
		// the masonry rather than standing a plinth's width off it; a second copy reopens that slot.
		inline constexpr double PlinthProjectionCm = 11.0;
	}

	// The compound's plan. Everything here is a placement figure: the buildings are the ordinary
	// generators at the sizes above, and this is the ground they stand on.
	namespace Compound
	{
		// The frontages are the house table's own: the compound sizes a slot and then builds the
		// type that stands in it, and the two must be one number or the plot and the building
		// silently disagree.
		inline constexpr double HallFrontageCm = House::MainHall.FrontageCm;
		inline constexpr double MinEarRoomFrontageCm = 230.0;

		inline constexpr double WingFrontageCm = House::SideHouse.FrontageCm;
		inline constexpr double MaxWingFrontageCm = 1500.0;
		inline constexpr double WingEarRoomFrontageCm = House::EarRoom.FrontageCm;

		// JUDGEMENT — how far below the 正房's eave an 耳房 is held. Its eave derives from its own bay
		// width like everything else, and a wide slot pushed one to within 8 cm of the hall it is an
		// ear of; the hierarchy is the point of the type.
		inline constexpr double EarRoomBelowHallCm = 30.0;

		// JUDGEMENT — 小天井: the shallowest light well worth walling off rather than leaving as yard.
		inline constexpr double MinLightWellDepthCm = 300.0;

		// JUDGEMENT — 過道: the covered way through to the 後院 at the plot edge, one flank only.
		inline constexpr double PassageWidthCm = 240.0;
		inline constexpr double PassageBearingCm = 8.0;

		// JUDGEMENT — 後罩房 and the 後院 it closes, on a 三進 plan.
		inline constexpr double RearRowDepthCm = 400.0;
		inline constexpr double RearCourtDepthCm = 450.0;
		inline constexpr double TypicalRearCourtDepthCm = 700.0;

		// JUDGEMENT — 外院: a shallow strip between the gate row and the 垂花門, never a second court.
		inline constexpr double OuterCourtDepthCm = 240.0;
		inline constexpr double TypicalOuterCourtDepthCm = 620.0;

		// JUDGEMENT — 內院 at its smallest, and at the size an ordinary compound was built to. A
		// standard Beijing 內院 is nearer ten metres than eight, which is why the two differ.
		inline constexpr double MinCourtyardWidthCm = 800.0;
		inline constexpr double MinCourtyardDepthCm = 800.0;
		inline constexpr double CourtyardWidthCm = 1500.0;
		inline constexpr double CourtyardDepthCm = 2000.0;

		// JUDGEMENT — the clear walk a 抄手遊廊 ring is laid at. The corridor's own minimum is the
		// narrowest a 遊廊 may be, not one worth building.
		inline constexpr double CorridorWalkWidthCm = 155.0;

		// JUDGEMENT — 巽位: the gate stands at the southeast corner, never on the axis, with the
		// 門房 carrying the street row past it to the plot's edge.
		inline constexpr double GateLodgeFrontageCm = 320.0;

		// JUDGEMENT — the service yard partitioned off the 外院's far end: the well, the store and
		// the privy.
		inline constexpr double OuterYardWidthCm = 460.0;

		// JUDGEMENT — the gap left between a building and its neighbours, so nothing is drawn
		// touching. Never applied on the plot boundary, where two buildings butt.
		//
		// UNRESOLVED: the compound tool ships 24 rather than this 20, and nothing says why. The
		// tool's figure is the one a placed compound is built with; this one is what the layout
		// falls back to when a caller (the tests) does not set it. Reconciling them changes plot
		// arithmetic, so it is stated here rather than quietly unified.
		inline constexpr double GapCm = 20.0;
	}
}
