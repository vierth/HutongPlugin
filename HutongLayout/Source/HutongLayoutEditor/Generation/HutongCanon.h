#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongRearEave.h"

// Every proportion figure, checkable against the sources without reading the generators.
//
//   CANON     — a source states it (named on the line above).
//   DERIVED   — arithmetic over other entries here.
//   JUDGEMENT — looks right; no source. First suspect when a building looks wrong.
//
// Sources:
//   則例    《工程做法則例》, 1734 (Qing, 工部).
//   營造    《清式營造則例》, 梁思成, 1934.
//   元大都  the Yuan grid as surveyed and as 《乾隆京城全圖》 (1745-50) shows it.
//
// Shipped defaults; every one is editable through a params struct.
namespace HutongCanon
{
	// 小式 module: everything is a multiple of 柱徑.
	namespace Module
	{
		// CANON 則例 — 小式 columns run 11 柱徑 tall.
		inline constexpr double ColumnHeightInDiameters = 11.0;

		// CANON 則例 — 臺明高 is 2 柱徑, off the same eave as the column.
		inline constexpr double PlatformHeightInDiameters = 2.0;

		// CANON 營造 — 上檐出 is about three tenths of 柱高 on the courtyard side.
		inline constexpr double EaveOverhangRatio = 0.30;

		// CANON 則例 — 封護檐: a lane back projects nothing.
		inline constexpr double LaneEaveOverhangRatio = 0.0;

		// CANON 營造 — 檐柱高 = 8/10 明間面闊.
		inline constexpr double ColumnHeightPerCentralBay = 0.8;

		// JUDGEMENT — 8/10 gives a 耳房 a 1.99 m column; small vernacular runs taller. 2.35 m is room
		// height, floored on 柱高 (not the doorway) so the opening stack stays undistorted.
		inline constexpr double MinColumnHeightCm = 235.0;

		// CANON 營造 — 明間 is widest; each 次間 a fixed fraction of it.
		inline constexpr double SideBayWidthRatio = 0.85;

		// CANON 則例 — 收分: a column narrows toward its head by a hundredth of 柱高.
		inline constexpr double ColumnTaperRatio = 0.01;

		// CANON 營造 — 柱徑 is 1/10 to 1/11 of 柱高. Derived path uses 1/11 (ColumnHeightInDiameters);
		// this is the absolute fallback, at 1/10.
		inline constexpr double ColumnDiameterRatio = 0.1;
	}

	// 舉架 and 檁數: the roof section polyline; its depth is not free.
	namespace Roof
	{
		// CANON 則例 — 舉 sequence, eave first (檐步五舉), steepening to the ridge. Each entry is one
		// 步架's rise over its own run.
		inline constexpr double JuThree[] = { 0.5 };
		inline constexpr double JuFive[]  = { 0.5, 0.7 };
		inline constexpr double JuSeven[] = { 0.5, 0.7, 0.9 };
		inline constexpr double JuNine[]  = { 0.5, 0.65, 0.75, 0.9 };

		// CANON 則例 — 進深 = (檁數 - 1) × 步架; purlin count decides, depth follows.
		// 三檁 游廊/影壁/垂花門, 五檁 houses and shops, 七檁 a 正房 with 前廊, 九檁 a hall.

		// 收山: 山花 inset from each end eave; makes a roof 歇山 rather than 廡殿.
		// CANON 營造 — 清式 sets the gable face one 檩徑 inside the 山面檐檩 centre.
		// JUDGEMENT — the centimetres, per type's scale.
		inline constexpr double PavilionShouInsetCm = 88.0;
		inline constexpr double HallShouInsetCm = 150.0;

		// 蠍子尾 on a 清水脊 (四合院建築及其構造 圖5-1-7.1, scaled by its 筒瓦; 圖5-1-5.1, 5-1-7.3): a straight blade
		// tapering from its root to a small outward hook, leaning out at about 50°, rising about 2.5 ridge
		// courses above the ridge, rooted well inside the ridge so its tip barely passes the ridge end; it
		// stands on a raised block (盤子) at the end. Was a quarter-circle curl from the gable, 1.4 courses high.
		inline constexpr double TailRiseInCourses = 2.5;
		inline constexpr double TailLeanDeg = 50.0;
		inline constexpr double TailTipPastEndCm = 4.0;
		inline constexpr double TailHookShare = 0.18;      // of the blade's length, turned out toward level
		inline constexpr double TailHookDeg = 25.0;
		inline constexpr double TailTipTaper = 0.4;       // tip section as a share of the root's
		// The 盤子 under the root: as wide as the ridge and a fifth, as long as the blade's run, a third of a
		// course above the ridge's top.
		inline constexpr double EndBlockWidthShare = 1.2;
		inline constexpr double EndBlockAboveCourses = 0.35;

		// CANON 清式營造則例 表十 (歇山、懸山各部, 小式) — 博風板 1⅘ 檐柱徑 wide, ¼ thick.
		inline constexpr double BargeboardInD = 1.8;
		inline constexpr double BargeboardThicknessInD = 0.25;
	}

	// The 檻框 stack: heights fall out of member sections in 柱徑, not fractions of 柱高.
	namespace Openings
	{
		// CANON 則例 — 額枋高 is one 柱徑; on an 11-柱徑 column its underside is at 0.909 柱高.
		inline constexpr double ArchitraveInDiameters = 1.0;




		// JUDGEMENT — crouch floor (88 cm crouched capsule plus room), not standing: real 耳房 are
		// low. Mesh is its own collision, so a lower doorway is impassable.
		inline constexpr double MinClearHeightCm = 110.0;

		// JUDGEMENT — a garden doorway must pass a standing capsule with margin (mannequin 34×176).
		// Shaped openings (月亮門, 八角門) scale up until this fits inside.
		inline constexpr double WalkerRadiusCm = 40.0;
		inline constexpr double WalkerHeightCm = 190.0;

		// 高窗 under a 封護檐 eave. Head measured down from the eave, so the sill clears head height
		// on a 279 cm 倒座房 and a 364 cm 正房 alike; the lane sees nothing.
		// JUDGEMENT — the three figures, justified by the sill height they produce.
		inline constexpr double RearWindowWidthCm = 70.0;
		inline constexpr double RearWindowHeightCm = 45.0;
		inline constexpr double RearWindowHeadBelowEaveCm = 45.0;

		// DERIVED — half the bay (a two-leaf door folds flat), less jamb and column-radius clearance.
		inline constexpr double DoorWidthFraction = 0.44;
	}

	// 裝修 of a house facade: 隔扇 in the 明間, 支摘窗 in the 次間, 橫陂 over both, 簾架 and 風門 on the door.
	// 四合院建築及其構造 pp.104–105, 圖5-4-5 and 圖5-4-6.
	namespace Joinery
	{
		// CANON 清式營造則例 表十三 檻框, in 柱徑 (D): 下檻 ⅘ high, 中檻 ⅔, 上檻 ½, 風檻 (the window's sill rail on the
		// 榻板) ½, 抱柱 (抱框) ⅔ wide, every 檻 ³⁄₁₀ thick; 榻板 1½ wide, ⅜ thick. The 隔扇 run to one rail at the
		// 檐枋's line (p.104: 上檻 between 檐柱; 中檻 between 金柱, the 橫陂 above it). Replaced shares of the opening
		// (0.034 for the rail) and fixed centimetres (下檻 12, 抱框 22, 榻板 8).
		inline constexpr double LowerSillInD = 0.8;
		inline constexpr double MiddleSillInD = 2.0 / 3.0;
		inline constexpr double UpperSillInD = 0.5;
		inline constexpr double WindowSillRailInD = 0.5;
		inline constexpr double PostInD = 2.0 / 3.0;
		inline constexpr double SillThicknessInD = 0.3;
		inline constexpr double SillBoardWidthInD = 1.5;
		inline constexpr double SillBoardThicknessInD = 0.375;
		// CANON 表十三 — 門簪 as long as a seventh of the 門口's width, a ninth of it across.
		inline constexpr double PegLengthOfDoorway = 1.0 / 7.0;
		inline constexpr double PegDiameterOfDoorway = 1.0 / 9.0;

		// CANON p.104 — 明間 隔扇 一樘四扇; the middle two open, the outer two are fixed.
		inline constexpr int32 GeshanPerBay = 4;
		// CANON p.104 — 橫陂 are fixed, 3–5 per bay between 短間框, only over 隔扇 set between 金柱 (behind a
		// 前廊), filling the 金柱's height above the 檐枋's line; between 檐柱 "民居中極為罕見".
		inline constexpr int32 TransomPanes = 3;
		// JUDGEMENT — least 橫陂 worth building, cm; less and the 金枋 face comes down to the 中檻.
		inline constexpr double MinTransomCm = 15.0;
		// CANON 圖5-4-5 — 短間框 between 橫陂, share of the bay's clear width; 間框 splitting a 支摘窗 the same.
		inline constexpr double TransomPostShare = 0.06;
		inline constexpr double WindowPostShare = 0.05;
		// CANON 圖5-4-5 — the 支窗 over the 摘窗, split at mid-height; the 摘窗 a plain pane (photographs of
		// Mei Lanfang's 正房 and others: no lattice in the lower sash).
		inline constexpr double WindowSplitShare = 0.5;

		// CANON 清式營造則例 表十四 隔扇, in the leaf's width W: 邊梃 and 抹頭 1/10 on the face, 3/20 deep; 仔邊 ⅔ of the
		// 邊梃's face and 7/10 its depth; 欞條 ⅘ of the 仔邊's face, 9/10 its depth; 絛環板 ⅕ high, 裙板 ⅘, both 1/20
		// thick. A 五抹 leaf bottom up: 抹頭, 下絛環, 抹頭, 裙板 | 抹頭, 中絛環, 抹頭, 槅心, 抹頭; the 槅心 takes what the
		// leaf's height leaves (the table's "3/5" for it does not fit a leaf of the width). The lower run is 1.2 W,
		// and its top is the 榻板's (p.104, 四六分): the sill follows the leaf. Replaced shares of the leaf's height read
		// off 圖5-4-6.
		inline constexpr double GeshanStileOfWidth = 0.1;
		inline constexpr double GeshanDepthOfWidth = 0.15;
		inline constexpr double GeshanRimOfStile = 2.0 / 3.0;
		inline constexpr double GeshanRimDepthOfStile = 0.7;
		inline constexpr double GeshanBarOfRim = 0.8;
		inline constexpr double GeshanSmallPanelOfWidth = 0.2;
		inline constexpr double GeshanSkirtOfWidth = 0.8;
		inline constexpr double GeshanPanelThicknessOfWidth = 0.05;
		inline constexpr double GeshanLowerOfWidth = 2.0 * GeshanStileOfWidth + GeshanSmallPanelOfWidth + GeshanSkirtOfWidth;

		// CANON p.105, 圖5-4-6 — 簾架: as wide as the two opening 隔扇, 下檻 to 中檻, 邊梃 on 荷葉墩, 栓斗 at
		// the 中檻. Shares of its height top down: 抹頭, 簾架橫陂, 抹頭, 楣子, 風門 (to the 啞吧檻), 啞吧檻.
		inline constexpr double CurtainTopRail = 0.017;
		inline constexpr double CurtainTransom = 0.156;
		inline constexpr double CurtainMidRail = 0.017;
		inline constexpr double CurtainLintel = 0.105;
		inline constexpr double CurtainDumbSill = 0.044;
		// JUDGEMENT — the 門口's clear height. p.105 sizes it by 吉門尺寸 (the 門光尺), not as a share of the
		// 簾架; the table is not in hand, so a standing walker's height stands in. The 楣子, the 簾架橫陂 and the
		// 啞吧檻 give way to it; Mei Lanfang's 正房 shows the 風門 about 0.7 of the 隔扇.
		inline constexpr double FengmenClearCm = 190.0;
		// CANON 圖5-4-6 — 邊梃 0.045 of the 簾架's width; the 門口 0.61 of it, 餘塞 either side.
		inline constexpr double CurtainStileShare = 0.045;
		inline constexpr double CurtainDoorShare = 0.61;

		// JUDGEMENT — thicknesses: 隔扇 and sashes, 簾架 and 風門; panels set back in their frames.
		inline constexpr double LeafThicknessCm = 6.0;
		inline constexpr double CurtainThicknessCm = 5.0;
		inline constexpr double PanelRecessCm = 2.5;
		// JUDGEMENT — each board carries a raised field inset from its frame, the double line every panel has
		// in 圖5-4-5/5-4-6 (one box, where a moulded ring would be four).
		inline constexpr double PanelFieldProudCm = 1.5;
		inline constexpr double PanelFieldInsetShare = 0.12;
		// CANON p.90, 圖5-3-10.1 — 海棠池子 on a 檻牆: 大枋子 about 15 cm wide round the face, a 線枋子 inside it,
		// the 墻心 a diagonal 方磚 field. JUDGEMENT — the 線枋子's width and each course's standing-off.
		inline constexpr double SillPoolBorderCm = 15.0;
		inline constexpr double SillPoolLineCm = 3.0;
		inline constexpr double SillPoolBorderProudCm = 1.5;
		inline constexpr double SillPoolLineProudCm = 1.0;
		inline constexpr double SillPoolFieldProudCm = 0.5;
		// JUDGEMENT — open leaves: the middle 隔扇, opened for the season behind the 簾架, fold back flat against the
		// backs of the fixed ones (178°, a hair off so the two never share a face; at 88° one stood square across
		// the doorway behind the 風門 and read as a second door); the 風門 swings out ("風門一般向外開啟"), back
		// against the 簾架 as far as it goes — this only its floor.
		inline constexpr double GeshanOpenDeg = 178.0;
		inline constexpr double FengmenOpenDeg = 95.0;
	}

	// 封護檐 cornices (檐子), bottom course first: height, standing-off of its face (a round course bulges
	// from Proud to Outer at mid-height; a block course stands its blocks at Outer, Width wide every Pitch).
	// Scaled down together where the wall carried up above the eave is shorter than the stack.
	namespace Cornice
	{
		enum class ECourse : uint8 { Flat, Round, Blocks };
		struct FCourse { ECourse Kind; double HeightCm; double ProudCm; double OuterCm = 0.0; double WidthCm = 0.0; double PitchCm = 0.0; };

		// JUDGEMENT — the four-course 冰盤檐 the house had before the figures, 14 cm out.
		inline constexpr FCourse IceTray[] = {
			{ ECourse::Flat, 6.5, 3.5 }, { ECourse::Flat, 6.5, 7.0 }, { ECourse::Flat, 6.5, 10.5 }, { ECourse::Flat, 6.5, 14.0 } };
		// 圖5-3-9.8 三層雞素子檐: a plain course, a deep rounded one, a cover course. Heights and reach JUDGEMENT
		// off the photograph (the rounded course about a course and a half high).
		inline constexpr FCourse Rounded[] = {
			{ ECourse::Flat, 6.0, 4.0 }, { ECourse::Round, 9.0, 4.0, 11.0 }, { ECourse::Flat, 6.0, 13.0 } };
		// 圖5-3-9.9 三層抽屜檐: a plain course, blocks with gaps half as wide again, a cover course.
		inline constexpr FCourse Drawer[] = {
			{ ECourse::Flat, 6.0, 4.0 }, { ECourse::Blocks, 7.0, 4.0, 10.0, 9.0, 22.0 }, { ECourse::Flat, 6.0, 13.0 } };
		// 圖5-3-9.10 七層: 頭層檐, 連珠混 (a round course: beads the length of a house cost thousands of
		// triangles), 半混, the tall plain 炉口, 梟, 磚椽 blocks close-set, the cover course.
		inline constexpr FCourse SevenCourse[] = {
			{ ECourse::Flat, 5.0, 2.0 }, { ECourse::Round, 5.0, 2.0, 5.0 }, { ECourse::Round, 7.0, 5.0, 8.0 },
			{ ECourse::Flat, 12.0, 8.0 }, { ECourse::Flat, 5.0, 10.0 }, { ECourse::Blocks, 7.0, 10.0, 15.0, 7.0, 14.0 },
			{ ECourse::Flat, 6.0, 17.0 } };
	}

	// CANON 清式營造則例 表十二 瓦作, 小式, in 檐柱徑: 山牆 2.4 thick, 支摘窗 and 檻窗 檻牆 1½. The 檐牆 (the back
	// wall) is given for 大式 only, 1½ and its 八字; taken at 1½ for a 小式 house too.
	namespace Masonry
	{
		inline constexpr double GableWallInD = 2.4;
		inline constexpr double SillWallInD = 1.5;
		inline constexpr double RearWallInD = 1.5;
	}

	// 院牆 faces the lane, 隔牆 divides one household's courts. Ordering solid; centimetres chosen to clear.
	namespace BaseCourse
	{
		// JUDGEMENT — 下鹼 top above ground, one line along a whole frontage (倒座房 rear wall, 大門,
		// walls between). 營造算例's 3/10 檐柱高 above 臺基 lands near this for the presets; walls
		// have no rule. Each type subtracts its own floor or plinth.
		inline constexpr double TopCm = 110.0;
	}

	namespace Wall
	{
		// JUDGEMENT — 300 to cap underside ≈ 一丈 overall, level with the wings' eaves, under the 正房 roof.
		inline constexpr double PerimeterHeightCm = 300.0;

		// CANON 營造 — 一磚半 laid in 青磚.
		inline constexpr double PerimeterThicknessCm = 37.0;

		// JUDGEMENT — corbelled 磚檐 courses under the cap.
		inline constexpr int32 PerimeterCapCourses = 3;
		inline constexpr double PerimeterCapOverhangCm = 9.0;

		// JUDGEMENT — under the 垂花門's 310 eave so the gate stands clear of its wall.
		inline constexpr double CourtyardHeightCm = 240.0;

		// CANON 營造 — 一磚.
		inline constexpr double CourtyardThicknessCm = 24.0;

		// JUDGEMENT — one course fewer than the perimeter's.
		inline constexpr int32 CourtyardCapCourses = 2;
		inline constexpr double CourtyardCapOverhangCm = 6.0;

		// JUDGEMENT — 瓦頂 牆帽: two-slope 合瓦 cap, low 眉子 ridge, thin drip course each eave.
		inline constexpr double CapRidgeCourseHeightCm = 8.0;
		inline constexpr double CapRidgeCourseWidthCm = 10.0;
		inline constexpr double CapDripDepthCm = 4.0;

		// JUDGEMENT — drip lip stands wholly past the corbelling, inner face on the corbel's outer
		// face (opposite normals). Laid over the corbel it z-fought.
		inline constexpr double CapDripProjectionCm = 3.0;

		// JUDGEMENT — 墀頭 projects column base radius plus this, or the corner 檐柱's foot pokes through.
		inline constexpr double ChitouColumnClearanceCm = 1.0;
	}

	// The four courtyard gates: one 硬山 bay each, ranked by door-plane depth behind the 檐柱 line.
	namespace Gate
	{
		// Plan band: 面闊, 進深, eave (cm). Both gate params structs use it as FSizeRange; all zero = unconstrained.
		struct FSizeBand
		{
			double FrontageMin = 0.0, FrontageMax = 0.0;
			double DepthMin = 0.0, DepthMax = 0.0;
			double EaveMin = 0.0, EaveMax = 0.0;
		};

		// CANON 營造 — 廣亮大門 doors on the 中柱, half the depth back.
		inline constexpr double GuangliangDoorPlane = 0.5;

		// CANON 營造 — 金柱大門 doors on the 前金柱. CANON 四合院建築及其構造 圖5-1-3 — one 步架 of the five-purlin
		// section's four behind the 檐柱 (plan 0.22, section 0.25): the 前廊 of FrameLayout's 鑽金柱 frame.
		inline constexpr double JinzhuDoorPlane = 0.25;

		// CANON 營造 — 蠻子門 and 如意門 doors on the 檐柱 line, flush.
		inline constexpr double FlushDoorPlane = 0.0;

		// CANON 營造 — rank: 廣亮 and 金柱 are 五檁 with 筒瓦; 如意門 is 三檁, a doorway in a brick
		// screen. 筒瓦 on a commoner's house is 逾制.
		// JUDGEMENT — the bands; ordering is the claim, centimetres a reading.
		inline constexpr FSizeBand GuangliangSize = { 330.0, 420.0, 400.0, 520.0, 320.0, 400.0 };
		inline constexpr FSizeBand JinzhuSize     = { 320.0, 400.0, 350.0, 460.0, 300.0, 375.0 };
		inline constexpr FSizeBand ManziSize      = { 300.0, 380.0, 300.0, 400.0, 285.0, 350.0 };
		inline constexpr FSizeBand RuyiSize       = { 260.0, 350.0, 250.0, 350.0, 265.0, 330.0 };

		// JUDGEMENT — 垂花門. Depth is the 擔梁's run, columns at mid-run: one 步架 deep, eaves past the frame.
		inline constexpr FSizeBand InnerGateSize  = { 280.0, 400.0, 105.0, 215.0, 265.0, 365.0 };

		// JUDGEMENT — 一殿一卷式垂花門 (Fig 2-9.1): width ≈ a hall's 次間; depth = 垂蓮柱 cantilever
		// ahead of the 前簷柱 plus the run to the 後簷柱. Stands in the 內院 behind its wall.
		// Depth middle 406: Fig 2-9.1 walk floor, three steps each side, 6.26 m foot to foot
		// (1.7 cm on the page at 3.68 m/cm).
		inline constexpr FSizeBand InnerGateGrandSize = { 320.0, 400.0, 380.0, 432.0, 300.0, 370.0 };

		// JUDGEMENT — 麻葉抱頭梁 cantilever of the 垂蓮柱 ahead of the 前簷柱; front 殿 roof's share of
		// depth before the 天溝 meeting the rear 捲棚.
		inline constexpr double InnerGateCantileverCm = 75.0;
		inline constexpr double InnerGateFrontRoofShare = 0.56;

		// JUDGEMENT — share of a house's 上檐出 each rank carries: 廣亮 full, 如意門 little.
		// Ordering is the claim, fractions a reading.
		inline constexpr double GuangliangEaveShare = 1.0;
		inline constexpr double JinzhuEaveShare = 0.95;
		inline constexpr double ManziEaveShare = 0.8;
		inline constexpr double RuyiEaveShare = 0.5;

		// CANON 四合院建築及其構造 圖5-1-6 — 如意門 street face: brick between the 墀頭, no timber but the
		// door. Doorway 0.37 of the clear span between the piers.
		inline constexpr double RuyiDoorShare = 0.37;
		// CANON p.67 text with 圖5-1-5 — the 門頭 (a 朝天欄杆) from the door surround up: 掛落, 頭層檐,
		// 連珠混, 半混, 蓋板, 欄板望柱; over them the dentils and top course 圖5-1-6 shows under the roof.
		// Heights are shares of the frieze (door surround's top to roof base), JUDGEMENT within 圖5-1-6's
		// bands: its lower band → 掛落, its moulding → the four ledges, its panels → 欄板望柱.
		// Proud = share of DoorHeadProjection each stands out; each ledge steps further out to the 蓋板,
		// the balustrade stands back on it.
		inline constexpr double RuyiFriezeHanging = 0.18;
		inline constexpr double RuyiFriezeFirstEave = 0.06;
		inline constexpr double RuyiFriezeBead = 0.045;
		inline constexpr double RuyiFriezeHalfRound = 0.06;
		inline constexpr double RuyiFriezeCover = 0.045;
		inline constexpr double RuyiFriezePanels = 0.38;
		inline constexpr double RuyiFriezeDentils = 0.135;
		inline constexpr double RuyiHangingProud = 0.15;
		inline constexpr double RuyiFirstEaveProud = 0.4;
		inline constexpr double RuyiBeadProud = 0.5;
		inline constexpr double RuyiHalfRoundProud[2] = { 0.5, 0.7 };
		inline constexpr double RuyiCoverProud = 0.8;
		inline constexpr double RuyiPanelProud = 0.35;
		inline constexpr double RuyiPostProud = 0.5;
		inline constexpr double RuyiDentilProud = 0.85;
		inline constexpr int32 RuyiFriezePanelCount = 3;
		inline constexpr double RuyiFriezePostShare = 0.04;
		// 望柱 heads stand above the 欄板 by this share of the balustrade's height.
		inline constexpr double RuyiPostHeadShare = 0.15;
		// 掛落 frame and 連珠混 beads, shares of the course they sit in.
		inline constexpr double RuyiHangingBorderShare = 0.18;
		inline constexpr double RuyiBeadPitchShare = 1.6;
		inline constexpr double RuyiDentilSpacingShare = 0.048;

		// CANON 四合院建築及其構造 圖5-1-4.2 — 蠻子門 street face: 檻框 fills the bay between the piers.
		// Leaves about half that span; 中檻 at the leaf top across the bay carrying four 門簪; 走馬板
		// above it, then the 上檻 (UpperSillShare of the height from 中檻 to 額枋) under a column-top
		// 額枋. Each 餘塞板 split by two 腰枋 with a short panel between, at these shares of the leaf
		// height down from the 中檻. 如意門 keeps two 門簪 (rank, like its 門墩).
		inline constexpr double UpperSillShare = 0.4;
		inline constexpr double YusaiRailUpper[2] = { 0.54, 0.58 };
		inline constexpr double YusaiRailLower[2] = { 0.70, 0.74 };
		inline constexpr double PanelRecessCm = 3.0;
		inline constexpr int32 RuyiDoorPegs = 2;

		// CANON 圖5-1-4.2 plan and section — 蠻子門 is 五檁中柱式: the side walls are built round the 檐柱 and
		// 中柱, a quarter 柱徑 of each standing proud of the passage face, the 步梁 and 瓜柱 over them showing.
		// 廊心 between them: a framed field of diagonal 方磚 over the 下鹼, the frame ColumnMargin 柱徑 off each
		// column, a TopGap / BottomGap share of 下鹼-to-column-top clear of beam and 下鹼. 門墩 about 44 cm.
		inline constexpr double ColumnProudShare = 0.25;
		inline constexpr double LangxinColumnMargin = 0.5;
		inline constexpr double LangxinTopGap = 0.086;
		inline constexpr double LangxinBottomGap = 0.05;
		inline constexpr double LangxinBorderCm = 5.0;
		inline constexpr double LangxinBorderProudCm = 2.0;
		inline constexpr double LangxinFieldProudCm = 1.0;
		inline constexpr double ManziDoorStoneCm = 44.0;

		// CANON 圖5-1-3 and its text — 金柱大門: three columns a side (檐柱, 前金柱 carrying the door, 後檐柱),
		// the 鑽金柱 frame of 圖5-3-2, 雀替 under the front 額枋 "as on the 廣亮大門". 清式營造則例 表四: 雀替 reach a
		// quarter of the 明間's clear span, 1¼ 柱徑 high, ⅔ thick; the underside sags from the tip to the column.
		inline constexpr double QuetiReachShare = 0.25;
		inline constexpr double QuetiHeightInD = 1.25;
		inline constexpr double QuetiThicknessInD = 2.0 / 3.0;
		inline constexpr double QuetiSag = 1.4;

		// CANON 圖5-1-2 — 反八字影壁 (撇山影壁) on a 廣亮大門, "在實際中也很常見". Plan: each from its pier's
		// front outer corner, 45° off the facade (read 42°/44°), 0.68 of the frontage long, 0.06 of it thick.
		// Elevation, heights as shares of the gate's roof base: 下鹼 0.27, 方磚 field to 0.55, a band over it
		// to the cornice at 0.62, cap ridge 0.77. Along the wing from the gate the figure has brick posts
		// 0.10–0.16 and 0.55–0.61 round the field, a 盤頭 pier from 0.72 to the outer end.
		inline constexpr double WingAngleDeg = 45.0;
		inline constexpr double WingLengthShare = 0.68;
		inline constexpr double WingThicknessShare = 0.06;
		inline constexpr double WingMinThicknessCm = 20.0;
		inline constexpr double WingBaseShare = 0.27;
		inline constexpr double WingFieldTopShare = 0.55;
		inline constexpr double WingBodyShare = 0.62;
		inline constexpr double WingRidgeShare = 0.77;
		// The panel (posts and field, the figure's 0.51 of the wing) centred on the wing, not on the figure's
		// stretch between gate and pier: the pier is barely proud and the same brick, so the user read the
		// figure's placement as off-centre. The pier narrowed to stay clear of it.
		inline constexpr double WingPostInner[2] = { 0.245, 0.305 };
		inline constexpr double WingPostOuter[2] = { 0.695, 0.755 };
		inline constexpr double WingPierFrom = 0.86;
		// JUDGEMENT — the cornice: two courses stepping 3 cm out, 0.045 of the roof base high; the cap's eave
		// 4 cm past them; relief of posts, pier and field on the face.
		inline constexpr double WingCorniceShare = 0.045;
		inline constexpr double WingCorbelStepCm = 3.0;
		inline constexpr double WingPostProudCm = 2.0;
		inline constexpr double WingFieldProudCm = 1.0;

		// JUDGEMENT — a 大門 in a street row: ridge this far above the row's. Applies in compound,
		// street row and hand placement against a traced 倒座房 (the band assumes a lone gate).
		inline constexpr double RidgeAboveRowCm = 35.0;
	}

	// 門墩 / 門枕石 at each gate jamb. Form is rank: 抱鼓石 for the two official gates; within a
	// rank, 文官 block or 武官 drum.
	namespace Stone
	{
		// JUDGEMENT — 方門墩 at knee height, the 50-60 cm band in photographs.
		inline constexpr double BlockHeightCm = 54.0;

		// JUDGEMENT — a 抱鼓石 with plinth stands 90-110; a gate's a little under.
		inline constexpr double DrumHeightCm = 82.0;

		// JUDGEMENT — disc share of the whole stone, so the drum overhangs its plinth, not a knob.
		inline constexpr double DrumFraction = 0.62;

		// JUDGEMENT — projection ahead of the doorway, about its height.
		inline constexpr double ProjectionCm = 32.0;

		// JUDGEMENT — inner gate: a small 門軸 bearing, not an announcement; 垂花門 板門 still need one.
		inline constexpr double InnerGateHeightCm = 40.0;
		inline constexpr double InnerGateProjectionCm = 20.0;
	}

	// The 元大都 grid, in 步.
	namespace Urban
	{
		// CANON 元大都 — 1 步 = 5 尺 at a Yuan 尺 of ~31.6 cm.
		inline constexpr double PaceCm = 158.0;

		// CANON 元大都 — the street classes, in 步.
		inline constexpr double HutongPaces = 6.0;       // 胡同, ~9.5 m
		inline constexpr double MinorStreetPaces = 12.0; // 小街, ~19 m
		inline constexpr double MajorStreetPaces = 24.0; // 大街, ~38 m

		// CANON 元大都 — lane centre to centre, leaving a ~70 m plot band.
		inline constexpr double LanePitchPaces = 50.0;

		// JUDGEMENT — tolerance off a class's nominal width. Off-band is a note, not an error: the
		// Outer City was never regular, the Inner City has 500 years of encroachment.
		inline constexpr double InBandToleranceCm = 120.0;
	}

	// House types: one 硬山 hall at several footprints. Eave derives from 檐柱高 = 8/10 明間面闊;
	// EaveCm is the fallback with derivation off.
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

		// JUDGEMENT unless marked — sizes read off plans. CANON is the table's shape: hall tallest and
		// alone with a 前廊, lane-facing rows carry 高窗, 進深 = (檁數 - 1) × 步架 so never given.

		// CANON 四合院建築及其構造 p.84 — 正房: 七檁前後廊, four column rows (前檐柱, 前檐金柱,
		// 後檐金柱, 後檐柱), 門窗 on the 前金柱 line, 後檐牆 on the 後檐柱 line enclosing the 後廊.
		// 進深 ≥ 7 m, 明間 3.9-4.2 m, 次間 ~3.3 m, 檐柱 3.3-3.5 m.
		// DERIVED — 1060 at 0.82 → 明間 402, 次間 329; 6 × 117 → 702; 0.84 × 明間 → 337 column
		// (8/10 gives 321, under the source's range).
		inline constexpr FHouse MainHall =
			{ 399.0, 320.0, 420.0, true,  EHutongRearEave::Lane, 1060.0, EHutongPurlins::Seven, 117.0, true,
			  true, 0.82, 0.84 };

		// 五間正房: MainHall two 次間 wider.
		// JUDGEMENT — 402 + 4 × 330, so bays, 檐柱 and section match MainHall.
		inline constexpr FHouse MainHallFiveBay =
			{ 399.0, 320.0, 420.0, true,  EHutongRearEave::Lane, 1720.0, EHutongPurlins::Seven, 117.0, true,
			  true, 0.82, 0.84 };

		// CANON same work, p.85 and 圖5-3-2 — 前廊後無廊: smaller court's 正房, no 後檐柱. 鑽金柱 keeps
		// the ridge centred: 五檁, four 步架 with the 廊 first, eaves level, roof lower than 七檁.
		// JUDGEMENT — 150 步架 and 600 進深 scaled off the figure; 1040 frontage since the page says
		// only 面寬要酌減.
		inline constexpr FHouse MainHallSmall =
			{ 365.0, 300.0, 380.0, true,  EHutongRearEave::Lane, 1040.0, EHutongPurlins::Five, 150.0, true };

		// 廂房: east and west side houses.
		// CANON same work, p.85 and 圖5-3-3 (left) — 大中型 court: 前檐帶外廊, 六檁前出廊 set right as the 正房's
		// 前廊後無廊 (鑽金柱, ridge centred): 五檁, four 步架 with the 廊 first. p.83: 廂房進深 5.5 m 含外廊.
		inline constexpr FHouse SideHouse =
			{ 340.0, 280.0, 340.0, true,  EHutongRearEave::Lane,  900.0, EHutongPurlins::Five, 137.5, false };
		// CANON p.85 and 圖5-3-3 (right) — 小型 court: no room for a 廊, 五檁無廊, 四步架, 封護檐 behind.
		// p.83: 3.5–4 m 進深 無外廊.
		inline constexpr FHouse SideHouseSmall =
			{ 340.0, 280.0, 340.0, false, EHutongRearEave::Lane,  900.0, EHutongPurlins::Five, 100.0, false };

		// 倒座房: street row, low and long, back is the lane wall.
		inline constexpr FHouse FrontRow =
			{ 322.0, 250.0, 300.0, false, EHutongRearEave::Lane, 1300.0, EHutongPurlins::Five,  90.0, true };

		// 後罩房: closes the back of a 三進 plot.
		inline constexpr FHouse RearRow =
			{ 328.0, 260.0, 320.0, false, EHutongRearEave::Courtyard, 1300.0, EHutongPurlins::Five, 100.0, true };

		// 耳房: low rooms at the hall's flanks; the 柱高 floor bites here.
		// CANON p.83 — 3.0 m, two per flank (三正四耳) in a 大型 court; 2.4 m, one, in a 小型 (compound size table).
		inline constexpr FHouse EarRoom =
			{ 316.0, 240.0, 320.0, false, EHutongRearEave::Lane,  600.0, EHutongPurlins::Five,  85.0, false };
	}

	// 構架 members, in 檐柱 柱徑 (D).
	// CANON 清式營造則例 表二–表六 (小式) where named; 寸 of the 營造尺 (32 cm). JUDGEMENT otherwise.
	namespace Frame
	{
		inline constexpr double CunCm = 3.2;
		inline constexpr double GoldColumnExtraCm = 1.0 * CunCm;   // 表三: 金柱徑 = 檐柱徑 + 1 寸
		inline constexpr double GableColumnExtraCm = 2.0 * CunCm;  // 表三: 山柱徑 = 檐柱徑 + 2 寸 (a gate's 中柱)
		inline constexpr double PurlinDiameter = 1.0;          // 表六: 檁徑 1 D, every line
		inline constexpr double BoardHeight = 0.65;            // 墊板
		inline constexpr double BoardThickness = 0.25;
		// 表四: 檐枋 高 D, 厚 ⅘ D; 金枋 and 脊枋 each 2 寸 less.
		inline constexpr double EaveTieHeight = 1.0;
		inline constexpr double EaveTieThickness = 0.8;
		inline constexpr double InnerTieLessCm = 2.0 * CunCm;
		// 表二: 穿插枋 1 × ¾ D; 隨梁 1 D high, 2 寸 less than D thick.
		inline constexpr double ThroughTieHeight = 1.0;
		inline constexpr double ThroughTieThickness = 0.75;
		inline constexpr double FollowTieLessCm = 2.0 * CunCm;
		// 表二: 抱頭梁 (and the 插梁 of a 鑽金柱 frame) 1½ × 1⅕ D.
		inline constexpr double HeadBeamHeight = 1.5;
		inline constexpr double HeadBeamWidth = 1.2;
		// 表二 算梁通例: the lowest beam (大柁), whatever its span, D + 2 寸 thick and 1.2 of that high; the 雙步梁
		// the same. Each layer up (二柁, 上柁; the 單步梁 over its 雙步梁) ⅚ of the one below high, ⅘ thick.
		inline constexpr double MainBeamExtraCm = 2.0 * CunCm;
		inline constexpr double MainBeamDepthOfWidth = 1.2;
		// 卷棚 (圖18, 則例 卷十二 四檁卷棚, 檐柱徑 0.70 尺): 四架梁 1.17 × 0.90 — the table's other ratio, 1.3.
		inline constexpr double RollBeamDepthOfWidth = 1.3;
		inline constexpr double UpperBeamDepth = 5.0 / 6.0;
		inline constexpr double UpperBeamWidth = 4.0 / 5.0;
		// 頂梁 (a 卷棚's 月梁, 0.97 × 0.70 on 圖18): 2 寸 less than the beam below, each way.
		inline constexpr double TopBeamLessCm = 2.0 * CunCm;
		// Under this much clear height (柱徑) the 月梁 sits on one 柁墩 rather than two 瓜柱 (the 抄手遊廊 section).
		inline constexpr double TuodunBelow = 1.5;
		// 表五: 瓜柱 1 × 1 D. Head bevels to seat the 檁 and runs up into it; its sides stay clear of the round 檁.
		inline constexpr double StrutSection = 1.0;
		inline constexpr double StrutHeadWidth = 0.6;          // of the section, where it meets the 檁
		inline constexpr double StrutHeadBevel = 0.35;         // of the section, the bevel's own height
		inline constexpr double StrutIntoPurlin = 0.33;        // of 檁徑, how far the head rises into it

		// 表五: 角背 one 步架 long, ½ the 脊瓜柱's height high, ⅓ of its own height thick. Flat top, angled
		// edge down (圖5-3-1).
		inline constexpr double BraceReach = 0.5;              // 步架, each side of the post
		inline constexpr double BraceCut = 0.12;               // 步架, the corner cut's own run
		inline constexpr double BraceHeight = 0.5;             // of the post's exposed height
		inline constexpr double BraceThickness = 1.0 / 3.0;    // of its own height
		inline constexpr double BraceStraight = 0.45;          // of its height, standing before the cut

		// 表四: 燕尾枋 ½ × ⅙ D, under each 檁 where a 懸山 carries it past the gable column.
		inline constexpr double SwallowTailHeight = 0.5;
		inline constexpr double SwallowTailThickness = 1.0 / 6.0;
		// JUDGEMENT: the underside rises over the outer part of its run to this share of its height at the end.
		inline constexpr double SwallowTailRun = 0.35;
		inline constexpr double SwallowTailEnd = 0.45;

		// 出頭: the 穿插枋's tenon past a column — smaller than the member, on its underside (圖5-3-1).
		inline constexpr double TenonHeight = 0.5;             // of the member's own height
		inline constexpr double TenonWidth = 0.75;             // of its thickness
		inline constexpr double TenonReach = 0.9;              // 柱徑, past the column's axis
		// Overrun of every frontage member past its last column; all nine end on one line (圖5-3-1).
		// Ending on the column axis reads as buried.
		inline constexpr double RunProjection = 0.8;           // 檁, 墊板 and 枋 alike
		inline constexpr double RafterDiameter = 1.0 / 3.0;    // 椽徑, laid one 椽徑 apart
		// 望板, 苫背 and tile over the rafters, measured plumb.
		inline constexpr double RoofCover = 0.45;
		inline constexpr double FlyingShareOfEave = 1.0 / 3.0; // 飛椽出 of 上檐出
		inline constexpr double FlyingTailRatio = 2.5;         // 飛椽 tail against its head
		inline constexpr double PlatformReachOfEave = 0.8;     // 臺明 下出 inside 上出, the 回水
		inline constexpr double PlatformSide = 2.0;            // 臺明 past the gable columns
		inline constexpr double EdgeStoneWidth = 1.3;          // 階條
		inline constexpr double BaseStoneSide = 2.0;           // 柱頂石
		inline constexpr double StringerWidth = 1.3;           // 垂帶
	}


	// 亭: 則例 卷二十二 肆角攢尖方亭 (面闊 10.00 尺, 檐柱徑 0.70 尺), 橫斷面, 正面立面, 台基平面 and 步架平面.
	// Plan and heights as shares of 面闊 (Bay); member sections in 柱徑 (D), as the figure labels them.
	namespace Pavilion
	{
		// CANON 則例 — the figure's 面闊, 10 營造尺 of 32 cm.
		inline constexpr double FigureBayCm = 320.0;
		inline constexpr double ColumnPerBay = 0.07;           // 檐柱徑 0.70
		inline constexpr double ColumnHeightPerBay = 0.8;      // 檐柱高 8.00
		inline constexpr double PlatformHeightPerBay = 0.12;   // 台高 1.2
		inline constexpr double PlatformReachPerBay = 0.192;   // 台出 1.92, column centre to edge
		inline constexpr double EaveOverhangPerBay = 0.24;     // 出檐 2.4, from the 檐桁 centre
		// 檐步 伍舉, 脊步 柒伍舉, each 步架 2.5; the overhang takes the eave step's.
		inline constexpr double Ju[] = { 0.5, 0.75 };

		inline constexpr double BoardHeight = 0.6 / 0.7;       // 平水 (墊板) 0.6
		inline constexpr double PurlinDiameter = 1.0;          // 檩木 0.7
		inline constexpr double LintelHeight = 1.0;            // 箍頭檐枋 0.7 × 0.5
		inline constexpr double LintelThickness = 0.5 / 0.7;
		inline constexpr double LintelHeadReach = 1.0;         // 箍頭 出0.7, past the corner column
		inline constexpr double BeamHeadLength = 2.94 / 0.7;   // 肆角花梁頭 2.94 × 0.9 × 0.9
		inline constexpr double BeamHeadSection = 0.9 / 0.7;
		inline constexpr double RafterDiameter = 0.3;          // 檐椽 徑0.21
		inline constexpr double RafterSpacing = 0.6;           // 間椽 12 支 over half a side
		inline constexpr double CornerRun = 3.0;               // 衝三翹四: 翼角斜出 0.63 = 3 椽徑
		inline constexpr double CornerRise = 4.0;              // and 4 椽徑 up

		// 柱頂石 1.4 見方 with a 古鏡 0.21 high; 階條石 1.22 × 0.48.
		inline constexpr double BaseStoneSide = 2.0;
		inline constexpr double MirrorRise = 0.3;
		inline constexpr double MirrorRadius = 0.6;            // JUDGEMENT — just proud of the column
		inline constexpr double EdgeStoneWidth = 1.22 / 0.7;
		inline constexpr double EdgeStoneThickness = 0.48 / 0.7;

		// 坐凳欄杆 高1.6 寬0.7, from each column 3.2 尺 along the side (read off the plan), the middle open.
		inline constexpr double BenchHeightPerBay = 0.16;
		inline constexpr double BenchDepthPerBay = 0.07;
		inline constexpr double BenchRunPerBay = 0.32;

		// 如意踏跺 at the middle of each side, read off 台基平面: a step and the 如意石 below it,
		// receding on three sides, three risers to the 台高.
		inline constexpr double StepWidthPerBay = 0.42;
		inline constexpr double StepReachPerBay = 0.11;        // from the platform edge
		inline constexpr double RuyiWidthPerBay = 0.62;
		inline constexpr double RuyiReachPerBay = 0.21;

		// 方式花甎寶頂 一座 3.2 on a 寶頂博脊 0.5, courses bottom to top: 博脊, 番荷葉 (方2.7), 須彌座
		// (花線 方1.9, 梟 1.7, 束腰 1.2), 削砌水盤沿 (徑1.4), 方式塔子 (方1.0). Height, side per 面闊; the
		// course heights are read off the elevation.
		struct FFinialCourse { double Height, Side; bool bRound; };
		inline constexpr FFinialCourse FinialCourses[] = {
			{ 0.05,  0.24, false },   // 寶頂博脊
			{ 0.03,  0.27, false },   // 番荷葉
			{ 0.025, 0.19, false },   // 下花線
			{ 0.025, 0.17, false },   // 下梟
			{ 0.03,  0.12, false },   // 束腰
			{ 0.025, 0.17, false },   // 上梟
			{ 0.025, 0.19, false },   // 上花線
			{ 0.02,  0.14, true  },   // 水盤沿
			{ 0.14,  0.10, false },   // 塔子
		};
		inline constexpr double FinialHeightPerBay = 0.32;     // above the 博脊
		inline constexpr double FinialWidthPerBay = 0.27;

		// 徹上明造 (步架平面): 抹角梁 joining the side midpoints, 交金墩 on them under the 金桁 ring's
		// corners (one 步架 in), 由戧 up the hips to the 雷公柱 that carries the 寶頂.
		inline constexpr double CornerBeamWidth = 0.9 / 0.7;   // 抹角梁 0.9 × 1.08
		inline constexpr double CornerBeamHeight = 1.08 / 0.7;
		inline constexpr double BlockWidth = 0.4 / 0.7;        // 交金墩 0.4 × 2.1 × 0.7
		inline constexpr double BlockLength = 2.1 / 0.7;
		inline constexpr double BlockHeight = 1.0;
		inline constexpr double GoldPurlinHeadReach = 1.5;     // 搭交出頭 1.05
		inline constexpr double GoldTieThickness = 0.3 / 0.7;  // 金枋 0.3 × 0.5
		inline constexpr double GoldTieHeight = 0.5 / 0.7;
		inline constexpr double HipBraceHeight = 0.63 / 0.7;   // 由戧 0.63 × 0.42
		inline constexpr double HipBraceWidth = 0.42 / 0.7;
		inline constexpr double KingPostDiameter = 1.05 / 0.7; // 雷公柱 徑1.05

		// 則例 卷二十三 陸柱圓亭 (進深 10.00 across the column centres, 面闊 5.00 between neighbours, 檐柱徑
		// 0.70, 高 8.00): the square's section, eave and members over again, on a circle. "Bay" is the
		// 進深. Only what differs is here.
		inline constexpr int32 RoundColumns = 6;
		inline constexpr double RoundPlatformReachPerBay = 0.17;   // 台基 circle, read off 台基平面 and 橫斷面
		inline constexpr double RoundBenchHeightPerBay = 0.15;     // 坐凳欄杆 高1.5
		inline constexpr double RoundEntryBenchShare = 0.2;        // of the entry bay, from each column (正面立面)
		inline constexpr double RoundFriezeDropPerBay = 0.14;      // 倒掛楣子, read off 正面立面
		inline constexpr double RoundBracketDropPerBay = 0.08;     // 花牙子 below it
		inline constexpr double RoundBracketReachPerBay = 0.06;
		inline constexpr int32 RoundRaftersPerBay = 18;            // 每面椽數 18支
		// 尺二方磚車輞砍墁: rings of 1.2 尺 squares cut to wedges, round a centre stone. JUDGEMENT — the stone's
		// size; the page draws the rings, not the middle.
		inline constexpr double RoundPaverPerBay = 0.12;
		inline constexpr double RoundCentreStonePerBay = 0.07;
		// 垂帶踏跺 before the entry bay: 圓踏跺石 1.2 × 0.4 twice and the 圓硯窩石 at the ground, 垂帶石 1.22.
		inline constexpr double RoundStepWidthPerBay = 0.44;       // between the 垂帶, read off 台基平面
		inline constexpr double RoundStepTreadPerBay = 0.12;
		inline constexpr double StringerWidth = 1.22 / 0.7;
		// 扒梁 on the columns either side, 井口扒梁 across them under the 金桁 ring (步架平面 仰視).
		inline constexpr double RoundBeamHeight = 1.08 / 0.7;      // 扒梁, 井口扒梁 0.9 × 1.08
		inline constexpr double RoundBeamWidth = 0.9 / 0.7;

		// 圓式花甎寶頂, read off the user's drawing of it and scaled on the 寶珠's 高1.4: it caps the roof, the
		// 平甎 laid where the courses end. Courses bottom to top as (height, radius at foot, radius at top) per
		// 進深: 平甎 (高0.25), 翻荷葉, a step, the 須彌座 (lower lotus, 束腰, upper lotus), 水盤沿 (高0.2).
		struct FRoundCourse { double Height, FootRadius, TopRadius; };
		inline constexpr FRoundCourse RoundFinialCourses[] = {
			{ 0.021,  0.111, 0.111 },   // 寶頂平甎
			{ 0.0175, 0.107, 0.107 },   // 翻荷葉
			{ 0.0175, 0.081, 0.081 },   // step
			{ 0.0175, 0.070, 0.056 },   // lower lotus
			{ 0.045,  0.044, 0.044 },   // 束腰
			{ 0.0195, 0.044, 0.068 },   // upper lotus
			{ 0.0195, 0.080, 0.080 },   // 水盤沿
		};
		// 寶珠 高1.4, a barrel: (radius, height) per 進深 over the 水盤沿, foot to crown.
		inline constexpr double RoundJewel[][2] = {
			{ 0.045, 0.0 }, { 0.052, 0.014 }, { 0.058, 0.049 }, { 0.058, 0.084 }, { 0.052, 0.112 },
			{ 0.040, 0.130 }, { 0.022, 0.139 }, { 0.0, 0.14 },
		};
	}

	// 殿, 大式: 則例 卷二 玖檁歇山轉角前後廊單翹單昂斗栱 斗口二寸 (圖6 橫斷面 and 山面立面, 圖7 屋頂 and 步架
	// 平面, 台基 and 柱頭平面, 正面立面). Every figure in 尺 as printed; the generator scales them by the
	// frontage it is given, so the figure's own is 69.30 尺 between the end columns.
	namespace GrandHall
	{
		// CANON — 面闊: 梢間, 次間, 明間, 次間, 梢間; 進深: 廊深 6.60, 29.70, 廊深 6.60.
		inline constexpr double Bays[] = { 13.20, 13.20, 16.50, 13.20, 13.20 };
		inline constexpr int32 CentralBay = 2;
		inline constexpr double Frontage = 69.30;
		inline constexpr double Veranda = 6.60;
		inline constexpr double Depth = 42.90;
		inline constexpr double PlatformHeight = 4.00;       // 臺基明高
		inline constexpr double PlatformReach = 6.07;        // 台出, from the 檐柱 centre
		inline constexpr double EdgeStone = 1.72;            // 階條石 4.30 × 1.72
		inline constexpr double StepTread = 1.00;            // 踏跺 1.00 × 0.50
		inline constexpr double StepRiser = 0.50;
		inline constexpr double EaveColumn = 1.80;           // 檐柱徑
		inline constexpr double GoldColumn = 2.00;           // 金柱徑
		inline constexpr double ColumnHeight = 17.64;        // 檐柱淨高
		inline constexpr double ToPurlin = 21.00;            // 檐柱通高: floor to the 正心桁
		inline constexpr double EaveBase = 3.60;             // 檐柱頂 3.60 見方, 古鏡 0.54
		inline constexpr double EaveMirror = 0.54;
		inline constexpr double GoldBase = 4.00;             // 金柱頂 4.00 見方, 古鏡 0.60
		inline constexpr double GoldMirror = 0.60;
		inline constexpr double GableWall = 3.80;            // 山牆 裙肩寬
		inline constexpr double GableSkirt = 3.00;           // JUDGEMENT — 裙肩 height, the wall's 下鹼
		// Under the 檐柱 heads, top down: 大額枋 1.80 × 1.60, 由額墊板 0.60 × 0.30, 小額枋 1.20 × 1.00; over
		// them the 平板枋 0.60 × 0.90 (height × width).
		inline constexpr double BigLintelH = 1.80, BigLintelW = 1.60;
		inline constexpr double LintelBoardH = 0.60, LintelBoardW = 0.30;
		inline constexpr double SmallLintelH = 1.20, SmallLintelW = 1.00;
		inline constexpr double PlateH = 0.60, PlateW = 0.90;
		inline constexpr double Purlin = 1.20;               // 正心桁, 金桁 徑
		inline constexpr double OuterPurlin = 1.00;          // 挑檐桁 徑
		inline constexpr double Overhang = 8.10;             // 出檐, from the 檐柱 centre
		// JUDGEMENT — the overhang's pitch, read off 正面立面: the 飛椽 carry the eave out nearly level past the
		// 挑檐桁, its edge ~20.8 over the floor, the 斗栱 band clear beneath (at 五舉 the eave hid it).
		inline constexpr double OverhangJu = 0.3;
		// 廊步 6.60 then 下金, 上金, 脊步 4.95 each, at 五舉, 六五舉, 七五舉, 九舉.
		inline constexpr double Steps[] = { 6.60, 4.95, 4.95, 4.95 };
		inline constexpr double Ju[] = { 0.5, 0.65, 0.75, 0.9 };
		// 收山, from the eave: the hips end where 圖7's 屋頂平面 puts the 博脊, ~11.6 in (scaled on its 斗栱 dimension
		// lines), so the skirt is a quarter of the roof's height and the 山花 three quarters, as 山面立面 and 正面立面
		// draw it. 15.90 (misread at the wrong scale) raised the skirt to 38% and shrank the gable.
		inline constexpr double ShouInset = 11.60;
		inline constexpr double Rafter = 0.42;               // 檐椽, 飛椽 0.42 見方
		inline constexpr double RafterSpacing = 0.825;       // 明間椽 20 枝
		inline constexpr double BargeDepth = 2.52;           // 博縫板 0.30 × 2.52
		inline constexpr double BargeThickness = 0.30;
		inline constexpr double RidgeHeight = 2.40;          // JUDGEMENT — 正脊's courses together
		inline constexpr double RidgeWidth = 1.20;
		inline constexpr double TileRow = 0.80;              // JUDGEMENT — a 大式 筒瓦's 壟

		// 斗栱 單翹單昂 五踩, 斗口 0.20: the band from the 平板枋 to the 正心桁 (21.00 − 17.64 − 平板枋 − half the
		// 桁); 出踩 two of 3 斗口 to the 挑檐桁. 平身科 per bay as 圖7 counts them, 柱頭科 on every 檐柱, 角科
		// at the corners; the 山面 (轉角) as many to the 攢 as the 次間.
		inline constexpr double Doukou = 0.20;
		inline constexpr int32 BracketSets[] = { 4, 4, 5, 4, 4 };
		inline constexpr double BracketBand = 2.16;

		// 隔扇 in the 明間 and 次間, 檻窗 on 檻牆 in the 梢間, front and back at the 金柱 lines (正面立面).
		// JUDGEMENT — the heights, read off the elevation: 中檻 at 13.50, 檻牆 3.00.
		inline constexpr double MiddleRail = 13.50;
		inline constexpr double SillWall = 3.00;
		inline constexpr double Rail = 0.72;                 // 下檻, 風檻, 中檻 高0.72
		inline constexpr double TopRail = 0.92;              // 上檻 0.92 × 0.72
		inline constexpr double Jamb = 0.72;                 // 抱框

		// 正吻 高8.00 on the 正脊's ends; 仙人走獸 up each 戧脊 (正面立面 draws seven).
		inline constexpr double RidgeBeast = 8.00;
		inline constexpr int32 HipFigures = 7;
	}

	// 影壁, the screen facing the gate.
	namespace Screen
	{
		// JUDGEMENT — 須彌座 projection past the body each face. Compound pushes a 座山影壁 into its
		// backing wall by this; a second copy reopens the gap.
		inline constexpr double PlinthProjectionCm = 11.0;
	}

	// Compound plan: placement figures only; buildings use the sizes above.
	namespace Compound
	{
		// Frontages alias the house table so slot and building cannot disagree.
		inline constexpr double HallFrontageCm = House::MainHall.FrontageCm;
		inline constexpr double MinEarRoomFrontageCm = 230.0;

		inline constexpr double WingFrontageCm = House::SideHouse.FrontageCm;
		inline constexpr double MaxWingFrontageCm = 1500.0;
		inline constexpr double WingEarRoomFrontageCm = House::EarRoom.FrontageCm;

		// JUDGEMENT — minimum drop of an 耳房 eave below the 正房's; a wide slot otherwise lifts it to
		// within 8 cm.
		inline constexpr double EarRoomBelowHallCm = 30.0;

		// JUDGEMENT — 小天井: shallowest light well worth walling off.
		inline constexpr double MinLightWellDepthCm = 300.0;

		// JUDGEMENT — 過道: covered way to the 後院 at the plot edge, one flank.
		inline constexpr double PassageWidthCm = 240.0;
		inline constexpr double PassageBearingCm = 8.0;

		// JUDGEMENT — 後罩房 and the 後院 it closes, on a 三進 plan.
		inline constexpr double RearRowDepthCm = 400.0;
		inline constexpr double RearCourtDepthCm = 450.0;
		inline constexpr double TypicalRearCourtDepthCm = 700.0;

		// JUDGEMENT — 外院: shallow strip between gate row and 垂花門, never a second court.
		inline constexpr double OuterCourtDepthCm = 240.0;
		inline constexpr double TypicalOuterCourtDepthCm = 620.0;

		// JUDGEMENT — 內院 minimum and ordinary size (a standard one is nearer 10 m than 8).
		inline constexpr double MinCourtyardWidthCm = 800.0;
		inline constexpr double MinCourtyardDepthCm = 800.0;
		inline constexpr double CourtyardWidthCm = 1500.0;
		inline constexpr double CourtyardDepthCm = 2000.0;

		// JUDGEMENT — 抄手遊廊 clear walk; the corridor's own minimum is narrower than worth building.
		inline constexpr double CorridorWalkWidthCm = 155.0;

		// JUDGEMENT — 巽位: gate at the southeast corner, never on axis; the 門房 carries the row to the edge.
		inline constexpr double GateLodgeFrontageCm = 320.0;

		// JUDGEMENT — service yard at the 外院's far end: well, store, privy.
		inline constexpr double OuterYardWidthCm = 460.0;

		// JUDGEMENT — gap between neighbouring buildings; not on the plot boundary, where they butt.
		// UNRESOLVED: the compound tool ships 24; this 20 is the fallback when unset (tests).
		// Unifying changes plot arithmetic, so left explicit.
		inline constexpr double GapCm = 20.0;
	}

	// 院落尺度: 小型、中型、大型. Plot width decides the buildings (院落寬大, 房子也隨之高大), so a
	// size is a row of figures, not a scale factor.
	namespace Courtyard
	{
		struct FSize
		{
			double PlotWidthCm;
			double HallCentralBayCm;   // 正房明間面寬
			double HallSideBayCm;      // 次間面寬
			double EarRoomBayCm;       // 耳房面寬, per room
			int32 EarRoomsPerFlank;    // 三正兩耳 or 三正四耳
			double WingStepRunCm;      // 廂房 步架; 進深 = (檁數 - 1) × this, plus any 廊
			bool bWingVeranda;         // 廂房 外廊: in 大型's 5.5 m 進深, not 小型's
			bool bHallRearVeranda;     // 七檁前後廊, against 前廊後無廊 in a small court
			int32 WingBays;            // 廂房 面闊: 三間 at the hall's 次間
			double CourtDepthCm;       // 內院: 正房 front to cross wall
			double OuterCourtDepthCm;  // 外院: 倒座房 front to cross wall
			double RearCourtDepthCm;   // 後院: 正房 back to 後罩房 front
			bool bGrandInnerGate;      // 一殿一卷式垂花門 rather than 獨立柱
			double FrontRowDepthCm;    // 倒座房 and 大門 進深; zero keeps the house table's
			bool bGrandGate;           // 廣亮大門 rather than 如意門
			double HallDepthCm;        // 正房 進深 as 七檁, front 步架 the 前廊; zero keeps the house table's
			double EarRoomDepthCm;     // 耳房 進深, 五檁; zero keeps the house table's
		};

		// CANON 四合院建築及其構造 p.83 — 小型: 占地寬 16 m, 三正兩耳共五間, 明間 3.3, 次間 3.0,
		// 耳房 2.4, 廂房進深 3.5-4 m 無外廊, 院當寬度 7-8 m.
		inline constexpr FSize Small = { 1600.0, 330.0, 300.0, 240.0, 1, 100.0, false, false,
			3, Compound::CourtyardDepthCm, Compound::TypicalOuterCourtDepthCm, Compound::TypicalRearCourtDepthCm, false,
			0.0, false, 0.0, 0.0 };

		// JUDGEMENT — 中型 interpolated; the page gives only 小型 and 大型.
		inline constexpr FSize Medium = { 2000.0, 360.0, 315.0, 300.0, 1, 110.0, false, false,
			3, Compound::CourtyardDepthCm, Compound::TypicalOuterCourtDepthCm, Compound::TypicalRearCourtDepthCm, false,
			0.0, false, 0.0, 0.0 };

		// CANON same page — 大型: 占地寬 25 m, 三正四耳共七間, 明間 3.9-4.2, 次間 3.3, 耳房 3.0,
		// 廂房進深 5.5 m 含外廊, 院當寬度 13 m 左右. House table's 正房 is this hall.
		// JUDGEMENT — 一殿一卷 垂花門 and 廣亮大門.
		inline constexpr FSize Large = { 2500.0, 405.0, 330.0, 300.0, 2, 110.0, true, true,
			3, Compound::CourtyardDepthCm, Compound::TypicalOuterCourtDepthCm, Compound::TypicalRearCourtDepthCm, true,
			0.0, true, 0.0, 0.0 };

		// CANON Fig 2-9.1 標准的三進院落, the default, scaled by its 正房 (2.65 cm ≈ 9.8 m, 3.7 m/cm):
		// plot 22 m (just under 6 cm), not p.83's 25 m. 正房 三間 3.6/3.15, 前廊 only, 1.8 cm deep
		// (七檁, rear 步架 inside the back wall); 廂房 三間 at the 次間, 1.25 cm (4.6 m); 耳房 rooms
		// 0.85-0.9 clear; 倒座房, 後罩房 1.1 each; 內院 deep enough to square the covered walk;
		// 外院 4.9, 後院 5.1; 一殿一卷 垂花門 in the 內院; 廣亮大門.
		inline constexpr FSize Standard = { 2200.0, 360.0, 315.0, 300.0, 2, 101.0, true, false,
			3, 1330.0, 490.0, 510.0, true,
			405.0, true, 695.0, 395.0 };
	}
}
