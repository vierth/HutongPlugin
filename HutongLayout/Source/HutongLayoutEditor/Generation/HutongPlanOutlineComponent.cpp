#include "Generation/HutongPlanOutlineComponent.h"

#include "Containers/Ticker.h"
#include "DynamicMeshBuilder.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MaterialDomain.h"
#include "MaterialEditingLibrary.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialRenderProxy.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveUniformShaderParametersBuilder.h"
#include "PrimitiveViewRelevance.h"
#include "SceneInterface.h"
#include "SceneManagement.h"
#include "StaticMeshResources.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/ObjectKey.h"
#include "UObject/UObjectIterator.h"
#include <atomic>

namespace
{
	constexpr float FillAlpha = 0.18f;
	// Selected: fill this strong, outline in HutongPlanColours::Selected this thick. Every type
	// colour is a hue; white is none of them, so a selected plan reads apart from its neighbours.
	constexpr float SelectedFillAlpha = 0.38f;
	// The layer has already drawn the selected plan's own fill under the overlay's.
	constexpr float SelectedOverlayFillAlpha = 1.0f - (1.0f - SelectedFillAlpha) / (1.0f - FillAlpha);
	constexpr float SelectedOutlineThickness = 7.0f;
	// Hatch ticks along the facade: this far apart, this long, standing this far in off the edge
	// (on it they fought the neighbours' lines).
	constexpr double HatchSpacing = 60.0;
	constexpr double HatchDepth = 35.0;
	constexpr double HatchGap = 15.0;
	// Facing chevron behind the ticks in every bay, pointing out through the facade: half-width limits.
	constexpr double ChevronMin = 30.0;
	constexpr double ChevronMax = 120.0;
	// An arrow mark's square, at most (a city wall's ramp).
	constexpr double ArrowMarkSize = 300.0;
	// A wall's openings: the tool's orange for a handle being dragged.
	const FLinearColor OpeningColor(1.0f, 0.45f, 0.1f, 1.0f);
	// Slightly above ground to avoid z-fighting a map plane at Z = 0.
	constexpr double Lift = 2.0;
	// Edges this close count as one line.
	constexpr double SharedEdgeTolerance = 1.0;
	// The layer's neighbour search buckets plans in squares this wide.
	constexpr double LayerGridCell = 5000.0;

	double QuadArea(const FVector C[4])
	{
		double A = 0.0;
		for (int32 i = 0, j = 3; i < 4; j = i++) A += C[j].X * C[i].Y - C[i].X * C[j].Y;
		return 0.5 * FMath::Abs(A);
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

namespace
{
	// Bumped whenever a plan's drawing may have changed; a layer behind it rebuilds on the next tick.
	std::atomic<uint32> GPlanVersion{ 1 };

	void BumpPlanVersion() { GPlanVersion.fetch_add(1, std::memory_order_relaxed); }

	struct FPlanWorld
	{
		TSet<TWeakObjectPtr<UHutongPlanOutlineComponent>> Plans;
		UHutongPlanLayerComponent* Layer = nullptr;
	};
	TMap<TObjectKey<UWorld>, FPlanWorld> GPlanWorlds;
	FTSTicker::FDelegateHandle GLayerTicker;
	FDelegateHandle GWorldCleanupHandle;

	UMaterial* GPlanMaterial = nullptr;

	// Translucent, unlit, vertex colour; a line's corners carry their offset from the centre line in
	// pixels (UV0) and the vertex shader turns it into world units for this view: depth over the
	// projection's focal length in perspective, the ortho width per pixel in ortho.
	void EnsurePlanMaterial()
	{
		if (GPlanMaterial || GUsingNullRHI) return;
		UMaterial* M = NewObject<UMaterial>(GetTransientPackage(), TEXT("HutongPlanOutlineMaterial"), RF_Transient);
		M->BlendMode = BLEND_Translucent;
		M->SetShadingModel(MSM_Unlit);
		M->TwoSided = true;
		M->bUseTranslucencyVertexFog = false;

		UMaterialExpression* Colour = UMaterialEditingLibrary::CreateMaterialExpression(M, UMaterialExpressionVertexColor::StaticClass());
		auto* CameraRelative = Cast<UMaterialExpressionWorldPosition>(
			UMaterialEditingLibrary::CreateMaterialExpression(M, UMaterialExpressionWorldPosition::StaticClass()));
		CameraRelative->WorldPositionShaderOffset = WPT_CameraRelativeNoOffsets;
		UMaterialExpression* PixelOffset = UMaterialEditingLibrary::CreateMaterialExpression(M, UMaterialExpressionTextureCoordinate::StaticClass());
		auto* Widen = Cast<UMaterialExpressionCustom>(
			UMaterialEditingLibrary::CreateMaterialExpression(M, UMaterialExpressionCustom::StaticClass()));
		Widen->Inputs.Reset();
		Widen->Inputs.AddDefaulted(2);
		Widen->Inputs[0].InputName = TEXT("CameraRelative");
		Widen->Inputs[0].Input.Connect(0, CameraRelative);
		Widen->Inputs[1].InputName = TEXT("PixelOffset");
		Widen->Inputs[1].Input.Connect(0, PixelOffset);
		Widen->OutputType = CMOT_Float3;
		Widen->Code = TEXT(
			"float Depth = View.ViewToClip[3][3] > 0.5f ? 1.0f : max(dot(CameraRelative, View.ViewForward), 1.0f);\n"
			"float WorldPerPixel = 2.0f * Depth / (View.ViewToClip[0][0] * View.ViewSizeAndInvSize.x);\n"
			"return float3(PixelOffset * WorldPerPixel, 0.0f);");

		UMaterialEditorOnlyData* Inputs = M->GetEditorOnlyData();
		Inputs->EmissiveColor.Connect(0, Colour);
		Inputs->Opacity.Connect(4, Colour);
		Inputs->WorldPositionOffset.Connect(0, Widen);

		M->AddToRoot();
		for (const FString& Error : UMaterialEditingLibrary::RecompileMaterial(M))
		{
			UE_LOG(LogTemp, Error, TEXT("Hutong plan material: %s"), *Error);
		}
		GPlanMaterial = M;
	}

	// Under -nullrhi there is no plan material; the debug mesh material stands in for the hit-proxy fill.
	UMaterialInterface* PlanMaterialOrFallback()
	{
		return GPlanMaterial ? GPlanMaterial : (GEngine ? GEngine->DebugMeshMaterial.Get() : nullptr);
	}

	FMaterialRenderProxy* PlanMaterialProxy()
	{
		UMaterialInterface* M = PlanMaterialOrFallback();
		return M ? M->GetRenderProxy() : nullptr;
	}

	FMaterialRelevance PlanMaterialRelevance(EShaderPlatform Platform)
	{
		UMaterialInterface* M = PlanMaterialOrFallback();
		return M ? M->GetRelevance_Concurrent(Platform) : FMaterialRelevance();
	}

	bool IsPlanMaterialReady()
	{
		if (!GPlanMaterial) return false;
		const FMaterialResource* Resource = GPlanMaterial->GetMaterialResource(GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel]);
		return Resource && Resource->IsCompilationFinished();
	}

	// Triangles in world space, vertex colour, UV0 = a line corner's offset in pixels.
	struct FPlanMesh
	{
		TArray<FDynamicMeshVertex> Vertices;
		TArray<uint32> Indices;

		void Quad(const FVector Q[4], const FLinearColor& Colour)
		{
			const uint32 Base = Vertices.Num();
			const FColor C = Colour.ToFColor(false);
			for (int32 k = 0; k < 4; ++k) Vertices.Emplace(FVector3f(Q[k]), FVector2f::ZeroVector, C);
			Indices.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		}

		// Square-capped, Pixels wide on screen at any zoom.
		void Line(const FVector& A, const FVector& B, const FLinearColor& Colour, float Pixels)
		{
			FVector2f Along(float(B.X - A.X), float(B.Y - A.Y));
			if (!Along.Normalize()) return;
			const FVector2f Side(-Along.Y, Along.X);
			const float Half = 0.5f * Pixels;
			const uint32 Base = Vertices.Num();
			const FColor C = Colour.ToFColor(false);
			Vertices.Emplace(FVector3f(A), (-Along - Side) * Half, C);
			Vertices.Emplace(FVector3f(A), (-Along + Side) * Half, C);
			Vertices.Emplace(FVector3f(B), (Along + Side) * Half, C);
			Vertices.Emplace(FVector3f(B), (Along - Side) * Half, C);
			Indices.Append({ Base, Base + 1, Base + 2, Base, Base + 2, Base + 3 });
		}

		void Append(const FPlanMesh& Other)
		{
			const uint32 Base = Vertices.Num();
			Vertices.Append(Other.Vertices);
			Indices.Reserve(Indices.Num() + Other.Indices.Num());
			for (const uint32 I : Other.Indices) Indices.Add(Base + I);
		}
	};

	// What a plan draws, copied off the component for the render thread.
	struct FPlanDraw
	{
		FMatrix LocalToWorld = FMatrix::Identity;
		FVector2D Footprint = FVector2D::ZeroVector;
		FHutongFootprintSkew Skew;
		bool bHasFacade = false;
		EHutongBaySide Facade = EHutongBaySide::MinusY;
		TArray<FVector2D> Openings;
		bool bRunAlongY = false;
		TArray<double> BayBoundaries;
		bool bBaysAlongX = true;
		int32 DoorBay = INDEX_NONE;
		TArray<FVector4> ArrowMarks;
		FLinearColor Colour = FLinearColor::White;
		// Neighbours' edges drawn by them, left out of this plan's outline (never when selected).
		TArray<TPair<FVector, FVector>> Yielded;

		explicit FPlanDraw(const UHutongPlanOutlineComponent& C)
			: LocalToWorld(C.GetComponentTransform().ToMatrixWithScale())
			, Footprint(C.Footprint)
			, Skew(C.Skew)
			, bHasFacade(C.bHasFacade)
			, Facade(C.Facade)
			, Openings(C.Openings)
			, bRunAlongY(C.bRunAlongY)
			, BayBoundaries(C.BayBoundaries)
			, bBaysAlongX(C.bBaysAlongX)
			, DoorBay(C.DoorBay)
			, ArrowMarks(C.ArrowMarks)
			, Colour(C.Colour)
		{
		}
	};

	// A line along a plan's edge, less the stretches a neighbour draws.
	void AppendOwnPart(FPlanMesh& Out, const TArray<TPair<FVector, FVector>>& Yielded, const FVector& A, const FVector& B,
		const FLinearColor& Colour, float Thickness)
	{
		const FVector2D A2(A), AB = FVector2D(B) - A2;
		const double LengthSq = AB.SizeSquared();
		TArray<FVector2D, TInlineAllocator<4>> Hidden;
		if (LengthSq > UE_KINDA_SMALL_NUMBER)
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
			if (T1 - T0 > 1e-4) Out.Line(FMath::Lerp(A, B, T0), FMath::Lerp(A, B, T1), Colour, Thickness);
		};
		for (const FVector2D& H : Hidden)
		{
			Draw(From, H.X);
			From = FMath::Max(From, H.Y);
		}
		Draw(From, 1.0);
	}

	// One plan: fills and patches into Under, lines into Over, so no fill covers a line.
	void AppendPlan(const FPlanDraw& P, const FMatrix& L2W, bool bSelected, FPlanMesh& Under, FPlanMesh& Over)
	{
		const double W = P.Footprint.X, D = P.Footprint.Y;
		FVector2D Quad[4];
		HutongFootprint::Corners(P.Footprint, P.Skew, Quad);
		auto At = [&](double X, double Y)
		{
			const FVector2D Q = HutongFootprint::Map(P.Footprint, P.Skew, X, Y);
			return L2W.TransformPosition(FVector(Q.X, Q.Y, Lift));
		};
		FVector World[4];
		for (int32 i = 0; i < 4; ++i) World[i] = L2W.TransformPosition(FVector(Quad[i].X, Quad[i].Y, Lift));

		static const TArray<TPair<FVector, FVector>> NoneYielded;
		const TArray<TPair<FVector, FVector>>& Yielded = bSelected ? NoneYielded : P.Yielded;
		FLinearColor Line = P.Colour;
		Line.A = bSelected ? 1.0f : 0.7f;
		const float Thickness = bSelected ? 3.0f : 2.0f;
		// Facade marks: same hue, heavier, so a plan shows type and facing.
		const FLinearColor FacadeColor = P.Colour * FLinearColor(1.0f, 0.72f, 0.45f, 1.0f);

		// Faint fill: a street of them reads as a tint.
		FLinearColor Fill = P.Colour;
		Fill.A = bSelected ? SelectedOverlayFillAlpha : FillAlpha;
		Under.Quad(World, Fill);

		if (bSelected)
		{
			for (int32 i = 0, j = 3; i < 4; j = i++) Over.Line(World[j], World[i], HutongPlanColours::Selected, SelectedOutlineThickness);
		}
		for (int32 i = 0, j = 3; i < 4; j = i++) AppendOwnPart(Over, Yielded, World[j], World[i], Line, Thickness);

		// Openings (a wall's gate or garden doorway): an orange patch across the thickness, edged in the
		// same orange — the slider's while dragged — so a gate reads unselected (user, 2026-10-06).
		for (const FVector2D& O : P.Openings)
		{
			const double A = O.X - 0.5 * O.Y, B = O.X + 0.5 * O.Y;
			auto Run = [&](double Along, double Across)
			{
				return P.bRunAlongY ? At(Across, Along) : At(Along, Across);
			};
			const double Across = P.bRunAlongY ? W : D;
			const FVector Q[4] = { Run(A, 0.0), Run(B, 0.0), Run(B, Across), Run(A, Across) };
			FLinearColor Patch = OpeningColor;
			Patch.A = 0.7f;
			Under.Quad(Q, Patch);
			for (int32 k = 0; k < 4; ++k) Over.Line(Q[k], Q[(k + 1) % 4], OpeningColor, Thickness + 1.0f);
		}

		// Arrow marks: a square, an arrow across it in the mark's direction, in the facade hue.
		for (const FVector4& M : P.ArrowMarks)
		{
			const double Half = 0.5 * FMath::Min(ArrowMarkSize, 0.3 * FMath::Min(W, D));
			const FVector2D C(M.X, M.Y);
			const FVector2D Dir = FVector2D(M.Z, M.W).GetSafeNormal();
			const FVector2D Side(-Dir.Y, Dir.X);
			FLinearColor Mark = FacadeColor;
			Mark.A = Line.A;
			const FVector2D Sq[4] = { C - Dir * Half - Side * Half, C + Dir * Half - Side * Half,
				C + Dir * Half + Side * Half, C - Dir * Half + Side * Half };
			for (int32 k = 0; k < 4; ++k)
			{
				const FVector2D A = Sq[k], B = Sq[(k + 1) % 4];
				Over.Line(At(A.X, A.Y), At(B.X, B.Y), Mark, Thickness + 1.0f);
			}
			const FVector2D Tail = C - Dir * 0.7 * Half, Tip = C + Dir * 0.7 * Half;
			const FVector2D Wing = Tip - Dir * 0.45 * Half;
			Over.Line(At(Tail.X, Tail.Y), At(Tip.X, Tip.Y), Mark, Thickness + 1.5f);
			for (const double S : { -1.0, 1.0 })
			{
				const FVector2D End = Wing + Side * S * 0.4 * Half;
				Over.Line(At(Tip.X, Tip.Y), At(End.X, End.Y), Mark, Thickness + 1.5f);
			}
		}

		// Bay divisions (間) at the columns; end boundaries are left to the outline.
		for (int32 i = 1; i + 1 < P.BayBoundaries.Num(); ++i)
		{
			const double T = P.BayBoundaries[i];
			const FVector A = P.bBaysAlongX ? At(T, 0.0) : At(0.0, T);
			const FVector B = P.bBaysAlongX ? At(T, D) : At(W, T);
			FLinearColor Division = Line;
			Division.A = Line.A * 0.7f;
			Over.Line(A, B, Division, FMath::Max(Thickness - 1.0f, 1.5f));
		}

		// Facade: short ticks just inside the edge, and a chevron per bay pointing out through it.
		if (P.bHasFacade)
		{
			const HutongGen::BaySide::FEdge E = HutongGen::BaySide::GetEdge((HutongGen::EBaySide)P.Facade, 0.0, 0.0, W, D);
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
				Over.Line(At(A.X, A.Y), At(B.X, B.Y), Hatch, Thickness);
			}

			// Bays as drawn; a type without them gets one chevron across the whole front.
			TArray<double> Spans = P.BayBoundaries.Num() >= 2 ? P.BayBoundaries : TArray<double>{ 0.0, Along };
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
				Over.Line(At(L.X, L.Y), At(Tip.X, Tip.Y), FacadeColor, Thickness + 1.5f);
				Over.Line(At(Tip.X, Tip.Y), At(R.X, R.Y), FacadeColor, Thickness + 1.5f);
			}
			// Facade edge, heavier, in the preview street colour.
			const FVector2D F0 = OnEdge(0.0), F1 = OnEdge(Along);
			AppendOwnPart(Over, Yielded, At(F0.X, F0.Y), At(F1.X, F1.Y), Hatch, Thickness + 1.5f);

			// Door bay heavier still: the 明間 is not always central, and it places the steps.
			if (P.BayBoundaries.IsValidIndex(P.DoorBay) && P.BayBoundaries.IsValidIndex(P.DoorBay + 1))
			{
				const FVector2D D0 = OnEdge(P.BayBoundaries[P.DoorBay]), D1 = OnEdge(P.BayBoundaries[P.DoorBay + 1]);
				AppendOwnPart(Over, Yielded, At(D0.X, D0.Y), At(D1.X, D1.Y), FacadeColor, Thickness + 4.0f);
			}
		}
	}

	// Who draws a shared edge: the smaller plan (a gate reads over its house); equal areas by name.
	struct FPlanEdgeKey
	{
		FVector Quad[4];
		double Area = 0.0;
		FBox Box;
		FString Path;

		explicit FPlanEdgeKey(const UHutongPlanOutlineComponent& C)
			: Box(C.Bounds.GetBox())
			, Path(C.GetPathName())
		{
			C.GetWorldQuad(Quad);
			Area = QuadArea(Quad);
		}

		FBox Near() const { return Box.ExpandBy(FVector(SharedEdgeTolerance, SharedEdgeTolerance, 100.0)); }

		bool YieldsTo(const FPlanEdgeKey& Other) const
		{
			if (!Other.Box.Intersect(Near())) return false;
			return !FMath::IsNearlyEqual(Other.Area, Area, 1.0) ? Other.Area < Area : Other.Path < Path;
		}

		void AddEdgesTo(TArray<TPair<FVector, FVector>>& Edges) const
		{
			for (int32 i = 0, j = 3; i < 4; j = i++) Edges.Emplace(Quad[j], Quad[i]);
		}
	};

	bool HasFootprint(const UHutongPlanOutlineComponent& C) { return C.Footprint.X > 0.0 && C.Footprint.Y > 0.0; }

	bool IsPlanDrawn(const UHutongPlanOutlineComponent& C)
	{
		const AActor* Owner = C.GetOwner();
		// IsVisible() folds in bHiddenInGame, which every plan sets; game views skip the layer itself.
		return C.IsRegistered() && C.IsVisibleInEditor() && HasFootprint(C) && !(Owner && Owner->IsHiddenEd());
	}

	FPlanWorld* FindPlanWorld(const UWorld* World)
	{
		return World ? GPlanWorlds.Find(TObjectKey<UWorld>(World)) : nullptr;
	}

	void DestroyLayer(FPlanWorld& PlanWorld)
	{
		if (UHutongPlanLayerComponent* Layer = PlanWorld.Layer)
		{
			if (Layer->IsRegistered()) Layer->UnregisterComponent();
			Layer->RemoveFromRoot();
			Layer->MarkAsGarbage();
			PlanWorld.Layer = nullptr;
		}
	}

	// A layer per world with plans in it, rebuilt (render state) when any plan's drawing changed.
	bool TickLayers(float)
	{
		const uint32 Version = GPlanVersion.load(std::memory_order_relaxed);
		for (auto It = GPlanWorlds.CreateIterator(); It; ++It)
		{
			UWorld* World = It.Key().ResolveObjectPtr();
			FPlanWorld& PlanWorld = It.Value();
			if (!World || PlanWorld.Plans.IsEmpty())
			{
				DestroyLayer(PlanWorld);
				It.RemoveCurrent();
				continue;
			}
			if (!PlanWorld.Layer)
			{
				// Until its shaders are in, the material draws as the default checker.
				EnsurePlanMaterial();
				if (!IsPlanMaterialReady() || !World->Scene || World->bIsTearingDown) continue;
				UHutongPlanLayerComponent* Layer = NewObject<UHutongPlanLayerComponent>(GetTransientPackage(), NAME_None, RF_Transient);
				Layer->AddToRoot();
				Layer->RegisterComponentWithWorld(World);
				PlanWorld.Layer = Layer;
			}
			else if (PlanWorld.Layer->DrawnVersion != Version)
			{
				PlanWorld.Layer->MarkRenderStateDirty();
			}
		}
		return true;
	}

	void OnWorldCleanup(UWorld* World, bool, bool)
	{
		if (FPlanWorld* PlanWorld = FindPlanWorld(World))
		{
			DestroyLayer(*PlanWorld);
			GPlanWorlds.Remove(TObjectKey<UWorld>(World));
		}
	}
}

namespace HutongPlanOutline
{
	void StartLayers()
	{
		GLayerTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&TickLayers));
		GWorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddStatic(&OnWorldCleanup);
	}

	UMaterial* GetPlanMaterial()
	{
		EnsurePlanMaterial();
		return GPlanMaterial;
	}

	bool IsLayerCurrent(const UWorld* World)
	{
		const FPlanWorld* PlanWorld = FindPlanWorld(World);
		return PlanWorld && PlanWorld->Layer && PlanWorld->Layer->IsRegistered() && !PlanWorld->Layer->IsRenderStateDirty()
			&& PlanWorld->Layer->DrawnVersion == GPlanVersion.load(std::memory_order_relaxed);
	}

	void StopLayers()
	{
		FTSTicker::GetCoreTicker().RemoveTicker(GLayerTicker);
		FWorldDelegates::OnWorldCleanup.Remove(GWorldCleanupHandle);
		// At editor exit modules unload after every UObject is gone, rooted or not: forget, never touch.
		const bool bObjectsAlive = UObjectInitialized() && !IsEngineExitRequested();
		if (bObjectsAlive)
		{
			for (TPair<TObjectKey<UWorld>, FPlanWorld>& Pair : GPlanWorlds) DestroyLayer(Pair.Value);
			if (GPlanMaterial) GPlanMaterial->RemoveFromRoot();
		}
		GPlanWorlds.Empty();
		GPlanMaterial = nullptr;
	}
}

// One plan's own proxy: the selected overlay, and the fill in the hit-proxy pass so a click selects the
// building. Otherwise it draws nothing; the layer has it.
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
		, Plan(*C)
		, Material(PlanMaterialProxy())
		, MaterialRelevance(PlanMaterialRelevance(GetScene().GetShaderPlatform()))
		, DPG(HutongPlanOutline::ArePlansOverBuildings() ? SDPG_Foreground : SDPG_World)
	{
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
		if (!Material) return;
		const bool bHitTesting = ViewFamily.EngineShowFlags.HitProxies;
		FPlanMesh Under, Over;
		if (bHitTesting)
		{
			FVector2D Quad[4];
			HutongFootprint::Corners(Plan.Footprint, Plan.Skew, Quad);
			FVector World[4];
			for (int32 i = 0; i < 4; ++i) World[i] = GetLocalToWorld().TransformPosition(FVector(Quad[i].X, Quad[i].Y, Lift));
			Under.Quad(World, FLinearColor::White);
		}
		else
		{
			AppendPlan(Plan, GetLocalToWorld(), /*bSelected*/ true, Under, Over);
			Under.Append(Over);
		}
		if (Under.Indices.IsEmpty()) return;

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			if ((VisibilityMap & (1 << ViewIndex)) == 0) continue;
			FDynamicMeshBuilder Builder(Views[ViewIndex]->GetFeatureLevel());
			Builder.AddVertices(Under.Vertices);
			Builder.AddTriangles(Under.Indices);
			Builder.GetMesh(FMatrix::Identity, Material, DPG, /*bDisableBackfaceCulling*/ true, /*bReceivesDecals*/ false,
				/*bUseSelectionOutline*/ false, ViewIndex, Collector, HitProxyId);
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View) && (IsSelected() || View->Family->EngineShowFlags.HitProxies);
		Result.bDynamicRelevance = true;
		Result.bShadowRelevance = false;
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	// Never occlusion-culled: the bounds are a slab a few centimetres thick lying on the map, and the last
	// frame's coarse depth test judged a thin wall hidden one frame and shown the next (user, 2026-10-06).
	virtual bool CanBeOccluded() const override { return false; }

	virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }

private:
	FPlanDraw Plan;
	FMaterialRenderProxy* Material;
	FMaterialRelevance MaterialRelevance;
	uint8 DPG;
	FHitProxyId HitProxyId;
};

// Every plan of a world, one vertex buffer built once per change, one batch a frame.
class FHutongPlanLayerSceneProxy final : public FPrimitiveSceneProxy
{
public:
	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	FHutongPlanLayerSceneProxy(const UHutongPlanLayerComponent* C, TArray<FPlanDraw>&& InPlans)
		: FPrimitiveSceneProxy(C)
		, Plans(MoveTemp(InPlans))
		, VertexFactory(GetScene().GetFeatureLevel(), "FHutongPlanLayerSceneProxy")
		, Material(PlanMaterialProxy())
		, MaterialRelevance(PlanMaterialRelevance(GetScene().GetShaderPlatform()))
		, DPG(HutongPlanOutline::ArePlansOverBuildings() ? SDPG_Foreground : SDPG_World)
	{
		bWillEverBeLit = false;
	}

	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override
	{
		// All fills first, then all lines: no plan's tint lies over another's outline.
		FPlanMesh Under, Over;
		for (const FPlanDraw& P : Plans) AppendPlan(P, P.LocalToWorld, /*bSelected*/ false, Under, Over);
		Plans.Empty();
		Under.Append(Over);
		if (Under.Indices.IsEmpty() || !Material) return;

		NumTriangles = Under.Indices.Num() / 3;
		NumVertices = Under.Vertices.Num();
		IndexBuffer.Indices = MoveTemp(Under.Indices);
		VertexBuffers.InitFromDynamicVertex(RHICmdList, &VertexFactory, Under.Vertices);
		IndexBuffer.InitResource(RHICmdList);
		bHasMesh = true;
	}

	virtual void DestroyRenderThreadResources() override
	{
		if (!bHasMesh) return;
		VertexBuffers.PositionVertexBuffer.ReleaseResource();
		VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
		VertexBuffers.ColorVertexBuffer.ReleaseResource();
		IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
		bHasMesh = false;
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views,
		const FSceneViewFamily& ViewFamily, uint32 VisibilityMap,
		FMeshElementCollector& Collector) const override
	{
		if (!bHasMesh) return;
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			if ((VisibilityMap & (1 << ViewIndex)) == 0) continue;
			FMeshBatch& Mesh = Collector.AllocateMesh();
			FMeshBatchElement& Element = Mesh.Elements[0];
			Element.IndexBuffer = &IndexBuffer;
			Mesh.VertexFactory = &VertexFactory;
			Mesh.MaterialRenderProxy = Material;

			FDynamicPrimitiveUniformBuffer& UniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
			FPrimitiveUniformShaderParametersBuilder Builder;
			BuildUniformShaderParameters(Builder);
			UniformBuffer.Set(Collector.GetRHICommandList(), Builder);
			Element.PrimitiveUniformBufferResource = &UniformBuffer.UniformBuffer;

			Element.FirstIndex = 0;
			Element.NumPrimitives = NumTriangles;
			Element.MinVertexIndex = 0;
			Element.MaxVertexIndex = NumVertices - 1;
			Mesh.Type = PT_TriangleList;
			Mesh.DepthPriorityGroup = DPG;
			Mesh.bDisableBackfaceCulling = true;
			Mesh.bCanApplyViewModeOverrides = false;
			Collector.AddMesh(ViewIndex, Mesh);
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		// Owned by no actor, it has no hit proxy to draw: the plans' own proxies take the clicks.
		Result.bDrawRelevance = bHasMesh && IsShown(View) && !View->Family->EngineShowFlags.HitProxies;
		Result.bDynamicRelevance = true;
		Result.bShadowRelevance = false;
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		return Result;
	}

	virtual bool CanBeOccluded() const override { return false; }

	virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }

private:
	TArray<FPlanDraw> Plans;
	FStaticMeshVertexBuffers VertexBuffers;
	FDynamicMeshIndexBuffer32 IndexBuffer;
	FLocalVertexFactory VertexFactory;
	FMaterialRenderProxy* Material;
	FMaterialRelevance MaterialRelevance;
	uint8 DPG;
	bool bHasMesh = false;
	uint32 NumTriangles = 0;
	uint32 NumVertices = 0;
};

UHutongPlanOutlineComponent::UHutongPlanOutlineComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bHiddenInGame = true;
	bUseEditorCompositing = false;
	CastShadow = false;
	// The selected overlay draws over the layer.
	TranslucencySortPriority = 1;
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
	UpdateBounds();
	MarkRenderStateDirty();
}

FPrimitiveSceneProxy* UHutongPlanOutlineComponent::CreateSceneProxy()
{
	if (!HasFootprint(*this)) return nullptr;
	if (!HutongPlanOutline::ArePlansVisible()) return nullptr;
	return new FHutongPlanOutlineSceneProxy(this);
}

void UHutongPlanOutlineComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (UMaterialInterface* M = PlanMaterialOrFallback()) OutMaterials.Add(M);
}

void UHutongPlanOutlineComponent::GetWorldQuad(FVector OutCorners[4]) const
{
	FVector2D Quad[4];
	HutongFootprint::Corners(Footprint, Skew, Quad);
	const FTransform& Xform = GetComponentTransform();
	for (int32 i = 0; i < 4; ++i) OutCorners[i] = Xform.TransformPosition(FVector(Quad[i].X, Quad[i].Y, 0.0));
}

int32 UHutongPlanOutlineComponent::GetStackLevel() const
{
	return HutongPlanColours::LayerOf(Colour);
}

TArray<TPair<FVector, FVector>> UHutongPlanOutlineComponent::CollectWinningNeighbourEdges() const
{
	TArray<TPair<FVector, FVector>> Edges;
	const FPlanWorld* PlanWorld = FindPlanWorld(GetWorld());
	if (!PlanWorld || !HasFootprint(*this)) return Edges;
	const FPlanEdgeKey Mine(*this);
	for (const TWeakObjectPtr<UHutongPlanOutlineComponent>& Weak : PlanWorld->Plans)
	{
		const UHutongPlanOutlineComponent* Other = Weak.Get();
		if (!Other || Other == this || !Other->IsRegistered() || !HasFootprint(*Other)) continue;
		const FPlanEdgeKey Theirs(*Other);
		if (Mine.YieldsTo(Theirs)) Theirs.AddEdgesTo(Edges);
	}
	return Edges;
}

void UHutongPlanOutlineComponent::OnRegister()
{
	EnsurePlanMaterial();
	Super::OnRegister();
	if (UWorld* World = GetWorld())
	{
		GPlanWorlds.FindOrAdd(TObjectKey<UWorld>(World)).Plans.Add(this);
	}
	BumpPlanVersion();
}

void UHutongPlanOutlineComponent::OnUnregister()
{
	if (FPlanWorld* PlanWorld = FindPlanWorld(GetWorld()))
	{
		PlanWorld->Plans.Remove(this);
	}
	BumpPlanVersion();
	Super::OnUnregister();
}

void UHutongPlanOutlineComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
	BumpPlanVersion();
}

// Every path that changes what a plan draws (SetPlan, hiding, the visibility switches) recreates its
// render state; the layer follows.
void UHutongPlanOutlineComponent::CreateRenderState_Concurrent(FRegisterComponentContext* Context)
{
	Super::CreateRenderState_Concurrent(Context);
	BumpPlanVersion();
}

void UHutongPlanOutlineComponent::DestroyRenderState_Concurrent()
{
	Super::DestroyRenderState_Concurrent();
	BumpPlanVersion();
}

FBoxSphereBounds UHutongPlanOutlineComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// Without this the actor's bounds collapse to a point.
	FBox Box(ForceInit);
	FVector2D Quad[4];
	HutongFootprint::Corners(Footprint, Skew, Quad);
	for (const FVector2D& C : Quad) Box += LocalToWorld.TransformPosition(FVector(C.X, C.Y, 0.0));
	Box = Box.ExpandBy(FVector(0.0, 0.0, Lift + 1.0));
	return FBoxSphereBounds(Box);
}

UHutongPlanLayerComponent::UHutongPlanLayerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bHiddenInGame = true;
	bUseEditorCompositing = false;
	CastShadow = false;
	SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	SetGenerateOverlapEvents(false);
}

FPrimitiveSceneProxy* UHutongPlanLayerComponent::CreateSceneProxy()
{
	DrawnVersion = GPlanVersion.load(std::memory_order_relaxed);
	const FPlanWorld* PlanWorld = FindPlanWorld(GetWorld());
	if (!PlanWorld || !GPlanMaterial || !HutongPlanOutline::ArePlansVisible()) return nullptr;

	TArray<const UHutongPlanOutlineComponent*> Drawn;
	for (const TWeakObjectPtr<UHutongPlanOutlineComponent>& Weak : PlanWorld->Plans)
	{
		const UHutongPlanOutlineComponent* C = Weak.Get();
		if (C && IsPlanDrawn(*C)) Drawn.Add(C);
	}
	TArray<FPlanEdgeKey> Keys;
	Keys.Reserve(Drawn.Num());
	for (const UHutongPlanOutlineComponent* C : Drawn) Keys.Emplace(*C);

	// Bottom layer first (HutongPlanColours::Layers), larger before smaller, then by name: a fixed order,
	// so where two plans overlap the same one shows every frame.
	TArray<int32> Order;
	Order.Reserve(Drawn.Num());
	for (int32 i = 0; i < Drawn.Num(); ++i) Order.Add(i);
	TArray<int32> Levels;
	Levels.Reserve(Drawn.Num());
	for (const UHutongPlanOutlineComponent* C : Drawn) Levels.Add(C->GetStackLevel());
	Order.Sort([&](int32 L, int32 R)
	{
		if (Levels[L] != Levels[R]) return Levels[L] < Levels[R];
		if (!FMath::IsNearlyEqual(Keys[L].Area, Keys[R].Area, 1.0)) return Keys[L].Area > Keys[R].Area;
		return Keys[L].Path < Keys[R].Path;
	});

	// Shared edges: each plan asks only the plans in the grid squares its bounds touch.
	TMap<FIntPoint, TArray<int32>> Grid;
	auto Cells = [](const FBox& Box, auto&& Visit)
	{
		const int32 X0 = FMath::FloorToInt32(Box.Min.X / LayerGridCell), X1 = FMath::FloorToInt32(Box.Max.X / LayerGridCell);
		const int32 Y0 = FMath::FloorToInt32(Box.Min.Y / LayerGridCell), Y1 = FMath::FloorToInt32(Box.Max.Y / LayerGridCell);
		for (int32 X = X0; X <= X1; ++X)
		{
			for (int32 Y = Y0; Y <= Y1; ++Y) Visit(FIntPoint(X, Y));
		}
	};
	for (int32 i = 0; i < Keys.Num(); ++i)
	{
		Cells(Keys[i].Box, [&](const FIntPoint& Cell) { Grid.FindOrAdd(Cell).Add(i); });
	}

	TArray<FPlanDraw> Plans;
	Plans.Reserve(Order.Num());
	TArray<int32> SeenBy;
	SeenBy.Init(INDEX_NONE, Keys.Num());
	for (const int32 i : Order)
	{
		FPlanDraw& Plan = Plans.Emplace_GetRef(*Drawn[i]);
		Cells(Keys[i].Near(), [&](const FIntPoint& Cell)
		{
			const TArray<int32>* InCell = Grid.Find(Cell);
			if (!InCell) return;
			for (const int32 j : *InCell)
			{
				if (j == i || SeenBy[j] == i) continue;
				SeenBy[j] = i;
				if (Keys[i].YieldsTo(Keys[j])) Keys[j].AddEdgesTo(Plan.Yielded);
			}
		});
	}
	return new FHutongPlanLayerSceneProxy(this, MoveTemp(Plans));
}

void UHutongPlanLayerComponent::GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const
{
	if (UMaterialInterface* M = PlanMaterialOrFallback()) OutMaterials.Add(M);
}

FBoxSphereBounds UHutongPlanLayerComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	// Plans anywhere in the world, as the world's line batcher.
	return FBoxSphereBounds(FVector::ZeroVector, FVector(HALF_WORLD_MAX), HALF_WORLD_MAX);
}
