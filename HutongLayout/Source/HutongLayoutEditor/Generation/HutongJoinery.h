#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongCanon.h"

namespace HutongGen
{
	// 裝修: the joinery filling a timber facade between its 抱框 (四合院建築及其構造 pp.104–105).
	// Every member lies across X, shows its street face at the given Y and is Depth deep in +Y; the room is +Y.
	namespace Joinery
	{
		struct FLattice
		{
			int32 Mullions = 2;
			int32 Rails = 2;
			double BarWidth = 4.0;
			bool bBars = true;
			bool bPaper = true;
		};

		// A 五抹 隔扇, Z0 to Z1, the underside of the rail under its 中絛環 at SplitZ (the 榻板's top).
		void AppendGeshan(UE::Geometry::FDynamicMesh3& Mesh, double X0, double X1, double Z0, double Z1,
			double SplitZ, double Y0, double Depth, const FLattice& Lattice);

		// 支摘窗 between two 抱框: a 間框 at the middle (PostY0..PostY1 deep), a 支窗 over a 摘窗 each side.
		void AppendWindow(UE::Geometry::FDynamicMesh3& Mesh, double X0, double X1, double Z0, double Z1,
			double Y0, double Depth, double PostY0, double PostY1, const FLattice& Lattice);

		// 橫陂: Panes fixed sashes between 短間框, over 中檻 to 上檻.
		void AppendTransom(UE::Geometry::FDynamicMesh3& Mesh, double X0, double X1, double Z0, double Z1,
			double Y0, double Depth, double PostY0, double PostY1, int32 Panes, const FLattice& Lattice);

		// The 明間: 下檻, four 隔扇 on it, and before the middle two a 簾架 holding a 風門.
		struct FDoorBay
		{
			double X0 = 0.0, X1 = 360.0;        // clear between the 抱框
			double FloorZ = 0.0;
			double SillTopZ = 12.0;              // 下檻 top
			double RailZ = 230.0;                // 中檻 underside
			double RailTopZ = 250.0;             // 中檻 top, for the 栓斗
			double SplitZ = 90.0;                // 榻板 top: the 隔扇's 中絛環下抹頭下皮
			double RailY0 = 0.5, RailY1 = 18.0;  // the 檻 through the wall
			bool bCurtain = true;
			bool bOpen = true;
			double MinDoorWidth = 90.0;
			double MinClearHeight = 110.0;
			double DoorClearHeight = 190.0;      // 吉門: what the 門口 is sized to, the bands over it giving way
		};

		// The 簾架's members, from its height and the bay; shared by the builder and the tests.
		struct FCurtain
		{
			bool bValid = false;
			double X0 = 0.0, X1 = 0.0, Stile = 0.0;   // outer 邊梃 faces
			double DoorX0 = 0.0, DoorX1 = 0.0;         // 門口
			double DoorBottomZ = 0.0, DoorTopZ = 0.0;  // 啞吧檻 top, 楣子 underside
			double LintelTopZ = 0.0, TransomBottomZ = 0.0, TransomTopZ = 0.0;
			double Y0 = 0.0, Y1 = 0.0;                 // its plane, street face to back
		};
		FCurtain LayoutCurtain(const FDoorBay& Bay);

		double LeafThickness(const FDoorBay& Bay);

		void AppendDoorBay(UE::Geometry::FDynamicMesh3& Mesh, const FDoorBay& Bay, const FLattice& Lattice);
	}
}
