#include "PlaceLabelsValidation.h"

#include "PlaceLabelGeometry.h"
#include "PlaceLabelTypes.h"
#include "PlaceRegionComponent.h"
#include "Tools/PlaceRegionEditCore.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "PlaceLabelsValidation"

namespace PlaceLabelsValidation
{

FText DescribeIssue(EIssue Issue)
{
	switch (Issue)
	{
	case EIssue::NoType:           return LOCTEXT("IssueNoType", "No type set");
	case EIssue::NoName:           return LOCTEXT("IssueNoName", "No name in any language");
	case EIssue::SelfIntersecting: return LOCTEXT("IssueSelfIntersecting", "Outline crosses itself");
	case EIssue::Degenerate:       return LOCTEXT("IssueDegenerate", "Outline is too small or has too few corners");
	case EIssue::DuplicateName:    return LOCTEXT("IssueDuplicateName", "Another region of this type has the same name");
	case EIssue::DanglingParent:   return LOCTEXT("IssueDanglingParent", "Explicit parent no longer exists");
	}
	return FText::GetEmpty();
}

bool IsSevere(EIssue Issue)
{
	// The line is whether the region still works at runtime.
	switch (Issue)
	{
	case EIssue::SelfIntersecting:
	case EIssue::Degenerate:
	case EIssue::DanglingParent:
		return true;
	default:
		return false;
	}
}

int32 FReport::IssueCountFor(const UPlaceRegionComponent* Region) const
{
	const int32* Found = IssueCountByRegion.Find(Region);
	return Found ? *Found : 0;
}

void ValidateRegions(const TArray<TWeakObjectPtr<UPlaceRegionComponent>>& Regions,
	double SeamToleranceCm, FReport& OutReport)
{
	OutReport = FReport();

	// Keyed on type plus the primary written name.
	TMap<TPair<FName, FString>, TWeakObjectPtr<UPlaceRegionComponent>> SeenNames;

	auto AddIssue = [&OutReport](const TWeakObjectPtr<UPlaceRegionComponent>& Region, EIssue Issue,
		FText Detail = FText::GetEmpty())
	{
		FRegionIssue Entry;
		Entry.Region = Region;
		Entry.Issue = Issue;
		Entry.Detail = MoveTemp(Detail);
		OutReport.Issues.Add(MoveTemp(Entry));
		++OutReport.IssueCountByRegion.FindOrAdd(Region);
	};

	for (const TWeakObjectPtr<UPlaceRegionComponent>& Weak : Regions)
	{
		UPlaceRegionComponent* Region = Weak.Get();
		if (!Region)
		{
			continue;
		}

		++OutReport.RegionCount;

		if (!Region->Type)
		{
			AddIssue(Weak, EIssue::NoType);
		}
		else
		{
			++OutReport.TypedCount;
		}

		if (Region->Name.IsEmpty())
		{
			AddIssue(Weak, EIssue::NoName);
		}
		else
		{
			++OutReport.NamedCount;

			const FString Key = Region->Name.GetDisplayText().ToString();
			const TPair<FName, FString> NameKey(Region->GetTypeId(), Key);
			if (const TWeakObjectPtr<UPlaceRegionComponent>* Existing = SeenNames.Find(NameKey))
			{
				const UPlaceRegionComponent* Other = Existing->Get();
				const FString OtherLabel = Other && Other->GetOwner()
					? Other->GetOwner()->GetActorLabel()
					: FString(TEXT("another region"));
				AddIssue(Weak, EIssue::DuplicateName,
					FText::Format(LOCTEXT("DuplicateNameDetail", "\"{0}\", same as {1}"),
						FText::FromString(Key), FText::FromString(OtherLabel)));
			}
			else
			{
				SeenNames.Add(NameKey, Weak);
			}
		}

		const TArray<FVector2D>& World = Region->GetWorldPoints2D();
		if (World.Num() < 3
			|| FMath::Abs(PlaceLabelsGeo::SignedArea2D(World)) < PlaceLabelsEdit::MinPolygonAreaCmSq)
		{
			AddIssue(Weak, EIssue::Degenerate);
		}
		else
		{
			TArray<TPair<int32, int32>> Crossings;
			PlaceLabelsGeo::FindSelfIntersections(World, Crossings);
			if (Crossings.Num() > 0)
			{
				AddIssue(Weak, EIssue::SelfIntersecting,
					FText::Format(LOCTEXT("CrossingDetail", "{0} crossing edge pair(s)"),
						FText::AsNumber(Crossings.Num())));
			}
		}

		// Deliberately no "this region found no parent" check.
	}

	// Explicit parents are checked in a second pass so a parent that is merely later in the array does not read as missing.
	for (const TWeakObjectPtr<UPlaceRegionComponent>& Weak : Regions)
	{
		UPlaceRegionComponent* Region = Weak.Get();
		if (!Region)
		{
			continue;
		}
		if (Region->ExplicitParent && !IsValid(Region->ExplicitParent))
		{
			AddIssue(Weak, EIssue::DanglingParent);
		}
	}

	PlaceLabelsTopology::FindSeamIssues(Regions, SeamToleranceCm, OutReport.Seams);
}

void ValidateWorld(UWorld* World, double SeamToleranceCm, FReport& OutReport)
{
	TArray<TWeakObjectPtr<UPlaceRegionComponent>> Regions;
	PlaceLabelsEdit::GatherRegions(World, Regions);
	ValidateRegions(Regions, SeamToleranceCm, OutReport);
}

} // namespace PlaceLabelsValidation

#undef LOCTEXT_NAMESPACE
