#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/BaySide.h"

// Baked meshes keep the generator's frame and the mesh component carries the turn, so twins facing other ways share one library mesh.
namespace HutongGen
{
	// A quarter turn about Z (counter-clockwise from above, +X toward +Y), then a slide. Exact: no trig.
	struct FMeshTurn
	{
		int32 Quarters = 0;
		FVector2d Offset = FVector2d::ZeroVector;

		bool IsIdentity() const { return Quarters == 0 && Offset.IsZero(); }
		FVector3d Apply(const FVector3d& V) const;
		FVector3d Unapply(const FVector3d& V) const;
		FMeshTurn Then(const FMeshTurn& Next) const;
		FTransform ToTransform() const;
		bool operator==(const FMeshTurn& O) const { return Quarters == O.Quarters && Offset == O.Offset; }

		static FMeshTurn OntoSide(EBaySide Side, double SizeX, double SizeY);
		static FMeshTurn About(const FVector2d& Pivot, int32 Quarters);
	};

	void ApplyMeshTurn(UE::Geometry::FDynamicMesh3& Mesh, const FMeshTurn& Turn);

	// Collects every ApplyMeshTurn made while it lives (one build at a time, game thread).
	class FMeshTurnRecorder
	{
	public:
		FMeshTurnRecorder();
		~FMeshTurnRecorder();
		const FMeshTurn& Get() const { return Recorded; }

	private:
		friend void ApplyMeshTurn(UE::Geometry::FDynamicMesh3&, const FMeshTurn&);
		FMeshTurn Recorded;
		FMeshTurnRecorder* Outer = nullptr;
	};

	// Snaps to a 1/1024 cm grid so a mesh turned and turned back hashes as the unturned one.
	void UnturnAndSnap(UE::Geometry::FDynamicMesh3& Mesh, const FMeshTurn& Turn);
}
