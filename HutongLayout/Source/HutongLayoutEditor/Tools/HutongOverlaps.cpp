#include "Tools/HutongOverlaps.h"

#include "Tools/HutongDetailOps.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongMetadata.h"

#include "Editor.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GameFramework/Actor.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "HutongOverlaps"

namespace HutongOverlaps
{

namespace
{
	double SignedArea(const TArray<FVector2D>& P)
	{
		double A = 0.0;
		for (int32 i = 0, j = P.Num() - 1; i < P.Num(); j = i++) A += P[j].X * P[i].Y - P[i].X * P[j].Y;
		return 0.5 * A;
	}

	TArray<FVector2D> CounterClockwise(TArray<FVector2D> P)
	{
		if (SignedArea(P) < 0.0) Algo::Reverse(P);
		return P;
	}

	// Sutherland–Hodgman: Subject clipped to the inside of convex, counter-clockwise Clip.
	TArray<FVector2D> Clip(const TArray<FVector2D>& Subject, const TArray<FVector2D>& ClipPoly)
	{
		TArray<FVector2D> Out = Subject;
		for (int32 e = 0; e < ClipPoly.Num() && Out.Num() > 0; ++e)
		{
			const FVector2D E0 = ClipPoly[e], E1 = ClipPoly[(e + 1) % ClipPoly.Num()];
			auto Inside = [&](const FVector2D& P) { return FVector2D::CrossProduct(E1 - E0, P - E0) >= 0.0; };
			const TArray<FVector2D> In = MoveTemp(Out);
			Out.Reset();
			for (int32 i = 0; i < In.Num(); ++i)
			{
				const FVector2D P = In[i], Q = In[(i + 1) % In.Num()];
				const bool bP = Inside(P), bQ = Inside(Q);
				if (bP) Out.Add(P);
				if (bP != bQ)
				{
					const double DP = FVector2D::CrossProduct(E1 - E0, P - E0);
					const double DQ = FVector2D::CrossProduct(E1 - E0, Q - E0);
					Out.Add(P + (Q - P) * (DP / (DP - DQ)));
				}
			}
		}
		return Out;
	}

	FBox2D BoxOf(const TArray<FVector2D>& P)
	{
		FBox2D Box(ForceInit);
		for (const FVector2D& V : P) Box += V;
		return Box;
	}

	FText Describe(const UHutongBuildingComponent* B)
	{
		const AActor* Actor = B ? B->GetOwner() : nullptr;
		if (!Actor) return LOCTEXT("Gone", "(gone)");
		return FText::Format(LOCTEXT("Describe", "{0} \"{1}\", confidence {2}"),
			B->GetTypeLabel(), FText::FromString(Actor->GetActorLabel()),
			StaticEnum<EHutongConfidence>()->GetDisplayNameTextByValue((int64)B->Confidence));
	}

	enum class EKeep : uint8 { First, Second, Both };

	struct FDecision
	{
		FPair Pair;
		EKeep Keep = EKeep::First;
		bool bTransfer = true;
	};

	TWeakPtr<SWindow> OpenWindowPtr;

	void SelectPair(const FPair& Pair)
	{
		if (!GEditor) return;
		GEditor->SelectNone(/*bNoteSelectionChange*/ false, /*bDeselectBSPSurfs*/ true);
		TArray<AActor*> Actors;
		for (const TWeakObjectPtr<UHutongBuildingComponent>& B : { Pair.First, Pair.Second })
		{
			if (AActor* Actor = B.IsValid() ? B->GetOwner() : nullptr)
			{
				GEditor->SelectActor(Actor, true, false);
				Actors.Add(Actor);
			}
		}
		GEditor->NoteSelectionChange();
		GEditor->MoveViewportCamerasToActor(Actors, /*bActiveViewportOnly*/ false);
	}

	int32 Resolve(const TArray<TSharedRef<FDecision>>& Decisions)
	{
		const FScopedTransaction Transaction(LOCTEXT("MergeOverlaps", "Merge Overlapping Hutong Buildings"));
		int32 Removed = 0;
		for (const TSharedRef<FDecision>& D : Decisions)
		{
			if (D->Keep == EKeep::Both) continue;
			UHutongBuildingComponent* Keep = (D->Keep == EKeep::First ? D->Pair.First : D->Pair.Second).Get();
			UHutongBuildingComponent* Drop = (D->Keep == EKeep::First ? D->Pair.Second : D->Pair.First).Get();
			AActor* DropActor = Drop ? Drop->GetOwner() : nullptr;
			if (!Keep || !DropActor || !IsValid(DropActor)) continue;
			if (D->bTransfer)
			{
				Keep->Modify();
				TransferMetadata(Drop, Keep);
			}
			if (UWorld* World = DropActor->GetWorld())
			{
				DropActor->Modify();
				if (World->EditorDestroyActor(DropActor, /*bShouldModifyLevel*/ true)) ++Removed;
			}
		}
		return Removed;
	}

	TSharedRef<SWidget> MakeRow(const TSharedRef<FDecision>& D)
	{
		auto KeepBox = [D](EKeep Which, const FText& Label)
		{
			return SNew(SCheckBox)
				.Style(FAppStyle::Get(), "RadioButton")
				.IsChecked_Lambda([D, Which] { return D->Keep == Which ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([D, Which](ECheckBoxState) { D->Keep = Which; })
				[
					SNew(STextBlock).Text(Label)
				];
		};
		return SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(8.0f, 6.0f))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Show", "Show"))
						.ToolTipText(LOCTEXT("ShowTip", "Selects both buildings and frames them in the viewport."))
						.OnClicked_Lambda([D] { SelectPair(D->Pair); return FReply::Handled(); })
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.AutoWrapText(true)
						.Text(FText::Format(LOCTEXT("PairText", "First: {0}\nSecond: {1}\n{2}% of the smaller footprint overlaps"),
							Describe(D->Pair.First.Get()), Describe(D->Pair.Second.Get()),
							FText::AsNumber(FMath::RoundToInt32(100.0 * D->Pair.Share))))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)[KeepBox(EKeep::First, LOCTEXT("KeepFirst", "Keep first"))]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 0.0f)[KeepBox(EKeep::Second, LOCTEXT("KeepSecond", "Keep second"))]
					+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 24.0f, 0.0f)[KeepBox(EKeep::Both, LOCTEXT("KeepBoth", "Keep both"))]
					+ SHorizontalBox::Slot().AutoWidth()
					[
						SNew(SCheckBox)
						.IsChecked_Lambda([D] { return D->bTransfer ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
						.OnCheckStateChanged_Lambda([D](ECheckBoxState S) { D->bTransfer = S == ECheckBoxState::Checked; })
						.IsEnabled_Lambda([D] { return D->Keep != EKeep::Both; })
						.ToolTipText(LOCTEXT("TransferTip", "The kept building takes the removed one's notes, the higher confidence, and its court and role where it has none."))
						[
							SNew(STextBlock).Text(LOCTEXT("Transfer", "Transfer metadata"))
						]
					]
				]
			];
	}
}

double OverlapShare(const TArray<FVector2D>& A, const TArray<FVector2D>& B)
{
	if (A.Num() < 3 || B.Num() < 3) return 0.0;
	const TArray<FVector2D> CA = CounterClockwise(A), CB = CounterClockwise(B);
	const double Smaller = FMath::Min(SignedArea(CA), SignedArea(CB));
	if (Smaller <= UE_KINDA_SMALL_NUMBER) return 0.0;
	return FMath::Clamp(FMath::Abs(SignedArea(Clip(CA, CB))) / Smaller, 0.0, 1.0);
}

TArray<FVector2D> WorldFootprint(const UHutongBuildingComponent* Building)
{
	TArray<FVector2D> Out;
	const AActor* Actor = Building ? Building->GetOwner() : nullptr;
	if (!Actor) return Out;
	FVector2D Quad[4];
	Building->GetFootprintCorners(Quad);
	const FTransform Xform = Actor->GetActorTransform();
	for (const FVector2D& C : Quad) Out.Add(FVector2D(Xform.TransformPosition(FVector(C.X, C.Y, 0.0))));
	return Out;
}

TArray<FPair> Find(const TArray<UHutongBuildingComponent*>& Candidates, const TArray<UHutongBuildingComponent*>& Against)
{
	struct FShape { UHutongBuildingComponent* B; TArray<FVector2D> P; FBox2D Box; };
	auto Shapes = [](const TArray<UHutongBuildingComponent*>& In)
	{
		TArray<FShape> Out;
		for (UHutongBuildingComponent* B : In)
		{
			if (!B) continue;
			TArray<FVector2D> P = WorldFootprint(B);
			if (P.Num() >= 3) Out.Add({ B, P, BoxOf(P) });
		}
		return Out;
	};
	const TArray<FShape> New = Shapes(Candidates), Old = Shapes(Against);
	const TSet<UHutongBuildingComponent*> NewSet(Candidates);

	TArray<FPair> Out;
	TSet<TPair<UHutongBuildingComponent*, UHutongBuildingComponent*>> Seen;
	for (const FShape& N : New)
	{
		for (const FShape& O : Old)
		{
			if (N.B == O.B || !N.Box.Intersect(O.Box)) continue;
			// Candidate against candidate is met twice; keep one ordering.
			const bool bBothNew = NewSet.Contains(O.B);
			const TPair<UHutongBuildingComponent*, UHutongBuildingComponent*> Key =
				bBothNew && O.B < N.B ? MakeTuple(O.B, N.B) : MakeTuple(N.B, O.B);
			if (Seen.Contains(Key)) continue;
			Seen.Add(Key);
			const double Share = OverlapShare(N.P, O.P);
			if (Share <= OfferShare) continue;
			// The one already there comes first.
			FPair Pair;
			Pair.First = bBothNew ? Key.Key : O.B;
			Pair.Second = bBothNew ? Key.Value : N.B;
			Pair.Share = Share;
			Out.Add(Pair);
		}
	}
	Out.Sort([](const FPair& L, const FPair& R) { return L.Share > R.Share; });
	return Out;
}

void TransferMetadata(const UHutongBuildingComponent* From, UHutongBuildingComponent* To)
{
	if (!From || !To) return;
	if (!From->Notes.IsEmpty() && !To->Notes.Contains(From->Notes))
	{
		const AActor* Owner = From->GetOwner();
		const FString Header = FString::Printf(TEXT("Merged from %s:"), Owner ? *Owner->GetActorLabel() : TEXT("a duplicate"));
		To->Notes = To->Notes.IsEmpty() ? Header + TEXT("\n") + From->Notes : To->Notes + TEXT("\n\n") + Header + TEXT("\n") + From->Notes;
	}
	if ((int32)From->Confidence > (int32)To->Confidence) To->Confidence = From->Confidence;
	if (To->Court.IsEmpty()) To->Court = From->Court;
	if (To->CourtRole == EHutongCourtRole::Auto) To->CourtRole = From->CourtRole;
}

void OpenWindow(const TArray<FPair>& Pairs)
{
	if (!FSlateApplication::IsInitialized() || IsRunningCommandlet() || Pairs.Num() == 0) return;
	if (const TSharedPtr<SWindow> Old = OpenWindowPtr.Pin()) Old->RequestDestroyWindow();

	TArray<TSharedRef<FDecision>> Decisions;
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
	for (const FPair& Pair : Pairs)
	{
		TSharedRef<FDecision> D = MakeShared<FDecision>();
		D->Pair = Pair;
		// Default: keep the better attested; on a tie the one already there.
		const UHutongBuildingComponent* A = Pair.First.Get();
		const UHutongBuildingComponent* B = Pair.Second.Get();
		D->Keep = (A && B && (int32)B->Confidence > (int32)A->Confidence) ? EKeep::Second : EKeep::First;
		Decisions.Add(D);
		Rows->AddSlot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)[MakeRow(D)];
	}

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("Title", "Overlapping Buildings"))
		.ClientSize(FVector2D(720.0, 520.0))
		.SupportsMaximize(false)
		.SupportsMinimize(false);
	const TWeakPtr<SWindow> WeakWindow = Window;
	Window->SetContent(
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
		.Padding(8.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.AutoWrapText(true)
				.Text(FText::Format(LOCTEXT("Intro",
					"{0} pair(s) of buildings overlap by over {1}% of the smaller footprint. Choose which to keep; Apply removes the other."),
					FText::AsNumber(Pairs.Num()), FText::AsNumber(FMath::RoundToInt32(100.0 * OfferShare))))
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SScrollBox) + SScrollBox::Slot()[Rows]
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Apply", "Apply"))
					.ToolTipText(LOCTEXT("ApplyTip", "Removes the building not kept in each pair."))
					.OnClicked_Lambda([Decisions, WeakWindow]
					{
						const int32 Removed = Resolve(Decisions);
						FNotificationInfo Info(FText::Format(LOCTEXT("Done", "Removed {0} overlapping building(s)."), FText::AsNumber(Removed)));
						Info.ExpireDuration = 4.0f;
						FSlateNotificationManager::Get().AddNotification(Info);
						if (const TSharedPtr<SWindow> W = WeakWindow.Pin()) W->RequestDestroyWindow();
						return FReply::Handled();
					})
				]
				+ SHorizontalBox::Slot().AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("Close", "Close"))
					.ToolTipText(LOCTEXT("CloseTip", "Closes the window and keeps every building."))
					.OnClicked_Lambda([WeakWindow]
					{
						if (const TSharedPtr<SWindow> W = WeakWindow.Pin()) W->RequestDestroyWindow();
						return FReply::Handled();
					})
				]
			]
		]);
	OpenWindowPtr = Window;
	const TSharedPtr<SWindow> Root = FGlobalTabmanager::Get()->GetRootWindow();
	if (Root.IsValid()) FSlateApplication::Get().AddWindowAsNativeChild(Window, Root.ToSharedRef());
	else FSlateApplication::Get().AddWindow(Window);
}

void CheckAfterImport(const TArray<TWeakObjectPtr<AActor>>& Placed)
{
	TArray<UHutongBuildingComponent*> New;
	UWorld* World = nullptr;
	for (const TWeakObjectPtr<AActor>& Actor : Placed)
	{
		if (UHutongBuildingComponent* B = Actor.IsValid() ? Actor->FindComponentByClass<UHutongBuildingComponent>() : nullptr)
		{
			New.Add(B);
			World = Actor->GetWorld();
		}
	}
	if (New.Num() == 0) return;
	OpenWindow(Find(New, HutongDetailOps::CollectLoaded(World)));
}

}

#undef LOCTEXT_NAMESPACE
