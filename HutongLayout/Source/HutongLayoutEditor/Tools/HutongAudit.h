#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "HutongAudit.generated.h"

class UHutongBuildingComponent;

// What the map's outline of a building traces: its walls, or its roof's edge (the eaves past the walls).
UENUM()
enum class EHutongTracedOutline : uint8
{
	Walls UMETA(DisplayName="Walls (牆身)", ToolTip="Outlines were traced on the walls: a footprint is the building's plan."),
	Eaves UMETA(DisplayName="Eaves (檐口)", ToolTip="Outlines were traced on the roof's edge: the eave overhangs come off the depth before it is judged."),
};

// Bands from HutongCanon::Audit.
namespace HutongAudit
{
	struct FCheck
	{
		FString Name;
		double Value = 0.0;
		HutongCanon::Audit::FBand Band;
		bool bCount = false;

		// Signed distance outside the band (cm, or a count); zero inside.
		double Off() const;
		bool Passes() const { return FMath::IsNearlyZero(Off(), 0.5); }
		// "depth (進深) 5.20 m · 3.50–4.00 m (CANON p.83) · 1.20 m over"
		FString Describe() const;
	};

	struct FResult
	{
		FString Role;
		TArray<FCheck> Checks;
		TArray<FString> Notes;

		bool IsJudged() const { return !Role.IsEmpty() && Checks.Num() > 0; }
		int32 Failing() const;
	};

	FResult Audit(const UHutongBuildingComponent* Building, EHutongTracedOutline Outline);

	TArray<FString> ReadoutLines(const FResult& Result, int32 MaxFailing = 3);

	FString SourceName(HutongCanon::Audit::ESource Source);

	struct FAudited
	{
		const UHutongBuildingComponent* Building = nullptr;
		FResult Result;
	};

	FString SummaryTable(const TArray<FAudited>& Audited);

	// Columns: label, id, role, check, value, band, source, off; cm.
	bool WriteCsv(const TArray<FAudited>& Audited, const FString& Path);
}
