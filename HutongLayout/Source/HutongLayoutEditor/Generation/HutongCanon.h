#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongRearEave.h"

//   CANON     — a source states it (named on the line above).
//   DERIVED   — arithmetic over other entries here.
//   JUDGEMENT — looks right; no source. First suspect when a building looks wrong.
// Sources:
//   則例    《工程做法則例》, 1734 (Qing, 工部).
//   營造    《清式營造則例》, 梁思成, 1934.
//   元大都  the Yuan grid as surveyed and as 《乾隆京城全圖》 (1745-50) shows it.
namespace HutongCanon
{
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

		// JUDGEMENT — 8/10 gives a 耳房 a 1.99 m column; 2.35 m is room height.
		inline constexpr double MinColumnHeightCm = 235.0;

		// JUDGEMENT — the bay count steps with the drawn width, and each step's 明間 would swing the eave.
		inline constexpr double DerivedEaveBandShare = 0.10;

		// CANON 營造 — 明間 is widest; each 次間 a fixed fraction of it.
		inline constexpr double SideBayWidthRatio = 0.85;

		// CANON 則例 — 收分: a column narrows toward its head by a hundredth of 柱高.
		inline constexpr double ColumnTaperRatio = 0.01;

		// CANON 營造 — 柱徑 1/10 to 1/11 of 柱高; the absolute fallback, at 1/10.
		inline constexpr double ColumnDiameterRatio = 0.1;
	}

	namespace Roof
	{
		// CANON 則例 — 舉 per 步架, eave first (檐步五舉).
		inline constexpr double JuThree[] = { 0.5 };
		inline constexpr double JuFive[]  = { 0.5, 0.7 };
		inline constexpr double JuSeven[] = { 0.5, 0.7, 0.9 };
		inline constexpr double JuNine[]  = { 0.5, 0.65, 0.75, 0.9 };

		// CANON 則例 — 進深 = (檁數 - 1) × 步架; purlin count decides, depth follows.

		// 收山 — CANON 營造: gable face one 檩徑 inside the 山面檐檩 centre; JUDGEMENT the centimetres.
		inline constexpr double PavilionShouInsetCm = 88.0;
		inline constexpr double HallShouInsetCm = 150.0;

		// 蠍子尾 on a 清水脊: 四合院建築及其構造 圖5-1-7.1 (scaled by its 筒瓦), 圖5-1-5.1, 5-1-7.3.
		inline constexpr double TailRiseInCourses = 2.5;
		inline constexpr double TailLeanDeg = 50.0;
		inline constexpr double TailTipPastEndCm = 4.0;
		inline constexpr double TailHookShare = 0.18;      // of the blade's length, turned out toward level
		inline constexpr double TailHookDeg = 25.0;
		inline constexpr double TailTipTaper = 0.4;       // tip section as a share of the root's
		// 盤子 under the root, in ridge widths and courses.
		inline constexpr double EndBlockWidthShare = 1.2;
		inline constexpr double EndBlockAboveCourses = 0.35;

		// CANON 清式營造則例 表十 (歇山、懸山各部, 小式) — 博風板 1⅘ 檐柱徑 wide, ¼ thick.
		inline constexpr double BargeboardInD = 1.8;
		inline constexpr double BargeboardThicknessInD = 0.25;
	}

	namespace Openings
	{
		// CANON 則例 — 額枋高 is one 柱徑; on an 11-柱徑 column its underside is at 0.909 柱高.
		inline constexpr double ArchitraveInDiameters = 1.0;




		// JUDGEMENT — crouch floor (88 cm crouched capsule plus room): real 耳房 are low.
		inline constexpr double MinClearHeightCm = 110.0;

		// JUDGEMENT — a standing capsule with margin (mannequin 34×176).
		inline constexpr double WalkerRadiusCm = 40.0;
		inline constexpr double WalkerHeightCm = 190.0;

		// JUDGEMENT — 高窗 head measured down from the eave, so the sill clears head height on a 279 cm 倒座房 and a 364 cm 正房.
		inline constexpr double RearWindowWidthCm = 70.0;
		inline constexpr double RearWindowHeightCm = 45.0;
		inline constexpr double RearWindowHeadBelowEaveCm = 45.0;

		// DERIVED — half the bay (a two-leaf door folds flat), less jamb and column-radius clearance.
		inline constexpr double DoorWidthFraction = 0.44;
	}

	// 四合院建築及其構造 pp.104–105, 圖5-4-5 and 圖5-4-6.
	namespace Joinery
	{
		// CANON 清式營造則例 表十三 檻框, in 柱徑 (D).
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
		// CANON p.104 — 橫陂 3–5 per bay between 短間框, over 隔扇 set between 金柱.
		inline constexpr int32 TransomPanes = 3;
		// JUDGEMENT — least 橫陂 worth building, cm; less and the 金枋 face comes down to the 中檻.
		inline constexpr double MinTransomCm = 15.0;
		// CANON 圖5-4-5 — 短間框 between 橫陂, share of the bay's clear width; 間框 splitting a 支摘窗 the same.
		inline constexpr double TransomPostShare = 0.06;
		inline constexpr double WindowPostShare = 0.05;
		// CANON 圖5-4-5 — 支窗 over 摘窗, split at mid-height.
		inline constexpr double WindowSplitShare = 0.5;

		// CANON 清式營造則例 表十四 隔扇, in the leaf's width W; the lower run 1.2 W (p.104, 四六分).
		// The 槅心 takes what the leaf's height leaves: the table's 3/5 is too tall for a leaf of the width.
		inline constexpr double GeshanStileOfWidth = 0.1;
		inline constexpr double GeshanDepthOfWidth = 0.15;
		inline constexpr double GeshanRimOfStile = 2.0 / 3.0;
		inline constexpr double GeshanRimDepthOfStile = 0.7;
		inline constexpr double GeshanBarOfRim = 0.8;
		inline constexpr double GeshanSmallPanelOfWidth = 0.2;
		inline constexpr double GeshanSkirtOfWidth = 0.8;
		inline constexpr double GeshanPanelThicknessOfWidth = 0.05;
		inline constexpr double GeshanLowerOfWidth = 2.0 * GeshanStileOfWidth + GeshanSmallPanelOfWidth + GeshanSkirtOfWidth;

		// CANON p.105, 圖5-4-6 — 簾架, shares of its height top down.
		inline constexpr double CurtainTopRail = 0.017;
		inline constexpr double CurtainTransom = 0.156;
		inline constexpr double CurtainMidRail = 0.017;
		inline constexpr double CurtainLintel = 0.105;
		inline constexpr double CurtainDumbSill = 0.044;
		// JUDGEMENT — p.105 sizes the 門口 by 吉門尺寸, a table out of hand; a standing walker's height stands in.
		inline constexpr double FengmenClearCm = 190.0;
		// CANON 圖5-4-6 — 邊梃 0.045 of the 簾架's width; the 門口 0.61 of it, 餘塞 either side.
		inline constexpr double CurtainStileShare = 0.045;
		inline constexpr double CurtainDoorShare = 0.61;

		inline constexpr double LeafThicknessCm = 6.0;
		inline constexpr double CurtainThicknessCm = 5.0;
		inline constexpr double PanelRecessCm = 2.5;
		// JUDGEMENT — the raised field, the double line every panel has in 圖5-4-5/5-4-6.
		inline constexpr double PanelFieldProudCm = 1.5;
		inline constexpr double PanelFieldInsetShare = 0.12;
		// CANON p.90, 圖5-3-10.1 — 海棠池子 on a 檻牆, 大枋子 about 15 cm; the rest JUDGEMENT.
		inline constexpr double SillPoolBorderCm = 15.0;
		inline constexpr double SillPoolLineCm = 3.0;
		inline constexpr double SillPoolBorderProudCm = 1.5;
		inline constexpr double SillPoolLineProudCm = 1.0;
		inline constexpr double SillPoolFieldProudCm = 0.5;
		// JUDGEMENT — 隔扇 fold back to 178°, a hair off the fixed leaves; 風門一般向外開啟 (the floor of its swing).
		inline constexpr double GeshanOpenDeg = 178.0;
		inline constexpr double FengmenOpenDeg = 95.0;
	}

	// Bottom course first; a round course bulges from Proud to Outer, a block course sets Width blocks every Pitch at Outer.
	// Scaled down together where the wall carried up above the eave is shorter than the stack.
	namespace Cornice
	{
		enum class ECourse : uint8 { Flat, Round, Blocks };
		struct FCourse { ECourse Kind; double HeightCm; double ProudCm; double OuterCm = 0.0; double WidthCm = 0.0; double PitchCm = 0.0; };

		// JUDGEMENT — a four-course 冰盤檐, 14 cm out.
		inline constexpr FCourse IceTray[] = {
			{ ECourse::Flat, 6.5, 3.5 }, { ECourse::Flat, 6.5, 7.0 }, { ECourse::Flat, 6.5, 10.5 }, { ECourse::Flat, 6.5, 14.0 } };
		// 圖5-3-9.8 三層雞素子檐; heights and reach JUDGEMENT off the photograph.
		inline constexpr FCourse Rounded[] = {
			{ ECourse::Flat, 6.0, 4.0 }, { ECourse::Round, 9.0, 4.0, 11.0 }, { ECourse::Flat, 6.0, 13.0 } };
		// 圖5-3-9.9 三層抽屜檐.
		inline constexpr FCourse Drawer[] = {
			{ ECourse::Flat, 6.0, 4.0 }, { ECourse::Blocks, 7.0, 4.0, 10.0, 9.0, 22.0 }, { ECourse::Flat, 6.0, 13.0 } };
		// 圖5-3-9.10 七層; the 連珠混 a round course (beads the length of a house cost thousands of triangles).
		inline constexpr FCourse SevenCourse[] = {
			{ ECourse::Flat, 5.0, 2.0 }, { ECourse::Round, 5.0, 2.0, 5.0 }, { ECourse::Round, 7.0, 5.0, 8.0 },
			{ ECourse::Flat, 12.0, 8.0 }, { ECourse::Flat, 5.0, 10.0 }, { ECourse::Blocks, 7.0, 10.0, 15.0, 7.0, 14.0 },
			{ ECourse::Flat, 6.0, 17.0 } };
	}

	// CANON 清式營造則例 表十二 瓦作, 小式, in 檐柱徑; the 檐牆 taken at 大式's 1½.
	namespace Masonry
	{
		inline constexpr double GableWallInD = 2.4;
		inline constexpr double SillWallInD = 1.5;
		inline constexpr double RearWallInD = 1.5;
	}

	namespace BaseCourse
	{
		// JUDGEMENT — 營造算例's 3/10 檐柱高 above 臺基 lands near this for the presets.
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

		// CANON (user, 2026-11-06) — 皇城牆: about 6 m overall, 2 m thick at the foot, 1.7 m at the top.
		inline constexpr double ImperialOverallHeightCm = 600.0;
		inline constexpr double ImperialThicknessCm = 200.0;
		inline constexpr double ImperialTopThicknessCm = 170.0;
		// JUDGEMENT — from the photographs: a deep grey 磚檐 of four courses under a yellow 筒瓦 cap.
		inline constexpr int32 ImperialCapCourses = 4;
		inline constexpr double ImperialCapOverhangCm = 24.0;
		inline constexpr double ImperialCorniceHeightCm = 44.0;
		inline constexpr double ImperialCapRidgeHeightCm = 62.0;

		// JUDGEMENT — 瓦頂 牆帽: two-slope 合瓦 cap, low 眉子 ridge, thin drip course each eave.
		inline constexpr double CapRidgeCourseHeightCm = 8.0;
		inline constexpr double CapRidgeCourseWidthCm = 10.0;
		inline constexpr double CapDripDepthCm = 4.0;

		// JUDGEMENT — inner face on the corbel's outer face (opposite normals).
		inline constexpr double CapDripProjectionCm = 3.0;

		// The body to the cornice's foot, so the whole stands at the overall height.
		inline constexpr double ImperialHeightCm = ImperialOverallHeightCm - ImperialCorniceHeightCm - ImperialCapRidgeHeightCm - CapRidgeCourseHeightCm;

		// JUDGEMENT — 墀頭 projects column base radius plus this, covering the corner 檐柱's foot.
		inline constexpr double ChitouColumnClearanceCm = 1.0;
	}

	// 城牆 NOMINAL — published round figures for the Ming-Qing inner city (高約12米, 底寬約20米, 頂寬約16米).
	namespace CityWall
	{
		inline constexpr double HeightCm = 1200.0;
		inline constexpr double BaseWidthCm = 2000.0;
		inline constexpr double TopWidthCm = 1600.0;

		// JUDGEMENT — dressed stone courses at the foot (城基 條石).
		inline constexpr double FootingHeightCm = 100.0;

		// NOMINAL — 垛口牆 on the outer edge, about a man's height and a half; 垛口 cut down from its top.
		inline constexpr double ParapetHeightCm = 190.0;
		inline constexpr double ParapetThicknessCm = 70.0;
		inline constexpr double MerlonWidthCm = 160.0;
		inline constexpr double CrenelWidthCm = 50.0;
		inline constexpr double CrenelDepthCm = 70.0;
		// JUDGEMENT — one square 射眼 low in each 垛.
		inline constexpr double LoopholeSizeCm = 24.0;

		// NOMINAL — 宇牆 (女牆) on the inner edge, plain and waist high.
		inline constexpr double InnerParapetHeightCm = 100.0;
		inline constexpr double InnerParapetThicknessCm = 50.0;

		// UNVERIFIED — 外城, 嘉靖三十二年 record: 高二丈, 垛口五尺, 基厚二丈, 頂收一丈四尺 at 32 cm the 尺.
		inline constexpr double OuterHeightCm = 640.0;
		inline constexpr double OuterBaseWidthCm = 640.0;
		inline constexpr double OuterTopWidthCm = 448.0;
		inline constexpr double OuterParapetHeightCm = 160.0;
		inline constexpr double OuterParapetThicknessCm = 50.0;

		// JUDGEMENT — 馬面 from the user's photographs of the 東南角樓.
		inline constexpr double BastionWidthCm = 1500.0;
		inline constexpr double BastionProjectionCm = 1000.0;
		inline constexpr double BastionSpacingCm = 9000.0;
		// JUDGEMENT — the outer city's, in proportion to its lower wall.
		inline constexpr double OuterBastionWidthCm = 800.0;
		inline constexpr double OuterBastionProjectionCm = 550.0;

		// JUDGEMENT — 馬道: a horse ramp, about 1:4, wide enough for a cart; landing at the top a ramp's width.
		inline constexpr double RampWidthCm = 500.0;
		inline constexpr double RampSlopeDeg = 14.0;
		inline constexpr double RampParapetHeightCm = 100.0;
		inline constexpr double RampParapetThicknessCm = 40.0;
		inline constexpr double RampGateHeightCm = 380.0;
		inline constexpr double RampGateOpeningWidthCm = 220.0;
		inline constexpr double RampGateOpeningHeightCm = 260.0;
		inline constexpr double RampGateThicknessCm = 60.0;

		// 城角 platforms, square. CANON (user, 2026-11-06, from the aerial photographs): the Forbidden City's 27 m.
		// JUDGEMENT — the inner and outer cities' in proportion until figures come.
		inline constexpr double CornerPlatformCm = 4000.0;
		inline constexpr double OuterCornerPlatformCm = 1300.0;
		inline constexpr double PalaceCornerPlatformCm = 2700.0;

		// CANON (user, 2026-11-06) — 紫禁城城牆: 高 7.9 m, 底寬 8.62 m, 頂寬 6.66 m; the height read as the walk's.
		inline constexpr double PalaceHeightCm = 790.0;
		inline constexpr double PalaceBaseWidthCm = 862.0;
		inline constexpr double PalaceTopWidthCm = 666.0;
		// JUDGEMENT — the outer city's parapet and bastion sizes, the palace wall being of its scale.
		inline constexpr double PalaceParapetHeightCm = OuterParapetHeightCm;
		inline constexpr double PalaceParapetThicknessCm = OuterParapetThicknessCm;
		inline constexpr double PalaceBastionWidthCm = OuterBastionWidthCm;
		inline constexpr double PalaceBastionProjectionCm = OuterBastionProjectionCm;
		// JUDGEMENT — the user's photograph (2026-11-06): each 垛 two equal steps, an upside-down T — a wide block,
		// a block half as wide centred on it — the crenels narrow and down to the parapet.
		inline constexpr double PalaceMerlonWidthCm = 120.0;
		inline constexpr double PalaceMerlonTopWidthCm = 60.0;
		inline constexpr double PalaceMerlonStepCm = 25.0;
		inline constexpr double PalaceCrenelWidthCm = 30.0;
		inline constexpr double PalaceCrenelDepthCm = 2.0 * PalaceMerlonStepCm;
	}

	// 箭樓 — UNVERIFIED, 德勝門's as commonly published (user, 2026-11-06: "use them, marked unverified"): tower seven
	// bays, 32 m wide, 12 m deep; rear 抱廈 five bays, 23.5 m; 82 箭窗; about 32 m overall on a 12 m 城台.
	// JUDGEMENT — the rest from the user's photographs of 德勝門 and 安定門: three rows of 箭窗 under the 腰檐, one
	// above, twelve to a row on the front and four on each side; the 城台 a little ahead of the barbican's face.
	namespace ArrowTower
	{
		inline constexpr double TowerWidthCm = 3200.0;
		inline constexpr double TowerDepthCm = 1200.0;
		inline constexpr double LowerHeightCm = 1050.0;
		inline constexpr double UpperHeightCm = 420.0;
		inline constexpr double UpperInsetCm = 60.0;
		inline constexpr double PlatformMarginCm = 150.0;
		inline constexpr double PlatformRearCm = 2550.0;
		// JUDGEMENT — the user's recent photograph of 德勝門: the platform stands well ahead of the barbican walls.
		inline constexpr double ProjectionCm = 800.0;
		inline constexpr int32 FrontWindows = 12;
		inline constexpr int32 SideWindows = 4;
		inline constexpr int32 LowerRows = 3;
		inline constexpr double WindowSizeCm = 120.0;
		inline constexpr double WindowRecessCm = 40.0;
		inline constexpr double LowerEaveOverhangCm = 180.0;
		inline constexpr double UpperEaveOverhangCm = 220.0;
		inline constexpr double AnnexWidthCm = 2350.0;
		inline constexpr double AnnexDepthCm = 600.0;
		inline constexpr double AnnexHeightCm = 650.0;
	}

	// 城門 — JUDGEMENT, the barbican's side gate under its 閘樓 from the user's engraving of 安定門 and photographs:
	// a gate piece a little longer than the tower, a passage under a round arch, a small tower of two rows of 箭窗.
	namespace CityGate
	{
		inline constexpr double LengthCm = 1800.0;
		inline constexpr double PassageWidthCm = 600.0;
		inline constexpr double PassageHeightCm = 850.0;
		inline constexpr double ArchRingCm = 60.0;
		inline constexpr double TowerWidthCm = 1400.0;
		inline constexpr double TowerDepthCm = 700.0;
		inline constexpr double TowerHeightCm = 650.0;
		inline constexpr int32 TowerFrontWindows = 6;
		inline constexpr int32 TowerSideWindows = 2;
		inline constexpr int32 TowerRows = 2;
		inline constexpr double TowerWindowCm = 90.0;
		inline constexpr double TowerOverhangCm = 160.0;

		// 城樓 — UNVERIFIED, 安定門's as commonly published: five bays by three, 31.4 × 16.4 m, about 36 m overall on
		// its 城台. JUDGEMENT — the rest off the user's photograph of 安定門 (andingmen2): a red-walled hall in a
		// 周圍廊 under the first eave, a 平座 with its railing over it, the upper hall under 重檐歇山.
		inline constexpr double GateTowerWidthCm = 3140.0;
		inline constexpr double GateTowerDepthCm = 1640.0;
		inline constexpr int32 GateTowerBays = 5;
		inline constexpr int32 GateTowerSideBays = 3;
		inline constexpr double GateTowerPlatformMarginCm = 200.0;
		inline constexpr double PlinthCm = 45.0;
		inline constexpr double LowerColumnCm = 800.0;
		inline constexpr double ColumnDiameterCm = 60.0;
		inline constexpr double VerandaCm = 280.0;
		inline constexpr double LowerEaveOverhangCm = 200.0;
		inline constexpr double BalconyRiseCm = 140.0;
		inline constexpr double UpperInsetCm = 180.0;
		inline constexpr double BalconyCm = 150.0;
		inline constexpr double RailingCm = 100.0;
		inline constexpr double UpperWallCm = 650.0;
		inline constexpr double MiddleEaveOverhangCm = 220.0;
		inline constexpr double ClerestoryCm = 220.0;
		inline constexpr double ClerestoryInsetCm = 100.0;
		inline constexpr double TopEaveOverhangCm = 300.0;
	}

	// 角箭樓 — UNVERIFIED, 東南角樓 as commonly published: an L of two arms, 144 箭窗 in four rows (14 a face, 4 on
	// each end); about 29 m from the ground to the ridge. JUDGEMENT — the arms the 箭樓's depth and storeys.
	namespace CornerTower
	{
		inline constexpr double DepthCm = 1200.0;
		inline constexpr int32 FrontWindowsPerFace = 14;
		inline constexpr int32 EndWindows = 4;
		// The tower's walls rise from the platform's top edge.
		inline constexpr double MarginCm = 20.0;
	}

	// 德勝門 as a whole — JUDGEMENT: the barbican's proportions off the 乾隆京城全圖 sheet the user overlaid (2026-11-06),
	// longer north to south than across; the 閘樓 in the east side wall; a stretch of city wall either side.
	namespace GateComplex
	{
		inline constexpr double BarbicanWidthCm = 12500.0;
		inline constexpr double BarbicanDepthCm = 13500.0;
		inline constexpr double WallStubCm = 4000.0;
		inline constexpr double MinFrontWallCm = 500.0;
	}

	// 紫禁城角樓 — UNVERIFIED, as commonly quoted: about 27.5 m from the ground, three eaves (三重檐), a 十字脊 top.
	// JUDGEMENT — the plan and storeys off the user's aerial photographs (2026-11-06): a square core with a 抱廈 on
	// each side, the cross centred on the corner platform; heights chosen so the whole stands 27.5 m on a 7.9 m wall.
	namespace PalaceCornerTower
	{
		inline constexpr double CoreCm = 1300.0;
		inline constexpr double ArmWidthCm = 900.0;
		inline constexpr double ArmProjectionCm = 350.0;
		inline constexpr double PlinthCm = 60.0;
		inline constexpr double LowerStoreyCm = 480.0;
		inline constexpr double ArmUpperCm = 420.0;
		inline constexpr double CoreAboveArmsCm = 400.0;
		inline constexpr double UpperInsetCm = 60.0;
		inline constexpr double LowerEaveOverhangCm = 150.0;
		inline constexpr double ArmEaveOverhangCm = 150.0;
		inline constexpr double TopEaveOverhangCm = 200.0;
		inline constexpr double FinialRadiusCm = 45.0;
		inline constexpr double FinialHeightCm = 140.0;
		inline constexpr double MarginCm = 100.0;
	}

	namespace Gate
	{
		// 面闊, 進深, eave (cm); all zero = unconstrained.
		struct FSizeBand
		{
			double FrontageMin = 0.0, FrontageMax = 0.0;
			double DepthMin = 0.0, DepthMax = 0.0;
			double EaveMin = 0.0, EaveMax = 0.0;
		};

		// CANON 營造 — 廣亮大門 doors on the 中柱, half the depth back.
		inline constexpr double GuangliangDoorPlane = 0.5;

		// CANON 四合院建築及其構造 圖5-1-3 — 金柱大門: one 步架 of four behind the 檐柱 (plan 0.22, section 0.25).
		inline constexpr double JinzhuDoorPlane = 0.25;

		// CANON 營造 — 蠻子門 and 如意門 doors on the 檐柱 line, flush.
		inline constexpr double FlushDoorPlane = 0.0;

		// CANON 營造 — rank: 廣亮 and 金柱 五檁 with 筒瓦, 如意門 三檁. JUDGEMENT — the centimetres.
		inline constexpr FSizeBand GuangliangSize = { 330.0, 420.0, 400.0, 520.0, 320.0, 400.0 };
		inline constexpr FSizeBand JinzhuSize     = { 320.0, 400.0, 350.0, 460.0, 300.0, 375.0 };
		inline constexpr FSizeBand ManziSize      = { 300.0, 380.0, 300.0, 400.0, 285.0, 350.0 };
		inline constexpr FSizeBand RuyiSize       = { 260.0, 350.0, 250.0, 350.0, 265.0, 330.0 };

		// JUDGEMENT — 垂花門. Depth is the 擔梁's run, columns at mid-run: one 步架 deep, eaves past the frame.
		inline constexpr FSizeBand InnerGateSize  = { 280.0, 400.0, 105.0, 215.0, 265.0, 365.0 };

		// JUDGEMENT — 一殿一卷式垂花門 (Fig 2-9.1); depth middle 406 gives 6.26 m step foot to step foot (1.7 cm on the page).
		inline constexpr FSizeBand InnerGateGrandSize = { 320.0, 400.0, 380.0, 432.0, 300.0, 370.0 };

		// JUDGEMENT — 麻葉抱頭梁 cantilever; front 殿 roof's share of depth before the 天溝.
		inline constexpr double InnerGateCantileverCm = 75.0;
		inline constexpr double InnerGateFrontRoofShare = 0.56;

		// JUDGEMENT — share of a house's 上檐出 each rank carries; the ordering is the claim.
		inline constexpr double GuangliangEaveShare = 1.0;
		inline constexpr double JinzhuEaveShare = 0.95;
		inline constexpr double ManziEaveShare = 0.8;
		inline constexpr double RuyiEaveShare = 0.5;

		// CANON 四合院建築及其構造 圖5-1-6 — 如意門 doorway, share of the clear span between the piers.
		inline constexpr double RuyiDoorShare = 0.37;
		// CANON p.67 text with 圖5-1-5, 圖5-1-6 — 門頭 courses: heights shares of the frieze, Proud of DoorHeadProjection; JUDGEMENT within 圖5-1-6's bands.
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

		// CANON 四合院建築及其構造 圖5-1-4.2 — 蠻子門 檻框; 腰枋 shares of the leaf height down from the 中檻.
		inline constexpr double UpperSillShare = 0.4;
		inline constexpr double YusaiRailUpper[2] = { 0.54, 0.58 };
		inline constexpr double YusaiRailLower[2] = { 0.70, 0.74 };
		inline constexpr double PanelRecessCm = 3.0;
		inline constexpr int32 RuyiDoorPegs = 2;

		// CANON 圖5-1-4.2 plan and section — 蠻子門 五檁中柱式, 廊心 and 門墩.
		inline constexpr double ColumnProudShare = 0.25;
		inline constexpr double LangxinColumnMargin = 0.5;
		inline constexpr double LangxinTopGap = 0.086;
		inline constexpr double LangxinBottomGap = 0.05;
		inline constexpr double LangxinBorderCm = 5.0;
		inline constexpr double LangxinBorderProudCm = 2.0;
		inline constexpr double LangxinFieldProudCm = 1.0;
		inline constexpr double ManziDoorStoneCm = 44.0;

		// CANON 圖5-1-3 — 金柱大門 雀替, sized by 清式營造則例 表四.
		inline constexpr double QuetiReachShare = 0.25;
		inline constexpr double QuetiHeightInD = 1.25;
		inline constexpr double QuetiThicknessInD = 2.0 / 3.0;
		inline constexpr double QuetiSag = 1.4;

		// CANON 圖5-1-2 — 反八字影壁: shares of the frontage (plan) and of the gate's roof base (elevation).
		inline constexpr double WingAngleDeg = 45.0;
		inline constexpr double WingLengthShare = 0.68;
		inline constexpr double WingThicknessShare = 0.06;
		inline constexpr double WingMinThicknessCm = 20.0;
		inline constexpr double WingBaseShare = 0.27;
		inline constexpr double WingFieldTopShare = 0.55;
		inline constexpr double WingBodyShare = 0.62;
		inline constexpr double WingRidgeShare = 0.77;
		// Panel centred on the wing (the user's call); the pier narrowed to stay clear of it.
		inline constexpr double WingPostInner[2] = { 0.245, 0.305 };
		inline constexpr double WingPostOuter[2] = { 0.695, 0.755 };
		inline constexpr double WingPierFrom = 0.86;
		// JUDGEMENT — the cornice and relief.
		inline constexpr double WingCorniceShare = 0.045;
		inline constexpr double WingCorbelStepCm = 3.0;
		inline constexpr double WingPostProudCm = 2.0;
		inline constexpr double WingFieldProudCm = 1.0;

		// JUDGEMENT — 大門 ridge above its row's.
		inline constexpr double RidgeAboveRowCm = 35.0;
	}

	namespace Stone
	{
		// JUDGEMENT — 方門墩 at knee height, the 50-60 cm band in photographs.
		inline constexpr double BlockHeightCm = 54.0;

		// JUDGEMENT — a 抱鼓石 with plinth stands 90-110; a gate's a little under.
		inline constexpr double DrumHeightCm = 82.0;

		// JUDGEMENT — disc share of the whole stone, so the drum overhangs its plinth.
		inline constexpr double DrumFraction = 0.62;

		// JUDGEMENT — projection ahead of the doorway, about its height.
		inline constexpr double ProjectionCm = 32.0;

		// JUDGEMENT — inner gate: a small 門軸 bearing for the 垂花門's 板門.
		inline constexpr double InnerGateHeightCm = 40.0;
		inline constexpr double InnerGateProjectionCm = 20.0;
	}

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

		// JUDGEMENT — off-band is a note: the Outer City is irregular, the Inner City has 500 years of encroachment.
		inline constexpr double InBandToleranceCm = 120.0;
	}

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

		// JUDGEMENT unless marked — sizes read off plans.

		// CANON 四合院建築及其構造 p.84 — 正房 七檁前後廊: 進深 ≥ 7 m, 明間 3.9-4.2 m, 次間 ~3.3 m, 檐柱 3.3-3.5 m.
		// DERIVED — 1060 at 0.82 → 明間 402, 次間 329; 6 × 117 → 702; 0.84 × 明間 → 337 column
		// (8/10 gives 321, under the source's range).
		inline constexpr FHouse MainHall =
			{ 399.0, 320.0, 420.0, true,  EHutongRearEave::Lane, 1060.0, EHutongPurlins::Seven, 117.0, true,
			  true, 0.82, 0.84 };

		// JUDGEMENT — 五間正房: 402 + 4 × 330, matching MainHall.
		inline constexpr FHouse MainHallFiveBay =
			{ 399.0, 320.0, 420.0, true,  EHutongRearEave::Lane, 1720.0, EHutongPurlins::Seven, 117.0, true,
			  true, 0.82, 0.84 };

		// CANON p.85, 圖5-3-2 — 前廊後無廊; JUDGEMENT — 150 步架, 600 進深 scaled off the figure, 1040 frontage (面寬要酌減).
		inline constexpr FHouse MainHallSmall =
			{ 365.0, 300.0, 380.0, true,  EHutongRearEave::Lane, 1040.0, EHutongPurlins::Five, 150.0, true };

		// CANON p.85, 圖5-3-3 (left), p.83 — 廂房 大中型: 五檁, 前廊 the first 步架, 進深 5.5 m 含外廊.
		inline constexpr FHouse SideHouse =
			{ 340.0, 280.0, 340.0, true,  EHutongRearEave::Lane,  900.0, EHutongPurlins::Five, 137.5, false };
		// CANON p.85, 圖5-3-3 (right), p.83 — 小型 廂房: 五檁無廊, 3.5–4 m 進深.
		inline constexpr FHouse SideHouseSmall =
			{ 340.0, 280.0, 340.0, false, EHutongRearEave::Lane,  900.0, EHutongPurlins::Five, 100.0, false };

		inline constexpr FHouse FrontRow =
			{ 322.0, 250.0, 300.0, false, EHutongRearEave::Lane, 1300.0, EHutongPurlins::Five,  90.0, true };

		inline constexpr FHouse RearRow =
			{ 328.0, 260.0, 320.0, false, EHutongRearEave::Courtyard, 1300.0, EHutongPurlins::Five, 100.0, true };

		inline constexpr FHouse RearRowBlankBack =
			{ 328.0, 260.0, 320.0, false, EHutongRearEave::Lane, 1300.0, EHutongPurlins::Five, 100.0, false };

		// CANON p.83 — 3.0 m, two per flank (三正四耳) in a 大型 court; 2.4 m, one, in a 小型 (compound size table).
		inline constexpr FHouse EarRoom =
			{ 316.0, 240.0, 320.0, false, EHutongRearEave::Lane,  600.0, EHutongPurlins::Five,  85.0, false };
	}

	// In 檐柱 柱徑 (D). CANON 清式營造則例 表二–表六 (小式) where named; 寸 of the 營造尺 (32 cm).
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
		// 表二 算梁通例: lowest beam D + 2 寸 thick, 1.2 of that high; each layer up ⅚ high, ⅘ thick.
		inline constexpr double MainBeamExtraCm = 2.0 * CunCm;
		inline constexpr double MainBeamDepthOfWidth = 1.2;
		// 卷棚 (圖18, 則例 卷十二): 四架梁 1.17 × 0.90.
		inline constexpr double RollBeamDepthOfWidth = 1.3;
		inline constexpr double UpperBeamDepth = 5.0 / 6.0;
		inline constexpr double UpperBeamWidth = 4.0 / 5.0;
		// 頂梁 (a 卷棚's 月梁, 0.97 × 0.70 on 圖18): 2 寸 less than the beam below, each way.
		inline constexpr double TopBeamLessCm = 2.0 * CunCm;
		// Under this much clear height (柱徑) the 月梁 sits on one 柁墩 (the 抄手遊廊 section).
		inline constexpr double TuodunBelow = 1.5;
		// 表五: 瓜柱 1 × 1 D.
		inline constexpr double StrutSection = 1.0;
		inline constexpr double StrutHeadWidth = 0.6;          // of the section, where it meets the 檁
		inline constexpr double StrutHeadBevel = 0.35;         // of the section, the bevel's own height
		inline constexpr double StrutIntoPurlin = 0.33;        // of 檁徑, how far the head rises into it

		// 表五: 角背; flat top, angled edge down (圖5-3-1).
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

		// 出頭 (圖5-3-1).
		inline constexpr double TenonHeight = 0.5;             // of the member's own height
		inline constexpr double TenonWidth = 0.75;             // of its thickness
		inline constexpr double TenonReach = 0.9;              // 柱徑, past the column's axis
		// All nine frontage members end on one line past the last column (圖5-3-1).
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

		// 如意踏跺, read off 台基平面.
		inline constexpr double StepWidthPerBay = 0.42;
		inline constexpr double StepReachPerBay = 0.11;        // from the platform edge
		inline constexpr double RuyiWidthPerBay = 0.62;
		inline constexpr double RuyiReachPerBay = 0.21;

		// 方式花甎寶頂 一座 3.2 on a 寶頂博脊 0.5; height and side per 面闊, course heights read off the elevation.
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

		// 徹上明造 (步架平面).
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

		// 則例 卷二十三 陸柱圓亭: "Bay" is the 進深, 10.00 across the column centres.
		inline constexpr int32 RoundColumns = 6;
		inline constexpr double RoundPlatformReachPerBay = 0.17;   // 台基 circle, read off 台基平面 and 橫斷面
		inline constexpr double RoundBenchHeightPerBay = 0.15;     // 坐凳欄杆 高1.5
		inline constexpr double RoundEntryBenchShare = 0.2;        // of the entry bay, from each column (正面立面)
		inline constexpr double RoundFriezeDropPerBay = 0.14;      // 倒掛楣子, read off 正面立面
		inline constexpr double RoundBracketDropPerBay = 0.08;     // 花牙子 below it
		inline constexpr double RoundBracketReachPerBay = 0.06;
		inline constexpr int32 RoundRaftersPerBay = 18;            // 每面椽數 18支
		// 尺二方磚車輞砍墁; JUDGEMENT — the centre stone's size.
		inline constexpr double RoundPaverPerBay = 0.12;
		inline constexpr double RoundCentreStonePerBay = 0.07;
		// 垂帶踏跺 before the entry bay: 圓踏跺石 1.2 × 0.4 twice and the 圓硯窩石 at the ground, 垂帶石 1.22.
		inline constexpr double RoundStepWidthPerBay = 0.44;       // between the 垂帶, read off 台基平面
		inline constexpr double RoundStepTreadPerBay = 0.12;
		inline constexpr double StringerWidth = 1.22 / 0.7;
		// 扒梁 on the columns either side, 井口扒梁 across them under the 金桁 ring (步架平面 仰視).
		inline constexpr double RoundBeamHeight = 1.08 / 0.7;      // 扒梁, 井口扒梁 0.9 × 1.08
		inline constexpr double RoundBeamWidth = 0.9 / 0.7;

		// 圓式花甎寶頂, read off the user's drawing, scaled on the 寶珠's 高1.4: (height, foot radius, top radius) per 進深.
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

	// 則例 卷二 玖檁歇山轉角前後廊單翹單昂斗栱 斗口二寸 (圖6, 圖7); every figure in 尺 as printed, scaled by frontage.
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
		// Lintels and 平板枋 as height × width.
		inline constexpr double BigLintelH = 1.80, BigLintelW = 1.60;
		inline constexpr double LintelBoardH = 0.60, LintelBoardW = 0.30;
		inline constexpr double SmallLintelH = 1.20, SmallLintelW = 1.00;
		inline constexpr double PlateH = 0.60, PlateW = 0.90;
		inline constexpr double Purlin = 1.20;               // 正心桁, 金桁 徑
		inline constexpr double OuterPurlin = 1.00;          // 挑檐桁 徑
		inline constexpr double Overhang = 8.10;             // 出檐, from the 檐柱 centre
		// JUDGEMENT — read off 正面立面: the eave nearly level, the 斗栱 band clear beneath.
		inline constexpr double OverhangJu = 0.3;
		// 廊步 6.60 then 下金, 上金, 脊步 4.95 each, at 五舉, 六五舉, 七五舉, 九舉.
		inline constexpr double Steps[] = { 6.60, 4.95, 4.95, 4.95 };
		inline constexpr double Ju[] = { 0.5, 0.65, 0.75, 0.9 };
		// 收山 from the eave, where 圖7's 屋頂平面 puts the 博脊 (scaled on its 斗栱 dimension lines).
		inline constexpr double ShouInset = 11.60;
		inline constexpr double Rafter = 0.42;               // 檐椽, 飛椽 0.42 見方
		inline constexpr double RafterSpacing = 0.825;       // 明間椽 20 枝
		inline constexpr double BargeDepth = 2.52;           // 博縫板 0.30 × 2.52
		inline constexpr double BargeThickness = 0.30;
		inline constexpr double RidgeHeight = 2.40;          // JUDGEMENT — 正脊's courses together
		inline constexpr double RidgeWidth = 1.20;
		inline constexpr double TileRow = 0.80;              // JUDGEMENT — a 大式 筒瓦's 壟

		// 斗栱 單翹單昂 五踩, 斗口 0.20; 平身科 per bay as 圖7 counts them.
		inline constexpr double Doukou = 0.20;
		inline constexpr int32 BracketSets[] = { 4, 4, 5, 4, 4 };
		inline constexpr double BracketBand = 2.16;

		// JUDGEMENT — read off 正面立面.
		inline constexpr double MiddleRail = 13.50;
		inline constexpr double SillWall = 3.00;
		inline constexpr double Rail = 0.72;                 // 下檻, 風檻, 中檻 高0.72
		inline constexpr double TopRail = 0.92;              // 上檻 0.92 × 0.72
		inline constexpr double Jamb = 0.72;                 // 抱框

		// 正吻 高8.00 on the 正脊's ends; 仙人走獸 up each 戧脊 (正面立面 draws seven).
		inline constexpr double RidgeBeast = 8.00;
		inline constexpr int32 HipFigures = 7;
	}

	namespace Screen
	{
		// JUDGEMENT — the compound pushes a 座山影壁 into its backing wall by this.
		inline constexpr double PlinthProjectionCm = 11.0;
	}

	namespace Compound
	{
		// Frontages alias the house table so slot and building cannot disagree.
		inline constexpr double HallFrontageCm = House::MainHall.FrontageCm;
		inline constexpr double MinEarRoomFrontageCm = 230.0;

		inline constexpr double WingFrontageCm = House::SideHouse.FrontageCm;
		inline constexpr double MaxWingFrontageCm = 1500.0;
		inline constexpr double WingEarRoomFrontageCm = House::EarRoom.FrontageCm;

		// JUDGEMENT — a wide slot otherwise lifts the 耳房 eave to within 8 cm of the 正房's.
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

		// JUDGEMENT — 外院: shallow strip between gate row and 垂花門.
		inline constexpr double OuterCourtDepthCm = 240.0;
		inline constexpr double TypicalOuterCourtDepthCm = 620.0;

		// JUDGEMENT — 內院 minimum and ordinary size (a standard one is nearer 10 m than 8).
		inline constexpr double MinCourtyardWidthCm = 800.0;
		inline constexpr double MinCourtyardDepthCm = 800.0;
		inline constexpr double CourtyardWidthCm = 1500.0;
		inline constexpr double CourtyardDepthCm = 2000.0;

		// JUDGEMENT — 抄手遊廊 clear walk; the corridor's own minimum is narrower than worth building.
		inline constexpr double CorridorWalkWidthCm = 155.0;

		// JUDGEMENT — 巽位: gate at the southeast corner; the 門房 carries the row to the edge.
		inline constexpr double GateLodgeFrontageCm = 320.0;

		// JUDGEMENT — service yard at the 外院's far end: well, store, privy.
		inline constexpr double OuterYardWidthCm = 460.0;

		// JUDGEMENT — the compound tool ships 24; this is the fallback when unset (tests).
		inline constexpr double GapCm = 20.0;
	}

	// 院落寬大, 房子也隨之高大: a size is a row of figures.
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
			bool bWingVeranda;         // 廂房 外廊: in 大型's 5.5 m 進深
			bool bHallRearVeranda;     // 七檁前後廊
			int32 WingBays;            // 廂房 面闊: 三間 at the hall's 次間
			double CourtDepthCm;       // 內院: 正房 front to cross wall
			double OuterCourtDepthCm;  // 外院: 倒座房 front to cross wall
			double RearCourtDepthCm;   // 後院: 正房 back to 後罩房 front
			bool bGrandInnerGate;      // 一殿一卷式垂花門
			double FrontRowDepthCm;    // 倒座房 and 大門 進深; zero keeps the house table's
			bool bGrandGate;           // 廣亮大門
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
		inline constexpr FSize Large = { 2500.0, 405.0, 330.0, 300.0, 2, 110.0, true, true,
			3, Compound::CourtyardDepthCm, Compound::TypicalOuterCourtDepthCm, Compound::TypicalRearCourtDepthCm, true,
			0.0, true, 0.0, 0.0 };

		// CANON Fig 2-9.1 標准的三進院落, scaled by its 正房 (2.65 cm ≈ 9.8 m, 3.7 m/cm).
		inline constexpr FSize Standard = { 2200.0, 360.0, 315.0, 300.0, 2, 101.0, true, false,
			3, 1330.0, 490.0, 510.0, true,
			405.0, true, 695.0, 395.0 };
	}

	// The audit's bands per court role; a one-number figure's band is JUDGEMENT.
	namespace Audit
	{
		enum class ESource : uint8 { Canon, Derived, Judgement };

		struct FBand
		{
			double Min = 0.0, Max = 0.0;
			ESource Source = ESource::Judgement;
			const TCHAR* Cite = TEXT("");
			bool IsSet() const { return Max > 0.0; }
		};

		struct FRole
		{
			FBand CentralBay;      // 明間面闊
			FBand SideBay;         // 次間 (and every bay of a type that is all 次間)
			FBand Bays;            // bay count (間)
			FBand Depth;           // 進深, column line to column line
			FBand DepthLarge;      // 進深 of the larger form, where the source gives it apart (正房 七檁前後廊, 廂房 含外廊)
			FBand StepRun;         // 步架
		};

		// CANON 小式 rule; tolerance JUDGEMENT, the houses' drawn-eave band.
		inline constexpr double ColumnRuleTolerance = Module::DerivedEaveBandShare;

		// CANON 四合院建築及其構造 p.83 — 明間 3.3 (小型) to 3.9-4.2 (大型), 次間 3.0 to 3.3, 三正兩耳/三正四耳.
		// CANON p.84 — 七檁前後廊 進深 ≥ 7 m (upper bound JUDGEMENT); 前廊後無廊 6.0 scaled off 圖5-3-2 (JUDGEMENT ±0.5).
		// DERIVED — 步架 from House::MainHall / MainHallSmall (117, 150).
		inline constexpr FRole MainHall = {
			{ 330.0, 420.0, ESource::Canon, TEXT("p.83") },
			{ 300.0, 330.0, ESource::Canon, TEXT("p.83") },
			{ 3.0, 5.0, ESource::Canon, TEXT("p.83") },
			{ 550.0, 650.0, ESource::Judgement, TEXT("圖5-3-2") },
			{ 700.0, 800.0, ESource::Canon, TEXT("p.84") },
			{ 110.0, 160.0, ESource::Derived, TEXT("house table") } };

		// CANON p.83 — 廂房進深 3.5-4 m 無外廊, 5.5 m 含外廊 (band ±0.3 JUDGEMENT); 明間 at the hall's 次間
		// (JUDGEMENT, House::SideHouse 280-340), 次間 the module's share of it (DERIVED), 三間 (JUDGEMENT).
		inline constexpr FRole SideHouse = {
			{ 280.0, 340.0, ESource::Judgement, TEXT("house table") },
			{ 280.0 * Module::SideBayWidthRatio, 340.0 * Module::SideBayWidthRatio, ESource::Derived, TEXT("0.85 明間") },
			{ 3.0, 3.0, ESource::Judgement, TEXT("house table") },
			{ 350.0, 400.0, ESource::Canon, TEXT("p.83") },
			{ 520.0, 580.0, ESource::Canon, TEXT("p.83") },
			{ 90.0, 140.0, ESource::Derived, TEXT("house table") } };

		// CANON p.83 — 耳房 2.4 m (小型) to 3.0 (大型) a room, one or two per flank. Depth DERIVED: 五檁 at the
		// house table's 85 (340) to Fig 2-9.1's measured 395.
		inline constexpr FRole EarRoom = {
			{ 240.0, 300.0, ESource::Canon, TEXT("p.83") },
			{ 240.0, 300.0, ESource::Canon, TEXT("p.83") },
			{ 1.0, 2.0, ESource::Canon, TEXT("p.83") },
			{ 340.0, 400.0, ESource::Derived, TEXT("Fig 2-9.1") },
			{},
			{ 80.0, 100.0, ESource::Derived, TEXT("house table") } };

		// 倒座房: 明間 JUDGEMENT (House::FrontRow 250-300), 次間 its share (DERIVED); depth DERIVED, 五檁 at 90 (360)
		// to Fig 2-9.1's 405.
		inline constexpr FRole FrontRow = {
			{ 250.0, 300.0, ESource::Judgement, TEXT("house table") },
			{ 250.0 * Module::SideBayWidthRatio, 300.0 * Module::SideBayWidthRatio, ESource::Derived, TEXT("0.85 明間") },
			{},
			{ 360.0, 420.0, ESource::Derived, TEXT("Fig 2-9.1") },
			{},
			{ 85.0, 110.0, ESource::Derived, TEXT("house table") } };

		// 後罩房: 明間 JUDGEMENT (House::RearRow 260-320), 次間 its share (DERIVED); depth DERIVED, 五檁 at 100 (400)
		// and Fig 2-9.1's 405.
		inline constexpr FRole RearRow = {
			{ 260.0, 320.0, ESource::Judgement, TEXT("house table") },
			{ 260.0 * Module::SideBayWidthRatio, 320.0 * Module::SideBayWidthRatio, ESource::Derived, TEXT("0.85 明間") },
			{},
			{ 380.0, 430.0, ESource::Derived, TEXT("Fig 2-9.1") },
			{},
			{ 90.0, 115.0, ESource::Derived, TEXT("house table") } };
	}
}
