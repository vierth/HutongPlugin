#include "Generation/HutongMeshUtils.h"
#include "Misc/ScopeExit.h"
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

	void SetDiagonalPaverUVs(FDynamicMesh3& Mesh, int32 FirstTri, const FVector3d& U, const FVector3d& V, const FVector3d& Centre)
	{
		const float Half = float(0.5 * HutongGen::FloorPaverCm / 100.0);
		EnsureUVLayer(Mesh);
		UE::Geometry::FDynamicMeshUVOverlay* UV = Mesh.Attributes()->PrimaryUV();
		for (int32 tid = FirstTri; UV && tid < Mesh.MaxTriangleID(); ++tid)
		{
			if (!Mesh.IsTriangle(tid)) continue;
			const UE::Geometry::FIndex3i Tri = Mesh.GetTriangle(tid);
			int32 E[3];
			for (int32 c = 0; c < 3; ++c)
			{
				const FVector3d P = Mesh.GetVertex(Tri[c]) - Centre;
				const double A = P.Dot(U), B = P.Dot(V);
				E[c] = UV->AppendElement(FVector2f(float((A + B) * UE_INV_SQRT_2 / 100.0) + Half, float((B - A) * UE_INV_SQRT_2 / 100.0) + Half));
			}
			UV->SetTriangle(tid, UE::Geometry::FIndex3i(E[0], E[1], E[2]));
		}
	}

	void SetFaceUVs(FDynamicMesh3& Mesh, int32 FirstTri, int32 EndTri, const FVector3d& Outward,
		const FVector3d& Origin, const FVector3d& UAxis, double ULength, const FVector3d& VAxis, double VLength,
		double U0, double U1)
	{
		EnsureUVLayer(Mesh);
		UE::Geometry::FDynamicMeshUVOverlay* UV = Mesh.Attributes()->PrimaryUV();
		const double UL = FMath::Max(ULength, 0.01), VL = FMath::Max(VLength, 0.01);
		for (int32 tid = FMath::Max(FirstTri, 0); UV && tid < FMath::Min(EndTri, Mesh.MaxTriangleID()); ++tid)
		{
			// Pre-bake normals point inward.
			if (!Mesh.IsTriangle(tid) || Mesh.GetTriNormal(tid).Dot(Outward) > -0.9) continue;
			const UE::Geometry::FIndex3i Tri = Mesh.GetTriangle(tid);
			int32 E[3];
			for (int32 c = 0; c < 3; ++c)
			{
				const FVector3d P = Mesh.GetVertex(Tri[c]) - Origin;
				E[c] = UV->AppendElement(FVector2f(float(U0 + (U1 - U0) * P.Dot(UAxis) / UL), float(P.Dot(VAxis) / VL)));
			}
			UV->SetTriangle(tid, UE::Geometry::FIndex3i(E[0], E[1], E[2]));
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

	void TagUndersides(FDynamicMesh3& Mesh, int32 FirstTri, int32 EndTri, int32 Slot, double MinNormalZ)
	{
		FSlotScope::FlushCurrent(Mesh);
		Mesh.EnableAttributes();
		Mesh.Attributes()->EnableMaterialID();
		UE::Geometry::FDynamicMeshMaterialAttribute* MatIDs = Mesh.Attributes()->GetMaterialID();
		for (int32 tid = FMath::Max(FirstTri, 0); tid < FMath::Min(EndTri, Mesh.MaxTriangleID()); ++tid)
		{
			// Pre-bake normals point inward: +Z here faces down in the level.
			if (Mesh.IsTriangle(tid) && Mesh.GetTriNormal(tid).Z > MinNormalZ) MatIDs->SetValue(tid, Slot);
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

		// UVs in the building's own frame, carried by the warp (and interpolated by the seam splits): brick
		// runs along a skewed wall instead of being projected across it afterwards.
		FillUnsetUVsBoxProjected(Mesh);

		if (Skew.Mode == EHutongSkewMode::Ends)
		{
			// Split at the zone seam so a primitive spanning the run keeps its body exactly as built.
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

			// Project along the dominant normal axis.
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

	namespace
	{
		thread_local TArray<FSlotScope*> GOpenSlotScopes;

		void TagRange(FDynamicMesh3& Mesh, int32 FirstTriangleID, int32 EndTriangleID, int32 MaterialID)
		{
			Mesh.EnableAttributes();
			Mesh.Attributes()->EnableMaterialID();
			UE::Geometry::FDynamicMeshMaterialAttribute* MatIDs = Mesh.Attributes()->GetMaterialID();
			if (!MatIDs) return;
			const int32 End = FMath::Min(EndTriangleID, Mesh.MaxTriangleID());
			for (int32 tid = FMath::Max(FirstTriangleID, 0); tid < End; ++tid)
			{
				if (Mesh.IsTriangle(tid)) MatIDs->SetValue(tid, MaterialID);
			}
		}
	}

	FSlotScope::FSlotScope(FDynamicMesh3& InMesh, int32 InSlot)
		: Mesh(InMesh), Slot(InSlot)
	{
		if (FSlotScope* Outer = Current(Mesh)) Outer->Flush();
		Flushed = Mesh.MaxTriangleID();
		GOpenSlotScopes.Add(this);
	}

	void FSlotScope::Close()
	{
		if (!bOpen) return;
		const bool bCurrent = Current(Mesh) == this;
		if (bCurrent) Flush();
		bOpen = false;
		GOpenSlotScopes.RemoveSingle(this);
		// The scope beneath takes up from here; what was appended meanwhile was this one's.
		if (bCurrent)
		{
			if (FSlotScope* Outer = Current(Mesh)) Outer->Flushed = Mesh.MaxTriangleID();
		}
	}

	void FSlotScope::Flush()
	{
		TagRange(Mesh, Flushed, Mesh.MaxTriangleID(), Slot);
		Flushed = Mesh.MaxTriangleID();
	}

	FSlotScope* FSlotScope::Current(const FDynamicMesh3& InMesh)
	{
		for (int32 i = GOpenSlotScopes.Num() - 1; i >= 0; --i)
		{
			if (&GOpenSlotScopes[i]->Mesh == &InMesh) return GOpenSlotScopes[i];
		}
		return nullptr;
	}

	void FSlotScope::FlushCurrent(const FDynamicMesh3& InMesh)
	{
		if (FSlotScope* S = Current(InMesh)) S->Flush();
	}

	void SetMaterialIDForTriangleRange(
		FDynamicMesh3& Mesh, int32 FirstTriangleID, int32 EndTriangleID, int32 MaterialID)
	{
		FSlotScope::FlushCurrent(Mesh);
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

	void AppendHexahedron(FDynamicMesh3& Mesh, const FVector3d Corners[8])
	{
		int32 V[8];
		FVector3d Centre = FVector3d::ZeroVector;
		for (int32 i = 0; i < 8; ++i)
		{
			V[i] = Mesh.AppendVertex(Corners[i]);
			Centre += Corners[i] / 8.0;
		}
		// As AppendBox: (b - a) × (c - a) points out of the solid.
		auto Quad = [&](int32 a, int32 b, int32 c, int32 d)
		{
			const FVector3d FaceCentre = 0.25 * (Corners[a] + Corners[b] + Corners[c] + Corners[d]);
			const FVector3d N = (Corners[b] - Corners[a]).Cross(Corners[c] - Corners[a])
				+ (Corners[c] - Corners[a]).Cross(Corners[d] - Corners[a]);
			const bool bOut = N.Dot(FaceCentre - Centre) >= 0.0;
			auto Tri = [&](int32 i, int32 j, int32 k)
			{
				if ((Corners[j] - Corners[i]).Cross(Corners[k] - Corners[i]).SquaredLength() < 1.0e-6) return;
				if (bOut) Mesh.AppendTriangle(V[i], V[j], V[k]); else Mesh.AppendTriangle(V[i], V[k], V[j]);
			};
			Tri(a, b, c);
			Tri(a, c, d);
		};
		Quad(0, 1, 2, 3);
		Quad(4, 5, 6, 7);
		for (int32 i = 0; i < 4; ++i)
		{
			const int32 j = (i + 1) % 4;
			Quad(i, j, j + 4, i + 4);
		}
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
		// Ridge position across the width; 0 or 1 gives a one-sided wedge.
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

	namespace
	{
		double SignedArea2(const TArray<FVector2d>& P);
		bool EarClipCCW(const TArray<FVector2d>& P, TArray<int32>& OutTris);
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
		// Never reaches the ridge.
		const double Trim = FMath::Clamp(FarEaveTrim, 0.0, 0.9 * Ridge);

		TArray<double> Cross;
		{
			const double Span = FMath::Max(Section.HalfSpan(), UE_DOUBLE_KINDA_SMALL_NUMBER);
			const double Scale = Ridge / Span;

			// Crease distances from the ridge.
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
			// Far slope, out to its cut.
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
		UE::Geometry::FIndex2i* OutGableFaceRange,
		const FGableUnderside& Under)
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
		const double CLast = Cross.Last();

		// The underside in (cross, z), eave to far edge: at the eave line inside the eave course, a step
		// down its back face, the soffit rising to the column line, flat between the column lines, and
		// the same again at the rear when there is a rear eave course.
		TArray<FVector2d> Bottom = { FVector2d(0.0, Z0) };
		{
			const double Drop = FMath::Max(Under.Drop, 0.0);
			const double Rise = FMath::Max(Under.InnerRise, -0.5 * Drop);
			const double InStart = FMath::Clamp(Under.InnerStart, 0.0, CLast);
			const double InEnd = (Under.InnerEnd < 0.0) ? CLast : FMath::Clamp(Under.InnerEnd, InStart, CLast);
			const double Lap = FMath::Clamp(Under.FrontLap, 0.0, InStart);
			const double RearLap = FMath::Clamp(Under.RearLap, 0.0, CLast - InEnd);
			const double Cover = FMath::Max(Under.ShellCover, 0.0);
			// Never above the roof's top, or the end face's outline crosses itself and nothing is built.
			auto LevelAt = [&](double C) { return FMath::Min(Z0 + Rise, ProfileZ(C) - 1.0); };
			// Over a 前廊 the underside keeps the level ceiling's depth below the slope at the column line.
			const double VerEnd = (Cover <= 0.0 && Under.VerandaEnd > InStart + 1.0) ? FMath::Min(Under.VerandaEnd, InEnd) : -1.0;
			const double VerCover = (VerEnd > 0.0) ? ProfileZ(InStart) - LevelAt(InStart) : 0.0;
			auto CeilingAt = [&](double C)
			{
				if (Cover > 0.0) return ProfileZ(C) - Cover;
				if (VerEnd > 0.0 && C <= VerEnd + 1e-6) return ProfileZ(C) - VerCover;
				return LevelAt(C);
			};
			if (Drop > 0.0 && Lap > 0.0 && InStart > Lap + 1.0)
			{
				Bottom.Add(FVector2d(Lap, Z0));
				Bottom.Add(FVector2d(Lap, Z0 - Drop));
				Bottom.Add(FVector2d(InStart, CeilingAt(InStart)));
			}
			// Over the 廊 the underside follows the slope's creases to the wall on the 金柱 line, then steps down
			// its face to the rooms' level ceiling.
			if (VerEnd > 0.0)
			{
				if (Bottom.Last().X < InStart - 1e-6) Bottom.Add(FVector2d(InStart, CeilingAt(InStart)));
				for (const double C : Cross)
				{
					if (C > InStart + 1e-3 && C < VerEnd - 1e-3) Bottom.Add(FVector2d(C, CeilingAt(C)));
				}
				Bottom.Add(FVector2d(VerEnd, CeilingAt(VerEnd)));
				Bottom.Add(FVector2d(VerEnd, LevelAt(VerEnd)));
			}
			// A shell follows the slope's creases between the column lines.
			if (Cover > 0.0)
			{
				if (Bottom.Last().X < InStart - 1e-6) Bottom.Add(FVector2d(InStart, CeilingAt(InStart)));
				for (const double C : Cross)
				{
					if (C > InStart + 1e-3 && C < InEnd - 1e-3) Bottom.Add(FVector2d(C, CeilingAt(C)));
				}
			}
			if (Drop > 0.0 && RearLap > 0.0 && CLast - RearLap > InEnd + 1.0)
			{
				Bottom.Add(FVector2d(InEnd, CeilingAt(InEnd)));
				Bottom.Add(FVector2d(CLast - RearLap, Z0 - Drop));
				Bottom.Add(FVector2d(CLast - RearLap, Z0));
			}
			else if ((Cover > 0.0 || FMath::Abs(Rise) > 0.0 || VerEnd > 0.0) && InEnd < CLast - 1.0)
			{
				// The ceiling stays level (or on the slope) to the rear column line, then meets the rear edge.
				Bottom.Add(FVector2d(InEnd, CeilingAt(InEnd)));
			}
			Bottom.Add(FVector2d(CLast, Z0));
		}

		TArray<int32> TopA, TopB, BotA, BotB;   // A at Along = 0, B at Along = Length
		for (double C : Cross)
		{
			const double Z = ProfileZ(C);
			TopA.Add(Mesh.AppendVertex(MakeVertex(0.0, C, Z)));
			TopB.Add(Mesh.AppendVertex(MakeVertex(Length, C, Z)));
		}
		for (int32 k = 0; k < Bottom.Num(); ++k)
		{
			// Corners on the slope's own edge share its vertices.
			const int32 Top = (k == 0) ? 0 : (k == Bottom.Num() - 1) ? Cross.Num() - 1 : -1;
			const bool bShared = Top >= 0 && FMath::Abs(ProfileZ(Cross[Top]) - Bottom[k].Y) <= UE_DOUBLE_KINDA_SMALL_NUMBER;
			BotA.Add(bShared ? TopA[Top] : Mesh.AppendVertex(MakeVertex(0.0, Bottom[k].X, Bottom[k].Y)));
			BotB.Add(bShared ? TopB[Top] : Mesh.AppendVertex(MakeVertex(Length, Bottom[k].X, Bottom[k].Y)));
		}

		// Underside as a strip: from below a roof is rafters and 望板.
		{
			FSlotScope UnderTag(Mesh, HutongGen::MatSlot_Wood);
			for (int32 k = 0; k < Bottom.Num() - 1; ++k)
			{
				if ((Bottom[k + 1] - Bottom[k]).SquaredLength() < 1e-8) continue;
				Mesh.AppendTriangle(BotA[k], BotA[k + 1], BotB[k]);
				Mesh.AppendTriangle(BotB[k], BotA[k + 1], BotB[k + 1]);
			}
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

		// v folded at the apex: both slopes count up to the ridge, so tile lower edges match front and rear.
		int32 Apex = 0;
		for (int32 k = 1; k < Cross.Num(); ++k)
		{
			if (ProfileZ(Cross[k]) > ProfileZ(Cross[Apex])) Apex = k;
		}
		const double ApexArc = ArcV[Apex];

		TArray<int32> UvA, UvB;
		UvA.Reserve(Cross.Num()); UvB.Reserve(Cross.Num());
		for (int32 k = 0; k < Cross.Num(); ++k)
		{
			const float V = (float)((ApexArc - FMath::Abs(ArcV[k] - ApexArc)) * UVScale);
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

		// Gable ends: the section's outline, slope over the top and underside back, ear-clipped.
		const int32 GableFirst = Mesh.MaxTriangleID();
		{
			TArray<FVector2d> Outline;
			TArray<int32> IdxA, IdxB;
			for (int32 k = 0; k < Cross.Num(); ++k)
			{
				Outline.Add(FVector2d(Cross[k], ProfileZ(Cross[k])));
				IdxA.Add(TopA[k]); IdxB.Add(TopB[k]);
			}
			for (int32 k = Bottom.Num() - 1; k >= 0; --k)
			{
				if (BotA[k] == TopA[0] || BotA[k] == TopA.Last()) continue;
				if (FVector2d::DistSquared(Outline.Last(), Bottom[k]) < 1e-8) continue;
				Outline.Add(Bottom[k]);
				IdxA.Add(BotA[k]); IdxB.Add(BotB[k]);
			}
			TArray<int32> Order;
			for (int32 i = 0; i < Outline.Num(); ++i) Order.Add(i);
			TArray<FVector2d> Flat = Outline;
			if (SignedArea2(Flat) < 0.0) { Algo::Reverse(Flat); Algo::Reverse(Order); }
			TArray<int32> Tris;
			// CCW in (cross, z) faces +Along when Along is X, -Along when it is Y.
			const bool bFarKeeps = (AlongAxis == EAxis2D::X);
			if (EarClipCCW(Flat, Tris))
			{
				for (int32 t = 0; t + 2 < Tris.Num(); t += 3)
				{
					int32 a = Order[Tris[t]], b = Order[Tris[t + 1]], c = Order[Tris[t + 2]];
					if (!bFarKeeps) Swap(a, c);
					Mesh.AppendTriangle(IdxA[c], IdxA[b], IdxA[a]);
					Mesh.AppendTriangle(IdxB[a], IdxB[b], IdxB[c]);
				}
			}
		}
		if (OutGableFaceRange) *OutGableFaceRange = UE::Geometry::FIndex2i(GableFirst, Mesh.MaxTriangleID());

		// The cut face closing a trimmed far slope.
		const int32 L = Cross.Num() - 1;
		const int32 BL = Bottom.Num() - 1;
		if (FMath::Abs(ProfileZ(CLast) - Bottom[BL].Y) > UE_DOUBLE_KINDA_SMALL_NUMBER)
		{
			Mesh.AppendTriangle(BotA[BL], TopB[L], BotB[BL]);
			Mesh.AppendTriangle(BotA[BL], TopA[L], TopB[L]);
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
						// A vertex on either polygon edge of the ear is a collinear run, not a blocker.
						if (Cross2(P[ia], P[ib], P[m]) > 0.0 && Cross2(P[ib], P[ic], P[m]) > 0.0
							&& Cross2(P[ic], P[ia], P[m]) >= 0.0) { bEar = false; break; }
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
			if (FMath::Abs(Cross2(P[Idx[0]], P[Idx[1]], P[Idx[2]])) > 1e-6)
			{
				OutTris.Add(Idx[0]); OutTris.Add(Idx[1]); OutTris.Add(Idx[2]);
				return true;
			}
			// The last three are a collinear run: split the triangle across its outer two at the middle one,
			// so the run's vertex stays in the fan and no edge is left open.
			for (int32 k = 0; k < 3; ++k)
			{
				const int32 X = Idx[(k + 2) % 3], M = Idx[k], Z = Idx[(k + 1) % 3];
				if (FVector2d::DotProduct(P[X] - P[M], P[Z] - P[M]) >= 0.0) continue;
				for (int32 t = 0; t + 2 < OutTris.Num(); t += 3)
				{
					for (int32 e = 0; e < 3; ++e)
					{
						const int32 A = OutTris[t + e], B = OutTris[t + (e + 1) % 3], Apex = OutTris[t + (e + 2) % 3];
						if (!((A == X && B == Z) || (A == Z && B == X))) continue;
						OutTris[t] = A; OutTris[t + 1] = M; OutTris[t + 2] = Apex;
						OutTris.Add(M); OutTris.Add(B); OutTris.Add(Apex);
						return true;
					}
				}
			}
			return false;
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

		// Normalised here, not by the caller.
		TArray<FVector2d> P = Profile;
		if (SignedArea2(P) < 0.0)
		{
			Algo::Reverse(P);
		}
		if (FMath::Abs(SignedArea2(P)) < UE_DOUBLE_KINDA_SMALL_NUMBER)
		{
			return;
		}

		// Triangulate before appending: an untriangulable profile appends nothing.
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

		// Net for stations built against the sweep, which turns the solid inside out.
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
		// 勾頭 along a hipped-family eave, which turns and lifts at the corners.
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

				// Outward plan perpendicular.
				FVector2d Out(Tan.Y, -Tan.X);
				if (Out.Dot(FVector2d(P.X, P.Y) - CentreXY) < 0.0) Out = -Out;

				const FVector3d Outward(Out.X, Out.Y, 0.0);
				const FQuat Lay = FQuat::FindBetweenNormals(
					FVector::UpVector, FVector(Outward.X, Outward.Y, 0.0));

				// Sunk into the fascia by part of its radius.
				const FVector3d At = P - Outward * (0.4 * CapR) - FVector3d(0.0, 0.0, 0.5 * FasciaDrop);
				const int32 Mark = Mesh.MaxVertexID();
				AppendCylinder(Mesh, FVector3d::Zero(), CapR, CapLen, HutongGen::RoofTile::EaveCapSides);
				TransformVerticesFrom(Mesh, Mark,
					FTransform(Lay, FVector(At.X, At.Y, At.Z)));
			}
		}

		// Eave course face down to its base, then the soffit from that base up to the column-line
		// rectangle (Overhang in from the unflared eave) at eave height, closed flat inside it. Without
		// an overhang the soffit is flat at the base. The soffit tags itself wood.
		void AppendEaveUnderside(
			FDynamicMesh3& Mesh,
			const FVector3d& EaveMin, double W, double D, int32 Nu,
			TFunctionRef<FVector3d(int32 Panel, double U)> EaveAt,
			double FasciaDrop, double Overhang, double Rise,
			const TFunction<FVector3d(int32 Panel, double U)>& ShellAt = nullptr)
		{
			auto AppendTri = [&](int32 a, int32 b, int32 c)
			{
				if (a == b || b == c || a == c) return;
				const FVector3d A = Mesh.GetVertex(a), B = Mesh.GetVertex(b), C = Mesh.GetVertex(c);
				if (((B - A).Cross(C - A)).SquaredLength() < 1e-8) return;
				Mesh.AppendTriangle(a, b, c);
			};
			const double In = FMath::Clamp(Overhang, 0.0, 0.5 * FMath::Min(W, D) - 1.0);
			const double InnerZ = (In > 0.0) ? EaveMin.Z + FMath::Max(Rise, -0.5 * FasciaDrop) : EaveMin.Z - FasciaDrop;
			const FVector2d Corner[4] = {
				FVector2d(In, In), FVector2d(W - In, In), FVector2d(W - In, D - In), FVector2d(In, D - In) };

			TArray<FVector2d> InnerXY;
			TArray<int32> EaveRing, BaseRing, InnerRing;
			for (int32 p = 0; p < 4; ++p)
			{
				for (int32 iu = 0; iu < Nu; ++iu)
				{
					const double U = (double)iu / Nu;
					const FVector3d P = EaveAt(p, U);
					EaveRing.Add(Mesh.AppendVertex(P));
					// The eave course keeps its depth into a 翼角.
					BaseRing.Add(Mesh.AppendVertex(FVector3d(P.X, P.Y, P.Z - FasciaDrop)));
					const FVector2d Flat = FVector2d(EaveMin.X, EaveMin.Y) + Corner[p] + (Corner[(p + 1) % 4] - Corner[p]) * U;
					// Under a 徹上明造 shell the soffit meets the shell's own row at the column line.
					InnerRing.Add(Mesh.AppendVertex(ShellAt ? ShellAt(p, U) : FVector3d(Flat.X, Flat.Y, InnerZ)));
					InnerXY.Add(Flat);
				}
			}

			const int32 Ring = EaveRing.Num();
			for (int32 i = 0; i < Ring; ++i)
			{
				const int32 j = (i + 1) % Ring;
				AppendTri(BaseRing[i], BaseRing[j], EaveRing[i]);
				AppendTri(BaseRing[j], EaveRing[j], EaveRing[i]);
			}
			// The soffit, timber from below.
			FSlotScope SoffitTag(Mesh, HutongGen::MatSlot_Wood);
			for (int32 i = 0; i < Ring; ++i)
			{
				const int32 j = (i + 1) % Ring;
				AppendTri(InnerRing[i], InnerRing[j], BaseRing[i]);
				AppendTri(InnerRing[j], BaseRing[j], BaseRing[i]);
			}
			if (ShellAt) return;
			TArray<int32> CapTris;
			if (EarClipCCW(InnerXY, CapTris))
			{
				for (int32 t = 0; t + 2 < CapTris.Num(); t += 3)
				{
					AppendTri(InnerRing[CapTris[t + 2]], InnerRing[CapTris[t + 1]], InnerRing[CapTris[t]]);
				}
			}
		}
	}

	namespace
	{
		// 徹上明造: the underside of one slope panel, the panel itself Cover lower (plumb), over the rows Vs
		// (column line up), facing down. Tags itself wood.
		void AppendShellPanel(FDynamicMesh3& Mesh, TFunctionRef<FVector3d(double U, double V)> Sample,
			int32 Nu, const TArray<double>& Vs, double Cover)
		{
			FSlotScope ShellTag(Mesh, HutongGen::MatSlot_Wood);
			TArray<int32> Grid;
			for (const double V : Vs)
			{
				for (int32 iu = 0; iu <= Nu; ++iu) Grid.Add(Mesh.AppendVertex(Sample((double)iu / Nu, V) - FVector3d(0.0, 0.0, Cover)));
			}
			auto At = [&](int32 iu, int32 iv) { return Grid[iv * (Nu + 1) + iu]; };
			auto Tri = [&](int32 a, int32 b, int32 c)
			{
				const FVector3d A = Mesh.GetVertex(a), B = Mesh.GetVertex(b), C = Mesh.GetVertex(c);
				if (((B - A).Cross(C - A)).SquaredLength() > 1e-8) Mesh.AppendTriangle(a, b, c);
			};
			// The slope's own winding reversed: this face looks down.
			for (int32 iv = 0; iv + 1 < Vs.Num(); ++iv)
			{
				for (int32 iu = 0; iu < Nu; ++iu)
				{
					Tri(At(iu, iv), At(iu + 1, iv + 1), At(iu + 1, iv));
					Tri(At(iu, iv), At(iu, iv + 1), At(iu + 1, iv + 1));
				}
			}
		}
	}

	double HipStartV(TFunctionRef<FVector3d(double V)> Hip, double Inset, double VMax)
	{
		const FVector3d C = Hip(0.0);
		auto Far = [&](double V) { const FVector3d P = Hip(V); return FVector2d(P.X - C.X, P.Y - C.Y).Length() >= Inset; };
		double Lo = 0.0, Hi = 0.5 * VMax;
		if (!Far(Hi)) return Hi;
		for (int32 i = 0; i < 24; ++i) { const double M = 0.5 * (Lo + Hi); (Far(M) ? Hi : Lo) = M; }
		return Hi;
	}

	void AppendHippedRoof(
		FDynamicMesh3& Mesh,
		const FVector3d& EaveMin,
		const FHipRoofSpec& Spec,
		TArray<FRoofPanel>* OutPanels)
	{
		const double W = FMath::Max(Spec.Width, 1.0);
		const double D = FMath::Max(Spec.Depth, 1.0);
		const double Rise = FMath::Max(Spec.Rise, 0.1);
		const int32 Nu = FMath::Clamp(Spec.EaveSegments, 2, 64);
		const int32 Nv = FMath::Clamp(Spec.SlopeSegments, 1, 16);

		const double L = FMath::Clamp(Spec.RidgeLength, 0.0, W);
		const double Rx0 = 0.5 * (W - L);
		const double Rx1 = 0.5 * (W + L);
		const double Cy = 0.5 * D;

		// Flare stops short of the shortest eave's middle, or two corners' sweeps collide; run held under the length.
		const double FlareLen = FMath::Clamp(Spec.FlareLength, 0.0, 0.5 * FMath::Min(W, D));
		const double FlareRun = (FlareLen > 0.0) ? FMath::Clamp(Spec.FlareRun, 0.0, 0.8 * FlareLen) : 0.0;
		const double FlareRise = (FlareLen > 0.0) ? FMath::Max(Spec.FlareRise, 0.0) : 0.0;
		const bool bFlare = (FlareRun > 0.0 || FlareRise > 0.0);

		const FVector2d Corner[4] = {
			FVector2d(0.0, 0.0), FVector2d(W, 0.0), FVector2d(W, D), FVector2d(0.0, D) };
		const double Rt2 = 1.0 / FMath::Sqrt(2.0);
		const FVector2d Diag[4] = {
			FVector2d(-Rt2, -Rt2), FVector2d(Rt2, -Rt2), FVector2d(Rt2, Rt2), FVector2d(-Rt2, Rt2) };

		// Panels follow the corner order, each rising to its own ridge stretch.
		const int32 EaveA[4] = { 0, 1, 2, 3 };
		const int32 EaveB[4] = { 1, 2, 3, 0 };
		const FVector2d RidgeA[4] = {
			FVector2d(Rx0, Cy), FVector2d(Rx1, Cy), FVector2d(Rx1, Cy), FVector2d(Rx0, Cy) };
		const FVector2d RidgeB[4] = {
			FVector2d(Rx1, Cy), FVector2d(Rx1, Cy), FVector2d(Rx0, Cy), FVector2d(Rx0, Cy) };

		auto Sample = [=](int32 p, double u, double v)
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

		if (OutPanels)
		{
			for (int32 p = 0; p < 4; ++p)
			{
				const FVector2d Ea = Corner[EaveA[p]], Eb = Corner[EaveB[p]];
				const double EL = (Eb - Ea).Length();
				const FVector2d Dir = (Eb - Ea) / FMath::Max(EL, 1e-6);
				const double RA = (RidgeA[p] - Ea).Dot(Dir), RB = (RidgeB[p] - Ea).Dot(Dir);
				FRoofPanel& Pn = OutPanels->AddDefaulted_GetRef();
				Pn.EaveA = FVector2d(EaveMin.X, EaveMin.Y) + Ea;
				Pn.EaveB = FVector2d(EaveMin.X, EaveMin.Y) + Eb;
				Pn.DiagA = Diag[EaveA[p]];
				Pn.DiagB = Diag[EaveB[p]];
				Pn.FlareLength = bFlare ? FlareLen : 0.0;
				Pn.Sample = [Sample, p](double U, double V) { return Sample(p, U, V); };
				// Along-eave position is linear in U at each V: (1-V)·U·EL + V·(RA + U·(RB-RA)).
				Pn.SolveU = [EL, RA, RB](double A, double V)
				{
					const double Den = (1.0 - V) * EL + V * (RB - RA);
					return (FMath::Abs(Den) < 1e-6) ? -1.0 : (A - V * RA) / Den;
				};
			}
		}

		// Skips degenerate triangles: an end panel's apex row collapses (every panel's with no ridge).
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

		// 徹上明造: the soffit meets the shell's column-line row, and each panel's underside runs on up.
		const double Cover = (Spec.EaveOverhang > 0.0) ? FMath::Max(Spec.ShellCover, 0.0) : 0.0;
		auto ShellV = [&](int32 p) { return FMath::Clamp(Spec.EaveOverhang / FMath::Max((p == 0 || p == 2) ? Cy : Rx0, 1.0), 0.0, 0.9); };
		TFunction<FVector3d(int32, double)> ShellAt;
		if (Cover > 0.0) ShellAt = [&](int32 p, double U) { return Sample(p, U, ShellV(p)) - FVector3d(0.0, 0.0, Cover); };
		AppendEaveUnderside(Mesh, EaveMin, W, D, Nu,
			[&](int32 p, double U) { return Sample(p, U, 0.0); }, FMath::Max(Spec.FasciaDrop, 0.0), Spec.EaveOverhang, Spec.UndersideRise, ShellAt);
		if (Cover > 0.0)
		{
			for (int32 p = 0; p < 4; ++p)
			{
				TArray<double> Rows;
				for (int32 k = 0; k <= Nv; ++k) Rows.Add(FMath::Lerp(ShellV(p), 1.0, (double)k / Nv));
				AppendShellPanel(Mesh, [&](double U, double V) { return Sample(p, U, V); }, Nu, Rows, Cover);
			}
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

	}

	namespace
	{
		// Small solid strip along roof-surface points: 正脊, 垂脊, 戧脊, 博風板.
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
				// Central difference where possible, so the frame turns smoothly.
				const FVector3d Prev = Path[FMath::Max(i - 1, 0)];
				const FVector3d Next = Path[FMath::Min(i + 1, Path.Num() - 1)];
				FVector3d T = Next - Prev;
				if (T.SquaredLength() < UE_DOUBLE_KINDA_SMALL_NUMBER) continue;
				T.Normalize();

				FVector3d Across = AcrossHint - T * AcrossHint.Dot(T);
				if (Across.SquaredLength() < UE_DOUBLE_KINDA_SMALL_NUMBER) continue;
				Across.Normalize();
				// Profile Y = T × Across must point up, or a path run the other way (a back slope) builds its
				// strip upside down: the 博風板 under the rake, the 垂脊 sunk into the roof.
				if (T.Cross(Across).Z < 0.0) Across = -Across;

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
		const FXieshanRoofSpec& Spec,
		TArray<FRoofPanel>* OutPanels,
		TArray<UE::Geometry::FIndex2i>* OutTimber)
	{
		const double W = FMath::Max(Spec.Width, 1.0);
		const double D = FMath::Max(Spec.Depth, 1.0);
		const double Rise = FMath::Max(Spec.Rise, 0.1);
		const double Hy = 0.5 * D;
		const int32 Nu = FMath::Clamp(Spec.EaveSegments, 2, 64);
		const int32 Nv = FMath::Clamp(Spec.SlopeSegments, 2, 16);

		// 收山 as a fraction of the plan run to the ridge.
		const double Ts = FMath::Clamp(
			FMath::Min(Spec.ShouInset, 0.2 * W) / Hy, 0.05, 0.75);
		const double S = Ts * Hy;

		// Force a row onto the 收山 line.
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

		const FVector2d Corner[4] = {
			FVector2d(0.0, 0.0), FVector2d(W, 0.0), FVector2d(W, D), FVector2d(0.0, D) };
		const double Rt2 = 1.0 / FMath::Sqrt(2.0);
		const FVector2d Diag[4] = {
			FVector2d(-Rt2, -Rt2), FVector2d(Rt2, -Rt2), FVector2d(Rt2, Rt2), FVector2d(-Rt2, Rt2) };

		// Panels run round the eave: 0 faces -Y, 1 faces +X, 2 faces +Y, 3 faces -X.
		const int32 EaveA[4] = { 0, 1, 2, 3 };
		const int32 EaveB[4] = { 1, 2, 3, 0 };

		auto SlopeZ = [=](double v)
		{
			return EaveMin.Z + Rise * Spec.Section.HeightFraction(1.0 - v);
		};

		// One formula for all four panels.
		auto Sample = [=](int32 p, double u, double v)
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
				// Zero at the 收山 line, not the ridge: above it the 山花 is a plane, and flare would bow it.
				const double G = 1.0 - FMath::Min(v / Ts, 1.0);
				XY += Diag[C] * (Wt * G * G * FlareRun);
				Z += Wt * G * G * FlareRise;
			}

			return FVector3d(EaveMin.X + XY.X, EaveMin.Y + XY.Y, Z);
		};

		if (OutPanels)
		{
			for (int32 p = 0; p < 4; ++p)
			{
				const FVector2d Ea = Corner[EaveA[p]], Eb = Corner[EaveB[p]];
				const double EL = (Eb - Ea).Length();
				FRoofPanel& Pn = OutPanels->AddDefaulted_GetRef();
				Pn.EaveA = FVector2d(EaveMin.X, EaveMin.Y) + Ea;
				Pn.EaveB = FVector2d(EaveMin.X, EaveMin.Y) + Eb;
				Pn.DiagA = Diag[EaveA[p]];
				Pn.DiagB = Diag[EaveB[p]];
				Pn.FlareLength = bFlare ? FlareLen : 0.0;
				// End panels stop at the 收山 line, under the 山花.
				Pn.VMax = (p == 0 || p == 2) ? 1.0 : Ts;
				Pn.Sample = [Sample, p](double U, double V) { return Sample(p, U, V); };
				Pn.SolveU = [EL, Ts, Hy](double A, double V)
				{
					const double In = FMath::Min(V, Ts) * Hy;
					const double Span = EL - 2.0 * In;
					return (Span < 1e-6) ? -1.0 : (A - In) / Span;
				};
			}
		}

		// Returns the new triangle, or -1 if degenerate.
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

		// 山花 at each end: vertical face between the rakes, on the end panel's top edge.
		// Outward = the way the face looks. Down and Inset shift it (plumb, and along Outward) for the 山花's
		// inner face under a shell.
		auto AppendGableFace = [&](int32 FrontPanel, double FrontU, int32 BackPanel, double BackU,
								   int32 SillPanel, bool bSillReversed, const FVector3d& Outward,
								   double Down = 0.0, double Inset = 0.0)
		{
			const FVector3d Shift = Outward * Inset - FVector3d(0.0, 0.0, Down);
			TArray<FVector3d> Loop;                       // front foot -> ridge -> back foot -> sill
			Loop.Reserve(2 * NvUp + Nu + 2);
			for (int32 iv = NvLow; iv <= Nv; ++iv) Loop.Add(Sample(FrontPanel, FrontU, Vs[iv]) + Shift);
			for (int32 iv = Nv - 1; iv >= NvLow; --iv) Loop.Add(Sample(BackPanel, BackU, Vs[iv]) + Shift);
			// Interior sill points only; the feet are already in the loop.
			for (int32 iu = 1; iu < Nu; ++iu)
			{
				const double u = bSillReversed ? 1.0 - (double)iu / Nu : (double)iu / Nu;
				Loop.Add(Sample(SillPanel, u, Ts) + Shift);
			}
			if (Loop.Num() < 3) return;

			// Face is at constant X, so (Y, Z) projects faithfully.
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

			// Orient the whole face once, not per triangle.
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

		// Left face: front panel u = 0 and back panel u = 1, both at x = 收山 above the sill. 山花板: timber.
		const int32 GableFirst = Mesh.MaxTriangleID();
		AppendGableFace(0, 0.0, 2, 1.0, 3, false, FVector3d(-1.0, 0.0, 0.0));
		AppendGableFace(0, 1.0, 2, 0.0, 1, true,  FVector3d(1.0, 0.0, 0.0));
		// Under a shell the 山花 is seen from inside too: its inner face, down with the shell, 1.5 cm in.
		const double Cover = (Spec.EaveOverhang > 0.0) ? FMath::Max(Spec.ShellCover, 0.0) : 0.0;
		if (Cover > 0.0)
		{
			AppendGableFace(0, 0.0, 2, 1.0, 3, false, FVector3d(1.0, 0.0, 0.0), Cover, 1.5);
			AppendGableFace(0, 1.0, 2, 0.0, 1, true,  FVector3d(-1.0, 0.0, 0.0), Cover, 1.5);
		}
		if (OutTimber) OutTimber->Emplace(GableFirst, Mesh.MaxTriangleID());

		// 徹上明造: the soffit meets the shell's column-line row, and each panel's underside runs on up.
		const double ShellV = FMath::Clamp(Spec.EaveOverhang / FMath::Max(Hy, 1.0), 0.0, 0.9 * Ts);
		TFunction<FVector3d(int32, double)> ShellAt;
		if (Cover > 0.0) ShellAt = [&](int32 p, double U) { return Sample(p, U, ShellV) - FVector3d(0.0, 0.0, Cover); };
		AppendEaveUnderside(Mesh, EaveMin, W, D, Nu,
			[&](int32 p, double U) { return Sample(p, U, 0.0); }, FMath::Max(Spec.FasciaDrop, 0.0), Spec.EaveOverhang, Spec.UndersideRise, ShellAt);
		if (Cover > 0.0)
		{
			for (int32 p = 0; p < 4; ++p)
			{
				const double Top = (p == 0 || p == 2) ? 1.0 : Ts;
				TArray<double> Rows = { ShellV };
				for (const double V : Vs) if (V > ShellV + 1e-4 && V <= Top + 1e-6) Rows.Add(V);
				AppendShellPanel(Mesh, [&](double U, double V) { return Sample(p, U, V); }, Nu, Rows, Cover);
			}
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


		// 正脊 along the top, 垂脊 down each rake, 戧脊 out along each corner hip.
		if (Spec.RidgeWidth > 0.0 && Spec.RidgeHeight > 0.0)
		{
			const double HalfW = 0.5 * Spec.RidgeWidth;
			const double Below = 0.35 * Spec.RidgeHeight;
			const double Above = 0.65 * Spec.RidgeHeight;

			// No 正脊 on a 捲棚; the crown replaces it.
			if (Spec.Section.ApexRoll <= 0.0)
			{
				TArray<FVector3d> RidgeLine = {
					Sample(0, 0.0, 1.0), Sample(0, 1.0, 1.0) };
				FSlotScope RidgeTag(Mesh, HutongGen::MatSlot_Ridge);
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
					// 戧脊: corner hip, from a ridge height in from the eave corner to the 收山 corner. Two per panel.
					const double V0 = HipStartV([&](double V) { return Sample(p, u, V); }, 1.5 * Spec.RidgeHeight, Ts);
					TArray<FVector3d> Hip = { Sample(p, u, V0) };
					for (int32 iv = 1; iv <= NvLow; ++iv) if (Vs[iv] > V0 + 1e-4) Hip.Add(Sample(p, u, Vs[iv]));
					SweepStripAlongPath(Mesh, Hip,
						bLong ? FVector3d(0.0, 1.0, 0.0) : FVector3d(1.0, 0.0, 0.0),
						HalfW, Below, Above);
					// 垂脊: rake, 收山 corner to ridge. Long panels only.
					if (bLong)
					{
						SweepStripAlongPath(Mesh, Line(p, u, NvLow, Nv), Across, HalfW, Below, Above);
					}
				}
				// 博脊: across the 山花's foot, where the skirt's courses end against it (圖6 山面立面).
				if (!bLong)
				{
					TArray<FVector3d> Foot;
					for (int32 iu = 0; iu <= Nu; ++iu) Foot.Add(Sample(p, (double)iu / Nu, Ts));
					SweepStripAlongPath(Mesh, Foot, FVector3d(1.0, 0.0, 0.0), 0.8 * HalfW, 0.8 * Below, 0.8 * Above);
				}
			}
		}

		// 博風板 down each rake, proud of the gable face. Timber, like the 山花.
		if (Spec.BargeThickness > 0.0 && Spec.BargeDepth > 0.0)
		{
			const int32 BargeFirst = Mesh.MaxTriangleID();
			ON_SCOPE_EXIT { if (OutTimber) OutTimber->Emplace(BargeFirst, Mesh.MaxTriangleID()); };
			auto Barge = [&](int32 p, double u)
			{
				TArray<FVector3d> Path;
				for (int32 iv = NvLow; iv <= Nv; ++iv) Path.Add(Sample(p, u, Vs[iv]));
				// Across = gable face normal.
				SweepStripAlongPath(Mesh, Path, FVector3d(1.0, 0.0, 0.0),
					0.5 * Spec.BargeThickness, 0.8 * Spec.BargeDepth, 0.2 * Spec.BargeDepth);
			};
			Barge(0, 0.0); Barge(2, 1.0);
			Barge(0, 1.0); Barge(2, 0.0);
		}
	}

	void AppendRidgeStrip(
		FDynamicMesh3& Mesh,
		const TArray<FVector3d>& Path,
		const FVector3d& AcrossHint,
		double HalfWidth, double Below, double Above)
	{
		SweepStripAlongPath(Mesh, Path, AcrossHint, HalfWidth, Below, Above);
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

		// Base (-Z) closes the overhang underside.
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

namespace HutongMeshUtils
{
	void AppendRevolvedProfile(
		FDynamicMesh3& Mesh,
		const FVector2d& Centre,
		const TArray<FVector2d>& RZ,
		int32 Segments,
		bool bClosed,
		TArray<int32>* OutEdgeFirstTri)
	{
		const int32 S = FMath::Clamp(Segments, 3, 512);
		const int32 N = RZ.Num();
		if (N < 2) return;
		const int32 FirstTri = Mesh.MaxTriangleID();

		// A ring per point, or one pole on the axis.
		TArray<TArray<int32>> Rings;
		for (const FVector2d& P : RZ)
		{
			TArray<int32>& Ring = Rings.AddDefaulted_GetRef();
			if (P.X <= 1e-4)
			{
				Ring.Add(Mesh.AppendVertex(FVector3d(Centre.X, Centre.Y, P.Y)));
				continue;
			}
			for (int32 k = 0; k < S; ++k)
			{
				const double A = 2.0 * PI * k / S;
				Ring.Add(Mesh.AppendVertex(FVector3d(Centre.X + P.X * FMath::Cos(A), Centre.Y + P.X * FMath::Sin(A), P.Y)));
			}
		}
		auto At = [&](int32 i, int32 k) { return Rings[i].Num() == 1 ? Rings[i][0] : Rings[i][k % S]; };
		auto Tri = [&](int32 a, int32 b, int32 c)
		{
			if (a == b || b == c || a == c) return;
			const FVector3d A = Mesh.GetVertex(a), B = Mesh.GetVertex(b), C = Mesh.GetVertex(c);
			if (((B - A).Cross(C - A)).SquaredLength() < 1e-8) return;
			Mesh.AppendTriangle(a, b, c);
		};

		const int32 Edges = bClosed ? N : N - 1;
		for (int32 e = 0; e < Edges; ++e)
		{
			if (OutEdgeFirstTri) OutEdgeFirstTri->Add(Mesh.MaxTriangleID());
			const int32 i = e, j = (e + 1) % N;
			if (Rings[i].Num() == 1 && Rings[j].Num() == 1) continue;
			for (int32 k = 0; k < S; ++k)
			{
				Tri(At(i, k), At(j, k), At(j, k + 1));
				Tri(At(i, k), At(j, k + 1), At(i, k + 1));
			}
		}
		if (OutEdgeFirstTri) OutEdgeFirstTri->Add(Mesh.MaxTriangleID());

		// Whichever way the chain ran, outward.
		double Volume = 0.0;
		for (int32 tid = FirstTri; tid < Mesh.MaxTriangleID(); ++tid)
		{
			if (!Mesh.IsTriangle(tid)) continue;
			const UE::Geometry::FIndex3i T = Mesh.GetTriangle(tid);
			Volume += Mesh.GetVertex(T.A).Dot(Mesh.GetVertex(T.B).Cross(Mesh.GetVertex(T.C)));
		}
		if (Volume < 0.0)
		{
			for (int32 tid = FirstTri; tid < Mesh.MaxTriangleID(); ++tid)
			{
				if (Mesh.IsTriangle(tid)) Mesh.ReverseTriOrientation(tid);
			}
		}
	}

	void AppendArcSweep(
		FDynamicMesh3& Mesh,
		const FVector2d& Centre, double Radius, double Z,
		double A0, double A1,
		const TArray<FVector2d>& Profile,
		int32 Segments)
	{
		const int32 N = FMath::Max(Segments, 1);
		TArray<FTransform> Stations;
		for (int32 s = 0; s <= N; ++s)
		{
			const double A = FMath::DegreesToRadians(FMath::Lerp(A0, A1, double(s) / N));
			const FVector Radial(FMath::Cos(A), FMath::Sin(A), 0.0);
			// Up × outward = the CCW tangent: a rotation, never a mirror.
			const FMatrix M(FVector::UpVector, Radial, FVector(-FMath::Sin(A), FMath::Cos(A), 0.0),
				FVector(Centre.X + Radius * Radial.X, Centre.Y + Radius * Radial.Y, Z));
			Stations.Add(FTransform(M));
		}
		AppendSweptProfile(Mesh, Profile, Stations);
	}

	double RoundRoofPitch(const FRoundRoofSpec& Spec)
	{
		const int32 Np = FMath::Max(Spec.Panels, 3);
		return 2.0 * FMath::Max(Spec.Radius, 1.0) * FMath::Sin(PI / Np) / FMath::Max(Spec.CoursesPerPanel, 1);
	}

	void AppendRoundRoof(
		FDynamicMesh3& Mesh,
		const FVector3d& Centre,
		const FRoundRoofSpec& Spec,
		UE::Geometry::FIndex2i* OutUnderside)
	{
		const double Re = FMath::Max(Spec.Radius, 1.0);
		const double Z0 = Centre.Z;
		const HutongGen::FHutongRoofSection& Section = Spec.Section;
		auto SurfaceZ = [&](double R) { return Z0 + RoundRoofHeight(Spec, R); };

		// Slope radii: even steps, plus each 步架's breakpoint so the creases fall on the purlins.
		TArray<double> Rs;
		const int32 Nv = FMath::Clamp(Spec.SlopeSegments, 2, 64);
		for (int32 k = 0; k <= Nv; ++k) Rs.Add(Re * k / Nv);
		{
			const double Half = FMath::Max(Section.HalfSpan(), 1.0);
			double FromEave = 0.0;
			for (const double Run : Section.Run)
			{
				FromEave += Run;
				const double R = Re * (Half - FromEave) / Half;
				if (R > 1.0 && R < Re - 1.0) Rs.Add(R);
			}
		}
		Rs.Sort();
		for (int32 i = Rs.Num() - 1; i > 0; --i) if (Rs[i] - Rs[i - 1] < 0.5) Rs.RemoveAt(i);
		Rs[0] = 0.0;
		Rs.Last() = Re;

		const double Fascia = FMath::Max(Spec.FasciaDrop, 0.0);
		const double O = FMath::Clamp(Spec.EaveOverhang, 0.0, 0.9 * Re);
		const double Cover = O > 0.0 ? FMath::Max(Spec.ShellCover, 0.0) : 0.0;
		const double CeilZ = O > 0.0 ? Z0 + Spec.UndersideRise : Z0 - Fascia;

		// Apex out down the slope, the eave course, the soffit in to the column line, the underside back to
		// the axis.
		TArray<FVector2d> Chain;
		for (const double R : Rs) Chain.Add(FVector2d(R, SurfaceZ(R)));
		const int32 SlopeEdges = Chain.Num() - 1;
		if (Fascia > 0.0) Chain.Add(FVector2d(Re, Z0 - Fascia));
		if (O > 0.0) Chain.Add(FVector2d(Re - O, CeilZ));
		if (Cover > 0.0)
		{
			for (int32 i = Rs.Num() - 1; i >= 0; --i)
			{
				if (Rs[i] < Re - O - 0.5) Chain.Add(FVector2d(Rs[i], SurfaceZ(Rs[i]) - Cover));
			}
		}
		else
		{
			Chain.Add(FVector2d(0.0, CeilZ));
		}

		TArray<int32> EdgeTri;
		AppendRevolvedProfile(Mesh, FVector2d(Centre.X, Centre.Y), Chain, Spec.Segments, false, &EdgeTri);
		if (EdgeTri.Num() < SlopeEdges + 1) return;
		if (OutUnderside) *OutUnderside = UE::Geometry::FIndex2i(EdgeTri[SlopeEdges + (Fascia > 0.0 ? 1 : 0)], EdgeTri.Last());

		// Slope UVs: u = courses round the eave, the courses' own lines at half-units; v = arc up the slope.
		EnsureUVLayer(Mesh);
		UE::Geometry::FDynamicMeshUVOverlay* UV = Mesh.Attributes()->PrimaryUV();
		if (!UV) return;
		const double Pitch = FMath::Max(RoundRoofPitch(Spec), 1.0);
		const double Courses = double(FMath::Max(Spec.Panels, 3) * FMath::Max(Spec.CoursesPerPanel, 1));
		TArray<double> Arc;
		Arc.SetNum(Rs.Num());
		Arc.Last() = 0.0;
		for (int32 i = Rs.Num() - 2; i >= 0; --i) Arc[i] = Arc[i + 1] + (Chain[i + 1] - Chain[i]).Length();
		auto ArcAt = [&](double R)
		{
			for (int32 i = 0; i + 1 < Rs.Num(); ++i)
			{
				if (R <= Rs[i + 1]) return FMath::Lerp(Arc[i], Arc[i + 1], (R - Rs[i]) / FMath::Max(Rs[i + 1] - Rs[i], 1e-6));
			}
			return 0.0;
		};
		for (int32 tid = EdgeTri[0]; tid < EdgeTri[SlopeEdges]; ++tid)
		{
			if (!Mesh.IsTriangle(tid)) continue;
			const UE::Geometry::FIndex3i T = Mesh.GetTriangle(tid);
			FVector3d P[3] = { Mesh.GetVertex(T.A), Mesh.GetVertex(T.B), Mesh.GetVertex(T.C) };
			double Th[3], R[3];
			for (int32 c = 0; c < 3; ++c)
			{
				R[c] = FVector2d(P[c].X - Centre.X, P[c].Y - Centre.Y).Length();
				Th[c] = FMath::Atan2(P[c].Y - Centre.Y, P[c].X - Centre.X);
				if (Th[c] < 0.0) Th[c] += 2.0 * PI;
			}
			// Unwrap across the seam; the apex takes its neighbours' angle.
			const double Hi = FMath::Max3(Th[0], Th[1], Th[2]), Lo = FMath::Min3(Th[0], Th[1], Th[2]);
			if (Hi - Lo > PI) for (double& A : Th) if (A < PI) A += 2.0 * PI;
			for (int32 c = 0; c < 3; ++c)
			{
				if (R[c] < 1e-3) Th[c] = 0.5 * (Th[(c + 1) % 3] + Th[(c + 2) % 3]);
			}
			int32 E[3];
			for (int32 c = 0; c < 3; ++c)
			{
				E[c] = UV->AppendElement(FVector2f(float(Th[c] / (2.0 * PI) * Courses), float(ArcAt(R[c]) / Pitch)));
			}
			UV->SetTriangle(tid, UE::Geometry::FIndex3i(E[0], E[1], E[2]));
		}
	}

	double RoundRoofHeight(const FRoundRoofSpec& Spec, double Radius)
	{
		return FMath::Max(Spec.Rise, 0.1) * Spec.Section.HeightFraction(FMath::Clamp(Radius / FMath::Max(Spec.Radius, 1.0), 0.0, 1.0));
	}

	TArray<FRoofPanel> MakeRoundRoofPanels(const FVector3d& Centre, const FRoundRoofSpec& Spec)
	{
		TArray<FRoofPanel> Panels;
		const int32 Np = FMath::Max(Spec.Panels, 3);
		const double Re = FMath::Max(Spec.Radius, 1.0);
		const double Delta = 2.0 * PI / Np;
		const double Chord = 2.0 * Re * FMath::Sin(0.5 * Delta);
		for (int32 p = 0; p < Np; ++p)
		{
			const double T0 = p * Delta;
			FRoofPanel& Pn = Panels.AddDefaulted_GetRef();
			Pn.EaveA = FVector2d(Centre.X + Re * FMath::Cos(T0), Centre.Y + Re * FMath::Sin(T0));
			Pn.EaveB = FVector2d(Centre.X + Re * FMath::Cos(T0 + Delta), Centre.Y + Re * FMath::Sin(T0 + Delta));
			Pn.Sample = [=](double U, double V)
			{
				const double Th = T0 + U * Delta;
				const double R = Re * (1.0 - FMath::Clamp(V, 0.0, 1.0));
				return FVector3d(Centre.X + R * FMath::Cos(Th), Centre.Y + R * FMath::Sin(Th), Centre.Z + RoundRoofHeight(Spec, R));
			};
			Pn.SolveU = [Chord](double A, double V) { return A / FMath::Max(Chord, 1e-6); };
		}
		return Panels;
	}

	void SetSweptUVs(
		FDynamicMesh3& Mesh, int32 FirstVertex, int32 FirstTri,
		const TArray<FTransform>& Stations,
		TFunctionRef<FVector2f(int32 Station, const FVector2d& Local)> UVAt)
	{
		const int32 Count = Mesh.MaxVertexID() - FirstVertex;
		if (Stations.Num() < 2 || Count <= 0 || Count % Stations.Num() != 0) return;
		const int32 N = Count / Stations.Num();
		EnsureUVLayer(Mesh);
		UE::Geometry::FDynamicMeshUVOverlay* UV = Mesh.Attributes()->PrimaryUV();
		if (!UV) return;
		TArray<int32> Element;
		Element.SetNum(Count);
		for (int32 i = 0; i < Count; ++i)
		{
			const int32 S = i / N;
			const FVector Local = Stations[S].InverseTransformPosition(FVector(Mesh.GetVertex(FirstVertex + i)));
			Element[i] = UV->AppendElement(UVAt(S, FVector2d(Local.X, Local.Y)));
		}
		for (int32 tid = FirstTri; tid < Mesh.MaxTriangleID(); ++tid)
		{
			if (!Mesh.IsTriangle(tid)) continue;
			const UE::Geometry::FIndex3i T = Mesh.GetTriangle(tid);
			if (T.A < FirstVertex || T.B < FirstVertex || T.C < FirstVertex) continue;
			UV->SetTriangle(tid, UE::Geometry::FIndex3i(Element[T.A - FirstVertex], Element[T.B - FirstVertex], Element[T.C - FirstVertex]));
		}
	}

	void AppendYZPrism(FDynamicMesh3& Mesh, const TArray<FVector2d>& ProfileYZ, double X0, double X1)
	{
		// Profile X → world Y, profile Y → Z, sweep → X: a rotation, never a mirror.
		const FMatrix Basis(FVector(0, 1, 0), FVector(0, 0, 1), FVector(1, 0, 0), FVector::ZeroVector);
		FTransform A(Basis), B(Basis);
		A.SetTranslation(FVector(X0, 0, 0));
		B.SetTranslation(FVector(X1, 0, 0));
		AppendSweptProfile(Mesh, ProfileYZ, { A, B });
	}
}
