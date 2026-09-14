#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PlaceLabelTypes.generated.h"

class UTexture2D;
class UPlaceRegionComponent;

// A place name in the three forms the readout can show.
USTRUCT(BlueprintType)
struct PLACELABELS_API FPlaceName
{
	GENERATED_BODY()

	// 大柵欄 — the name as written. This is the primary form.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Place Name")
	FText Chinese;

	// Dashilan / Dàshílánr. Romanization, not a translation.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Place Name")
	FText Pinyin;

	// Optional gloss. Leaving this empty is normal; the widget collapses the line.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Place Name")
	FText English;

	bool IsEmpty() const
	{
		return Chinese.IsEmpty() && Pinyin.IsEmpty() && English.IsEmpty();
	}

	// First non-empty of Chinese, Pinyin, English. For log lines and actor labels.
	FText GetDisplayText() const
	{
		if (!Chinese.IsEmpty()) { return Chinese; }
		if (!Pinyin.IsEmpty()) { return Pinyin; }
		return English;
	}
};

// How far a region's name and outline are evidence and how far they are a guess. A lane traced off
// the 乾隆京城全圖 and one drawn from a photograph of a street sign are the same polygon, and
// nothing in the geometry can say afterwards which is which — so the region carries it, beside the
// note saying where it was read.
//
// The values *are* the scale, so `Confidence >= Probable` reads as it looks; hence a zero entry
// that is none of the five. It is hidden from the picker and exists because a reflected enum must
// have one — anything that zero-initialises the field lands on "unknown" rather than on "not
// attested", which would be a confident wrong answer. The same scale as HutongLayout's, deliberately.
UENUM(BlueprintType)
enum class EPlaceConfidence : uint8
{
	// No answer recorded; not offered in the picker.
	Unknown = 0 UMETA(Hidden),

	// No source names this place; the name is here to fill a gap.
	Absent = 1 UMETA(DisplayName = "1 · Not attested"),

	// A guess in the right family: something was called something along here.
	Conjectural = 2 UMETA(DisplayName = "2 · Conjectural"),

	// Inferred from what the neighbouring places are called rather than read anywhere.
	Inferred = 3 UMETA(DisplayName = "3 · Inferred"),

	// Named in a source, but its extent or which place it attaches to had to be read into it.
	Probable = 4 UMETA(DisplayName = "4 · Probable"),

	// Named in a source, in this place, with this extent.
	Attested = 5 UMETA(DisplayName = "5 · Attested in a source"),
};

// How a region finds its parent. Hierarchy is not containment — see UPlaceLabelTypeAsset.
UENUM(BlueprintType)
enum class EPlaceParentRelation : uint8
{
	// The parent's polygon contains this region. A hutong sits inside its district.
	Containing UMETA(DisplayName = "Containing"),

	// The parent's polygon is merely near this region.
	Adjacent UMETA(DisplayName = "Adjacent"),
};

// One kind of place — district, hutong, compound, temple.
UCLASS(BlueprintType)
class PLACELABELS_API UPlaceLabelTypeAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Stable identifier that other types' Parent Types refer to.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName TypeId;

	// The name of the type itself.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FPlaceName TypeLabel;

	// Higher means more specific.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (ClampMin = "0"))
	int32 DisplayPriority = 0;

	// Type Ids this type may hang off, in preference order.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hierarchy")
	TArray<FName> ParentTypes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hierarchy")
	EPlaceParentRelation ParentRelation = EPlaceParentRelation::Containing;

	// Adjacent only: how far this region's boundary may be from the candidate's before the candidate stops counting as its parent.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hierarchy",
		meta = (EditCondition = "ParentRelation == EPlaceParentRelation::Adjacent",
				ClampMin = "0.0", Units = "cm"))
	double ParentSearchRadius = 1500.0;

	// Tint for this type's line in the on-screen readout. The widget decides what to do with it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FLinearColor AccentColor = FLinearColor::White;

	// Outline colour in the editor viewport.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	FLinearColor EditorOutlineColor = FLinearColor(1.0f, 0.9f, 0.15f);

	// Soft: an icon is a texture, and a level with twenty types should not hold twenty textures resident just to answer "what type is this".
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation")
	TSoftObjectPtr<UTexture2D> Icon;
};

// One region in the chain handed to the readout widget.
USTRUCT(BlueprintType)
struct PLACELABELS_API FPlaceLabelEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Place Label")
	FPlaceName Name;

	UPROPERTY(BlueprintReadOnly, Category = "Place Label")
	TObjectPtr<UPlaceLabelTypeAsset> Type;

	// Copied out of Type so a graph can branch on it without a null check.
	UPROPERTY(BlueprintReadOnly, Category = "Place Label")
	FName TypeId;

	UPROPERTY(BlueprintReadOnly, Category = "Place Label")
	FText Note;

	// 0 for the region the player is actually standing in, increasing outward through parents.
	UPROPERTY(BlueprintReadOnly, Category = "Place Label")
	int32 Depth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Place Label")
	TWeakObjectPtr<UPlaceRegionComponent> Region;
};
