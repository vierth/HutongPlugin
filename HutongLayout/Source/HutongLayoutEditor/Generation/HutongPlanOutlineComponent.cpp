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
	constexpr float FillAlpha = 0.10f;
	// Selected: fill this strong, outline in HutongPlanColours::Selected this thick. Every type
	// colour is a hue; white is none of them, so a selected plan reads apart from its neighbours.
	constexpr float SelectedFillAlpha = 0.38f;
	constexpr float SelectedOutlineThickness = 7.0f;
	// Hatch ticks along the facade: this far apart, this deep into the footprint.
	constexpr double HatchSpacing = 60.0;
	constexpr double HatchDepth = 45.0;
	// Slightly above ground to avoid z-fighting a map plane at Z = 0.
	constexpr double Lift = 2.0;

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
		, ColumnRows(C->ColumnRows)
		, ColumnRadius(C->ColumnRadius)
		, FootingSize(C->FootingSize)
	{
		OutlineColor = C->Colour;
		// Facade marks: same hue, heavier, so a plan shows type and facing.
		FacadeColor = C->Colour * FLinearColor(1.0f, 0.72f, 0.45f, 1.0f);
		bWillEverBeLit = false;
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
		auto At = [&](double X, double Y)
		{
			const FVector2D Q = HutongFootprint::Map(Footprint, Skew, X, Y);
			return L2W.TransformPosition(FVector(Q.X, Q.Y, Lift));
		};
		FVector World[4];
		for (int32 i = 0; i < 4; ++i) World[i] = L2W.TransformPosition(FVector(Quad[i].X, Quad[i].Y, Lift));

		const bool bSelected = IsSelected();
		FLinearColor Line = OutlineColor;
		Line.A = bSelected ? 1.0f : 0.7f;
		const float Thickness = bSelected ? 3.0f : 2.0f;

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			if ((VisibilityMap & (1 << ViewIndex)) == 0) continue;
			FPrimitiveDrawInterface* PDI = Collector.GetPDI(ViewIndex);
			constexpr uint8 DPG = SDPG_Foreground;

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
				PDI->DrawLine(World[j], World[i], Line, DPG, Thickness, 0.0f, true);
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

			// Columns: 柱頂石 square with the column circle inside; rows show 前廊/後廊 before building.
			for (const double Row : ColumnRows)
			{
				for (const double T : BayBoundaries)
				{
					const double X = bBaysAlongX ? T : Row, Y = bBaysAlongX ? Row : T;
					FLinearColor Footing = Line;
					Footing.A = Line.A * 0.85f;
					const double H = 0.5 * FootingSize;
					const FVector S[4] = { At(X - H, Y - H), At(X + H, Y - H), At(X + H, Y + H), At(X - H, Y + H) };
					for (int32 k = 0, m = 3; k < 4; m = k++)
					{
						PDI->DrawLine(S[m], S[k], Footing, DPG, FMath::Max(Thickness - 1.0f, 1.5f), 0.0f, true);
					}
					if (ColumnRadius > 0.0)
					{
						DrawCircle(PDI, At(X, Y), L2W.GetUnitAxis(EAxis::X), L2W.GetUnitAxis(EAxis::Y),
							Line, ColumnRadius, 16, DPG, Thickness, 0.0f, true);
					}
				}
			}

			// Hatch along the facade: short ticks from the edge into the footprint.
			if (bHasFacade)
			{
				const HutongGen::BaySide::FEdge E =
					HutongGen::BaySide::GetEdge((HutongGen::EBaySide)Facade, 0.0, 0.0, W, D);
				const double Along = E.bAlongX ? W : D;
				const int32 Count = FMath::Max(1, (int32)(Along / HatchSpacing));
				const double Step = Along / Count;
				const double Depth = FMath::Min(HatchDepth, 0.4 * (E.bAlongX ? D : W));
				FLinearColor Hatch = FacadeColor;
				Hatch.A = Line.A;
				auto OnEdge = [&](double T) { return E.bAlongX ? FVector2D(T, E.FixedCoord) : FVector2D(E.FixedCoord, T); };
				for (int32 k = 0; k <= Count; ++k)
				{
					const FVector2D A = OnEdge(k * Step);
					const FVector2D B = A - E.OutDir * Depth;
					PDI->DrawLine(At(A.X, A.Y), At(B.X, B.Y), Hatch, DPG, Thickness, 0.0f, true);
				}
				// Facade edge, heavier, in the preview street colour.
				const FVector2D F0 = OnEdge(0.0), F1 = OnEdge(Along);
				PDI->DrawLine(At(F0.X, F0.Y), At(F1.X, F1.Y), Hatch, DPG, Thickness + 1.5f, 0.0f, true);

				// Door bay heavier still: the 明間 is not always central, and it places the steps.
				if (BayBoundaries.IsValidIndex(DoorBay) && BayBoundaries.IsValidIndex(DoorBay + 1))
				{
					const FVector2D D0 = OnEdge(BayBoundaries[DoorBay]), D1 = OnEdge(BayBoundaries[DoorBay + 1]);
					PDI->DrawLine(At(D0.X, D0.Y), At(D1.X, D1.Y), FacadeColor, DPG, Thickness + 4.0f, 0.0f, true);
				}
			}
		}
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
	TArray<double> ColumnRows;
	double ColumnRadius;
	double FootingSize;
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
	ColumnRows = InBays.ColumnRows;
	ColumnRadius = InBays.ColumnRadius;
	FootingSize = InBays.FootingSize;
	Colour = InColour;
	// Colour is baked into the proxy: a retint rebuilds it.
	MarkRenderStateDirty();
}

FPrimitiveSceneProxy* UHutongPlanOutlineComponent::CreateSceneProxy()
{
	if (Footprint.X <= 0.0 || Footprint.Y <= 0.0) return nullptr;
	if (!HutongPlanOutline::ArePlansVisible()) return nullptr;
	return new FHutongPlanOutlineSceneProxy(this);
}

FBoxSphereBounds UHutongPlanOutlineComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// Without this the actor's bounds collapse to a point.
	FBox Box(ForceInit);
	FVector2D Quad[4];
	HutongFootprint::Corners(Footprint, Skew, Quad);
	for (const FVector2D& C : Quad) Box += LocalToWorld.TransformPosition(FVector(C.X, C.Y, 0.0));
	Box = Box.ExpandBy(FVector(0.0, 0.0, 10.0));
	return FBoxSphereBounds(Box);
}
