#include "Generation/GateComplexLayout.h"
#include "Tools/HutongWallChain.h"

namespace HutongGen
{
	namespace
	{
		double GateComplexCornerSize(const FHutongCityWallParams& W);
	}

	void LayOutGateComplex(const FHutongGateComplexParams& P, TArray<FGateComplexPiece>& Out)
	{
		using namespace HutongCanon::GateComplex;
		Out.Reset();
		FHutongCityWallParams W;
		W.ChooseRank(P.Rank);
		W.bBastions = false;
		W.bRamp = false;
		W.bLoopholes = P.Rank != EHutongCityWallRank::Palace;
		const double B = W.GetBaseWidth(), Bat = W.GetBatter();

		auto AddWall = [&](const FVector2D& From, const FVector2D& To, const FVector2D& Outside)
		{
			if (FVector2D::Distance(From, To) < 50.0) return;
			TArray<HutongWallChain::FSegment> Segs;
			if (!HutongWallChain::Build({ From, To }, B, 0.5 * B, {}, {}, Segs) || Segs.Num() != 1) return;
			FGateComplexPiece Piece;
			Piece.Kind = FGateComplexPiece::EKind::Wall;
			Piece.Origin = Segs[0].Origin;
			Piece.YawDeg = Segs[0].YawDeg;
			Piece.Length = Segs[0].Length;
			Piece.Size = FVector2D(Segs[0].Length, B);
			Piece.Skew = Segs[0].Skew;
			Piece.Wall = W;
			// A leg's local +Y is to the left of its run.
			const FVector2D Dir = (To - From).GetSafeNormal(), Left(-Dir.Y, Dir.X);
			Piece.OuterSide = (Outside - From).Dot(Left) > 0.0 ? EHutongBaySide::PlusY : EHutongBaySide::MinusY;
			Out.Add(Piece);
		};

		// The main gate with its 城樓, its outer face on the wall's.
		FGateComplexPiece Main;
		Main.Kind = FGateComplexPiece::EKind::Gate;
		Main.Gate.Rank = P.Rank;
		Main.Gate.Tower = EHutongCityGateTower::Chenglou;
		const double Lg = Main.Gate.GetLength();
		Main.Size = FVector2D(Lg, Main.Gate.GetDepth());
		Main.Origin = FVector2D(-0.5 * Lg, 0.0);
		Main.Facade = EHutongBaySide::MinusY;
		Out.Add(Main);

		const FVector2D Away(0.0, -1.0e6);
		if (P.WallStub > 1.0)
		{
			AddWall(FVector2D(-0.5 * Lg, 0.5 * B), FVector2D(-0.5 * Lg - P.WallStub, 0.5 * B), Away);
			AddWall(FVector2D(0.5 * Lg, 0.5 * B), FVector2D(0.5 * Lg + P.WallStub, 0.5 * B), Away);
		}

		// The 箭樓, its platform ahead of the barbican's front wall.
		FGateComplexPiece Tower;
		Tower.Kind = FGateComplexPiece::EKind::ArrowTower;
		Tower.Tower.Rank = P.Rank;
		const double Wp = Tower.Tower.GetPlatformWidth(), Dp = Tower.Tower.GetPlatformDepth();
		const double S = GateComplexCornerSize(W);
		FGateComplexPiece Side;
		Side.Gate.Rank = P.Rank;
		Side.Gate.Tower = EHutongCityGateTower::Zhalou;
		const double Lside = Side.Gate.GetLength();
		const double Wb = FMath::Max(P.BarbicanWidth, Wp + 2.0 * (S + MinFrontWallCm));
		const double Db = FMath::Max(P.BarbicanDepth, S + Lside + 2.0 * MinFrontWallCm);
		const double YFront = -Db + 0.5 * B;
		Tower.Origin = FVector2D(-0.5 * Wp, -Db - Tower.Tower.GetLegOuterY());
		Tower.Size = FVector2D(Wp, Dp);
		Tower.Facade = EHutongBaySide::MinusY;
		Out.Add(Tower);

		// Rounded corners at the barbican's front, their outside corners on its outer faces.
		for (const int32 Hand : { -1, 1 })
		{
			FGateComplexPiece Corner;
			Corner.Kind = FGateComplexPiece::EKind::Corner;
			Corner.Wall = W;
			Corner.CornerSize = S;
			Corner.Size = FVector2D(S, S);
			Corner.Origin = FVector2D(Hand < 0 ? -0.5 * Wb : 0.5 * Wb - S, -Db);
			Corner.OuterCorner = Hand < 0 ? 0 : 1;
			Out.Add(Corner);
		}
		const FVector2D Ahead(0.0, -10.0 * Db);
		AddWall(FVector2D(-0.5 * Wb + S, YFront), FVector2D(-0.5 * Wp, YFront), Ahead);
		AddWall(FVector2D(0.5 * Wp, YFront), FVector2D(0.5 * Wb - S, YFront), Ahead);

		// Side walls from the corners back into the city wall, the barbican's gate in one of them.
		const double YCorner = -Db + S, YWall = Bat;
		for (const int32 Hand : { -1, 1 })
		{
			const double Xc = Hand * (0.5 * Wb - 0.5 * B);
			const FVector2D Outside(Hand * 10.0 * Wb, 0.0);
			const bool bGate = (Hand > 0) == (P.SideGate == EHutongBarbicanGateSide::East);
			if (!bGate)
			{
				AddWall(FVector2D(Xc, YCorner), FVector2D(Xc, YWall), Outside);
				continue;
			}
			const double Yc = 0.5 * (YCorner + 0.0);
			FGateComplexPiece G = Side;
			G.Kind = FGateComplexPiece::EKind::Gate;
			G.Facade = Hand > 0 ? EHutongBaySide::PlusX : EHutongBaySide::MinusX;
			G.Size = FVector2D(G.Gate.GetDepth(), Lside);
			G.Origin = FVector2D(Hand > 0 ? 0.5 * Wb - G.Gate.GetDepth() : -0.5 * Wb, Yc - 0.5 * Lside);
			Out.Add(G);
			AddWall(FVector2D(Xc, YCorner), FVector2D(Xc, Yc - 0.5 * Lside), Outside);
			AddWall(FVector2D(Xc, Yc + 0.5 * Lside), FVector2D(Xc, YWall), Outside);
		}
	}

	void GateComplexPieceCorners(const FGateComplexPiece& Piece, FVector2D OutCorners[4])
	{
		const double R = FMath::DegreesToRadians(Piece.YawDeg);
		const FVector2D X(FMath::Cos(R), FMath::Sin(R)), Y(-X.Y, X.X);
		const FVector2D Local[4] = { { 0.0, 0.0 }, { Piece.Size.X, 0.0 }, { Piece.Size.X, Piece.Size.Y }, { 0.0, Piece.Size.Y } };
		for (int32 k = 0; k < 4; ++k) OutCorners[k] = Piece.Origin + X * Local[k].X + Y * Local[k].Y;
	}

	namespace
	{
		// The rounded corner's footprint, as the corner piece sizes it.
		double GateComplexCornerSize(const FHutongCityWallParams& W)
		{
			return FMath::Max(W.GetRoundedCorner(), W.GetBaseWidth() + 10.0);
		}
	}
}
