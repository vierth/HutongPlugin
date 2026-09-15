#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongFootprint.h"
#include "Operations/MeshPlaneCut.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Algo/Reverse.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongMeshUtils
{
	void YawVerticesFrom(FDynamicMesh3& Mesh, int32 FirstVertexID,
		const FVector2d& PivotXY, double YawDegrees)
	{
		if (FMath::IsNearlyZero(YawDegrees))
		{
			return;
		}

		const double Rad = FMath::DegreesToRadians(YawDegrees);
		const double C = FMath::Cos(Rad);
		const double S = FMath::Sin(Rad);

		for (int32 vid = FirstVertexID; vid < Mesh.MaxVertexID(); ++vid)
		{
			if (!Mesh.IsVertex(vid))
			{
				continue;
			}
			FVector3d P = Mesh.GetVertex(vid);
			const double DX = P.X - PivotXY.X;
			const double DY = P.Y - PivotXY.Y;
			P.X = PivotXY.X + DX * C - DY * S;
			P.Y = PivotXY.Y + DX * S + DY * C;
			Mesh.SetVertex(vid, P);
		}
	}

	void TransformVerticesFrom(FDynamicMesh3& Mesh, int32 FirstVertexID,
		const FTransform& Transform)
	{
		for (int32 vid = FirstVertexID; vid < Mesh.MaxVertexID(); ++vid)
		{
			if (!Mesh.IsVertex(vid))
			{
				continue;
			}
			Mesh.SetVertex(vid, FVector3d(Transform.TransformPosition(FVector(Mesh.GetVertex(vid)))));
		}
	}

	void EnsureUVLayer(FDynamicMesh3& Mesh)
	{
		Mesh.EnableAttributes();
		if (Mesh.Attributes()->NumUVLayers() < 1)
		{
			Mesh.Attributes()->SetNumUVLayers(1);
		}
	}

	void RetagDownwardFaces(FDynamicMesh3& Mesh, int32 FromSlot, int32 ToSlot, double MinNormalZ)
	{
		UE::Geometry::FDynamicMeshMaterialAttribute* MatIDs =
			Mesh.HasAttributes() ? Mesh.Attributes()->GetMaterialID() : nullptr;
		if (!MatIDs) return;
		for (int32 tid : Mesh.TriangleIndicesItr())
		{
			if (MatIDs->GetValue(tid) != FromSlot) continue;
			// Inward normals: up here is a face that looks down in the level.
			if (Mesh.GetTriNormal(tid).Z > MinNormalZ) MatIDs->SetValue(tid, ToSlot);
		}
	}

	bool WarpFootprint(FDynamicMesh3& Mesh, double Width, double Depth, const FHutongFootprintSkew& Skew)
	{
		if (Width <= 0.0 || Depth <= 0.0) return false;
		const FVector2D Size(Width, Depth);
		bool bIdentity = true;
		for (int32 i = 0; i < 4 && bIdentity; ++i) bIdentity = HutongFootprint::EffectiveOffset(Size, Skew, i).IsNearlyZero();
		if (bIdentity) return true;
		if (!HutongFootprint::IsSkewValid(Size, Skew))
		{
			UE_LOG(LogTemp, Warning, TEXT("WarpFootprint: the corner offsets fold the footprint; mesh left rectangular."));
			return false;
		}

		if (Skew.Mode == EHutongSkewMode::Ends)
		{
			// A primitive that spans the whole run has no vertex where the zone begins; give it one,
			// so the body up to the plane stays exactly where it was built.
			const bool bX = HutongFootprint::RunAlongX(Size);
			const double L = bX ? Width : Depth;
			const FVector3d Normal = bX ? FVector3d(1, 0, 0) : FVector3d(0, 1, 0);
			for (const bool bStart : { true, false })
			{
				const double Z = HutongFootprint::EndZone(Size, Skew, bStart);
				if (Z <= 0.0) continue;
				const double At = bStart ? Z : L - Z;
				UE::Geometry::FMeshPlaneCut Split(&Mesh, Normal * At, Normal);
				Split.SplitEdgesOnly(/*bAssignNewGroups*/ false, nullptr);
			}
		}

		for (int32 vid = 0; vid < Mesh.MaxVertexID(); ++vid)
		{
			if (!Mesh.IsVertex(vid)) continue;
			const FVector3d P = Mesh.GetVertex(vid);
			const FVector2D Q = HutongFootprint::Map(Size, Skew, P.X, P.Y);
			Mesh.SetVertex(vid, FVector3d(Q.X, Q.Y, P.Z));
		}
		return true;
	}

	void FillUnsetUVsBoxProjected(FDynamicMesh3& Mesh, double WorldPerUV, double RoofWorldPerUV)
	{
		EnsureUVLayer(Mesh);
		UE::Geometry::FDynamicMeshUVOverlay* UV = Mesh.Attributes()->PrimaryUV();
		if (!UV) return;

		const UE::Geometry::FDynamicMeshMaterialAttribute* MatIDs =
			Mesh.HasAttributes() ? Mesh.Attributes()->GetMaterialID() : nullptr;
		const double BodyScale = 1.0 / FMath::Max(WorldPerUV, 0.01);
		const double RoofScale = 1.0 / FMath::Max(RoofWorldPerUV, 0.01);

		for (int32 tid = 0; tid < Mesh.MaxTriangleID(); ++tid)
		{
			if (!Mesh.IsTriangle(tid) || UV->IsSetTriangle(tid)) continue;
			const double Scale =
				(MatIDs && MatIDs->GetValue(tid) == HutongGen::MatSlot_Roof) ? RoofScale : BodyScale;

			const UE::Geometry::FIndex3i T = Mesh.GetTriangle(tid);
			const FVector3d P[3] = { Mesh.GetVertex(T.A), Mesh.GetVertex(T.B), Mesh.GetVertex(T.C) };
			const FVector3d N = (P[1] - P[0]).Cross(P[2] - P[0]);

			// Project along whichever axis the face most nearly points down.
			const double AX = FMath::Abs(N.X), AY = FMath::Abs(N.Y), AZ = FMath::Abs(N.Z);
			int32 Elems[3];
			for (int32 i = 0; i < 3; ++i)
			{
				FVector2f Uv;
				if (AZ >= AX && AZ >= AY)      Uv = FVector2f((float)(P[i].X * Scale), (float)(P[i].Y * Scale));
				else if (AX >= AY)             Uv = FVector2f((float)(P[i].Y * Scale), (float)(P[i].Z * Scale));
				else                           Uv = FVector2f((float)(P[i].X * Scale), (float)(P[i].Z * Scale));
				Elems[i] = UV->AppendElement(Uv);
			}
			UV->SetTriangle(tid, UE::Geometry::FIndex3i(Elems[0], Elems[1], Elems[2]));
		}
	}

	void SetMaterialIDForTriangleRange(
		FDynamicMesh3& Mesh, int32 FirstTriangleID, int32 EndTriangleID, int32 MaterialID)
	{
		Mesh.EnableAttributes();
		Mesh.Attributes()->EnableMaterialID();
		UE::Geometry::FDynamicMeshMaterialAttribute* MatIDs = Mesh.Attributes()->GetMaterialID();
		if (!MatIDs) return;

		// Triangles that predate EnableMaterialID keep the default slot 0.
		const int32 End = FMath::Min(EndTriangleID, Mesh.MaxTriangleID());
		for (int32 tid = FMath::Max(FirstTriangleID, 0); tid < End; ++tid)
		{
			if (Mesh.IsTriangle(tid))
			{
				MatIDs->SetValue(tid, MaterialID);
			}
		}
	}

	void SetMaterialIDForTrianglesFrom(FDynamicMesh3& Mesh, int32 FirstTriangleID, int32 MaterialID)
	{
		SetMaterialIDForTriangleRange(Mesh, FirstTriangleID, Mesh.MaxTriangleID(), MaterialID);
	}

	void AppendBox(FDynamicMesh3& Mesh, const FVector3d& Mn, const FVector3d& Mx)
	{
		if (Mx.X <= Mn.X || Mx.Y <= Mn.Y || Mx.Z <= Mn.Z) return;

		const FVector3d V[8] = {
			{Mn.X, Mn.Y, Mn.Z}, {Mx.X, Mn.Y, Mn.Z},
			{Mx.X, Mx.Y, Mn.Z}, {Mn.X, Mx.Y, Mn.Z},
			{Mn.X, Mn.Y, Mx.Z}, {Mx.X, Mn.Y, Mx.Z},
			{Mx.X, Mx.Y, Mx.Z}, {Mn.X, Mx.Y, Mx.Z}
		};
		int32 B[8];
		for (int32 i = 0; i < 8; ++i) B[i] = Mesh.AppendVertex(V[i]);

		auto T = [&](int32 a, int32 b, int32 c) { Mesh.AppendTriangle(B[a], B[b], B[c]); };
		T(0, 2, 1); T(0, 3, 2);        // -Z
		T(4, 5, 6); T(4, 6, 7);        // +Z
		T(0, 1, 5); T(0, 5, 4);        // -Y
		T(3, 7, 6); T(3, 6, 2);        // +Y
		T(0, 4, 7); T(0, 7, 3);        // -X
		T(1, 2, 6); T(1, 6, 5);        // +X
	}

	void AppendTriPrism(
		FDynamicMesh3& Mesh,
		const FVector3d& BaseMin,
		double Length,
		double Width,
		double ApexHeight,
		EAxis2D AlongAxis,
		double ApexFraction)
	{
		if (Length <= 0.0 || Width <= 0.0 || ApexHeight <= 0.0) return;

		FVector3d P0, P1, P2, P3, A0, A1;
		const double Z0 = BaseMin.Z;
		const double Za = Z0 + ApexHeight;
		// Where the ridge sits across the width. At 0 or 1 the prism becomes a one-sided wedge.
		const double Ax = FMath::Clamp(ApexFraction, 0.0, 1.0) * Width;

		if (AlongAxis == EAxis2D::X)
		{
			P0 = {BaseMin.X,          BaseMin.Y,         Z0};
			P1 = {BaseMin.X + Length, BaseMin.Y,         Z0};
			P2 = {BaseMin.X + Length, BaseMin.Y + Width, Z0};
			P3 = {BaseMin.X,          BaseMin.Y + Width, Z0};
			A0 = {BaseMin.X,          BaseMin.Y + Ax, Za};
			A1 = {BaseMin.X + Length, BaseMin.Y + Ax, Za};
		}
		else
		{
			P0 = {BaseMin.X,         BaseMin.Y,          Z0};
			P1 = {BaseMin.X + Width, BaseMin.Y,          Z0};
			P2 = {BaseMin.X + Width, BaseMin.Y + Length, Z0};
			P3 = {BaseMin.X,         BaseMin.Y + Length, Z0};
			A0 = {BaseMin.X + Ax, BaseMin.Y,          Za};
			A1 = {BaseMin.X + Ax, BaseMin.Y + Length, Za};
		}

		const int32 i0 = Mesh.AppendVertex(P0);
		const int32 i1 = Mesh.AppendVertex(P1);
		const int32 i2 = Mesh.AppendVertex(P2);
		const int32 i3 = Mesh.AppendVertex(P3);
		const int32 a0 = Mesh.AppendVertex(A0);
		const int32 a1 = Mesh.AppendVertex(A1);

		// Base (-Z)
		Mesh.AppendTriangle(i0, i2, i1);
		Mesh.AppendTriangle(i0, i3, i2);

		if (AlongAxis == EAxis2D::X)
		{
			// -Y slope
			Mesh.AppendTriangle(i0, i1, a1);
			Mesh.AppendTriangle(i0, a1, a0);
			// +Y slope
			Mesh.AppendTriangle(i3, a0, a1);
			Mesh.AppendTriangle(i3, a1, i2);
			// -X end cap
			Mesh.AppendTriangle(i0, a0, i3);
			// +X end cap
			Mesh.AppendTriangle(i1, i2, a1);
		}
		else
		{
			// -X slope
			Mesh.AppendTriangle(i0, a1, i3);
			Mesh.AppendTriangle(i0, a0, a1);
			// +X slope
			Mesh.AppendTriangle(i1, i2, a1);
			Mesh.AppendTriangle(i1, a1, a0);
			// -Y end cap
			Mesh.AppendTriangle(i0, i1, a0);
			// +Y end cap
			Mesh.AppendTriangle(i3, a1, i2);
		}
	}

	TArray<FVector2d> GableRoofProfile(
		double Width,
		double ApexHeight,
		const HutongGen::FHutongRoofSection& Section,
		int32 SlopeSegments,
		double FarEaveTrim)
	{
		TArray<FVector2d> Out;
		if (Width <= 0.0 || ApexHeight <= 0.0) return Out;

		const int32 N = FMath::Max(SlopeSegments, 1);
		const double Ridge = 0.5 * Width;
		// Never back to the ridge, let alone past it.
		const double Trim = FMath::Clamp(FarEaveTrim, 0.0, 0.9 * Ridge);

		// Sampled at the section's own breakpoints.
		TArray<double> Cross;
		{
			const double Span = FMath::Max(Section.HalfSpan(), UE_DOUBLE_KINDA_SMALL_NUMBER);
			const double Scale = Ridge / Span;

			// Distances from the ridge at which the profile creases.
			TArray<double> Breaks;
			{
				double d = 0.0;
				for (int32 i = Section.Run.Num() - 1; i >= 0; --i)
				{
					d += Section.Run[i] * Scale;
					Breaks.Add(d);
				}
			}
			const double RollBand = FMath::Clamp(Section.ApexRoll, 0.0, 1.0) * Ridge;

			auto SideDistances = [&](double MaxDist)
			{
				TArray<double> Ds;
				Ds.Add(0.0);
				for (int32 k = 1; k <= N && RollBand > 0.0; ++k)
				{
					Ds.Add(FMath::Min(RollBand * (double)k / (double)N, MaxDist));
				}
				for (double B : Breaks)
				{
					if (B > RollBand && B < MaxDist) Ds.Add(B);
				}
				Ds.Add(MaxDist);
				Ds.Sort();

				TArray<double> Out;
				for (double D : Ds)
				{
					if (Out.Num() == 0 || D - Out.Last() > 1e-6) Out.Add(D);
				}
				return Out;
			};

			// Near slope, ridge-ward: distances descend to zero at the ridge.
			const TArray<double> NearD = SideDistances(Ridge);
			for (int32 k = NearD.Num() - 1; k >= 0; --k) Cross.Add(Ridge - NearD[k]);
			// Far slope, out to wherever it is cut.
			const TArray<double> FarD = SideDistances(Ridge - Trim);
			for (int32 k = 1; k < FarD.Num(); ++k) Cross.Add(Ridge + FarD[k]);
		}


		for (double C : Cross)
		{
			const double D = FMath::Abs(2.0 * C / Width - 1.0);   // 1 at either eave, 0 at ridge
			Out.Add(FVector2d(C, ApexHeight * Section.HeightFraction(D)));
		}
		return Out;
	}

	void AppendCurvedGableRoof(
		FDynamicMesh3& Mesh,
		const FVector3d& BaseMin,
		double Length,
		double Width,
		double ApexHeight,
		const HutongGen::FHutongRoofSection& Section,
		int32 SlopeSegments,
		EAxis2D AlongAxis,
		double FarEaveTrim,
		double UVTileSize,
		UE::Geometry::FIndex2i* OutGableFaceRange)
	{
		if (OutGableFaceRange) *OutGableFaceRange = UE::Geometry::FIndex2i(0, 0);
		if (Length <= 0.0 || Width <= 0.0 || ApexHeight <= 0.0) return;

		const double Z0 = BaseMin.Z;
		const TArray<FVector2d> Profile = GableRoofProfile(Width, ApexHeight, Section, SlopeSegments, FarEaveTrim);
		if (Profile.Num() < 2) return;

		auto MakeVertex = [&](double Along, double Cross, double Z)
		{
			return (AlongAxis == EAxis2D::X)
				? FVector3d(BaseMin.X + Along, BaseMin.Y + Cross, Z)
				: FVector3d(BaseMin.X + Cross, BaseMin.Y + Along, Z);
		};

		TArray<double> Cross;
		for (const FVector2d& S : Profile) Cross.Add(S.X);
		auto ProfileZ = [&](double C)
		{
			const double D = FMath::Abs(2.0 * C / Width - 1.0);
			return Z0 + ApexHeight * Section.HeightFraction(D);
		};

		// Top chain along the profile, and a bottom chain on the base plane under it.
		TArray<int32> TopA, TopB, BotA, BotB;   // A at Along = 0, B at Along = Length
		for (double C : Cross)
		{
			const double Z = ProfileZ(C);
			const int32 ta = Mesh.AppendVertex(MakeVertex(0.0, C, Z));
			const int32 tb = Mesh.AppendVertex(MakeVertex(Length, C, Z));
			TopA.Add(ta);
			TopB.Add(tb);

			const bool bOnBase = (Z - Z0) <= UE_DOUBLE_KINDA_SMALL_NUMBER;
			BotA.Add(bOnBase ? ta : Mesh.AppendVertex(MakeVertex(0.0, C, Z0)));
			BotB.Add(bOnBase ? tb : Mesh.AppendVertex(MakeVertex(Length, C, Z0)));
		}

		// Base (-Z), as a strip rather than one quad.
		for (int32 k = 0; k < Cross.Num() - 1; ++k)
		{
			Mesh.AppendTriangle(BotA[k], BotA[k + 1], BotB[k]);
			Mesh.AppendTriangle(BotB[k], BotA[k + 1], BotB[k + 1]);
		}

		// Slope strips.
		EnsureUVLayer(Mesh);
		UE::Geometry::FDynamicMeshUVOverlay* UV = Mesh.Attributes()->PrimaryUV();
		const double UVScale = 1.0 / FMath::Max(UVTileSize, 0.01);

		TArray<double> ArcV;
		ArcV.Reserve(Cross.Num());
		{
			double S = 0.0;
			ArcV.Add(0.0);
			for (int32 k = 1; k < Cross.Num(); ++k)
			{
				const double dC = Cross[k] - Cross[k - 1];
				const double dZ = ProfileZ(Cross[k]) - ProfileZ(Cross[k - 1]);
				S += FMath::Sqrt(dC * dC + dZ * dZ);
				ArcV.Add(S);
			}
		}

		TArray<int32> UvA, UvB;
		UvA.Reserve(Cross.Num()); UvB.Reserve(Cross.Num());
		for (int32 k = 0; k < Cross.Num(); ++k)
		{
			const float V = (float)(ArcV[k] * UVScale);
			UvA.Add(UV ? UV->AppendElement(FVector2f(0.0f, V)) : -1);
			UvB.Add(UV ? UV->AppendElement(FVector2f((float)(Length * UVScale), V)) : -1);
		}

		for (int32 k = 0; k < Cross.Num() - 1; ++k)
		{
			const int32 t0 = Mesh.AppendTriangle(TopA[k], TopB[k], TopB[k + 1]);
			const int32 t1 = Mesh.AppendTriangle(TopA[k], TopB[k + 1], TopA[k + 1]);
			if (UV && t0 >= 0) UV->SetTriangle(t0, UE::Geometry::FIndex3i(UvA[k], UvB[k], UvB[k + 1]));
			if (UV && t1 >= 0) UV->SetTriangle(t1, UE::Geometry::FIndex3i(UvA[k], UvB[k + 1], UvA[k + 1]));
		}

		// Gable end caps.
		const int32 GableFirst = Mesh.MaxTriangleID();
		for (int32 k = 0; k < Cross.Num() - 1; ++k)
		{
			if (BotA[k] != TopA[k])
			{
				Mesh.AppendTriangle(BotA[k], TopA[k], BotA[k + 1]);
				Mesh.AppendTriangle(BotB[k], BotB[k + 1], TopB[k]);
			}
			if (BotA[k + 1] != TopA[k + 1])
			{
				Mesh.AppendTriangle(TopA[k], TopA[k + 1], BotA[k + 1]);
				Mesh.AppendTriangle(TopB[k], BotB[k + 1], TopB[k + 1]);
			}
		}

		if (OutGableFaceRange) *OutGableFaceRange = UE::Geometry::FIndex2i(GableFirst, Mesh.MaxTriangleID());

		// The cut face closing a trimmed far slope.
		const int32 L = Cross.Num() - 1;
		if (BotA[L] != TopA[L])
		{
			Mesh.AppendTriangle(BotA[L], TopB[L], BotB[L]);
			Mesh.AppendTriangle(BotA[L], TopA[L], TopB[L]);
		}
	}

	void AppendCylinder(
		FDynamicMesh3& Mesh,
		const FVector3d& BaseCenter,
		double Radius,
		double Height,
		int32 NumSides)
	{
		if (Radius <= 0.0 || Height <= 0.0 || NumSides < 3) return;

		const double Z0 = BaseCenter.Z;
		const double Z1 = Z0 + Height;

		TArray<int32> Bot, Top;
		Bot.Reserve(NumSides);
		Top.Reserve(NumSides);
		for (int32 i = 0; i < NumSides; ++i)
		{
			const double A = (2.0 * PI * i) / NumSides;
			const double X = BaseCenter.X + Radius * FMath::Cos(A);
			const double Y = BaseCenter.Y + Radius * FMath::Sin(A);
			Bot.Add(Mesh.AppendVertex(FVector3d(X, Y, Z0)));
			Top.Add(Mesh.AppendVertex(FVector3d(X, Y, Z1)));
		}
		const int32 CBot = Mesh.AppendVertex(FVector3d(BaseCenter.X, BaseCenter.Y, Z0));
		const int32 CTop = Mesh.AppendVertex(FVector3d(BaseCenter.X, BaseCenter.Y, Z1));

		for (int32 i = 0; i < NumSides; ++i)
		{
			const int32 j = (i + 1) % NumSides;
			Mesh.AppendTriangle(Bot[i], Bot[j], Top[j]);
			Mesh.AppendTriangle(Bot[i], Top[j], Top[i]);
			Mesh.AppendTriangle(CBot, Bot[j], Bot[i]);
			Mesh.AppendTriangle(CTop, Top[i], Top[j]);
		}
	}

	TArray<FVector2d> MakeCircleProfile(double Radius, int32 NumSides)
	{
		TArray<FVector2d> P;
		const int32 N = FMath::Max(NumSides, 3);
		const double R = FMath::Max(Radius, 0.01);
		P.Reserve(N);
		for (int32 i = 0; i < N; ++i)
		{
			const double A = (2.0 * PI * i) / N;
			P.Add(FVector2d(R * FMath::Cos(A), R * FMath::Sin(A)));
		}
		return P;
	}

	namespace
	{
		double SignedArea2(const TArray<FVector2d>& P)
		{
			double A = 0.0;
			for (int32 i = 0, N = P.Num(); i < N; ++i)
			{
				const FVector2d& a = P[i];
				const FVector2d& b = P[(i + 1) % N];
				A += a.X * b.Y - b.X * a.Y;
			}
			return 0.5 * A;
		}

		double Cross2(const FVector2d& A, const FVector2d& B, const FVector2d& C)
		{
			return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
		}

		bool InTriangle(const FVector2d& P, const FVector2d& A, const FVector2d& B, const FVector2d& C)
		{
			return Cross2(A, B, P) >= 0.0 && Cross2(B, C, P) >= 0.0 && Cross2(C, A, P) >= 0.0;
		}

		// Ear clipping over a CCW simple polygon.
		bool EarClipCCW(const TArray<FVector2d>& P, TArray<int32>& OutTris)
		{
			const int32 N = P.Num();
			TArray<int32> Idx;
			Idx.Reserve(N);
			for (int32 i = 0; i < N; ++i) Idx.Add(i);

			int32 Guard = N * N + 8;
			while (Idx.Num() > 3 && Guard-- > 0)
			{
				bool bClipped = false;
				for (int32 k = 0; k < Idx.Num(); ++k)
				{
					const int32 N2 = Idx.Num();
					const int32 ia = Idx[(k + N2 - 1) % N2];
					const int32 ib = Idx[k];
					const int32 ic = Idx[(k + 1) % N2];
					if (Cross2(P[ia], P[ib], P[ic]) <= 0.0) continue;   // reflex or collinear

					bool bEar = true;
					for (int32 m : Idx)
					{
						if (m == ia || m == ib || m == ic) continue;
						if (InTriangle(P[m], P[ia], P[ib], P[ic])) { bEar = false; break; }
					}
					if (!bEar) continue;

					OutTris.Add(ia); OutTris.Add(ib); OutTris.Add(ic);
					Idx.RemoveAt(k);
					bClipped = true;
					break;
				}
				if (!bClipped) return false;
			}

			if (Idx.Num() != 3) return false;
			OutTris.Add(Idx[0]); OutTris.Add(Idx[1]); OutTris.Add(Idx[2]);
			return true;
		}
	}

	void AppendSweptProfile(
		FDynamicMesh3& Mesh,
		const TArray<FVector2d>& Profile,
		const TArray<FTransform>& Stations)
	{
		if (Profile.Num() < 3 || Stations.Num() < 2)
		{
			return;
		}

		// Normalised here rather than asked of the caller.
		TArray<FVector2d> P = Profile;
		if (SignedArea2(P) < 0.0)
		{
			Algo::Reverse(P);
		}
		if (FMath::Abs(SignedArea2(P)) < UE_DOUBLE_KINDA_SMALL_NUMBER)
		{
			return;
		}

		// Triangulated before anything is appended.
		TArray<int32> CapTris;
		if (!EarClipCCW(P, CapTris))
		{
			return;
		}

		const int32 N = P.Num();
		const int32 FirstTri = Mesh.MaxTriangleID();

		TArray<int32> Rings;   // Stations.Num() * N vertex ids, station-major
		Rings.Reserve(Stations.Num() * N);
		for (const FTransform& S : Stations)
		{
			for (int32 i = 0; i < N; ++i)
			{
				Rings.Add(Mesh.AppendVertex(
					FVector3d(S.TransformPosition(FVector(P[i].X, P[i].Y, 0.0)))));
			}
		}

		auto Ring = [&](int32 s, int32 i) { return Rings[s * N + i]; };

		for (int32 s = 0; s + 1 < Stations.Num(); ++s)
		{
			for (int32 i = 0; i < N; ++i)
			{
				const int32 j = (i + 1) % N;
				Mesh.AppendTriangle(Ring(s, i), Ring(s, j), Ring(s + 1, j));
				Mesh.AppendTriangle(Ring(s, i), Ring(s + 1, j), Ring(s + 1, i));
			}
		}

		const int32 Last = Stations.Num() - 1;
		for (int32 t = 0; t + 2 < CapTris.Num(); t += 3)
		{
			// Start cap reversed: it faces back down the sweep.
			Mesh.AppendTriangle(Ring(0, CapTris[t + 2]), Ring(0, CapTris[t + 1]), Ring(0, CapTris[t]));
			Mesh.AppendTriangle(Ring(Last, CapTris[t]), Ring(Last, CapTris[t + 1]), Ring(Last, CapTris[t + 2]));
		}

		// Net for the common slip of building the stations against the sweep, which turns the whole solid inside out.
		double Volume = 0.0;
		for (int32 tid = FirstTri; tid < Mesh.MaxTriangleID(); ++tid)
		{
			if (!Mesh.IsTriangle(tid)) continue;
			const UE::Geometry::FIndex3i T = Mesh.GetTriangle(tid);
			const FVector3d A = Mesh.GetVertex(T.A), B = Mesh.GetVertex(T.B), C = Mesh.GetVertex(T.C);
			Volume += A.Dot(B.Cross(C));
		}
		if (Volume < 0.0)
		{
			for (int32 tid = FirstTri; tid < Mesh.MaxTriangleID(); ++tid)
			{
				if (Mesh.IsTriangle(tid)) Mesh.ReverseTriOrientation(tid);
			}
		}
	}

	namespace
	{
		// 勾頭 along a swept eave: a hipped-family eave turns corners and lifts at them.
		void AppendEaveCapRow(
			FDynamicMesh3& Mesh,
			const TArray<FVector3d>& Ring,
			const FVector2d& CentreXY,
			double Spacing,
			double FasciaDrop)
		{
			if (Ring.Num() < 3 || Spacing <= 0.0 || FasciaDrop <= 0.0) return;

			const int32 N = Ring.Num();
			TArray<double> Cumulative;
			Cumulative.Reserve(N + 1);
			double Total = 0.0;
			Cumulative.Add(0.0);
			for (int32 i = 0; i < N; ++i)
			{
				Total += (Ring[(i + 1) % N] - Ring[i]).Length();
				Cumulative.Add(Total);
			}
			if (Total <= Spacing) return;

			const int32 Count = FMath::Clamp(FMath::FloorToInt32(Total / Spacing), 1, 600);
			const double Step = Total / Count;

			const double CapR = FMath::Clamp(0.34 * Spacing, 1.5, 0.9 * FasciaDrop);
			const double CapLen = FMath::Max(0.8 * CapR, 1.0);

			int32 Seg = 0;
			for (int32 k = 0; k < Count; ++k)
			{
				const double S = (k + 0.5) * Step;
				while (Seg + 1 < Cumulative.Num() - 1 && Cumulative[Seg + 1] < S) ++Seg;

				const FVector3d& A = Ring[Seg];
				const FVector3d& B = Ring[(Seg + 1) % N];
				const double SegLen = FMath::Max(Cumulative[Seg + 1] - Cumulative[Seg], 1e-6);
				const double T = FMath::Clamp((S - Cumulative[Seg]) / SegLen, 0.0, 1.0);
				const FVector3d P = A + (B - A) * T;

				FVector2d Tan(B.X - A.X, B.Y - A.Y);
				if (Tan.SquaredLength() < 1e-12) continue;
				Tan.Normalize();

				// Plan perpendicular, turned away from the middle of the roof.
				FVector2d Out(Tan.Y, -Tan.X);
				if (Out.Dot(FVector2d(P.X, P.Y) - CentreXY) < 0.0) Out = -Out;

				const FVector3d Outward(Out.X, Out.Y, 0.0);
				const FQuat Lay = FQuat::FindBetweenNormals(
					FVector::UpVector, FVector(Outward.X, Outward.Y, 0.0));

				// Sunk back into the fascia band by part of its own radius.
				const FVector3d At = P - Outward * (0.4 * CapR) - FVector3d(0.0, 0.0, 0.5 * FasciaDrop);
				const int32 Mark = Mesh.MaxVertexID();
				AppendCylinder(Mesh, FVector3d::Zero(), CapR, CapLen, HutongGen::RoofTile::EaveCapSides);
				TransformVerticesFrom(Mesh, Mark,
					FTransform(Lay, FVector(At.X, At.Y, At.Z)));
			}
		}
	}

	void AppendHippedRoof(
		FDynamicMesh3& Mesh,
		const FVector3d& EaveMin,
		const FHipRoofSpec& Spec)
	{
		const double W = FMath::Max(Spec.Width, 1.0);
		const double D = FMath::Max(Spec.Depth, 1.0);
		const double Rise = FMath::Max(Spec.Rise, 0.1);
		const int32 Nu = FMath::Clamp(Spec.EaveSegments, 2, 32);
		const int32 Nv = FMath::Clamp(Spec.SlopeSegments, 1, 16);

		const double L = FMath::Clamp(Spec.RidgeLength, 0.0, W);
		const double Rx0 = 0.5 * (W - L);
		const double Rx1 = 0.5 * (W + L);
		const double Cy = 0.5 * D;

		// The flare must not reach the middle of the shortest eave, or two corners' sweeps fight over the same points; and the run is held under the length.
		const double FlareLen = FMath::Clamp(Spec.FlareLength, 0.0, 0.5 * FMath::Min(W, D));
		const double FlareRun = (FlareLen > 0.0) ? FMath::Clamp(Spec.FlareRun, 0.0, 0.8 * FlareLen) : 0.0;
		const double FlareRise = (FlareLen > 0.0) ? FMath::Max(Spec.FlareRise, 0.0) : 0.0;
		const bool bFlare = (FlareRun > 0.0 || FlareRise > 0.0);

		const double BaseZ = EaveMin.Z - FMath::Max(Spec.FasciaDrop, 0.0);

		const FVector2d Corner[4] = {
			FVector2d(0.0, 0.0), FVector2d(W, 0.0), FVector2d(W, D), FVector2d(0.0, D) };
		const double Rt2 = 1.0 / FMath::Sqrt(2.0);
		const FVector2d Diag[4] = {
			FVector2d(-Rt2, -Rt2), FVector2d(Rt2, -Rt2), FVector2d(Rt2, Rt2), FVector2d(-Rt2, Rt2) };

		// Panels run round the eave the same way the corners do, each rising to its own stretch of the ridge.
		const int32 EaveA[4] = { 0, 1, 2, 3 };
		const int32 EaveB[4] = { 1, 2, 3, 0 };
		const FVector2d RidgeA[4] = {
			FVector2d(Rx0, Cy), FVector2d(Rx1, Cy), FVector2d(Rx1, Cy), FVector2d(Rx0, Cy) };
		const FVector2d RidgeB[4] = {
			FVector2d(Rx1, Cy), FVector2d(Rx1, Cy), FVector2d(Rx0, Cy), FVector2d(Rx0, Cy) };

		auto Sample = [&](int32 p, double u, double v)
		{
			const FVector2d Ea = Corner[EaveA[p]];
			const FVector2d Eb = Corner[EaveB[p]];
			const FVector2d E = Ea + (Eb - Ea) * u;
			const FVector2d R = RidgeA[p] + (RidgeB[p] - RidgeA[p]) * u;

			FVector2d XY = E + (R - E) * v;
			double Z = EaveMin.Z + Rise * Spec.Section.HeightFraction(1.0 - v);

			if (bFlare)
			{
				const double EaveLen = (Eb - Ea).Length();
				const double Sa = u * EaveLen;
				const double Sb = (1.0 - u) * EaveLen;
				const int32 C = (Sa <= Sb) ? EaveA[p] : EaveB[p];
				double Wt = FMath::Clamp(1.0 - FMath::Min(Sa, Sb) / FlareLen, 0.0, 1.0);
				Wt = Wt * Wt * (3.0 - 2.0 * Wt);            // smoothstep, so the eave leaves flat
				const double G = (1.0 - v) * (1.0 - v);     // and the ridge is left alone
				XY += Diag[C] * (Wt * G * FlareRun);
				Z += Wt * G * FlareRise;
			}

			return FVector3d(EaveMin.X + XY.X, EaveMin.Y + XY.Y, Z);
		};

		// The apex row of an end panel collapses to a point, and with no ridge every panel does.
		auto AppendTri = [&](int32 a, int32 b, int32 c) -> int32
		{
			if (a == b || b == c || a == c) return -1;
			const FVector3d A = Mesh.GetVertex(a), B = Mesh.GetVertex(b), C = Mesh.GetVertex(c);
			if (((B - A).Cross(C - A)).SquaredLength() < 1e-8) return -1;
			return Mesh.AppendTriangle(a, b, c);
		};

		// Slopes.
		EnsureUVLayer(Mesh);
		UE::Geometry::FDynamicMeshUVOverlay* UV = Mesh.Attributes()->PrimaryUV();
		const double UVScale = 1.0 / FMath::Max(Spec.TileRowSpacing, 0.01);

		for (int32 p = 0; p < 4; ++p)
		{
			const int32 Rows = Nv;
			const FVector2d Ea = Corner[EaveA[p]];
			const FVector2d EDir = (Corner[EaveB[p]] - Ea).GetSafeNormal();

			TArray<double> ArcV;
			ArcV.Reserve(Rows + 1);
			{
				double S = 0.0;
				ArcV.Add(0.0);
				for (int32 iv = 1; iv <= Rows; ++iv)
				{
					S += (Sample(p, 0.5, (double)iv / Nv)
						- Sample(p, 0.5, (double)(iv - 1) / Nv)).Length();
					ArcV.Add(S);
				}
			}

			TArray<int32> Grid, UvGrid;
			Grid.Reserve((Nu + 1) * (Rows + 1));
			UvGrid.Reserve((Nu + 1) * (Rows + 1));
			for (int32 iv = 0; iv <= Rows; ++iv)
			{
				for (int32 iu = 0; iu <= Nu; ++iu)
				{
					const FVector3d P = Sample(p, (double)iu / Nu, (double)iv / Nv);
					Grid.Add(Mesh.AppendVertex(P));

					const double Along = FVector2d(P.X - EaveMin.X - Ea.X, P.Y - EaveMin.Y - Ea.Y)
						.Dot(EDir);
					UvGrid.Add(UV ? UV->AppendElement(FVector2f(
						(float)(Along * UVScale), (float)(ArcV[iv] * UVScale))) : -1);
				}
			}
			auto At = [&](int32 iu, int32 iv) { return Grid[iv * (Nu + 1) + iu]; };
			auto UvAt = [&](int32 iu, int32 iv) { return UvGrid[iv * (Nu + 1) + iu]; };

			for (int32 iv = 0; iv < Rows; ++iv)
			{
				for (int32 iu = 0; iu < Nu; ++iu)
				{
					const int32 t0 = AppendTri(At(iu, iv), At(iu + 1, iv), At(iu + 1, iv + 1));
					const int32 t1 = AppendTri(At(iu, iv), At(iu + 1, iv + 1), At(iu, iv + 1));
					if (UV && t0 >= 0) UV->SetTriangle(t0, UE::Geometry::FIndex3i(
						UvAt(iu, iv), UvAt(iu + 1, iv), UvAt(iu + 1, iv + 1)));
					if (UV && t1 >= 0) UV->SetTriangle(t1, UE::Geometry::FIndex3i(
						UvAt(iu, iv), UvAt(iu + 1, iv + 1), UvAt(iu, iv + 1)));
				}
			}
		}

		// The eave loop and the flat base under it.
		TArray<FVector2d> BaseXY;
		TArray<int32> EaveRing, BaseRing;
		BaseXY.Reserve(4 * Nu);
		for (int32 p = 0; p < 4; ++p)
		{
			for (int32 iu = 0; iu < Nu; ++iu)
			{
				const FVector3d P = Sample(p, (double)iu / Nu, 0.0);
				EaveRing.Add(Mesh.AppendVertex(P));
				BaseRing.Add(Mesh.AppendVertex(FVector3d(P.X, P.Y, BaseZ)));
				BaseXY.Add(FVector2d(P.X, P.Y));
			}
		}

		const int32 Ring = EaveRing.Num();
		for (int32 i = 0; i < Ring; ++i)
		{
			const int32 j = (i + 1) % Ring;
			AppendTri(BaseRing[i], BaseRing[j], EaveRing[i]);
			AppendTri(BaseRing[j], EaveRing[j], EaveRing[i]);
		}

		// 勾頭 round the eave, on a polyline sampled finely enough for the spacing.
		if (Spec.bEaveCaps && Spec.FasciaDrop > 0.0 && Spec.TileRowSpacing > 0.0)
		{
			TArray<FVector3d> CapRing;
			for (int32 p = 0; p < 4; ++p)
			{
				const double EaveLen = (p == 0 || p == 2) ? W : D;
				const int32 Fine = FMath::Clamp(
					FMath::CeilToInt32(EaveLen / Spec.TileRowSpacing), Nu, 400);
				for (int32 iu = 0; iu < Fine; ++iu)
				{
					CapRing.Add(Sample(p, (double)iu / Fine, 0.0));
				}
			}
			AppendEaveCapRow(Mesh, CapRing,
				FVector2d(EaveMin.X + 0.5 * W, EaveMin.Y + 0.5 * D),
				Spec.TileRowSpacing, Spec.FasciaDrop);
		}

		TArray<int32> CapTris;
		if (EarClipCCW(BaseXY, CapTris))
		{
			for (int32 t = 0; t + 2 < CapTris.Num(); t += 3)
			{
				AppendTri(BaseRing[CapTris[t + 2]], BaseRing[CapTris[t + 1]], BaseRing[CapTris[t]]);
			}
		}
	}

	namespace
	{
		// A small solid run along a line of roof-surface points: 正脊, 垂脊, 戧脊 and 博風板 are all this shape.
		void SweepStripAlongPath(
			FDynamicMesh3& Mesh,
			const TArray<FVector3d>& Path,
			const FVector3d& AcrossHint,
			double HalfWidth, double Below, double Above)
		{
			if (Path.Num() < 2 || HalfWidth <= 0.0 || (Above - -Below) <= 0.0) return;

			const TArray<FVector2d> Profile = {
				FVector2d(-HalfWidth, -Below), FVector2d(HalfWidth, -Below),
				FVector2d(HalfWidth, Above),   FVector2d(-HalfWidth, Above) };

			TArray<FTransform> Stations;
			Stations.Reserve(Path.Num());
			for (int32 i = 0; i < Path.Num(); ++i)
			{
				// Central difference where there is one, so the frame turns with the curve rather than stepping at each station.
				const FVector3d Prev = Path[FMath::Max(i - 1, 0)];
				const FVector3d Next = Path[FMath::Min(i + 1, Path.Num() - 1)];
				FVector3d T = Next - Prev;
				if (T.SquaredLength() < UE_DOUBLE_KINDA_SMALL_NUMBER) continue;
				T.Normalize();

				FVector3d Across = AcrossHint - T * AcrossHint.Dot(T);
				if (Across.SquaredLength() < UE_DOUBLE_KINDA_SMALL_NUMBER) continue;
				Across.Normalize();

				// MakeFromZX keeps the determinant positive.
				const FQuat Rot = FRotationMatrix::MakeFromZX(
					FVector(T.X, T.Y, T.Z), FVector(Across.X, Across.Y, Across.Z)).ToQuat();
				Stations.Add(FTransform(Rot, FVector(Path[i].X, Path[i].Y, Path[i].Z)));
			}

			if (Stations.Num() >= 2)
			{
				AppendSweptProfile(Mesh, Profile, Stations);
			}
		}
	}

	void AppendXieshanRoof(
		FDynamicMesh3& Mesh,
		const FVector3d& EaveMin,
		const FXieshanRoofSpec& Spec)
	{
		const double W = FMath::Max(Spec.Width, 1.0);
		const double D = FMath::Max(Spec.Depth, 1.0);
		const double Rise = FMath::Max(Spec.Rise, 0.1);
		const double Hy = 0.5 * D;
		const int32 Nu = FMath::Clamp(Spec.EaveSegments, 2, 32);
		const int32 Nv = FMath::Clamp(Spec.SlopeSegments, 2, 16);

		// 收山 as a fraction of the plan run to the ridge.
		const double Ts = FMath::Clamp(
			FMath::Min(Spec.ShouInset, 0.2 * W) / Hy, 0.05, 0.75);
		const double S = Ts * Hy;

		// A row lands exactly on the 收山 line.
		const int32 NvLow = FMath::Clamp(FMath::RoundToInt32(Nv * Ts), 1, Nv - 1);
		const int32 NvUp = Nv - NvLow;

		TArray<double> Vs;
		Vs.Reserve(Nv + 1);
		for (int32 i = 0; i <= NvLow; ++i) Vs.Add(Ts * i / NvLow);
		for (int32 i = 1; i <= NvUp; ++i)  Vs.Add(Ts + (1.0 - Ts) * i / NvUp);

		const double FlareLen = FMath::Clamp(Spec.FlareLength, 0.0, 0.5 * FMath::Min(W, D));
		const double FlareRun = (FlareLen > 0.0) ? FMath::Clamp(Spec.FlareRun, 0.0, 0.8 * FlareLen) : 0.0;
		const double FlareRise = (FlareLen > 0.0) ? FMath::Max(Spec.FlareRise, 0.0) : 0.0;
		const bool bFlare = (FlareRun > 0.0 || FlareRise > 0.0);

		const double BaseZ = EaveMin.Z - FMath::Max(Spec.FasciaDrop, 0.0);

		const FVector2d Corner[4] = {
			FVector2d(0.0, 0.0), FVector2d(W, 0.0), FVector2d(W, D), FVector2d(0.0, D) };
		const double Rt2 = 1.0 / FMath::Sqrt(2.0);
		const FVector2d Diag[4] = {
			FVector2d(-Rt2, -Rt2), FVector2d(Rt2, -Rt2), FVector2d(Rt2, Rt2), FVector2d(-Rt2, Rt2) };

		// Panels run round the eave: 0 faces -Y, 1 faces +X, 2 faces +Y, 3 faces -X.
		const int32 EaveA[4] = { 0, 1, 2, 3 };
		const int32 EaveB[4] = { 1, 2, 3, 0 };

		auto SlopeZ = [&](double v)
		{
			return EaveMin.Z + Rise * Spec.Section.HeightFraction(1.0 - v);
		};

		// One formula for all four panels.
		auto Sample = [&](int32 p, double u, double v)
		{
			const double In = FMath::Min(v, Ts) * Hy;
			const FVector2d Ea = Corner[EaveA[p]];
			const FVector2d Eb = Corner[EaveB[p]];
			const double EaveLen = (Eb - Ea).Length();
			const FVector2d Dir = (Eb - Ea) / FMath::Max(EaveLen, UE_DOUBLE_KINDA_SMALL_NUMBER);
			const FVector2d Nrm(-Dir.Y, Dir.X);   // inward: the eave runs CCW in plan

			const double Along = FMath::Min(In + u * FMath::Max(EaveLen - 2.0 * In, 0.0), EaveLen);
			FVector2d XY = Ea + Dir * Along + Nrm * (v * Hy);
			double Z = SlopeZ(v);

			if (bFlare)
			{
				const double Sa = Along;
				const double Sb = EaveLen - Along;
				const int32 C = (Sa <= Sb) ? EaveA[p] : EaveB[p];
				double Wt = FMath::Clamp(1.0 - FMath::Min(Sa, Sb) / FlareLen, 0.0, 1.0);
				Wt = Wt * Wt * (3.0 - 2.0 * Wt);            // smoothstep, so the eave leaves flat
				// Dead at the 收山 line, not at the ridge. Above it the 垂脊 is vertical in plan and the 山花 is a plane; a flare still bleeding up there would bow the gable face.
				const double G = 1.0 - FMath::Min(v / Ts, 1.0);
				XY += Diag[C] * (Wt * G * G * FlareRun);
				Z += Wt * G * G * FlareRise;
			}

			return FVector3d(EaveMin.X + XY.X, EaveMin.Y + XY.Y, Z);
		};

		// Returns the triangle it made, or -1 when it declined to.
		auto AppendTri = [&](int32 a, int32 b, int32 c) -> int32
		{
			if (a == b || b == c || a == c) return -1;
			const FVector3d A = Mesh.GetVertex(a), B = Mesh.GetVertex(b), C = Mesh.GetVertex(c);
			if (((B - A).Cross(C - A)).SquaredLength() < 1e-8) return -1;
			return Mesh.AppendTriangle(a, b, c);
		};

		// Slopes.
		EnsureUVLayer(Mesh);
		UE::Geometry::FDynamicMeshUVOverlay* UV = Mesh.Attributes()->PrimaryUV();
		const double UVScale = 1.0 / FMath::Max(Spec.TileRowSpacing, 0.01);

		for (int32 p = 0; p < 4; ++p)
		{
			const bool bLong = (p == 0 || p == 2);
			const int32 Rows = bLong ? Nv : NvLow;
			const FVector2d Ea = Corner[EaveA[p]];
			const FVector2d EDir = (Corner[EaveB[p]] - Ea).GetSafeNormal();

			TArray<double> ArcV;
			ArcV.Reserve(Rows + 1);
			{
				double Arc = 0.0;
				ArcV.Add(0.0);
				for (int32 iv = 1; iv <= Rows; ++iv)
				{
					Arc += (Sample(p, 0.5, Vs[iv])
						- Sample(p, 0.5, Vs[(iv - 1)])).Length();
					ArcV.Add(Arc);
				}
			}

			TArray<int32> Grid, UvGrid;
			Grid.Reserve((Nu + 1) * (Rows + 1));
			UvGrid.Reserve((Nu + 1) * (Rows + 1));
			for (int32 iv = 0; iv <= Rows; ++iv)
			{
				for (int32 iu = 0; iu <= Nu; ++iu)
				{
					const FVector3d P = Sample(p, (double)iu / Nu, Vs[iv]);
					Grid.Add(Mesh.AppendVertex(P));

					const double Along = FVector2d(P.X - EaveMin.X - Ea.X, P.Y - EaveMin.Y - Ea.Y)
						.Dot(EDir);
					UvGrid.Add(UV ? UV->AppendElement(FVector2f(
						(float)(Along * UVScale), (float)(ArcV[iv] * UVScale))) : -1);
				}
			}
			auto At = [&](int32 iu, int32 iv) { return Grid[iv * (Nu + 1) + iu]; };
			auto UvAt = [&](int32 iu, int32 iv) { return UvGrid[iv * (Nu + 1) + iu]; };

			for (int32 iv = 0; iv < Rows; ++iv)
			{
				for (int32 iu = 0; iu < Nu; ++iu)
				{
					const int32 t0 = AppendTri(At(iu, iv), At(iu + 1, iv), At(iu + 1, iv + 1));
					const int32 t1 = AppendTri(At(iu, iv), At(iu + 1, iv + 1), At(iu, iv + 1));
					if (UV && t0 >= 0) UV->SetTriangle(t0, UE::Geometry::FIndex3i(
						UvAt(iu, iv), UvAt(iu + 1, iv), UvAt(iu + 1, iv + 1)));
					if (UV && t1 >= 0) UV->SetTriangle(t1, UE::Geometry::FIndex3i(
						UvAt(iu, iv), UvAt(iu + 1, iv + 1), UvAt(iu, iv + 1)));
				}
			}
		}

		// 山花, one at each end: the vertical face between the two rakes, on the end panel's top edge.
		auto AppendGableFace = [&](int32 FrontPanel, double FrontU, int32 BackPanel, double BackU,
								   int32 SillPanel, bool bSillReversed, const FVector3d& Outward)
		{
			TArray<FVector3d> Loop;                       // front foot -> ridge -> back foot -> sill
			Loop.Reserve(2 * NvUp + Nu + 2);
			for (int32 iv = NvLow; iv <= Nv; ++iv) Loop.Add(Sample(FrontPanel, FrontU, Vs[iv]));
			for (int32 iv = Nv - 1; iv >= NvLow; --iv) Loop.Add(Sample(BackPanel, BackU, Vs[iv]));
			// Interior sill points only: the two feet are already in the loop as the rakes' ends.
			for (int32 iu = 1; iu < Nu; ++iu)
			{
				const double u = bSillReversed ? 1.0 - (double)iu / Nu : (double)iu / Nu;
				Loop.Add(Sample(SillPanel, u, Ts));
			}
			if (Loop.Num() < 3) return;

			// The face stands in a plane of constant X, so (Y, Z) is a faithful projection.
			TArray<FVector2d> Flat;
			Flat.Reserve(Loop.Num());
			for (const FVector3d& P : Loop) Flat.Add(FVector2d(P.Y, P.Z));

			TArray<int32> Order;
			Order.Reserve(Loop.Num());
			for (int32 i = 0; i < Loop.Num(); ++i) Order.Add(i);
			if (SignedArea2(Flat) < 0.0)
			{
				Algo::Reverse(Flat);
				Algo::Reverse(Order);
			}

			TArray<int32> Tris;
			if (!EarClipCCW(Flat, Tris)) return;

			TArray<int32> Verts;
			Verts.Reserve(Loop.Num());
			for (const FVector3d& P : Loop) Verts.Add(Mesh.AppendVertex(P));

			// Orientation is decided once for the whole face rather than per triangle.
			FVector3d FaceN = FVector3d::Zero();
			for (int32 i = 0; i < Order.Num(); ++i)
			{
				const FVector3d& P0 = Loop[Order[i]];
				const FVector3d& P1 = Loop[Order[(i + 1) % Order.Num()]];
				FaceN += FVector3d(
					(P0.Y - P1.Y) * (P0.Z + P1.Z),
					(P0.Z - P1.Z) * (P0.X + P1.X),
					(P0.X - P1.X) * (P0.Y + P1.Y));
			}
			const bool bFlip = (FaceN.Dot(Outward) < 0.0);

			for (int32 t = 0; t + 2 < Tris.Num(); t += 3)
			{
				const int32 a = Verts[Order[Tris[t]]];
				const int32 b = Verts[Order[Tris[t + 1]]];
				const int32 c = Verts[Order[Tris[t + 2]]];
				if (bFlip) Mesh.AppendTriangle(a, c, b);
				else       Mesh.AppendTriangle(a, b, c);
			}
		};

		// The left face is the front panel's u = 0 edge and the back panel's u = 1 edge; both land on x = 收山 above the sill.
		AppendGableFace(0, 0.0, 2, 1.0, 3, false, FVector3d(-1.0, 0.0, 0.0));
		AppendGableFace(0, 1.0, 2, 0.0, 1, true,  FVector3d(1.0, 0.0, 0.0));

		// The eave loop and the flat base under it.
		TArray<FVector2d> BaseXY;
		TArray<int32> EaveRing, BaseRing;
		BaseXY.Reserve(4 * Nu);
		for (int32 p = 0; p < 4; ++p)
		{
			for (int32 iu = 0; iu < Nu; ++iu)
			{
				const FVector3d P = Sample(p, (double)iu / Nu, 0.0);
				EaveRing.Add(Mesh.AppendVertex(P));
				BaseRing.Add(Mesh.AppendVertex(FVector3d(P.X, P.Y, BaseZ)));
				BaseXY.Add(FVector2d(P.X, P.Y));
			}
		}

		const int32 Ring = EaveRing.Num();
		for (int32 i = 0; i < Ring; ++i)
		{
			const int32 j = (i + 1) % Ring;
			AppendTri(BaseRing[i], BaseRing[j], EaveRing[i]);
			AppendTri(BaseRing[j], EaveRing[j], EaveRing[i]);
		}

		// 勾頭 round the eave, on a polyline sampled finely enough for the spacing.
		if (Spec.bEaveCaps && Spec.FasciaDrop > 0.0 && Spec.TileRowSpacing > 0.0)
		{
			TArray<FVector3d> CapRing;
			for (int32 p = 0; p < 4; ++p)
			{
				const double EaveLen = (p == 0 || p == 2) ? W : D;
				const int32 Fine = FMath::Clamp(
					FMath::CeilToInt32(EaveLen / Spec.TileRowSpacing), Nu, 400);
				for (int32 iu = 0; iu < Fine; ++iu)
				{
					CapRing.Add(Sample(p, (double)iu / Fine, 0.0));
				}
			}
			AppendEaveCapRow(Mesh, CapRing,
				FVector2d(EaveMin.X + 0.5 * W, EaveMin.Y + 0.5 * D),
				Spec.TileRowSpacing, Spec.FasciaDrop);
		}

		TArray<int32> CapTris;
		if (EarClipCCW(BaseXY, CapTris))
		{
			for (int32 t = 0; t + 2 < CapTris.Num(); t += 3)
			{
				AppendTri(BaseRing[CapTris[t + 2]], BaseRing[CapTris[t + 1]], BaseRing[CapTris[t]]);
			}
		}

		// 正脊 along the top, 垂脊 down each rake, 戧脊 out along each corner hip.
		if (Spec.RidgeWidth > 0.0 && Spec.RidgeHeight > 0.0)
		{
			const double HalfW = 0.5 * Spec.RidgeWidth;
			const double Below = 0.35 * Spec.RidgeHeight;
			const double Above = 0.65 * Spec.RidgeHeight;

			// 正脊, but not on a 捲棚, where the rounded crown is what stands in for it.
			if (Spec.Section.ApexRoll <= 0.0)
			{
				TArray<FVector3d> RidgeLine = {
					Sample(0, 0.0, 1.0), Sample(0, 1.0, 1.0) };
				SweepStripAlongPath(Mesh, RidgeLine, FVector3d(0.0, 1.0, 0.0), HalfW, Below, Above);
			}

			// One rake and one hip per corner.
			auto Line = [&](int32 p, double u, int32 iv0, int32 iv1)
			{
				TArray<FVector3d> Path;
				const int32 Step = (iv1 >= iv0) ? 1 : -1;
				for (int32 iv = iv0; iv != iv1 + Step; iv += Step) Path.Add(Sample(p, u, Vs[iv]));
				return Path;
			};

			for (int32 p = 0; p < 4; ++p)
			{
				const bool bLong = (p == 0 || p == 2);
				const FVector3d Across = bLong ? FVector3d(1.0, 0.0, 0.0) : FVector3d(0.0, 1.0, 0.0);
				for (int32 e = 0; e <= 1; ++e)
				{
					const double u = (double)e;
					// 戧脊: the corner hip, eave up to the 收山 corner. Every panel has two.
					SweepStripAlongPath(Mesh, Line(p, u, 0, NvLow),
						bLong ? FVector3d(0.0, 1.0, 0.0) : FVector3d(1.0, 0.0, 0.0),
						HalfW, Below, Above);
					// 垂脊: the rake, 收山 corner up to the ridge. Only the long panels have one.
					if (bLong)
					{
						SweepStripAlongPath(Mesh, Line(p, u, NvLow, Nv), Across, HalfW, Below, Above);
					}
				}
			}
		}

		// 博風板 down each rake, standing proud of the gable face it borders.
		if (Spec.BargeThickness > 0.0 && Spec.BargeDepth > 0.0)
		{
			auto Barge = [&](int32 p, double u)
			{
				TArray<FVector3d> Path;
				for (int32 iv = NvLow; iv <= Nv; ++iv) Path.Add(Sample(p, u, Vs[iv]));
				// Across is the gable face's own normal.
				SweepStripAlongPath(Mesh, Path, FVector3d(1.0, 0.0, 0.0),
					0.5 * Spec.BargeThickness, 0.8 * Spec.BargeDepth, 0.2 * Spec.BargeDepth);
			};
			Barge(0, 0.0); Barge(2, 1.0);
			Barge(0, 1.0); Barge(2, 0.0);
		}
	}

	void AppendHipRoof(
		FDynamicMesh3& Mesh,
		const FVector3d& EaveMin,
		const FVector3d& EaveMax,
		double RidgeHeight)
	{
		const double W = EaveMax.X - EaveMin.X;
		const double D = EaveMax.Y - EaveMin.Y;
		if (W <= 0.0 || D <= 0.0 || RidgeHeight <= 0.0) return;

		const double Ze = EaveMin.Z;
		const double Zr = Ze + RidgeHeight;

		const FVector3d ELF{EaveMin.X, EaveMin.Y, Ze};
		const FVector3d ERF{EaveMax.X, EaveMin.Y, Ze};
		const FVector3d ERB{EaveMax.X, EaveMax.Y, Ze};
		const FVector3d ELB{EaveMin.X, EaveMax.Y, Ze};

		const int32 elf = Mesh.AppendVertex(ELF);
		const int32 erf = Mesh.AppendVertex(ERF);
		const int32 erb = Mesh.AppendVertex(ERB);
		const int32 elb = Mesh.AppendVertex(ELB);

		// Base (-Z): closes the underside of the overhang so it isn't see-through.
		Mesh.AppendTriangle(elf, erb, erf);
		Mesh.AppendTriangle(elf, elb, erb);

		if (W >= D)
		{
			// Ridge along X, length = W - D (equal pitch on all 4 sides)
			const double Cx = 0.5 * (EaveMin.X + EaveMax.X);
			const double Cy = 0.5 * (EaveMin.Y + EaveMax.Y);
			const double Half = 0.5 * (W - D);
			const FVector3d RmX{Cx - Half, Cy, Zr};
			const FVector3d RpX{Cx + Half, Cy, Zr};
			const int32 rm = Mesh.AppendVertex(RmX);
			const int32 rp = Mesh.AppendVertex(RpX);

			// Front (-Y) trapezoid
			Mesh.AppendTriangle(elf, erf, rp);
			Mesh.AppendTriangle(elf, rp, rm);
			// Back (+Y) trapezoid
			Mesh.AppendTriangle(erb, elb, rm);
			Mesh.AppendTriangle(erb, rm, rp);
			// Left (-X) triangle
			Mesh.AppendTriangle(elb, elf, rm);
			// Right (+X) triangle
			Mesh.AppendTriangle(erf, erb, rp);
		}
		else
		{
			// Ridge along Y, length = D - W
			const double Cx = 0.5 * (EaveMin.X + EaveMax.X);
			const double Cy = 0.5 * (EaveMin.Y + EaveMax.Y);
			const double Half = 0.5 * (D - W);
			const FVector3d RmY{Cx, Cy - Half, Zr};
			const FVector3d RpY{Cx, Cy + Half, Zr};
			const int32 rm = Mesh.AppendVertex(RmY);
			const int32 rp = Mesh.AppendVertex(RpY);

			// Front (-Y) triangle
			Mesh.AppendTriangle(elf, erf, rm);
			// Back (+Y) triangle
			Mesh.AppendTriangle(erb, elb, rp);
			// Left (-X) trapezoid
			Mesh.AppendTriangle(elf, rm, rp);
			Mesh.AppendTriangle(elf, rp, elb);
			// Right (+X) trapezoid
			Mesh.AppendTriangle(erf, erb, rp);
			Mesh.AppendTriangle(erf, rp, rm);
		}
	}
}
