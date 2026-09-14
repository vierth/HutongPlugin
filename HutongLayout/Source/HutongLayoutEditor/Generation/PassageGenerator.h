#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongJiajia.h"
#include "PassageGenerator.generated.h"

// 過道: the roofed slot between a building's gable and the wall beside it, which on a 三進 plan is how the 後院 is reached.
USTRUCT(BlueprintType)
struct FHutongPassageParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Eave Height", UIMin="180", UIMax="320", ClampMin="120", Units="cm", ToolTip="Height of the underside of the roof above the ground, in cm."))
	double EaveHeight = 240.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Roof Rise", UIMin="0", UIMax="120", ClampMin="0", Units="cm", ToolTip="Rise of the ridge above the eave, in cm; zero uses the roof section's own rise."))
	double RoofRise = 0.0;

	double GetEaveHeight() const { return FMath::Max(EaveHeight, 60.0); }
	// Zero is the 三檁 section's own rise over the span.
	double GetRoofRise(double OverSpan) const
	{
		if (RoofRise > 0.0) return RoofRise;
		return FMath::Max(HutongGen::Jiajia::MakeSection(
			EHutongPurlins::Three, 0.5 * FMath::Max(OverSpan, 1.0), 0.0, 0.0).Rise(), 1.0);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage", meta=(DisplayName="Bearing", UIMin="2", UIMax="30", ClampMin="1", Units="cm", ToolTip="How far the roof runs into the wall at each side, in cm."))
	double Bearing = 8.0;

	// Set by the layout; not user-editable. Width is the clear way through, wall face to wall face.
	double Length = 300.0;
	double Width = 200.0;

	// The roof spans the clear way through plus its bearing at each side.
	double GetRoofSpan() const
	{
		return FMath::Max(Width, 1.0) + 2.0 * FMath::Max(Bearing, 0.0);
	}
};

namespace HutongGen
{
	void BuildPassage(UE::Geometry::FDynamicMesh3& Mesh, const FHutongPassageParams& P);
}
