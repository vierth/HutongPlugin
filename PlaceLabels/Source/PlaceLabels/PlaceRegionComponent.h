#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "PlaceLabelTypes.h"
#include "PlaceRegionComponent.generated.h"

// A named place: a 2D polygon plus the labels shown to the player standing in it.
UCLASS(ClassGroup = (PlaceLabels), meta = (BlueprintSpawnableComponent))
class PLACELABELS_API UPlaceRegionComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UPlaceRegionComponent();

	// Polygon vertices in component-local XY, in order, with the closing edge implied.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape")
	TArray<FVector2D> LocalPoints;

	// Off by default, which makes the region infinite in Z.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape")
	bool bUseHeightBounds = false;

	// World Z, not local: a floor's height should not shift when the actor is nudged upward.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape",
		meta = (EditCondition = "bUseHeightBounds", Units = "cm"))
	double MinZ = -100000.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape",
		meta = (EditCondition = "bUseHeightBounds", Units = "cm"))
	double MaxZ = 100000.0;

	// This place's own name — 大柵欄, not 大柵欄胡同. The type carries the 胡同 part.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Place Label")
	FPlaceName Name;

	// District, hutong, compound, temple.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Place Label")
	TObjectPtr<UPlaceLabelTypeAsset> Type;

	// Anything else the readout might want to show. Free-form; the plugin never reads it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Place Label", meta = (MultiLine = true))
	FText Note;

	// Where this region's name and extent were read: a sheet of the 全圖, a gazetteer, a street
	// sign, a conversation. Free text, and the half of the answer the outline cannot give.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Metadata", meta = (MultiLine = true))
	FText Source;

	// How far that source actually says this, on the five-point scale above.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Metadata")
	EPlaceConfidence Confidence = EPlaceConfidence::Attested;

	// Stable identity across an export and the import that comes back.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Place Label", AdvancedDisplay)
	FGuid RegionId;

	// Assigns an id if this region predates the field.
	bool EnsureRegionId();

	// Placed but not yet fully described: no type, or no name in any of the three forms.
	bool HasAuthoringWarning() const { return !Type || Name.IsEmpty(); }

	// Set this to override the derived parent when the geometry lies.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hierarchy")
	TObjectPtr<AActor> ExplicitParent;

	// Whether the derived parent is recomputed when this region is drawn, moved or edited.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hierarchy")
	bool bAutoParent = true;

	// Result of the last auto-resolution, baked at edit time so starting a level does not mean solving every region's adjacency query.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hierarchy")
	TObjectPtr<AActor> DerivedParent;

	// The explicit parent's region component if one is set, otherwise the derived parent's.
	UFUNCTION(BlueprintCallable, Category = "Hierarchy")
	UPlaceRegionComponent* GetEffectiveParent() const;

	// World-space polygon with the component transform already applied. Rebuilt by RebuildCache.
	const TArray<FVector2D>& GetWorldPoints2D() const { return CachedWorldPoints; }
	const FBox2D& GetWorldBounds2D() const { return CachedWorldBounds; }

	// Unsigned. Used to break ties between overlapping regions of equal display priority.
	double GetWorldArea() const { return CachedArea; }

	// Point count, height bounds, cheap bounds reject, then the full point-in-polygon test.
	bool ContainsWorldPoint(const FVector& WorldPos) const;

	bool ContainsWorldPoint2D(const FVector2D& WorldXY) const;

	// Recompute the world-space point cache, bounds and area from LocalPoints and the current transform, and push the result to the renderer.
	void RebuildCache();

	// Re-runs the type-driven parenting rule and writes Derived Parent, returning true if the result differed.
	bool RecomputeDerivedParent();

	// Outline colour: the type's editor colour, or white for an untyped region.
	FLinearColor GetOutlineColor() const;

	// Drawn as a solid line rather than the usual dim one. Only the editor sets this.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shape",
		meta = (DisplayName = "Draw Outline In Viewport"))
	bool bDrawOutline = true;

	// **Whether regions are drawn at all.** On while the Place Labels mode is up and off the rest
	// of the time: a region is furniture for the job of drawing regions, and a city's worth of
	// outlines over a level somebody is doing anything else in is a mess in the way of the work.
	// A region selected in the outliner still shows, drawn by its component visualizer.
	static void SetEditorDrawingVisible(bool bVisible);
	static bool IsEditorDrawingVisible();

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

	int32 GetDisplayPriority() const { return Type ? Type->DisplayPriority : 0; }

	FName GetTypeId() const { return Type ? Type->TypeId : NAME_None; }

	virtual void OnComponentCreated() override;
	virtual void OnRegister() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags,
		ETeleportType Teleport = ETeleportType::None) override;
	virtual void PostLoad() override;

	// Self-registration with UPlaceLabelSubsystem.
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif

private:
	// Transient by design: derived from LocalPoints plus the transform.
	TArray<FVector2D> CachedWorldPoints;
	FBox2D CachedWorldBounds = FBox2D(ForceInit);
	double CachedArea = 0.0;
};
