#include "Tools/HutongAudit.h"
#include "Tools/HutongPresetDefaults.h"
#include "Generation/HutongBuildingComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"

namespace HutongAudit
{
	using namespace HutongCanon::Audit;

	namespace
	{
		FString AuditMetres(double Cm) { return FString::Printf(TEXT("%.2f m"), Cm * 0.01); }

		const FRole* AuditRoleBands(EHutongCourtRole Role)
		{
			switch (Role)
			{
			case EHutongCourtRole::MainHall:  return &MainHall;
			case EHutongCourtRole::SideHouse: return &SideHouse;
			case EHutongCourtRole::EarRoom:   return &EarRoom;
			case EHutongCourtRole::FrontRow:  return &FrontRow;
			case EHutongCourtRole::RearRow:   return &RearRow;
			default: return nullptr;
			}
		}

		const HutongCanon::House::FHouse* AuditHouseFor(EHutongCourtRole Role)
		{
			namespace H = HutongCanon::House;
			switch (Role)
			{
			case EHutongCourtRole::MainHall:  return &H::MainHallSmall;
			case EHutongCourtRole::SideHouse: return &H::SideHouseSmall;
			case EHutongCourtRole::EarRoom:   return &H::EarRoom;
			case EHutongCourtRole::FrontRow:  return &H::FrontRow;
			case EHutongCourtRole::RearRow:   return &H::RearRow;
			default: return nullptr;
			}
		}

		bool AuditHouseParams(const UHutongBuildingComponent* B, FHutongSiheyuanParams& Out)
		{
			if (const UHutongSiheyuanBuildingComponent* H = Cast<UHutongSiheyuanBuildingComponent>(B))
			{
				Out = H->ParamsForFootprint();
				return true;
			}
			if (const UHutongEarPassageBuildingComponent* E = Cast<UHutongEarPassageBuildingComponent>(B))
			{
				if (!E->Params.HasRoom()) return false;
				Out = E->ParamsForFootprint().RoomParams();
				return true;
			}
			return false;
		}

		FBand AuditBandOf(double Min, double Max, ESource Source, const TCHAR* Cite)
		{
			FBand Band;
			Band.Min = Min; Band.Max = Max; Band.Source = Source; Band.Cite = Cite;
			return Band;
		}

		void AuditAdd(FResult& R, const FString& Name, double Value, const FBand& Band, bool bCount = false)
		{
			if (!Band.IsSet()) return;
			FCheck& C = R.Checks.AddDefaulted_GetRef();
			C.Name = Name;
			C.Value = Value;
			C.Band = Band;
			C.bCount = bCount;
		}

		FString AuditPurlinsThatFit(double Depth, const FBand& Step)
		{
			TArray<FString> Fit;
			for (const int32 N : { 3, 5, 7, 9 })
			{
				const double Run = Depth / (N - 1);
				if (Run >= Step.Min && Run <= Step.Max) Fit.Add(FString::Printf(TEXT("%d purlins (%d檁)"), N, N));
			}
			return Fit.IsEmpty() ? FString::Printf(TEXT("no purlin count (檁數) gives a step (步架) of %.0f–%.0f cm at this depth"), Step.Min, Step.Max)
				: FString::Printf(TEXT("depth fits %s"), *FString::Join(Fit, TEXT(" or ")));
		}

		FBand AuditFrontageBand(const FRole& Role)
		{
			const int32 Lo = Role.Bays.IsSet() ? FMath::RoundToInt(Role.Bays.Min) : 1;
			const int32 Hi = Role.Bays.IsSet() ? FMath::RoundToInt(Role.Bays.Max) : 7;
			const ESource Source = FMath::Max(Role.CentralBay.Source, Role.SideBay.Source);
			return AuditBandOf(Role.CentralBay.Min + (Lo - 1) * Role.SideBay.Min,
				Role.CentralBay.Max + (Hi - 1) * Role.SideBay.Max, Source, Role.CentralBay.Cite);
		}

		void AuditGate(FResult& R, const UHutongBuildingComponent* B, double Frontage, double Depth)
		{
			using HutongCanon::Gate::FSizeBand;
			FSizeBand Band;
			if (const UHutongGateHouseBuildingComponent* G = Cast<UHutongGateHouseBuildingComponent>(B))
			{
				FHutongGateHouseParams P = G->Params;
				P.bConstrainToHistoricalSize = true;
				Band = P.GetSizeRange();
				R.Role = FString::Printf(TEXT("Gate (大門), %s"), *StaticEnum<EHutongGateStyle>()->GetDisplayNameTextByValue((int64)P.Style).ToString());
			}
			else
			{
				using namespace HutongCanon::Gate;
				Band = { RuyiSize.FrontageMin, GuangliangSize.FrontageMax, RuyiSize.DepthMin, GuangliangSize.DepthMax,
					RuyiSize.EaveMin, GuangliangSize.EaveMax };
				R.Role = TEXT("Gate (大門), any rank");
			}
			AuditAdd(R, TEXT("frontage (面闊)"), Frontage, AuditBandOf(Band.FrontageMin, Band.FrontageMax, ESource::Judgement, TEXT("gate bands")));
			AuditAdd(R, TEXT("depth (進深)"), Depth, AuditBandOf(Band.DepthMin, Band.DepthMax, ESource::Judgement, TEXT("gate bands")));
			if (B->GetEaveHeight() > 0.0)
			{
				AuditAdd(R, TEXT("eave (檐高)"), B->GetEaveHeight(), AuditBandOf(Band.EaveMin, Band.EaveMax, ESource::Judgement, TEXT("gate bands")));
			}
		}

		void AuditInnerGate(FResult& R, const UHutongBuildingComponent* B, double Frontage, double Depth)
		{
			using namespace HutongCanon::Gate;
			const UHutongInnerGateBuildingComponent* G = Cast<UHutongInnerGateBuildingComponent>(B);
			const bool bGrand = G && G->Params.Style == EHutongInnerGateStyle::OneHallOneRoll;
			const FSizeBand Band = bGrand ? InnerGateGrandSize : InnerGateSize;
			R.Role = bGrand ? TEXT("Inner Gate (垂花門), hall-and-roll (一殿一卷)") : TEXT("Inner Gate (垂花門)");
			AuditAdd(R, TEXT("frontage (面闊)"), Frontage, AuditBandOf(Band.FrontageMin, Band.FrontageMax, ESource::Judgement, TEXT("inner gate bands")));
			AuditAdd(R, TEXT("depth (進深)"), Depth, AuditBandOf(Band.DepthMin, Band.DepthMax, ESource::Judgement, TEXT("inner gate bands")));
		}
	}

	double FCheck::Off() const
	{
		if (Value > Band.Max) return Value - Band.Max;
		if (Value < Band.Min) return Value - Band.Min;
		return 0.0;
	}

	FString FCheck::Describe() const
	{
		auto Show = [this](double V) { return bCount ? FString::Printf(TEXT("%.0f"), V) : AuditMetres(V); };
		const FString Range = FMath::IsNearlyEqual(Band.Min, Band.Max) ? Show(Band.Min)
			: FString::Printf(TEXT("%s–%s"), *Show(Band.Min), *Show(Band.Max));
		const double D = Off();
		const FString Verdict = Passes() ? FString(TEXT("within"))
			: FString::Printf(TEXT("%s %s"), *Show(FMath::Abs(D)), D > 0.0 ? TEXT("over") : TEXT("under"));
		return FString::Printf(TEXT("%s %s · %s (%s %s) · %s"), *Name, *Show(Value), *Range,
			*SourceName(Band.Source), Band.Cite, *Verdict);
	}

	int32 FResult::Failing() const
	{
		int32 N = 0;
		for (const FCheck& C : Checks) N += !C.Passes();
		return N;
	}

	FString SourceName(ESource Source)
	{
		switch (Source)
		{
		case ESource::Canon: return TEXT("CANON");
		case ESource::Derived: return TEXT("DERIVED");
		default: return TEXT("JUDGEMENT");
		}
	}

	FResult Audit(const UHutongBuildingComponent* B, EHutongTracedOutline Outline)
	{
		FResult R;
		if (!B) return R;

		// Frontage along the facade; a type without one is taken long side as frontage.
		const FVector2D Size = B->GetFootprintSize();
		EHutongBaySide Side;
		const bool bFacade = B->GetFacade(Side);
		const bool bFrontAlongX = bFacade ? HutongGen::BaySide::IsAlongX(Side) : Size.X >= Size.Y;
		const double Frontage = bFrontAlongX ? Size.X : Size.Y;
		double Depth = bFrontAlongX ? Size.Y : Size.X;

		const EHutongCourtRole Role = B->GetCourtRole();
		if (Role == EHutongCourtRole::Gate) { AuditGate(R, B, Frontage, Depth); return R; }
		if (Role == EHutongCourtRole::InnerGate) { AuditInnerGate(R, B, Frontage, Depth); return R; }

		const FRole* Bands = AuditRoleBands(Role);
		if (!Bands) return R;
		R.Role = StaticEnum<EHutongCourtRole>()->GetDisplayNameTextByValue((int64)Role).ToString();

		FHutongSiheyuanParams P;
		const bool bHouse = AuditHouseParams(B, P);
		if (!bHouse)
		{
			const HutongCanon::House::FHouse* House = AuditHouseFor(Role);
			P = House ? HutongPresets::MakeHouse(*House) : FHutongSiheyuanParams();
			P.Width = Frontage;
			P.Depth = Depth;
		}
		if (Outline == EHutongTracedOutline::Eaves)
		{
			const double Eaves = P.GetRoofOverhang() + P.GetRearRoofOverhang();
			Depth = FMath::Max(Depth - Eaves, 1.0);
			R.Notes.Add(FString::Printf(TEXT("traced on the eaves: %s of overhang taken off the depth"), *AuditMetres(Eaves)));
		}

		bool bLarge = false;
		if (bHouse)
		{
			bLarge = Role == EHutongCourtRole::MainHall
				? (P.Purlins == EHutongPurlins::Seven || P.Purlins == EHutongPurlins::Nine || P.bHasRearVeranda)
				: (Role == EHutongCourtRole::SideHouse && P.bHasFrontVeranda);
		}
		else if (Bands->DepthLarge.IsSet())
		{
			auto Gap = [Depth](const FBand& Band) { return Depth > Band.Max ? Depth - Band.Max : Depth < Band.Min ? Band.Min - Depth : 0.0; };
			bLarge = Gap(Bands->DepthLarge) < Gap(Bands->Depth);
		}
		const FBand& DepthBand = (bLarge && Bands->DepthLarge.IsSet()) ? Bands->DepthLarge : Bands->Depth;
		if (Bands->DepthLarge.IsSet())
		{
			R.Notes.Add(bLarge ? FString(TEXT("judged as the larger form (正房 七檁前後廊, 廂房 with 外廊)"))
				: FString(TEXT("judged as the plain form (正房 前廊後無廊, 廂房 without 外廊)")));
		}

		AuditAdd(R, TEXT("frontage (面闊)"), Frontage, AuditFrontageBand(*Bands));
		AuditAdd(R, TEXT("depth (進深)"), Depth, DepthBand);

		// The 明間 is the door bay (the middle).
		FHutongPlanBays Plan;
		B->GetPlanBays(Plan);
		TArray<double> Lines = Plan.Boundaries;
		Lines.Sort();
		if (Lines.Num() >= 2)
		{
			const int32 Count = Lines.Num() - 1;
			AuditAdd(R, TEXT("bays (間)"), Count, Bands->Bays, true);
			const int32 Central = Plan.DoorBay != INDEX_NONE ? FMath::Clamp(Plan.DoorBay, 0, Count - 1) : Count / 2;
			double WidestSide = 0.0, NarrowestSide = TNumericLimits<double>::Max();
			for (int32 i = 0; i < Count; ++i)
			{
				const double W = Lines[i + 1] - Lines[i];
				if (i == Central && Count > 1) AuditAdd(R, TEXT("central bay (明間)"), W, Bands->CentralBay);
				else if (Count == 1) AuditAdd(R, TEXT("bay (間)"), W, Bands->SideBay);
				else { WidestSide = FMath::Max(WidestSide, W); NarrowestSide = FMath::Min(NarrowestSide, W); }
			}
			// The side bay furthest out of its band speaks for them all.
			if (WidestSide > 0.0)
			{
				const double Mid = 0.5 * (Bands->SideBay.Min + Bands->SideBay.Max);
				const double Worst = FMath::Abs(WidestSide - Mid) > FMath::Abs(NarrowestSide - Mid) ? WidestSide : NarrowestSide;
				AuditAdd(R, TEXT("side bay (次間)"), Worst, Bands->SideBay);
			}
		}

		if (bHouse)
		{
			const int32 N = HutongGen::Jiajia::PurlinCount(P.Purlins);
			AuditAdd(R, FString::Printf(TEXT("step (步架) at %d purlins"), N), Depth / FMath::Max(N - 1, 1), Bands->StepRun);
			// 檐柱高 = share × 明間 (the house's own share: 0.84 on the 七檁 正房).
			const double Rule = P.ColumnHeightPerBay * P.GetCentralBayWidth();
			const double Tol = ColumnRuleTolerance * Rule;
			AuditAdd(R, TEXT("eave column (檐柱高)"), P.GetColumnHeight(),
				AuditBandOf(Rule - Tol, Rule + Tol, ESource::Canon, TEXT("8/10 明間")));
		}
		if (Bands->StepRun.IsSet()) R.Notes.Add(AuditPurlinsThatFit(Depth, Bands->StepRun));
		return R;
	}

	FString SummaryTable(const TArray<FAudited>& Audited)
	{
		struct FRow { TArray<double> Values; TArray<double> Offs; FBand Band; bool bCount = false; };
		TMap<FString, TMap<FString, FRow>> ByRole;
		TArray<FString> RoleOrder;
		int32 Judged = 0;
		for (const FAudited& A : Audited)
		{
			if (!A.Result.IsJudged()) continue;
			++Judged;
			if (!ByRole.Contains(A.Result.Role)) RoleOrder.Add(A.Result.Role);
			TMap<FString, FRow>& Rows = ByRole.FindOrAdd(A.Result.Role);
			for (const FCheck& C : A.Result.Checks)
			{
				FString Key = C.Name;
				int32 At;
				if (Key.FindChar(TEXT(' '), At) && Key.StartsWith(TEXT("step"))) Key = TEXT("step (步架)");
				FRow& Row = Rows.FindOrAdd(Key);
				Row.Values.Add(C.Value);
				if (!C.Passes()) Row.Offs.Add(C.Off());
				Row.Band = C.Band;
				Row.bCount = C.bCount;
			}
		}
		auto Median = [](TArray<double> V) { if (V.IsEmpty()) return 0.0; V.Sort(); return V[V.Num() / 2]; };
		auto Show = [](double V, bool bCount) { return bCount ? FString::Printf(TEXT("%.0f"), V) : FString::Printf(TEXT("%.2f m"), V * 0.01); };

		FString Out = FString::Printf(TEXT("%d of %d loaded buildings judged (a court role with figures).\n"), Judged, Audited.Num());
		for (const FString& Role : RoleOrder)
		{
			const TMap<FString, FRow>& Rows = ByRole[Role];
			int32 N = 0;
			for (const TPair<FString, FRow>& R : Rows) N = FMath::Max(N, R.Value.Values.Num());
			Out += FString::Printf(TEXT("\n%s — %d\n"), *Role, N);
			for (const TPair<FString, FRow>& R : Rows)
			{
				const FRow& Row = R.Value;
				Out += FString::Printf(TEXT("  %s: median %s against %s–%s; %d of %d outside"),
					*R.Key, *Show(Median(Row.Values), Row.bCount), *Show(Row.Band.Min, Row.bCount), *Show(Row.Band.Max, Row.bCount),
					Row.Offs.Num(), Row.Values.Num());
				Out += Row.Offs.IsEmpty() ? FString(TEXT("\n"))
					: FString::Printf(TEXT(", by a median %s%s\n"), Median(Row.Offs) > 0.0 ? TEXT("+") : TEXT(""), *Show(Median(Row.Offs), Row.bCount));
			}
		}
		return Out;
	}

	bool WriteCsv(const TArray<FAudited>& Audited, const FString& Path)
	{
		auto Quote = [](const FString& S) { return TEXT("\"") + S.Replace(TEXT("\""), TEXT("\"\"")) + TEXT("\""); };
		FString Out = TEXT("building,id,role,check,value_cm,min_cm,max_cm,source,cite,off_cm\n");
		for (const FAudited& A : Audited)
		{
			if (!A.Building || !A.Result.IsJudged()) continue;
			const AActor* Owner = A.Building->GetOwner();
			for (const FCheck& C : A.Result.Checks)
			{
				Out += FString::Printf(TEXT("%s,%s,%s,%s,%.1f,%.1f,%.1f,%s,%s,%.1f\n"),
					*Quote(Owner ? Owner->GetActorLabel() : FString()), *A.Building->BuildingId.ToString(),
					*Quote(A.Result.Role), *Quote(C.Name), C.Value, C.Band.Min, C.Band.Max,
					*SourceName(C.Band.Source), *Quote(C.Band.Cite), C.Off());
			}
		}
		return FFileHelper::SaveStringToFile(Out, *Path, FFileHelper::EEncodingOptions::ForceUTF8);
	}

	TArray<FString> ReadoutLines(const FResult& Result, int32 MaxFailing)
	{
		TArray<FString> Lines;
		if (!Result.IsJudged()) return Lines;
		const int32 Failing = Result.Failing();
		Lines.Add(Failing == 0
			? FString::Printf(TEXT("Qing figures (則例): %s, all %d within"), *Result.Role, Result.Checks.Num())
			: FString::Printf(TEXT("Qing figures (則例): %s, %d of %d off"), *Result.Role, Failing, Result.Checks.Num()));
		int32 Shown = 0;
		for (const FCheck& C : Result.Checks)
		{
			if (C.Passes() || Shown >= MaxFailing) continue;
			Lines.Add(TEXT("  ") + C.Describe());
			++Shown;
		}
		if (Failing > Shown) Lines.Add(FString::Printf(TEXT("  … %d more off"), Failing - Shown));
		for (const FString& Note : Result.Notes) Lines.Add(TEXT("  ") + Note);
		return Lines;
	}
}
