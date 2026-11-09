#include "Tools/HutongSharing.h"
#include "Tools/HutongDetailOps.h"
#include "Tools/HutongSnap.h"
#include "Generation/HutongActorSpawn.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongMeshTurn.h"
#include "Generation/HutongWallPieces.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "GameFramework/Actor.h"
#include "ScopedTransaction.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "HutongSharing"

namespace
{
	FString SharingCsvQuote(const FString& S)
	{
		return TEXT("\"") + S.Replace(TEXT("\""), TEXT("\"\"")) + TEXT("\"");
	}

	double SharingCornerOffset(const FHutongFootprintSkew& Skew)
	{
		return FMath::Max(
			FMath::Max(Skew.Corner00.GetAbsMax(), Skew.Corner10.GetAbsMax()),
			FMath::Max(Skew.Corner11.GetAbsMax(), Skew.Corner01.GetAbsMax()));
	}

	bool IsSharingExempt(const UHutongBuildingComponent* Building)
	{
		return Building->bLocked || Building->IsA<UHutongCityWallBuildingComponent>() || Building->IsA<UHutongWallCornerBuildingComponent>()
			|| Building->IsA<UHutongArrowTowerBuildingComponent>() || Building->IsA<UHutongCityGateBuildingComponent>();
	}

	double SnapSharingSize(double Size, const HutongSharing::FRule& Rule)
	{
		if (Rule.SizeStep <= 0.0) return Size;
		const double Snapped = FMath::RoundToDouble(Size / Rule.SizeStep) * Rule.SizeStep;
		return Snapped >= Rule.SizeStep ? Snapped : Size;
	}

	double SharingSideMove(double From, double To, const HutongSharing::FRule& Rule)
	{
		return FMath::Abs(From - To) * (Rule.bTowardFigures ? 0.5 : 1.0);
	}

	double SnapWallOpening(double& Fraction, double OldLength, double NewLength, const HutongSharing::FRule& Rule)
	{
		const double Inset = Rule.bTowardFigures ? 0.5 * (OldLength - NewLength) : 0.0;
		const double Was = Fraction * OldLength - Inset;
		const double Now = FMath::RoundToDouble(Was / Rule.SizeStep) * Rule.SizeStep;
		Fraction = FMath::Clamp(Now / NewLength, 0.0, 1.0);
		return FMath::Abs(Now - Was);
	}

	// Bay counts are often set by hand off the map: a new size keeps them.
	void SetSharingSize(UHutongBuildingComponent* Copy, const FVector2D& Size)
	{
		const int32 Bays = Copy->GetDrawnBayCount();
		Copy->SetFootprintSize(Size);
		if (HutongDetailOps::GetBayCountOverride(Copy) != INDEX_NONE && Copy->GetDrawnBayCount() != Bays)
		{
			HutongDetailOps::SetBayCountOverride(Copy, Bays);
		}
	}

	TArray<double> NeighbouringSteps(double Value, double Step)
	{
		if (Step <= 0.0) return { Value };
		const double Below = FMath::FloorToDouble(Value / Step) * Step;
		if (Below == Value) return { Value };
		return Below >= Step ? TArray<double>{ Below, Below + Step } : TArray<double>{ Below + Step };
	}

	struct FSharingOption
	{
		FVector2D Size = FVector2D::ZeroVector;
		double Height = 0.0;
		int32 Failing = 0;
		double Off = 0.0;
		double Move = 0.0;
		FVector2D Quad[4];
	};

	bool SharingOptionBeats(const FSharingOption& A, const FSharingOption& B)
	{
		if (A.Failing != B.Failing) return A.Failing < B.Failing;
		if (FMath::Abs(A.Off - B.Off) > 0.5) return A.Off < B.Off;
		return A.Move < B.Move;
	}

	bool SharingSquares(const UHutongBuildingComponent* Building, const HutongSharing::FRule& Rule)
	{
		const double Offset = SharingCornerOffset(Building->FootprintSkew);
		return Rule.SquareUpTo > 0.0 && Offset > 0.0 && Offset <= Rule.SquareUpTo;
	}

	void SharingQuad(const UHutongBuildingComponent* Building, const FVector2D& Size, bool bSquared, FVector2D OutQuad[4])
	{
		FVector2D Local[4];
		HutongFootprint::Corners(Size, bSquared ? FHutongFootprintSkew() : Building->FootprintSkew, Local);
		const FVector2D Inset = 0.5 * (Building->GetFootprintSize() - Size);
		const FTransform Placed = Building->GetOwner() ? Building->GetOwner()->GetActorTransform() : FTransform::Identity;
		for (int32 i = 0; i < 4; ++i)
		{
			const FVector At = Placed.TransformPosition(FVector(Local[i] + Inset, 0.0));
			OutQuad[i] = FVector2D(At.X, At.Y);
		}
	}

	TArray<FSharingOption> SharingOptions(const UHutongBuildingComponent* Building, const HutongSharing::FRule& Rule, EHutongTracedOutline Outline)
	{
		TArray<FSharingOption> Options;
		const FVector2D Old = Building->GetFootprintSize();
		const bool bSquared = SharingSquares(Building, Rule);
		for (const double X : NeighbouringSteps(Old.X, Rule.SizeStep))
		{
			for (const double Y : NeighbouringSteps(Old.Y, Rule.SizeStep))
			{
				UHutongBuildingComponent* Sized = DuplicateObject<UHutongBuildingComponent>(Building, GetTransientPackage());
				SetSharingSize(Sized, FVector2D(X, Y));
				const double Height = Sized->GetEditHeight();
				const bool bHeight = Sized->CanSetEditHeight();
				FSharingOption Best;
				bool bAny = false;
				for (const double H : bHeight ? NeighbouringSteps(Height, Rule.HeightStep) : TArray<double>{ Height })
				{
					if (bHeight) Sized->SetEditHeight(H);
					const HutongAudit::FResult Result = HutongAudit::Audit(Sized, Outline);
					FSharingOption Option;
					Option.Size = FVector2D(X, Y);
					Option.Height = H;
					Option.Failing = Result.Failing();
					for (const HutongAudit::FCheck& Check : Result.Checks) Option.Off += FMath::Abs(Check.Off());
					Option.Move = FMath::Abs(X - Old.X) + FMath::Abs(Y - Old.Y) + FMath::Abs(H - Height);
					if (!bAny || SharingOptionBeats(Option, Best)) Best = Option;
					bAny = true;
				}
				SharingQuad(Building, Best.Size, bSquared, Best.Quad);
				Options.Add(Best);
				Sized->MarkAsGarbage();
			}
		}
		Options.Sort(SharingOptionBeats);
		return Options;
	}

	// Mean distance from edge I of A to edge J of B along A's outward normal, where they face each other; negative overlaps.
	bool SharingEdgeGap(const FVector2D A[4], int32 I, const FVector2D B[4], int32 J, double& OutGap, double& OutSpan, FVector2D* OutNormal = nullptr)
	{
		const auto Outward = [](const FVector2D Q[4], int32 Edge, FVector2D& Along, double& Length)
		{
			const FVector2D Run = Q[(Edge + 1) % 4] - Q[Edge];
			Length = Run.Size();
			Along = Run / FMath::Max(Length, 1.0e-6);
			double Area = 0.0;
			for (int32 k = 0; k < 4; ++k) Area += FVector2D::CrossProduct(Q[k], Q[(k + 1) % 4]);
			return Area > 0.0 ? FVector2D(Along.Y, -Along.X) : FVector2D(-Along.Y, Along.X);
		};
		FVector2D AlongA, AlongB;
		double LengthA, LengthB;
		const FVector2D NormalA = Outward(A, I, AlongA, LengthA);
		const FVector2D NormalB = Outward(B, J, AlongB, LengthB);
		if (FVector2D::DotProduct(NormalA, NormalB) > -FMath::Cos(FMath::DegreesToRadians(12.0))) return false;
		const FVector2D From = B[J] - A[I], To = B[(J + 1) % 4] - A[I];
		const double U0 = FVector2D::DotProduct(From, AlongA), U1 = FVector2D::DotProduct(To, AlongA);
		const double Lo = FMath::Max(0.0, FMath::Min(U0, U1)), Hi = FMath::Min(LengthA, FMath::Max(U0, U1));
		if (FMath::Abs(U1 - U0) < 1.0e-6) return false;
		OutSpan = Hi - Lo;
		const double S0 = FVector2D::DotProduct(From, NormalA), S1 = FVector2D::DotProduct(To, NormalA);
		OutGap = S0 + (S1 - S0) * (0.5 * (Lo + Hi) - U0) / (U1 - U0);
		if (OutNormal) *OutNormal = NormalA;
		return true;
	}

	struct FSharingContact
	{
		int32 A = 0, B = 0, EdgeA = 0, EdgeB = 0;
		FVector2D Normal = FVector2D::ZeroVector;
	};

	struct FSharingNode
	{
		TArray<FSharingOption> Options;
		int32 Option = 0;
		FVector2D Shift = FVector2D::ZeroVector;
	};

	struct FSharingItem
	{
		const UHutongBuildingComponent* Building = nullptr;
		FVector2D Drawn[4];
		FVector2D Now[4];
		int32 Node = INDEX_NONE;
		const UHutongWallBuildingComponent* Wall = nullptr;
	};

	TArray<TPair<int32, int32>> FindSharingNearPairs(const TArray<FSharingItem>& Items, double Reach)
	{
		constexpr double Cell = 2000.0;
		TMap<FIntPoint, TArray<int32>> Grid;
		TArray<FBox2D> Bounds;
		for (int32 N = 0; N < Items.Num(); ++N)
		{
			FBox2D Box(EForceInit::ForceInit);
			for (const FVector2D& Corner : Items[N].Drawn) Box += Corner;
			Box = Box.ExpandBy(0.5 * Reach);
			Bounds.Add(Box);
			for (int32 X = FMath::FloorToInt(Box.Min.X / Cell); X <= FMath::FloorToInt(Box.Max.X / Cell); ++X)
			{
				for (int32 Y = FMath::FloorToInt(Box.Min.Y / Cell); Y <= FMath::FloorToInt(Box.Max.Y / Cell); ++Y)
				{
					Grid.FindOrAdd(FIntPoint(X, Y)).Add(N);
				}
			}
		}
		TSet<uint64> Seen;
		TArray<TPair<int32, int32>> Pairs;
		for (const TPair<FIntPoint, TArray<int32>>& Bucket : Grid)
		{
			for (int32 P = 0; P < Bucket.Value.Num(); ++P)
			{
				for (int32 Q = P + 1; Q < Bucket.Value.Num(); ++Q)
				{
					const int32 A = FMath::Min(Bucket.Value[P], Bucket.Value[Q]), B = FMath::Max(Bucket.Value[P], Bucket.Value[Q]);
					bool bSeen = false;
					Seen.Add((uint64(A) << 32) | uint64(B), &bSeen);
					if (!bSeen && Bounds[A].Intersect(Bounds[B])) Pairs.Emplace(A, B);
				}
			}
		}
		return Pairs;
	}

	// Facing edges within a drawn line of each other (25-35 cm), as drawn. A wall's end is its thickness across.
	bool FindSharingContact(const FSharingItem& A, const FSharingItem& B, FSharingContact& Out)
	{
		constexpr double Touching = 35.0;
		const double MinSpan = A.Wall || B.Wall ? 12.0 : 40.0;
		double Nearest = Touching;
		bool bFound = false;
		for (int32 I = 0; I < 4; ++I)
		{
			for (int32 J = 0; J < 4; ++J)
			{
				double Gap, Span;
				FVector2D Normal;
				if (!SharingEdgeGap(A.Drawn, I, B.Drawn, J, Gap, Span, &Normal) || Span < MinSpan || FMath::Abs(Gap) > Nearest) continue;
				Nearest = FMath::Abs(Gap);
				Out.EdgeA = I;
				Out.EdgeB = J;
				Out.Normal = Normal;
				bFound = true;
			}
		}
		return bFound;
	}

	double SharingOverlapDepth(const FVector2D A[4], const FVector2D B[4])
	{
		double Depth = TNumericLimits<double>::Max();
		for (const FVector2D* Q : { A, B })
		{
			for (int32 I = 0; I < 4; ++I)
			{
				const FVector2D Across = (Q[(I + 1) % 4] - Q[I]).GetRotated(90.0).GetSafeNormal();
				double MinA = TNumericLimits<double>::Max(), MaxA = -MinA, MinB = MinA, MaxB = -MinA;
				for (int32 K = 0; K < 4; ++K)
				{
					const double OnA = FVector2D::DotProduct(A[K], Across), OnB = FVector2D::DotProduct(B[K], Across);
					MinA = FMath::Min(MinA, OnA);
					MaxA = FMath::Max(MaxA, OnA);
					MinB = FMath::Min(MinB, OnB);
					MaxB = FMath::Max(MaxB, OnB);
				}
				const double Along = FMath::Min(MaxA, MaxB) - FMath::Max(MinA, MinB);
				if (Along <= 0.0) return 0.0;
				Depth = FMath::Min(Depth, Along);
			}
		}
		return Depth;
	}

	// Over 16 cm together (the noise a drawn line allows) and at least 5 cm deeper than drawn.
	bool SharingCollides(const FSharingItem& A, const FSharingItem& B)
	{
		constexpr double Noise = 16.0, Deeper = 5.0;
		const double Depth = SharingOverlapDepth(A.Now, B.Now);
		return Depth > Noise && Depth > SharingOverlapDepth(A.Drawn, B.Drawn) + Deeper;
	}

	double SolveSharingShifts(TArray<FSharingNode>& Nodes, const TArray<FSharingContact>& Contacts, int32 MaxSweeps)
	{
		for (int32 Sweep = 0; Sweep < MaxSweeps; ++Sweep)
		{
			double Worst = 0.0;
			for (const FSharingContact& C : Contacts)
			{
				FSharingNode& A = Nodes[C.A];
				FSharingNode& B = Nodes[C.B];
				double Gap = 0.0, Span = 0.0;
				if (!SharingEdgeGap(A.Options[A.Option].Quad, C.EdgeA, B.Options[B.Option].Quad, C.EdgeB, Gap, Span)) continue;
				const double Open = Gap + FVector2D::DotProduct(B.Shift - A.Shift, C.Normal);
				B.Shift -= 0.5 * Open * C.Normal;
				A.Shift += 0.5 * Open * C.Normal;
				Worst = FMath::Max(Worst, FMath::Abs(Open));
			}
			if (Worst < 0.05) break;
		}
		double Cost = 0.0;
		for (const FSharingNode& Node : Nodes) Cost += Node.Shift.SizeSquared();
		return Cost;
	}

	// Holding each building to its best leaves a row almost nothing to cancel with, hence Slack.
	void CancelSharingChanges(TArray<FSharingNode>& Nodes, const TArray<FSharingContact>& Contacts, double Slack)
	{
		double Cost = SolveSharingShifts(Nodes, Contacts, 3000);
		for (int32 Pass = 0; Pass < 8; ++Pass)
		{
			bool bImproved = false;
			for (FSharingNode& Node : Nodes)
			{
				const FSharingOption Best = Node.Options[0];
				for (int32 O = 0; O < Node.Options.Num(); ++O)
				{
					const FSharingOption& Option = Node.Options[O];
					if (O == Node.Option || Option.Failing > Best.Failing || Option.Off > Best.Off + Slack) continue;
					const int32 Was = Node.Option;
					TArray<FVector2D> Before;
					for (const FSharingNode& Each : Nodes) Before.Add(Each.Shift);
					Node.Option = O;
					const double Tried = SolveSharingShifts(Nodes, Contacts, 400);
					if (Tried < Cost - 1.0)
					{
						Cost = Tried;
						bImproved = true;
					}
					else
					{
						Node.Option = Was;
						for (int32 N = 0; N < Nodes.Num(); ++N) Nodes[N].Shift = Before[N];
					}
				}
			}
			if (!bImproved) break;
		}
		SolveSharingShifts(Nodes, Contacts, 3000);
	}

	double ApplySharingRule(UHutongBuildingComponent* Copy, const HutongSharing::FRule& Rule, EHutongTracedOutline Outline, const HutongSharing::FChosen* Given)
	{
		const FVector2D Old = Copy->GetFootprintSize();
		UHutongWallBuildingComponent* Wall = Cast<UHutongWallBuildingComponent>(Copy);
		FVector2D New(SnapSharingSize(Old.X, Rule), SnapSharingSize(Old.Y, Rule));
		if (Wall) (Wall->IsRunAlongY() ? New.X : New.Y) = (Wall->IsRunAlongY() ? Old.X : Old.Y);
		double ChosenHeight = 0.0;
		const bool bChosen = Rule.bTowardFigures && !Wall;
		if (Given)
		{
			New = Given->Size;
			ChosenHeight = Given->Height;
		}
		else if (bChosen)
		{
			const FSharingOption Best = SharingOptions(Copy, Rule, Outline)[0];
			New = Best.Size;
			ChosenHeight = Best.Height;
		}
		double Move = FMath::Max(SharingSideMove(Old.X, New.X, Rule), SharingSideMove(Old.Y, New.Y, Rule));
		SetSharingSize(Copy, New);

		if (Wall && Rule.SizeStep > 0.0)
		{
			const double OldLength = Wall->IsRunAlongY() ? Old.Y : Old.X;
			const double NewLength = Wall->IsRunAlongY() ? New.Y : New.X;
			Move = FMath::Max(Move, SnapWallOpening(Wall->Params.GatePosition, OldLength, NewLength, Rule));
			Move = FMath::Max(Move, SnapWallOpening(Wall->Params.DoorwayPosition, OldLength, NewLength, Rule));
			for (double& Position : Wall->Params.MoreGatePositions)
			{
				Move = FMath::Max(Move, SnapWallOpening(Position, OldLength, NewLength, Rule));
			}
		}

		const double Offset = SharingCornerOffset(Copy->FootprintSkew);
		// The neighbour search planned a wall's ends with their corners: squaring one would open its join.
		if (SharingSquares(Copy, Rule) && !(Wall && Given))
		{
			Copy->FootprintSkew.Corner00 = Copy->FootprintSkew.Corner10 = FVector2D::ZeroVector;
			Copy->FootprintSkew.Corner11 = Copy->FootprintSkew.Corner01 = FVector2D::ZeroVector;
			Move = FMath::Max(Move, Offset);
		}

		// After the footprint: a derived eave follows the bays.
		if (Rule.HeightStep > 0.0 && Copy->CanSetEditHeight())
		{
			Copy->SetEditHeight(bChosen ? ChosenHeight : FMath::RoundToDouble(Copy->GetEditHeight() / Rule.HeightStep) * Rule.HeightStep);
		}
		return Move;
	}
}

namespace HutongSharing
{
	TArray<FRule> Rules()
	{
		return {
			{ TEXT("As laid out"), 0.0, false, 0.0, 0.0 },
			{ TEXT("Small corners squared, up to 16 cm"), 0.0, false, 16.0, 0.0 },
			{ TEXT("Toward the Qing figures, each side up to 16 cm"), 32.0, true, 16.0, 32.0 },
			{ TEXT("Same, neighbours cancelling"), 32.0, true, 16.0, 32.0, 64.0 },
			{ TEXT("One side moves up to 32 cm (needs lanes marked)"), 64.0, false, 32.0, 32.0 },
		};
	}

	TMap<const UHutongBuildingComponent*, FChosen> ChooseWithNeighbours(const TArray<UHutongBuildingComponent*>& Buildings, const FRule& Rule, EHutongTracedOutline Outline)
	{
		TArray<FSharingItem> Items;
		TArray<FSharingNode> All;
		for (const UHutongBuildingComponent* Building : Buildings)
		{
			if (!Building->GetOwner()) continue;
			FSharingItem& Item = Items.AddDefaulted_GetRef();
			Item.Building = Building;
			Item.Wall = Cast<UHutongWallBuildingComponent>(Building);
			SharingQuad(Building, Building->GetFootprintSize(), false, Item.Drawn);
			FMemory::Memcpy(Item.Now, Item.Drawn, sizeof(Item.Now));
			if (IsSharingExempt(Building) || Item.Wall) continue;
			Item.Node = All.Num();
			FSharingNode& Node = All.AddDefaulted_GetRef();
			Node.Options = SharingOptions(Building, Rule, Outline);
		}

		// Far enough that a pushed building still meets whatever it could reach.
		const TArray<TPair<int32, int32>> Near = FindSharingNearPairs(Items, 300.0);
		TArray<FSharingContact> ItemContacts;
		TArray<TPair<int32, int32>> Apart;
		TArray<FSharingContact> Contacts;
		for (const TPair<int32, int32>& Pair : Near)
		{
			FSharingContact Contact;
			if (!FindSharingContact(Items[Pair.Key], Items[Pair.Value], Contact))
			{
				Apart.Add(Pair);
				continue;
			}
			Contact.A = Pair.Key;
			Contact.B = Pair.Value;
			ItemContacts.Add(Contact);
			if (Items[Pair.Key].Node != INDEX_NONE && Items[Pair.Value].Node != INDEX_NONE)
			{
				Contact.A = Items[Pair.Key].Node;
				Contact.B = Items[Pair.Value].Node;
				Contacts.Add(Contact);
			}
		}

		TMultiMap<int32, int32> Linked;
		TMultiMap<int32, int32> ContactsFrom;
		for (int32 C = 0; C < Contacts.Num(); ++C)
		{
			Linked.Add(Contacts[C].A, Contacts[C].B);
			Linked.Add(Contacts[C].B, Contacts[C].A);
			ContactsFrom.Add(Contacts[C].A, C);
		}
		TArray<int32> GroupOf;
		GroupOf.Init(INDEX_NONE, All.Num());
		TArray<TArray<int32>> Groups;
		for (int32 Start = 0; Start < All.Num(); ++Start)
		{
			if (GroupOf[Start] != INDEX_NONE) continue;
			const int32 Group = Groups.Num();
			TArray<int32>& Members = Groups.AddDefaulted_GetRef();
			Members.Add(Start);
			GroupOf[Start] = Group;
			for (int32 M = 0; M < Members.Num(); ++M)
			{
				TArray<int32> Next;
				Linked.MultiFind(Members[M], Next);
				for (const int32 N : Next)
				{
					if (GroupOf[N] == INDEX_NONE)
					{
						GroupOf[N] = Group;
						Members.Add(N);
					}
				}
			}
			TMap<int32, int32> LocalOf;
			TArray<FSharingNode> Nodes;
			for (const int32 M : Members)
			{
				LocalOf.Add(M, Nodes.Num());
				Nodes.Add(All[M]);
			}
			TArray<FSharingContact> Joins;
			for (const int32 M : Members)
			{
				TArray<int32> From;
				ContactsFrom.MultiFind(M, From);
				for (const int32 C : From)
				{
					FSharingContact Join = Contacts[C];
					Join.A = LocalOf[Join.A];
					Join.B = LocalOf[Join.B];
					Joins.Add(Join);
				}
			}
			if (Rule.CancelSlack >= 0.0) CancelSharingChanges(Nodes, Joins, Rule.CancelSlack);
			else SolveSharingShifts(Nodes, Joins, 3000);
			for (int32 M = 0; M < Members.Num(); ++M) All[Members[M]] = Nodes[M];
		}

		TArray<double> Held;
		Held.Init(1.0, Groups.Num());
		const auto PlaceNodes = [&Items, &All, &GroupOf, &Held]()
		{
			for (FSharingItem& Item : Items)
			{
				if (Item.Node == INDEX_NONE) continue;
				const FSharingNode& Node = All[Item.Node];
				const FVector2D Shift = Node.Shift * Held[GroupOf[Item.Node]];
				for (int32 K = 0; K < 4; ++K) Item.Now[K] = Node.Options[Node.Option].Quad[K] + Shift;
			}
		};
		PlaceNodes();
		for (int32 Round = 0; Round < 4; ++Round)
		{
			TSet<int32> Stepping;
			for (const TPair<int32, int32>& Pair : Apart)
			{
				if (!SharingCollides(Items[Pair.Key], Items[Pair.Value])) continue;
				for (const int32 Each : { Items[Pair.Key].Node, Items[Pair.Value].Node })
				{
					if (Each != INDEX_NONE && Held[GroupOf[Each]] > 0.0 && Groups[GroupOf[Each]].Num() > 1) Stepping.Add(GroupOf[Each]);
				}
			}
			if (Stepping.IsEmpty()) break;
			for (const int32 Group : Stepping) Held[Group] = FMath::Max(Held[Group] - 0.25, 0.0);
			PlaceNodes();
		}
		// Some collisions come from the resize alone: give back whatever push did not help.
		const auto CountCollisions = [&Items, &Apart]()
		{
			int32 Count = 0;
			for (const TPair<int32, int32>& Pair : Apart) Count += SharingCollides(Items[Pair.Key], Items[Pair.Value]);
			return Count;
		};
		int32 Collisions = CountCollisions();
		for (int32 Group = 0; Group < Groups.Num(); ++Group)
		{
			if (Held[Group] >= 1.0) continue;
			const double Was = Held[Group];
			Held[Group] = 1.0;
			PlaceNodes();
			const int32 Freed = CountCollisions();
			if (Freed <= Collisions) Collisions = Freed;
			else
			{
				Held[Group] = Was;
				PlaceNodes();
			}
		}

		struct FSharingWallMove
		{
			FVector2D Along = FVector2D::ZeroVector, Across = FVector2D::ZeroVector;
			double Offset = 0.0, Pull = 0.1, Sum = 0.0;
			double EndSum[2] = { 0.0, 0.0 };
			int32 EndCount[2] = { 0, 0 };
			TArray<TPair<int32, double>> Tied;
		};
		const auto RunEdge = [](const UHutongWallBuildingComponent* Wall, int32 Edge)
		{
			// Edges anticlockwise from the origin: 0 on y=0, 1 on x=W, 2 on y=D, 3 on x=0.
			if (Wall->IsRunAlongY()) return Edge == 0 ? 0 : (Edge == 2 ? 1 : INDEX_NONE);
			return Edge == 3 ? 0 : (Edge == 1 ? 1 : INDEX_NONE);
		};
		TMap<int32, FSharingWallMove> Moves;
		for (int32 N = 0; N < Items.Num(); ++N)
		{
			const FSharingItem& Item = Items[N];
			if (!Item.Wall || IsSharingExempt(Item.Building)) continue;
			FSharingWallMove& Move = Moves.Add(N);
			Move.Along = (Item.Wall->IsRunAlongY() ? Item.Drawn[3] - Item.Drawn[0] : Item.Drawn[1] - Item.Drawn[0]).GetSafeNormal();
			Move.Across = FVector2D(-Move.Along.Y, Move.Along.X);
		}
		for (const FSharingContact& C : ItemContacts)
		{
			FSharingWallMove* MoveA = Moves.Find(C.A);
			FSharingWallMove* MoveB = Moves.Find(C.B);
			const bool bSideA = MoveA && RunEdge(Items[C.A].Wall, C.EdgeA) == INDEX_NONE;
			const bool bSideB = MoveB && RunEdge(Items[C.B].Wall, C.EdgeB) == INDEX_NONE;
			if (MoveA && MoveB)
			{
				if (bSideA != bSideB) continue;
				const double Sign = FVector2D::DotProduct(MoveA->Across, MoveB->Across);
				MoveA->Tied.Emplace(C.B, Sign);
				MoveB->Tied.Emplace(C.A, Sign);
				continue;
			}
			double DrawnGap, NowGap, Span;
			if (!SharingEdgeGap(Items[C.A].Drawn, C.EdgeA, Items[C.B].Drawn, C.EdgeB, DrawnGap, Span)) continue;
			if (!SharingEdgeGap(Items[C.A].Now, C.EdgeA, Items[C.B].Now, C.EdgeB, NowGap, Span)) continue;
			if (bSideA)
			{
				MoveA->Sum += (NowGap - DrawnGap) * FVector2D::DotProduct(C.Normal, MoveA->Across);
				MoveA->Pull += 1.0;
			}
			else if (bSideB)
			{
				MoveB->Sum += (DrawnGap - NowGap) * FVector2D::DotProduct(C.Normal, MoveB->Across);
				MoveB->Pull += 1.0;
			}
		}
		for (int32 Sweep = 0; Sweep < 60; ++Sweep)
		{
			for (TPair<int32, FSharingWallMove>& Entry : Moves)
			{
				FSharingWallMove& Move = Entry.Value;
				double Sum = Move.Sum;
				for (const TPair<int32, double>& Tie : Move.Tied) Sum += Moves[Tie.Key].Offset * Tie.Value;
				Move.Offset = Sum / (Move.Pull + Move.Tied.Num());
			}
		}
		for (const TPair<int32, FSharingWallMove>& Entry : Moves)
		{
			FSharingItem& Item = Items[Entry.Key];
			for (int32 K = 0; K < 4; ++K) Item.Now[K] = Item.Drawn[K] + Entry.Value.Across * Entry.Value.Offset;
		}
		for (const FSharingContact& C : ItemContacts)
		{
			FSharingWallMove* MoveA = Moves.Find(C.A);
			FSharingWallMove* MoveB = Moves.Find(C.B);
			const int32 EndA = MoveA ? RunEdge(Items[C.A].Wall, C.EdgeA) : INDEX_NONE;
			const int32 EndB = MoveB ? RunEdge(Items[C.B].Wall, C.EdgeB) : INDEX_NONE;
			if (EndA == INDEX_NONE && EndB == INDEX_NONE) continue;
			double DrawnGap, NowGap, Span;
			if (!SharingEdgeGap(Items[C.A].Drawn, C.EdgeA, Items[C.B].Drawn, C.EdgeB, DrawnGap, Span)) continue;
			if (!SharingEdgeGap(Items[C.A].Now, C.EdgeA, Items[C.B].Now, C.EdgeB, NowGap, Span)) continue;
			const double Reach = (NowGap - DrawnGap) * (EndA != INDEX_NONE && EndB != INDEX_NONE ? 0.5 : 1.0);
			if (EndA != INDEX_NONE)
			{
				MoveA->EndSum[EndA] += Reach;
				++MoveA->EndCount[EndA];
			}
			if (EndB != INDEX_NONE)
			{
				MoveB->EndSum[EndB] += Reach;
				++MoveB->EndCount[EndB];
			}
		}

		TMap<const UHutongBuildingComponent*, FChosen> Chosen;
		for (const TPair<int32, FSharingWallMove>& Entry : Moves)
		{
			FSharingItem& Item = Items[Entry.Key];
			const FSharingWallMove& Move = Entry.Value;
			const double Start = Move.EndCount[0] > 0 ? Move.EndSum[0] / Move.EndCount[0] : 0.0;
			const double End = Move.EndCount[1] > 0 ? Move.EndSum[1] / Move.EndCount[1] : 0.0;
			const FVector2D Old = Item.Building->GetFootprintSize();
			const FVector2D Size = Item.Wall->IsRunAlongY() ? FVector2D(Old.X, Old.Y + Start + End) : FVector2D(Old.X + Start + End, Old.Y);
			FChosen Wall;
			Wall.Size = Size;
			Wall.Shift = Move.Across * Move.Offset + Move.Along * (0.5 * (End - Start));
			Wall.Stretch = Start + End;
			SharingQuad(Item.Building, Size, false, Item.Now);
			for (int32 K = 0; K < 4; ++K) Item.Now[K] += Wall.Shift;
			Chosen.Add(Item.Building, Wall);
		}
		for (const FSharingItem& Item : Items)
		{
			if (Item.Node == INDEX_NONE) continue;
			const FSharingNode& Node = All[Item.Node];
			const int32 Group = GroupOf[Item.Node];
			FChosen Each;
			Each.Size = Node.Options[Node.Option].Size;
			Each.Height = Node.Options[Node.Option].Height;
			Each.Shift = Node.Shift * Held[Group];
			Each.GroupSize = Groups[Group].Num();
			Each.bHeldBack = Held[Group] < 1.0;
			Chosen.Add(Item.Building, Each);
		}
		for (const TPair<int32, int32>& Pair : Apart)
		{
			if (!SharingCollides(Items[Pair.Key], Items[Pair.Value])) continue;
			const double Depth = SharingOverlapDepth(Items[Pair.Key].Now, Items[Pair.Value].Now);
			const double DrawnDepth = SharingOverlapDepth(Items[Pair.Key].Drawn, Items[Pair.Value].Drawn);
			for (const auto& [Each, Other] : { TPair<int32, int32>(Pair.Key, Pair.Value), TPair<int32, int32>(Pair.Value, Pair.Key) })
			{
				FChosen* Found = Chosen.Find(Items[Each].Building);
				if (!Found) continue;
				Found->bCollides = true;
				if (Depth <= Found->CollideDepth) continue;
				Found->CollidesWith = Items[Other].Building->GetOwner()->GetActorLabel();
				Found->CollideDepth = Depth;
				Found->DrawnDepth = DrawnDepth;
			}
		}
		return Chosen;
	}

	FOutcome Measure(const UHutongBuildingComponent* Building, const FRule& Rule, EHutongTracedOutline Outline, const FChosen* Chosen)
	{
		FOutcome Out;
		UHutongBuildingComponent* Copy = DuplicateObject<UHutongBuildingComponent>(Building, GetTransientPackage());
		// A plan builds nothing; the copy answers for what Generate would make of it.
		Copy->bPlanOnly = false;
		if (!IsSharingExempt(Building)) Out.EdgeMove = ApplySharingRule(Copy, Rule, Outline, Chosen);
		if (Chosen) Out.Chosen = *Chosen;
		Out.Size = Copy->GetFootprintSize();
		Out.Height = Copy->GetEditHeight();
		Out.Bays = Copy->GetDrawnBayCount();
		Out.bSquared = Building->HasFootprintSkew() && !Copy->HasFootprintSkew();
		HutongAudit::FResult Audited = HutongAudit::Audit(Copy, Outline);
		Out.AuditOff = Audited.Failing();
		Out.AuditRole = Audited.Role;
		Out.AuditChecks = MoveTemp(Audited.Checks);

		if (Building->HasGeometry() && !Building->BuildsInstanced())
		{
			TArray<UE::Geometry::FDynamicMesh3> LODs;
			HutongGen::FMeshTurn Turn;
			const int32 CollisionLOD = Copy->BuildLODs(LODs, &Turn);
			if (LODs.Num() > 0 && LODs[0].TriangleCount() > 0)
			{
				Out.Mesh = HutongGen::LibraryKeyFor(LODs, CollisionLOD);
				Out.Triangles = LODs[0].TriangleCount();
			}
		}
		Copy->MarkAsGarbage();
		return Out;
	}

	bool MeasureAll(const TArray<UHutongBuildingComponent*>& Buildings, EHutongTracedOutline Outline, bool bShowDialog,
		TArray<FRow>& OutRows)
	{
		const TArray<FRule> AllRules = Rules();
		FScopedSlowTask Task((float)Buildings.Num(), LOCTEXT("Measuring", "Counting meshes at each size rule…"));
		if (bShowDialog) Task.MakeDialog(/*bShowCancelButton*/ true);
		OutRows.Reset(Buildings.Num());
		TArray<TMap<const UHutongBuildingComponent*, FChosen>> ChosenPerRule;
		for (const FRule& Rule : AllRules)
		{
			ChosenPerRule.Add(Rule.bTowardFigures ? ChooseWithNeighbours(Buildings, Rule, Outline) : TMap<const UHutongBuildingComponent*, FChosen>());
		}
		for (const UHutongBuildingComponent* Building : Buildings)
		{
			if (Task.ShouldCancel()) return false;
			Task.EnterProgressFrame();
			FRow& Row = OutRows.AddDefaulted_GetRef();
			Row.Building = Building;
			for (int32 R = 0; R < AllRules.Num(); ++R)
			{
				Row.Outcomes.Add(Measure(Building, AllRules[R], Outline, ChosenPerRule[R].Find(Building)));
			}
		}
		return true;
	}

	FString SummaryTable(const TArray<FRow>& Rows)
	{
		const TArray<FRule> AllRules = Rules();
		FString Out = FString::Printf(TEXT("%d buildings. Nothing was moved.\n"), Rows.Num());
		TMap<FString, int32> TypeCounts;
		for (const FRow& Row : Rows)
		{
			++TypeCounts.FindOrAdd(Row.Building->GetPresetKey().ToString());
		}
		TypeCounts.ValueSort([](int32 A, int32 B) { return A > B; });

		for (int32 R = 0; R < AllRules.Num(); ++R)
		{
			TMap<FString, int32> MeshTriangles;
			TMap<FString, TSet<FString>> TypeMeshes;
			double Furthest = 0.0;
			int32 Squared = 0, Moved = 0, Worse = 0, FarShifts = 0, HeldBack = 0, Collides = 0;
			TArray<double> Shifts, WallShifts, Stretches;
			for (const FRow& Row : Rows)
			{
				const FOutcome& O = Row.Outcomes[R];
				if (!O.Mesh.IsEmpty())
				{
					MeshTriangles.Add(O.Mesh, O.Triangles);
					TypeMeshes.FindOrAdd(Row.Building->GetPresetKey().ToString()).Add(O.Mesh);
				}
				Furthest = FMath::Max(Furthest, O.EdgeMove);
				Squared += O.bSquared;
				Moved += O.EdgeMove > 0.0;
				Worse += O.AuditOff > Row.Outcomes[0].AuditOff;
				HeldBack += O.Chosen.bHeldBack;
				Collides += O.Chosen.bCollides;
				if (Row.Building->IsA<UHutongWallBuildingComponent>() && !O.Chosen.Size.IsZero())
				{
					WallShifts.Add(FMath::Abs(FVector2D::DotProduct(O.Chosen.Shift, Row.Building->IsRunAlongY() ? FVector2D(1.0, 0.0) : FVector2D(0.0, 1.0))));
					Stretches.Add(FMath::Abs(O.Chosen.Stretch));
				}
				if (O.Chosen.GroupSize > 1)
				{
					Shifts.Add(O.Chosen.Shift.Size());
					FarShifts += O.Chosen.Shift.Size() > 32.0;
				}
			}
			int64 Triangles = 0;
			for (const TPair<FString, int32>& Mesh : MeshTriangles) Triangles += Mesh.Value;
			Out += FString::Printf(TEXT("\n%s: %d meshes%s, %lld triangles stored\n"), *AllRules[R].Name, MeshTriangles.Num(),
				HutongGen::WallPieces::IsEnabled() ? TEXT(" besides walls") : TEXT(""), Triangles);
			if (R > 0)
			{
				Out += FString::Printf(TEXT("  %d buildings moved, furthest %.0f cm; %d squared; %d further from the Qing figures\n"),
					Moved, Furthest, Squared, Worse);
			}
			if (Shifts.Num() > 0)
			{
				Shifts.Sort();
				Out += FString::Printf(TEXT("  %d buildings pushed with their neighbours: typically %.0f cm, furthest %.0f cm, %d past 32 cm\n"),
					Shifts.Num(), Shifts[Shifts.Num() / 2], Shifts.Last(), FarShifts);
				Out += FString::Printf(TEXT("  %d of them pushed less to stay clear of something; %d buildings and walls still collide\n"), HeldBack, Collides);
			}
			if (Stretches.Num() > 0)
			{
				Stretches.Sort();
				WallShifts.Sort();
				Out += FString::Printf(TEXT("  Walls: length changes typically %.0f cm, furthest %.0f cm; moved across typically %.0f cm, furthest %.0f cm\n"),
					Stretches[Stretches.Num() / 2], Stretches.Last(), WallShifts[WallShifts.Num() / 2], WallShifts.Last());
			}
			Out += TEXT(" ");
			for (const TPair<FString, int32>& Type : TypeCounts)
			{
				const TSet<FString>* Meshes = TypeMeshes.Find(Type.Key);
				Out += FString::Printf(TEXT(" %s %d/%d"), *Type.Key, Meshes ? Meshes->Num() : 0, Type.Value);
			}
			Out += TEXT("\n");
		}
		return Out;
	}

	FWallPieceCount CountWallPieces(const TArray<UHutongBuildingComponent*>& Buildings, const TArray<FRow>* Rows)
	{
		TMap<const UHutongBuildingComponent*, FVector2D> Planned;
		if (Rows)
		{
			for (const FRow& Row : *Rows) Planned.Add(Row.Building, Row.Outcomes[AppliedRule].Chosen.Size);
		}
		using namespace HutongGen::WallPieces;
		FWallPieceCount Count;
		TMap<FString, FString> MeshOfPiece;
		TMap<FString, int32> Triangles;
		for (const UHutongBuildingComponent* Building : Buildings)
		{
			const UHutongWallBuildingComponent* Wall = Cast<UHutongWallBuildingComponent>(Building);
			if (!Wall) continue;
			++Count.Walls;
			const FVector2D* Size = Planned.Find(Wall);
			if (Size && !Size->IsZero())
			{
				UHutongWallBuildingComponent* Stretched = DuplicateObject<UHutongWallBuildingComponent>(Wall, GetTransientPackage());
				Stretched->SetFootprintSize(*Size);
				Wall = Stretched;
			}
			const UHutongWallBuildingComponent::FPieceRun Run = Wall->GetPieceRun();
			const FHutongWallParams& Params = Run.Params;
			const double Thickness = Run.Thickness;
			for (const FPiece& Piece : Plan(Params, Run.Length, Thickness, Run.StartExtend, Run.EndExtend, Run.Skew))
			{
				++Count.Placed;
				Count.WholeWalls += Piece.Kind == EKind::Whole;
				const FString Id = PieceIdentity(Params, Piece, Thickness) + FString::Printf(TEXT("|%d%d"), (int32)Wall->DetailLevel, Wall->bBuildLODChain ? 1 : 0);
				FString* Mesh = MeshOfPiece.Find(Id);
				if (!Mesh)
				{
					TArray<UE::Geometry::FDynamicMesh3> LODs;
					const int32 CollisionLOD = BuildPieceLODs(Params, Piece, Thickness, Wall->DetailLevel, Wall->bBuildLODChain, LODs);
					const FString Key = HutongGen::LibraryKeyFor(LODs, CollisionLOD);
					Mesh = &MeshOfPiece.Add(Id, Key);
					Triangles.Add(Key, LODs.Num() > 0 ? LODs[0].TriangleCount() : 0);
				}
				Count.TrianglesDrawn += Triangles[*Mesh];
			}
		}
		Count.Distinct = Triangles.Num();
		for (const TPair<FString, int32>& Mesh : Triangles) Count.TrianglesStored += Mesh.Value;
		return Count;
	}

	FString DescribeWallPieces(const FWallPieceCount& Count)
	{
		return FString::Printf(TEXT("\nWalls as repeated pieces: %d walls, %d different pieces placed %d times, %lld triangles stored, %lld drawn; %d walls stay whole\n"),
			Count.Walls, Count.Distinct, Count.Placed, Count.TrianglesStored, Count.TrianglesDrawn, Count.WholeWalls);
	}

	bool WriteChecksCsv(const TArray<FRow>& Rows, const FString& Path)
	{
		const TArray<FRule> AllRules = Rules();
		FString Out = TEXT("id,type,role,rule,check,value_cm,min_cm,max_cm,off_cm\n");
		for (const FRow& Row : Rows)
		{
			for (int32 R = 0; R < AllRules.Num(); ++R)
			{
				const FOutcome& O = Row.Outcomes[R];
				for (const HutongAudit::FCheck& C : O.AuditChecks)
				{
					Out += FString::Printf(TEXT("%s,%s,%s,%s,%s,%.1f,%.1f,%.1f,%.1f\n"),
						*Row.Building->BuildingId.ToString(), *Row.Building->GetClass()->GetName(), *SharingCsvQuote(O.AuditRole), *SharingCsvQuote(AllRules[R].Name),
						*SharingCsvQuote(C.Name), C.Value, C.Band.Min, C.Band.Max, C.Off());
				}
			}
		}
		return FFileHelper::SaveStringToFile(Out, *Path, FFileHelper::EEncodingOptions::ForceUTF8);
	}

	FApplied Apply(const TArray<UHutongBuildingComponent*>& Buildings, EHutongTracedOutline Outline, bool bShowDialog)
	{
		FApplied Applied;
		const FRule Rule = Rules()[AppliedRule];
		const TMap<const UHutongBuildingComponent*, FChosen> Chosen = ChooseWithNeighbours(Buildings, Rule, Outline);
		const FScopedTransaction Transaction(LOCTEXT("ApplySharing", "Apply Shared Sizes"));
		FScopedSlowTask Task((float)Buildings.Num(), LOCTEXT("ApplyingSharing", "Applying shared sizes..."));
		if (bShowDialog) Task.MakeDialog();
		for (UHutongBuildingComponent* Building : Buildings)
		{
			Task.EnterProgressFrame();
			const FChosen* Given = Chosen.Find(Building);
			if (!Given || !Building->HasGeometry()) continue;
			AActor* Owner = Building->GetOwner();

			const FVector2D Old = Building->GetFootprintSize();
			const double OldHeight = Building->GetEditHeight();
			const FTransform Placed = Owner->GetActorTransform();
			Building->Modify();
			Owner->Modify();
			const double Move = ApplySharingRule(Building, Rule, Outline, Given);
			const FVector2D Inset = 0.5 * (Old - Building->GetFootprintSize());
			const FVector Location = Placed.TransformPosition(FVector(Inset, 0.0)) + FVector(Given->Shift, 0.0);
			const bool bMoved = !Location.Equals(Placed.GetLocation(), 0.01);
			if (bMoved) Owner->SetActorLocation(Location);
			Applied.Resized += !Building->GetFootprintSize().Equals(Old, 0.01);
			Applied.Moved += bMoved;
			Applied.HeldBack += Given->bHeldBack;
			Applied.Collides += Given->bCollides;
			const bool bReshaped = Move > 0.01 || !FMath::IsNearlyEqual(Building->GetEditHeight(), OldHeight, 0.01);
			if (bReshaped) Building->Rebuild();
			if (bReshaped || bMoved) Building->ApplyPlacementAttachments();
		}
		// No tool is active to invalidate the snap cache.
		HutongSnap::Invalidate();
		return Applied;
	}

	bool WriteCsv(const TArray<FRow>& Rows, const FString& Path)
	{
		const TArray<FRule> AllRules = Rules();
		FString Out = TEXT("building,id,type,rule,size_x_cm,size_y_cm,height_cm,squared,edge_move_cm,mesh,triangles,audit_off,shift_x_cm,shift_y_cm,group,planned_x_cm,planned_y_cm,stretch_cm,held_back,collides,collides_with,collide_depth_cm,drawn_depth_cm\n");
		for (const FRow& Row : Rows)
		{
			const AActor* Owner = Row.Building->GetOwner();
			for (int32 R = 0; R < AllRules.Num(); ++R)
			{
				const FOutcome& O = Row.Outcomes[R];
				Out += FString::Printf(TEXT("%s,%s,%s,%s,%.1f,%.1f,%.1f,%d,%.1f,%s,%d,%d,%.2f,%.2f,%d,%.2f,%.2f,%.2f,%d,%d,%s,%.1f,%.1f\n"),
					*SharingCsvQuote(Owner ? Owner->GetActorLabel() : FString()), *Row.Building->BuildingId.ToString(),
					*Row.Building->GetClass()->GetName(), *SharingCsvQuote(AllRules[R].Name), O.Size.X, O.Size.Y, O.Height,
					O.bSquared ? 1 : 0, O.EdgeMove, *O.Mesh, O.Triangles, O.AuditOff, O.Chosen.Shift.X, O.Chosen.Shift.Y, O.Chosen.GroupSize,
					O.Chosen.Size.X, O.Chosen.Size.Y, O.Chosen.Stretch, O.Chosen.bHeldBack ? 1 : 0, O.Chosen.bCollides ? 1 : 0,
					*SharingCsvQuote(O.Chosen.CollidesWith), O.Chosen.CollideDepth, O.Chosen.DrawnDepth);
			}
		}
		return FFileHelper::SaveStringToFile(Out, *Path, FFileHelper::EEncodingOptions::ForceUTF8);
	}
}

#undef LOCTEXT_NAMESPACE
