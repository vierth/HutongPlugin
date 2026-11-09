#include "Generation/HutongMeshTurn.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	namespace
	{
		FMeshTurnRecorder* GMeshTurnRecorder = nullptr;

		FVector2d MeshTurnRotate(const FVector2d& P, int32 Quarters)
		{
			switch (Quarters & 3)
			{
			case 1: return FVector2d(-P.Y, P.X);
			case 2: return FVector2d(-P.X, -P.Y);
			case 3: return FVector2d(P.Y, -P.X);
			default: return P;
			}
		}

		double MeshTurnSnap(double V) { return FMath::RoundToDouble(V * 1024.0) / 1024.0; }
	}

	FVector3d FMeshTurn::Apply(const FVector3d& V) const
	{
		const FVector2d P = MeshTurnRotate(FVector2d(V.X, V.Y), Quarters) + Offset;
		return FVector3d(P.X, P.Y, V.Z);
	}

	FVector3d FMeshTurn::Unapply(const FVector3d& V) const
	{
		const FVector2d P = MeshTurnRotate(FVector2d(V.X, V.Y) - Offset, -Quarters);
		return FVector3d(P.X, P.Y, V.Z);
	}

	FMeshTurn FMeshTurn::Then(const FMeshTurn& Next) const
	{
		FMeshTurn Out;
		Out.Quarters = (Quarters + Next.Quarters) & 3;
		Out.Offset = MeshTurnRotate(Offset, Next.Quarters) + Next.Offset;
		return Out;
	}

	FTransform FMeshTurn::ToTransform() const
	{
		return FTransform(FRotator(0.0, 90.0 * (Quarters & 3), 0.0), FVector(Offset.X, Offset.Y, 0.0));
	}

	FMeshTurn FMeshTurn::OntoSide(EBaySide Side, double SizeX, double SizeY)
	{
		switch (Side)
		{
		case EBaySide::PlusX:  return { 1, FVector2d(SizeX, 0.0) };
		case EBaySide::PlusY:  return { 2, FVector2d(SizeX, SizeY) };
		case EBaySide::MinusX: return { 3, FVector2d(0.0, SizeY) };
		default:               return {};
		}
	}

	FMeshTurn FMeshTurn::About(const FVector2d& Pivot, int32 InQuarters)
	{
		return { InQuarters & 3, Pivot - MeshTurnRotate(Pivot, InQuarters) };
	}

	void ApplyMeshTurn(FDynamicMesh3& Mesh, const FMeshTurn& Turn)
	{
		if (Turn.IsIdentity()) return;
		for (const int32 Vid : Mesh.VertexIndicesItr()) Mesh.SetVertex(Vid, Turn.Apply(Mesh.GetVertex(Vid)));
		if (GMeshTurnRecorder) GMeshTurnRecorder->Recorded = GMeshTurnRecorder->Recorded.Then(Turn);
	}

	FMeshTurnRecorder::FMeshTurnRecorder() : Outer(GMeshTurnRecorder)
	{
		check(IsInGameThread());
		GMeshTurnRecorder = this;
	}

	FMeshTurnRecorder::~FMeshTurnRecorder()
	{
		GMeshTurnRecorder = Outer;
	}

	void UnturnAndSnap(FDynamicMesh3& Mesh, const FMeshTurn& Turn)
	{
		for (const int32 Vid : Mesh.VertexIndicesItr())
		{
			const FVector3d V = Turn.Unapply(Mesh.GetVertex(Vid));
			Mesh.SetVertex(Vid, FVector3d(MeshTurnSnap(V.X), MeshTurnSnap(V.Y), MeshTurnSnap(V.Z)));
		}
	}
}
