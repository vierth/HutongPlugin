#include "PlaceRegionComponent.h"

#include "PlaceLabelGeometry.h"
#include "PlaceLabelHierarchy.h"
#include "PlaceLabelSubsystem.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "DynamicMeshBuilder.h"
#include "Engine/Engine.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "MeshElementCollector.h"
#include "PrimitiveDrawingUtils.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "UObject/UObjectIterator.h"

namespace
{
	// Set while the Place Labels editor mode is active. Nothing about a region is drawn otherwise.
	bool GPlaceRegionDrawingVisible = false;

	// Enough to read as coverage at a glance without turning the viewport into soup when a district, an area and a hutong all overlap.
	constexpr float FillAlpha = 0.12f;

	// **Drawn a touch above the ground it is traced on.** A region traced onto a flat map plane
	// sits exactly on it, and coplanar is a fight the map plane wins — in the hit proxy buffer as
	// well as on screen, which is what made clicking a region select the plane under it.
	constexpr double DrawLift = 2.0;
}

// Draws the region outline in editor viewports.
class FPlaceRegionSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	explicit FPlaceRegionSceneProxy(const UPlaceRegionComponent* InComponent)
		: FPrimitiveSceneProxy(InComponent)
		, WorldPoints(InComponent->GetWorldPoints2D())
		, OutlineColor(InComponent->GetOutlineColor())
		, PlaneZ(InComponent->GetComponentLocation().Z)
		, MinZ(InComponent->MinZ)
		, MaxZ(InComponent->MaxZ)
		, bUseHeightBounds(InComponent->bUseHeightBounds)
		, bDrawFill(UPlaceRegionComponent::IsEditorDrawingVisible())
	{
		bWillEverBeLit = false;

		// Triangulated once here rather than per frame.
		if (bDrawFill)
		{
			PlaceLabelsGeo::TriangulatePolygon2D(WorldPoints, FillIndices);
		}
	}

	// A region has no mesh and no collision, so without this the only thing in the viewport that
	// can be clicked to select it is its billboard. The default proxy is the owning actor's; the
	// scene keeps it alive through OutHitProxies, which is why the raw pointer is safe to hold.
	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component,
		TArray<TRefCountPtr<HHitProxy>>& OutHitProxies) override
	{
		ActorHitProxy = FPrimitiveSceneProxy::CreateHitProxies(Component, OutHitProxies);
		return ActorHitProxy;
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily, uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override
	{
		const int32 N = WorldPoints.Num();
		if (N < 2)
		{
			return;
		}

		// Selected regions read at full strength; the rest stay quiet enough to draw a whole city without the viewport turning into a wireframe soup.
		const bool bSelected = IsSelected();
		FLinearColor Color = OutlineColor;
		Color.A = bSelected ? 1.0f : 0.5f;
		const float Thickness = bSelected ? 3.0f : 1.5f;

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			if ((VisibilityMap & (1 << ViewIndex)) == 0)
			{
				continue;
			}

			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);

			// Foreground, not the primitive's own group.
			constexpr uint8 DPG = SDPG_Foreground;

			// Interior fill, so it is obvious at a glance which ground already has a label on it.
			if (bDrawFill && FillIndices.Num() >= 3 && GEngine && GEngine->DebugMeshMaterial)
			{
				FLinearColor FillColor = OutlineColor;
				FillColor.A = bSelected ? FillAlpha * 2.0f : FillAlpha;

				// The engine's debug mesh material is the standard translucent-capable path for this; FColoredMaterialRenderProxy feeds it the tint.
				FMaterialRenderProxy* MaterialProxy =
					&Collector.AllocateOneFrameResource<FColoredMaterialRenderProxy>(
						GEngine->DebugMeshMaterial->GetRenderProxy(), FillColor);

				const double FillZ = (bUseHeightBounds ? MinZ : PlaneZ) + DrawLift;

				FDynamicMeshBuilder MeshBuilder(Views[ViewIndex]->GetFeatureLevel());
				for (const FVector2D& P : WorldPoints)
				{
					MeshBuilder.AddVertex(
						FVector3f(static_cast<float>(P.X), static_cast<float>(P.Y),
							static_cast<float>(FillZ)),
						FVector2f::ZeroVector,
						FVector3f(1.0f, 0.0f, 0.0f),
						FVector3f(0.0f, 1.0f, 0.0f),
						FVector3f(0.0f, 0.0f, 1.0f),
						FColor::White);
				}
				for (int32 i = 0; i + 2 < FillIndices.Num(); i += 3)
				{
					MeshBuilder.AddTriangle(FillIndices[i], FillIndices[i + 1], FillIndices[i + 2]);
				}

				// Vertices are already in world space, so the transform is identity. The fill
				// carries the actor's hit proxy: it is the only part of a region big enough to
				// aim at, and clicking the ground a region covers is how it gets selected.
				MeshBuilder.GetMesh(FMatrix::Identity, MaterialProxy, DPG,
					/*bDisableBackfaceCulling*/ true, /*bReceivesDecals*/ false,
					/*bUseSelectionOutline*/ false, ViewIndex, Collector,
					ActorHitProxy ? ActorHitProxy->Id : FHitProxyId());
			}

			// The outline is clickable too, which is what selects a region with the fill off —
			// outside the mode there is nothing else of it on screen.
			PDI->SetHitProxy(ActorHitProxy);

			auto DrawLoop = [&](double Z)
			{
				for (int32 i = 0, j = N - 1; i < N; j = i++)
				{
					PDI->DrawLine(
						FVector(WorldPoints[j].X, WorldPoints[j].Y, Z + DrawLift),
						FVector(WorldPoints[i].X, WorldPoints[i].Y, Z + DrawLift),
						Color, DPG, Thickness);
				}
			};

			if (bUseHeightBounds)
			{
				DrawLoop(MinZ);
				DrawLoop(MaxZ);
				for (int32 i = 0; i < N; ++i)
				{
					PDI->DrawLine(
						FVector(WorldPoints[i].X, WorldPoints[i].Y, MinZ + DrawLift),
						FVector(WorldPoints[i].X, WorldPoints[i].Y, MaxZ + DrawLift),
						Color, DPG, Thickness);
				}
			}
			else
			{
				DrawLoop(PlaneZ);
			}

			PDI->SetHitProxy(nullptr);
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bDynamicRelevance = true;
		Result.bShadowRelevance = false;

		// Deliberately not bEditorPrimitiveRelevance.
		Result.bSeparateTranslucency = Result.bNormalTranslucency = true;
		return Result;
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}

private:
	TArray<FVector2D> WorldPoints;
	TArray<int32> FillIndices;
	FLinearColor OutlineColor;
	double PlaneZ;
	double MinZ;
	double MaxZ;
	bool bUseHeightBounds;
	bool bDrawFill;

	// The owning actor's proxy, made in CreateHitProxies and kept alive by the scene.
	HHitProxy* ActorHitProxy = nullptr;
};

UPlaceRegionComponent::UPlaceRegionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsOnUpdateTransform = true;

	// Editor-only furniture: never rendered to the player, never collided with, never overlapped.
	bHiddenInGame = true;
	// Editor compositing is off on purpose — it suppresses the translucent fill.
	bUseEditorCompositing = false;
	CastShadow = false;
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);

#if WITH_EDITORONLY_DATA
	// The viewport picks the highest-priority hit proxy in the pick region rather than the nearest
	// one, so this is what makes a click on a region select the region rather than the ground it
	// is drawn over. Foreground, not UI: the transform gizmo is HPP_UI and must still win.
	HitProxyPriority = HPP_Foreground;
#endif
}

FPrimitiveSceneProxy* UPlaceRegionComponent::CreateSceneProxy()
{
	// Outline and fill go together. Drawing the outline with the mode down left a city of coloured
	// polygons over every other job done in the level, and half of a drawing is not a lighter
	// version of it — it is the same clutter with the part that says what the region covers gone.
	if (!bDrawOutline || !IsEditorDrawingVisible() || CachedWorldPoints.Num() < 2)
	{
		return nullptr;
	}
	return new FPlaceRegionSceneProxy(this);
}

FLinearColor UPlaceRegionComponent::GetOutlineColor() const
{
	// The colour says what kind of place this is and nothing else.
	if (!Type)
	{
		return FLinearColor(1.0f, 0.55f, 0.1f);
	}
	return Type->EditorOutlineColor;
}

bool UPlaceRegionComponent::EnsureRegionId()
{
	if (RegionId.IsValid())
	{
		return false;
	}
	Modify();
	RegionId = FGuid::NewGuid();
	return true;
}

void UPlaceRegionComponent::OnComponentCreated()
{
	Super::OnComponentCreated();

	// Here rather than in the constructor, which also runs for the class default object and would give every region in the level one guid.
	if (!RegionId.IsValid())
	{
		RegionId = FGuid::NewGuid();
	}
}

bool UPlaceRegionComponent::IsEditorDrawingVisible()
{
	return GPlaceRegionDrawingVisible;
}

void UPlaceRegionComponent::SetEditorDrawingVisible(bool bVisible)
{
	if (GPlaceRegionDrawingVisible == bVisible)
	{
		return;
	}
	GPlaceRegionDrawingVisible = bVisible;

	// The proxy bakes the flag and its triangle list at creation — and with the mode down there is
	// no proxy at all, so this is what makes and unmakes them.
	for (TObjectIterator<UPlaceRegionComponent> It; It; ++It)
	{
		if (It->IsRegistered())
		{
			It->MarkRenderStateDirty();
		}
	}
}

bool UPlaceRegionComponent::RecomputeDerivedParent()
{
	if (!bAutoParent || ExplicitParent)
	{
		return false;
	}

	TArray<UPlaceRegionComponent*> AllRegions;
	PlaceLabelsHierarchy::CollectRegions(GetWorld(), AllRegions);

	UPlaceRegionComponent* Resolved = PlaceLabelsHierarchy::ResolveParentFor(this, AllRegions);
	AActor* ResolvedActor = Resolved ? Resolved->GetOwner() : nullptr;

	if (ResolvedActor == DerivedParent)
	{
		return false;
	}

	Modify();
	DerivedParent = ResolvedActor;
	return true;
}

UPlaceRegionComponent* UPlaceRegionComponent::GetEffectiveParent() const
{
	const AActor* ParentActor = ExplicitParent ? ExplicitParent.Get() : DerivedParent.Get();
	if (!ParentActor)
	{
		return nullptr;
	}
	return ParentActor->FindComponentByClass<UPlaceRegionComponent>();
}

bool UPlaceRegionComponent::ContainsWorldPoint(const FVector& WorldPos) const
{
	if (bUseHeightBounds && (WorldPos.Z < MinZ || WorldPos.Z > MaxZ))
	{
		return false;
	}
	return ContainsWorldPoint2D(FVector2D(WorldPos.X, WorldPos.Y));
}

bool UPlaceRegionComponent::ContainsWorldPoint2D(const FVector2D& WorldXY) const
{
	if (CachedWorldPoints.Num() < 3)
	{
		return false;
	}
	// Bounds first: four compares reject nearly every region on the map before the ray cast.
	if (!CachedWorldBounds.bIsValid || !CachedWorldBounds.IsInside(WorldXY))
	{
		return false;
	}
	return PlaceLabelsGeo::PointInPolygon2D(CachedWorldPoints, WorldXY);
}

void UPlaceRegionComponent::RebuildCache()
{
	const FTransform& Xf = GetComponentTransform();

	CachedWorldPoints.Reset(LocalPoints.Num());
	for (const FVector2D& P : LocalPoints)
	{
		const FVector World = Xf.TransformPosition(FVector(P.X, P.Y, 0.0));
		CachedWorldPoints.Emplace(World.X, World.Y);
	}

	CachedWorldBounds = PlaceLabelsGeo::ComputeBounds2D(CachedWorldPoints);
	CachedArea = FMath::Abs(PlaceLabelsGeo::SignedArea2D(CachedWorldPoints));

	// The proxy bakes world points at creation, so it has to be rebuilt whenever they move.
	MarkRenderStateDirty();
}

void UPlaceRegionComponent::OnRegister()
{
	Super::OnRegister();
	RebuildCache();
}

void UPlaceRegionComponent::OnUpdateTransform(
	EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
	RebuildCache();
	UpdateBounds();
}

void UPlaceRegionComponent::PostLoad()
{
	Super::PostLoad();
	RebuildCache();
}

void UPlaceRegionComponent::BeginPlay()
{
	Super::BeginPlay();

	// The cache is normally built at registration, but a region spawned at runtime may not have had its transform applied yet.
	RebuildCache();

	if (UWorld* World = GetWorld())
	{
		if (UPlaceLabelSubsystem* Subsystem = World->GetSubsystem<UPlaceLabelSubsystem>())
		{
			Subsystem->RegisterRegion(this);
		}
	}
}

void UPlaceRegionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UPlaceLabelSubsystem* Subsystem = World->GetSubsystem<UPlaceLabelSubsystem>())
		{
			Subsystem->UnregisterRegion(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

FBoxSphereBounds UPlaceRegionComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// Without this the actor's bounds collapse to a point.
	if (LocalPoints.Num() == 0)
	{
		return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.0);
	}

	FBox Box(ForceInit);
	for (const FVector2D& P : LocalPoints)
	{
		Box += LocalToWorld.TransformPosition(FVector(P.X, P.Y, 0.0));
	}

	if (bUseHeightBounds)
	{
		// Height bounds are world Z, so they extend the world-space box directly.
		Box.Min.Z = FMath::Min(Box.Min.Z, MinZ);
		Box.Max.Z = FMath::Max(Box.Max.Z, MaxZ);
	}
	else
	{
		// Flat footprints give a zero-thickness box, which some editor paths treat as degenerate.
		Box = Box.ExpandBy(FVector(0.0, 0.0, 1.0));
	}

	return FBoxSphereBounds(Box);
}

#if WITH_EDITOR
void UPlaceRegionComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	RebuildCache();
	UpdateBounds();

	// Interactive means a slider or gizmo drag in progress.
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}

	static const TSet<FName> ReparentingProperties = {
		GET_MEMBER_NAME_CHECKED(UPlaceRegionComponent, LocalPoints),
		GET_MEMBER_NAME_CHECKED(UPlaceRegionComponent, Type),
		GET_MEMBER_NAME_CHECKED(UPlaceRegionComponent, bAutoParent),
		GET_MEMBER_NAME_CHECKED(UPlaceRegionComponent, ExplicitParent),
	};

	const FName ChangedName = PropertyChangedEvent.GetPropertyName();
	if (ReparentingProperties.Contains(ChangedName))
	{
		RecomputeDerivedParent();
	}
}

void UPlaceRegionComponent::PostEditUndo()
{
	Super::PostEditUndo();
	RebuildCache();
	UpdateBounds();
}
#endif
