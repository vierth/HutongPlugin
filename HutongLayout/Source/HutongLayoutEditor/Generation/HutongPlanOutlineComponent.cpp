#include "Generation/HutongPlanOutlineComponent.h"

#include "DynamicMeshBuilder.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "SceneManagement.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectIterator.h"

namespace
{
	constexpr float FillAlpha = 0.18f;
	// Selected: fill this strong, outline in HutongPlanColours::Selected this thick. Every type
	// colour is a hue; white is none of them, so a selected plan reads apart from its neighbours.
	constexpr float SelectedFillAlpha = 0.38f;
	constexpr float SelectedOutlineThickness = 7.0f;
	// Hatch ticks along the facade: this far apart, this long, standing this far in off the edge
	// (on it they fought the neighbours' lines).
	constexpr double HatchSpacing = 60.0;
	constexpr double HatchDepth = 35.0;
	constexpr double HatchGap = 15.0;
	// Facing chevron behind the ticks in every bay, pointing out through the facade: half-width limits.
	constexpr double ChevronMin = 30.0;
	constexpr double ChevronMax = 120.0;
	// Slightly above ground to avoid z-fighting a map plane at Z = 0.
	constexpr double Lift = 2.0;
	// Plans stack by type in drawing only (HutongPlanColours::Layers); the actor stays where it stands.
	constexpr int32 MaxStackLevel = HutongPlanColours::LayerCount;
	// Perspective depth resolves this at any editing distance; an ortho view's depth range grows
	// with its zoom, so there a step is at least this share of a pixel.
	constexpr double PerspectiveStackStep = 1.0;
	constexpr double OrthoStackStepPixels = 0.5;
	// Edges this close count as one line.
	constexpr double SharedEdgeTolerance = 1.0;
	// Hands the neighbours' edges to the proxy constructor, which takes only the component.
	TArray<TPair<FVector, FVector>> PendingEdges;

	double QuadArea(const FVector C[4])
	{
		double A = 0.0;
		for (int32 i = 0, j = 3; i < 4; j = i++) A += C[j].X * C[i].Y - C[i].X * C[j].Y;
		return 0.5 * FMath::Abs(A);
	}

	double StackStep(const FSceneView& View)
	{
		if (View.IsPerspectiveProjection()) return PerspectiveStackStep;
		const double WorldPerPixel = 2.0 / (View.ViewMatrices.GetViewToClip().M[0][0] * FMath::Max(1, View.UnscaledViewRect.Width()));
		return FMath::Max(PerspectiveStackStep, OrthoStackStepPixels * WorldPerPixel);
	}

	const TCHAR* VisibilitySection = TEXT("/Script/HutongLayoutEditor.HutongPlanOutlineComponent");
	const TCHAR* VisibilityKey = TEXT("bShowPlanOutlines");

	// The proxy cannot stop drawing, so toggling rebuilds every plan's proxy (as PlaceLabels' fill).
	void OnPlanVisibilityChanged(IConsoleVariable* Var)
	{
		GConfig->SetBool(VisibilitySection, VisibilityKey, Var->GetBool(), GEditorPerProjectIni);
		for (TObjectIterator<UHutongPlanOutlineComponent> It; It; ++It)
		{
			It->MarkRenderStateDirty();
		}
	}

	TAutoConsoleVariable<bool> CVarShowPlanOutlines(
		TEXT("hutong.ShowPlanOutlines"),
		true,
		TEXT("Draws the footprint outlines of laid-out (plan-only) Hutong buildings.\n")
		TEXT("Off hides every plan and stops it taking clicks; nothing placed is changed."),
		FConsoleVariableDelegate::CreateStatic(&OnPlanVisibilityChanged),
		ECVF_Default);
}

namespace HutongPlanOutline
{
	static bool GPlansOverBuildings = false;

	bool ArePlansOverBuildings() { return GPlansOverBuildings; }

	void SetPlansOverBuildings(bool bOver)
	{
		if (GPlansOverBuildings == bOver) return;
		GPlansOverBuildings = bOver;
		// The layer is baked into each proxy.
		for (TObjectIterator<UHutongPlanOutlineComponent> It; It; ++It) It->MarkRenderStateDirty();
	}

	bool ArePlansVisible()
	{
		return CVarShowPlanOutlines.GetValueOnGameThread();
	}

	void SetPlansVisible(bool bVisible)
	{
		if (bVisible != ArePlansVisible())
		{
			CVarShowPlanOutlines->Set(bVisible, ECVF_SetByCode);
		}
	}

	void LoadVisibilityFromConfig()
	{
		bool bStored = true;
		if (GConfig && GConfig->GetBool(VisibilitySection, VisibilityKey, bStored, GEditorPerProjectIni))
		{
			SetPlansVisible(bStored);
		}
	}
}

class FHutongPlanOutlineSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	explicit FHutongPlanOutlineSceneProxy(const UHutongPlanOutlineComponent* C)
		: FPrimitiveSceneProxy(C)
		, Footprint(C->Footprint)
		, Skew(C->Skew)
		, bHasFacade(C->bHasFacade)
		, Facade(C->Facade)
		, Openings(C->Openings)
		, bRunAlongY(C->bRunAlongY)
		, BayBoundaries(C->BayBoundaries)
		, bBaysAlongX(C->bBaysAlongX)
		, DoorBay(C->DoorBay)
		, Yielded(PendingEdges)
		, DPG(HutongPlanOutline::ArePlansOverBuildings() ? SDPG_Foreground : SDPG_World)
		, Level(C->GetStackLevel())
	{
		OutlineColor = C->Colour;
		// Facade marks: same hue, heavier, so a plan shows type and facing.
		FacadeColor = C->Colour * FLinearColor(1.0f, 0.72f, 0.45f, 1.0f);
		bWillEverBeLit = false;
		// Parallel gather pops proxies off a shared queue: the lines' draw order would change per frame.
		bSupportsParallelGDME = false;
	}

	// Default hit proxy is the owning actor's, so clicking the fill selects the building.
	virtual HHitProxy* CreateHitProxies(UPrimitiveComponent* Component,
		TArray<TRefCountPtr<HHitProxy>>& OutHitProxies) override
	{
		HHitProxy* Proxy = FPrimitiveSceneProxy::CreateHitProxies(Component, OutHitProxies);
		HitProxyId = Proxy ? Proxy->Id : FHitProxyId();
		return Proxy;
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily, uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override
	{
		const FMatrix& L2W = GetLocalToWorld();
		const double W = Footprint.X, D = Footprint.Y;
		// Points computed on the rectangle, then warped by the corner offsets.
		FVector2D Quad[4];
		HutongFootprint::Corners(Footprint, Skew, Quad);

		const bool bSelected = IsSelected();
		FLinearColor Line = OutlineColor;
		Line.A = bSelected ? 1.0f : 0.7f;
		const float Thickness = bSelected ? 3.0f : 2.0f;
		// The selected plan draws whole, over its neighbours.
		const int32 DrawLevel = bSelected ? MaxStackLevel + 1 : Level;

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			if ((VisibilityMap & (1 << ViewIndex)) == 0) continue;
			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
			const double Z = Lift + DrawLevel * StackStep(*Views[ViewIndex]);
			auto At = [&](double X, double Y)
			{
				const FVector2D Q = HutongFootprint::Map(Footprint, Skew, X, Y);
				return L2W.TransformPosition(FVector(Q.X, Q.Y, Z));
			};
			FVector World[4];
			for (int32 i = 0; i < 4; ++i) World[i] = L2W.TransformPosition(FVector(Quad[i].X, Quad[i].Y, Z));

			// Faint fill: a street of them reads as a tint.
			if (GEngine && GEngine->DebugMeshMaterial)
			{
				FLinearColor Fill = OutlineColor;
				Fill.A = bSelected ? SelectedFillAlpha : FillAlpha;
				FMaterialRenderProxy* Material =
					&Collector.AllocateOneFrameResource<FColoredMaterialRenderProxy>(
						GEngine->DebugMeshMaterial->GetRenderProxy(), Fill);

				FDynamicMeshBuilder Builder(Views[ViewIndex]->GetFeatureLevel());
				for (int32 i = 0; i < 4; ++i)
				{
					// Fill a step below the lines, or thin bay lines z-fight it.
					Builder.AddVertex(FVector3f(World[i] - FVector(0.0, 0.0, 1.5)), FVector2f::ZeroVector,
						FVector3f(1, 0, 0), FVector3f(0, 1, 0), FVector3f(0, 0, 1), FColor::White);
				}
				// Convex quad: either diagonal splits it.
				Builder.AddTriangle(0, 1, 2);
				Builder.AddTriangle(0, 2, 3);
				Builder.GetMesh(FMatrix::Identity, Material, DPG,
					/*bDisableBackfaceCulling*/ true, /*bReceivesDecals*/ false,
					/*bUseSelectionOutline*/ false, ViewIndex, Collector, HitProxyId);
			}

			for (int32 i = 0, j = 3; i < 4; j = i++)
			{
				if (bSelected) PDI->DrawLine(World[j], World[i], HutongPlanColours::Selected, DPG, SelectedOutlineThickness, 0.0f, true);
				DrawOwnPart(PDI, World[j], World[i], Line, Thickness);
			}

			// Openings: jambs across the thickness, bar between, in door colour.
			for (const FVector2D& O : Openings)
			{
				const double A = O.X - 0.5 * O.Y, B = O.X + 0.5 * O.Y;
				auto Run = [&](double Along, double Across)
				{
					return bRunAlongY ? At(Across, Along) : At(Along, Across);
				};
				const double Across = bRunAlongY ? W : D;
				FLinearColor Door = FacadeColor;
				Door.A = Line.A;
				PDI->DrawLine(Run(A, 0.0), Run(A, Across), Door, DPG, Thickness + 1.0f, 0.0f, true);
				PDI->DrawLine(Run(B, 0.0), Run(B, Across), Door, DPG, Thickness + 1.0f, 0.0f, true);
				PDI->DrawLine(Run(A, 0.5 * Across), Run(B, 0.5 * Across), Door, DPG, Thickness + 2.0f, 0.0f, true);
			}

			// Bay divisions (間) at the columns; end boundaries are left to the outline.
			for (int32 i = 1; i + 1 < BayBoundaries.Num(); ++i)
			{
				const double T = BayBoundaries[i];
				const FVector A = bBaysAlongX ? At(T, 0.0) : At(0.0, T);
				const FVector B = bBaysAlongX ? At(T, D) : At(W, T);
				FLinearColor Division = Line;
				Division.A = Line.A * 0.7f;
				PDI->DrawLine(A, B, Division, DPG, FMath::Max(Thickness - 1.0f, 1.5f), 0.0f, true);
			}

			// Facade: short ticks just inside the edge, and a chevron per bay pointing out through it.
			if (bHasFacade)
			{
				const HutongGen::BaySide::FEdge E =
					HutongGen::BaySide::GetEdge((HutongGen::EBaySide)Facade, 0.0, 0.0, W, D);
				const double Along = E.bAlongX ? W : D;
				const int32 Count = FMath::Max(1, (int32)(Along / HatchSpacing));
				const double Step = Along / Count;
				const double Across = E.bAlongX ? D : W;
				const double Gap = FMath::Min(HatchGap, 0.1 * Across);
				const double Depth = FMath::Min(HatchDepth, 0.25 * Across);
				FLinearColor Hatch = FacadeColor;
				Hatch.A = Line.A;
				auto OnEdge = [&](double T) { return E.bAlongX ? FVector2D(T, E.FixedCoord) : FVector2D(E.FixedCoord, T); };
				// End ticks would lie on the side edges: inner ones only.
				for (int32 k = 1; k < Count; ++k)
				{
					const FVector2D A = OnEdge(k * Step) - E.OutDir * Gap;
					const FVector2D B = A - E.OutDir * Depth;
					PDI->DrawLine(At(A.X, A.Y), At(B.X, B.Y), Hatch, DPG, Thickness, 0.0f, true);
				}

				// Bays as drawn; a type without them gets one chevron across the whole front.
				TArray<double> Spans = BayBoundaries.Num() >= 2 ? BayBoundaries : TArray<double>{ 0.0, Along };
				Spans.Sort();
				const double TipIn = Gap + Depth + Gap;
				const FVector2D Tangent = E.bAlongX ? FVector2D(1.0, 0.0) : FVector2D(0.0, 1.0);
				for (int32 b = 0; b + 1 < Spans.Num(); ++b)
				{
					const double Half = FMath::Min(FMath::Clamp(0.3 * (Spans[b + 1] - Spans[b]), ChevronMin, ChevronMax),
						0.5 * (0.8 * Across - TipIn));
					if (Half <= 0.0) continue;
					const FVector2D Tip = OnEdge(0.5 * (Spans[b] + Spans[b + 1])) - E.OutDir * TipIn;
					const FVector2D Back = Tip - E.OutDir * Half;
					const FVector2D L = Back - Tangent * Half, R = Back + Tangent * Half;
					PDI->DrawLine(At(L.X, L.Y), At(Tip.X, Tip.Y), FacadeColor, DPG, Thickness + 1.5f, 0.0f, true);
					PDI->DrawLine(At(Tip.X, Tip.Y), At(R.X, R.Y), FacadeColor, DPG, Thickness + 1.5f, 0.0f, true);
				}
				// Facade edge, heavier, in the preview street colour.
				const FVector2D F0 = OnEdge(0.0), F1 = OnEdge(Along);
				DrawOwnPart(PDI, At(F0.X, F0.Y), At(F1.X, F1.Y), Hatch, Thickness + 1.5f);

				// Door bay heavier still: the 明間 is not always central, and it places the steps.
				if (BayBoundaries.IsValidIndex(DoorBay) && BayBoundaries.IsValidIndex(DoorBay + 1))
				{
					const FVector2D D0 = OnEdge(BayBoundaries[DoorBay]), D1 = OnEdge(BayBoundaries[DoorBay + 1]);
					DrawOwnPart(PDI, At(D0.X, D0.Y), At(D1.X, D1.Y), FacadeColor, Thickness + 4.0f);
				}
			}
		}
	}

	// A line along this plan's edge, less the stretches a neighbour draws (unless selected: then
	// the whole outline shows).
	void DrawOwnPart(FPrimitiveDrawInterface* PDI, const FVector& A, const FVector& B,
		const FLinearColor& Color, float Thickness) const
	{
		const FVector2D A2(A), AB = FVector2D(B) - A2;
		const double LengthSq = AB.SizeSquared();
		TArray<FVector2D, TInlineAllocator<4>> Hidden;
		if (!IsSelected() && LengthSq > UE_KINDA_SMALL_NUMBER)
		{
			for (const TPair<FVector, FVector>& E : Yielded)
			{
				double T[2];
				bool bOnLine = true;
				const FVector2D Ends[2] = { FVector2D(E.Key), FVector2D(E.Value) };
				for (int32 k = 0; k < 2; ++k)
				{
					const FVector2D AP = Ends[k] - A2;
					bOnLine &= FMath::Abs(FVector2D::CrossProduct(AB, AP)) / FMath::Sqrt(LengthSq) < SharedEdgeTolerance;
					T[k] = FVector2D::DotProduct(AB, AP) / LengthSq;
				}
				const double T0 = FMath::Max(0.0, FMath::Min(T[0], T[1])), T1 = FMath::Min(1.0, FMath::Max(T[0], T[1]));
				if (bOnLine && T1 > T0) Hidden.Add(FVector2D(T0, T1));
			}
		}
		Hidden.Sort([](const FVector2D& L, const FVector2D& R) { return L.X < R.X; });
		double From = 0.0;
		auto Draw = [&](double T0, double T1)
		{
			if (T1 - T0 > 1e-4) PDI->DrawLine(FMath::Lerp(A, B, T0), FMath::Lerp(A, B, T1), Color, DPG, Thickness, 0.0f, true);
		};
		for (const FVector2D& H : Hidden)
		{
			Draw(From, H.X);
			From = FMath::Max(From, H.Y);
		}
		Draw(From, 1.0);
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bDynamicRelevance = true;
		Result.bShadowRelevance = false;
		// Not bEditorPrimitiveRelevance.
		Result.bSeparateTranslucency = Result.bNormalTranslucency = true;
		return Result;
	}

	virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }

private:
	FLinearColor OutlineColor;
	FLinearColor FacadeColor;
	FVector2D Footprint;
	FHutongFootprintSkew Skew;
	bool bHasFacade;
	EHutongBaySide Facade;
	TArray<FVector2D> Openings;
	bool bRunAlongY;
	TArray<double> BayBoundaries;
	bool bBaysAlongX;
	int32 DoorBay;
	TArray<TPair<FVector, FVector>> Yielded;
	uint8 DPG;
	int32 Level;
	FHitProxyId HitProxyId;
};

UHutongPlanOutlineComponent::UHutongPlanOutlineComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bHiddenInGame = true;
	bUseEditorCompositing = false;
	CastShadow = false;
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
}

void UHutongPlanOutlineComponent::SetPlan(const FVector2D& InFootprint, const FHutongFootprintSkew& InSkew, bool bInHasFacade, EHutongBaySide InFacade,
	const TArray<FVector2D>& InOpenings, bool bInRunAlongY,
	const FHutongPlanBays& InBays, bool bInBaysAlongX, const FLinearColor& InColour)
{
	Footprint = InFootprint;
	Skew = InSkew;
	bHasFacade = bInHasFacade;
	Facade = InFacade;
	Openings = InOpenings;
	bRunAlongY = bInRunAlongY;
	BayBoundaries = InBays.Boundaries;
	bBaysAlongX = bInBaysAlongX;
	DoorBay = InBays.DoorBay;
	Colour = InColour;
	// Colour is baked into the proxy: a retint rebuilds it. Neighbours re-decide their shared edges.
	MarkRenderStateDirty();
	UpdateBounds();
	DirtyNeighbours(LastDrawnBox);
	DirtyNeighbours(Bounds.GetBox());
}

FPrimitiveSceneProxy* UHutongPlanOutlineComponent::CreateSceneProxy()
{
	if (Footprint.X <= 0.0 || Footprint.Y <= 0.0) return nullptr;
	if (!HutongPlanOutline::ArePlansVisible()) return nullptr;
	LastDrawnBox = Bounds.GetBox();
	// Fills are translucent and write no depth: the same stacking orders them.
	TranslucencySortPriority = GetStackLevel();
	TGuardValue<TArray<TPair<FVector, FVector>>> Pass(PendingEdges, CollectWinningNeighbourEdges());
	return new FHutongPlanOutlineSceneProxy(this);
}

void UHutongPlanOutlineComponent::GetWorldQuad(FVector OutCorners[4]) const
{
	FVector2D Quad[4];
	HutongFootprint::Corners(Footprint, Skew, Quad);
	const FTransform& Xform = GetComponentTransform();
	for (int32 i = 0; i < 4; ++i) OutCorners[i] = Xform.TransformPosition(FVector(Quad[i].X, Quad[i].Y, 0.0));
}

namespace
{
	template <typename FFunc>
	void ForEachPlanTouching(const UHutongPlanOutlineComponent* Self, const FBox& Box, FFunc&& Func)
	{
		const UWorld* World = Self->GetWorld();
		if (!World || !Box.IsValid) return;
		const FBox Near = Box.ExpandBy(FVector(SharedEdgeTolerance, SharedEdgeTolerance, 100.0));
		for (TObjectIterator<UHutongPlanOutlineComponent> It; It; ++It)
		{
			UHutongPlanOutlineComponent* Other = *It;
			if (Other == Self || Other->GetWorld() != World || !Other->IsRegistered()) continue;
			if (Other->Bounds.GetBox().Intersect(Near)) Func(Other);
		}
	}
}

int32 UHutongPlanOutlineComponent::GetStackLevel() const
{
	return HutongPlanColours::LayerOf(Colour);
}

TArray<TPair<FVector, FVector>> UHutongPlanOutlineComponent::CollectWinningNeighbourEdges() const
{
	// The smaller plan keeps a shared edge (a gate reads over its house); equal areas by name.
	TArray<TPair<FVector, FVector>> Edges;
	FVector Mine[4];
	GetWorldQuad(Mine);
	const double MyArea = QuadArea(Mine);
	ForEachPlanTouching(this, Bounds.GetBox(), [&](const UHutongPlanOutlineComponent* Other)
	{
		if (Other->Footprint.X <= 0.0 || Other->Footprint.Y <= 0.0) return;
		FVector Theirs[4];
		Other->GetWorldQuad(Theirs);
		const double TheirArea = QuadArea(Theirs);
		const bool bTheyWin = !FMath::IsNearlyEqual(TheirArea, MyArea, 1.0)
			? TheirArea < MyArea
			: Other->GetPathName() < GetPathName();
		if (!bTheyWin) return;
		for (int32 i = 0, j = 3; i < 4; j = i++) Edges.Emplace(Theirs[j], Theirs[i]);
	});
	return Edges;
}

void UHutongPlanOutlineComponent::DirtyNeighbours(const FBox& Box) const
{
	ForEachPlanTouching(this, Box, [](UHutongPlanOutlineComponent* Other) { Other->MarkRenderStateDirty(); });
}

void UHutongPlanOutlineComponent::OnRegister()
{
	Super::OnRegister();
	DirtyNeighbours(Bounds.GetBox());
}

void UHutongPlanOutlineComponent::OnUnregister()
{
	DirtyNeighbours(LastDrawnBox);
	Super::OnUnregister();
}

void UHutongPlanOutlineComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
	// Which stretches are shared is decided in world space: moved, it is decided again.
	MarkRenderStateDirty();
	DirtyNeighbours(LastDrawnBox);
	DirtyNeighbours(Bounds.GetBox());
}

FBoxSphereBounds UHutongPlanOutlineComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// Without this the actor's bounds collapse to a point.
	FBox Box(ForceInit);
	FVector2D Quad[4];
	HutongFootprint::Corners(Footprint, Skew, Quad);
	for (const FVector2D& C : Quad) Box += LocalToWorld.TransformPosition(FVector(C.X, C.Y, 0.0));
	Box = Box.ExpandBy(FVector(0.0, 0.0, Lift + (MaxStackLevel + 1) * PerspectiveStackStep));
	return FBoxSphereBounds(Box);
}
