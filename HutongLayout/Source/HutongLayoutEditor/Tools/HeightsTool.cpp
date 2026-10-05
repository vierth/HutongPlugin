#include "Tools/HeightsTool.h"
#include "Tools/HeightsWindow.h"
#include "Tools/HutongDetailOps.h"
#include "Tools/HutongCourts.h"
#include "Tools/HutongPresetDefaults.h"
#include "Generation/HutongCanon.h"
#include "Generation/HutongFootprint.h"
#include "Generation/HutongBuildingComponent.h"
#include "Editor.h"
#include "InteractiveToolManager.h"
#include "Engine/Selection.h"
#include "ScopedTransaction.h"
#include "Misc/ScopedSlowTask.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/SWindow.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "HutongHeightsTool"

UInteractiveTool* UHutongHeightsToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongHeightsTool>(SceneState.ToolManager);
}

void UHutongHeightsToolProperties::OpenWindow() { if (Tool.IsValid()) Tool->OpenWindow(); }

namespace
{
	void NotifyHeights(const FText& Text)
	{
		FNotificationInfo Info(Text);
		Info.ExpireDuration = 4.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	TArray<UHutongBuildingComponent*> SelectedEaved()
	{
		// Paths, beds and jars have no height to set.
		return HutongDetailOps::CollectSelected().FilterByPredicate(
			[](const UHutongBuildingComponent* B) { return B->GetEditHeight() > 0.0; });
	}
}

void UHutongHeightsTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongHeightsToolProperties>(this);
	RegisterSettings(Settings, false);
	Settings->Tool = this;
}

void UHutongHeightsTool::Setup()
{
	Super::Setup();
	// Placement settings mean nothing to a tool that places nothing.
	for (UInteractiveToolPropertySet* Set : { (UInteractiveToolPropertySet*)MetadataSettings, (UInteractiveToolPropertySet*)Placement,
			(UInteractiveToolPropertySet*)DetailSettings, (UInteractiveToolPropertySet*)Snap, (UInteractiveToolPropertySet*)Appearance })
	{
		if (Set) SetToolPropertySourceEnabled(Set, false);
	}
	RefreshRows(true);
	OpenWindow();
}

void UHutongHeightsTool::Shutdown(EToolShutdownType ShutdownType)
{
	if (Window.IsValid())
	{
		const TSharedPtr<SWindow> Closing = Window;
		Window.Reset();
		Closing->RequestDestroyWindow();
	}
	Super::Shutdown(ShutdownType);
}

void UHutongHeightsTool::OpenWindow()
{
	if (!FSlateApplication::IsInitialized() || IsRunningCommandlet()) return;
	if (Window.IsValid())
	{
		Window->BringToFront();
		return;
	}
	Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "Eave Heights (檐高)"))
		.ClientSize(FVector2D(980.0, 460.0))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		// The viewport keeps focus: the window opens beside the selection, not over the clicking.
		.FocusWhenFirstShown(false)
		[
			HutongHeightsWindow::MakeContent(this)
		];
	Window->SetOnWindowClosed(FOnWindowClosed::CreateWeakLambda(this, [this](const TSharedRef<SWindow>&) { Window.Reset(); }));
	const TSharedPtr<SWindow> Root = FGlobalTabmanager::Get()->GetRootWindow();
	if (Root.IsValid()) FSlateApplication::Get().AddWindowAsNativeChild(Window.ToSharedRef(), Root.ToSharedRef());
	else FSlateApplication::Get().AddWindow(Window.ToSharedRef());
}

void UHutongHeightsTool::OnTick(float DeltaTime)
{
	Super::OnTick(DeltaTime);
	RefreshRows(false);
	if (bPanelDirty)
	{
		bPanelDirty = false;
		if (Settings) NotifyOfPropertyChangeByTool(Settings);
	}
}

void UHutongHeightsTool::SyncPanel()
{
	// Next tick: never rebuild the details view inside its own edit.
	bPanelDirty = true;
}

void UHutongHeightsTool::RefreshRows(bool bForce)
{
	TArray<UHutongBuildingComponent*> Selected = SelectedEaved();
	if (!bForce && Selected.Num() == RowBuildings.Num()
		&& !Selected.ContainsByPredicate([&](const UHutongBuildingComponent* B) { return !RowBuildings.Contains(B); }))
	{
		return;
	}

	// Highest rank first, then tallest: the 正房 heads the list and is the reference.
	Selected.Sort([](const UHutongBuildingComponent& A, const UHutongBuildingComponent& B)
	{
		const double RA = HutongPresets::EaveRatio(A.GetCourtRole()), RB = HutongPresets::EaveRatio(B.GetCourtRole());
		return RA != RB ? RA > RB : A.GetEditHeight() > B.GetEditHeight();
	});

	Rows.Reset();
	RowBuildings.Reset();
	for (UHutongBuildingComponent* B : Selected)
	{
		FHutongEaveRow Row;
		const AActor* Owner = B->GetOwner();
		Row.Label = Owner ? Owner->GetActorLabel() : TEXT("?");
		Row.Detail = B->GetTypeLabel().ToString();
		if (Cast<UHutongWallBuildingComponent>(B)) Row.Detail += TEXT(" · height is the wall top");
		if (!B->Court.IsEmpty()) Row.Detail += (B->GetCourts().Num() > 1 ? TEXT(" · courts ") : TEXT(" · court ")) + FString::Join(B->GetCourts(), TEXT(", "));
		Row.Role = B->GetCourtRole();
		Row.CurrentEave = FMath::RoundToDouble(B->GetEditHeight());
		Row.NewEave = Row.CurrentEave;
		Row.bEditable = B->CanSetEditHeight();
		Row.Preset = Row.NewPreset = B->Preset;
		Row.RidgeAboveEave = B->GetRidgeHeight() > 0.0 ? B->GetRidgeHeight() - B->GetEaveHeight() : 0.0;
		Rows.Add(Row);
		RowBuildings.Add(B);
	}
	RefreshSuggestions();
	++RowsRevision;

	// A court the whole selection shares names it; otherwise a fresh suggestion, unless typed.
	// The court every selected building is in (a shared wall is in more than one), if there is one.
	TArray<FString> Shared = Selected.Num() > 0 ? Selected[0]->GetCourts() : TArray<FString>();
	for (const UHutongBuildingComponent* B : Selected)
	{
		Shared.RemoveAll([B](const FString& Name) { return !B->IsInCourt(Name); });
	}
	if (Settings && Shared.Num() == 1)
	{
		Settings->CourtName = Shared[0];
		bCourtNameSuggested = true;
	}
	else if (Settings && Selected.Num() > 0 && (Settings->CourtName.IsEmpty() || bCourtNameSuggested))
	{
		SuggestCourtName();
	}
	SyncPanel();
}

int32 UHutongHeightsTool::ReferenceRow(const TArray<FHutongEaveRow>& InRows)
{
	int32 Best = INDEX_NONE;
	for (int32 i = 0; i < InRows.Num(); ++i)
	{
		const double R = HutongPresets::EaveRatio(InRows[i].Role);
		if (R <= 0.0) continue;
		if (Best == INDEX_NONE) { Best = i; continue; }
		const double RB = HutongPresets::EaveRatio(InRows[Best].Role);
		if (R > RB || (R == RB && InRows[i].NewEave > InRows[Best].NewEave)) Best = i;
	}
	return Best;
}

TArray<double> UHutongHeightsTool::Suggestions(const TArray<FHutongEaveRow>& InRows)
{
	TArray<double> Out;
	Out.Init(-1.0, InRows.Num());
	const int32 Ref = ReferenceRow(InRows);
	if (Ref == INDEX_NONE) return Out;
	for (int32 i = 0; i < InRows.Num(); ++i)
	{
		Out[i] = i == Ref ? InRows[Ref].NewEave : HutongPresets::SuggestEave(InRows[i].Role, InRows[Ref].Role, InRows[Ref].NewEave);
	}
	return Out;
}

void UHutongHeightsTool::SetNewEave(int32 Index, double Cm)
{
	if (!Rows.IsValidIndex(Index) || !Rows[Index].bEditable) return;
	const double Old = Rows[Index].NewEave;
	Rows[Index].NewEave = FMath::Max(Cm, 60.0);
	// Keep Proportions: the edited row sets the factor, the others follow it.
	if (GetKeepProportions() && Old > 0.0)
	{
		const double Factor = Rows[Index].NewEave / Old;
		for (int32 k = 0; k < Rows.Num(); ++k)
		{
			if (k != Index && Rows[k].bEditable) Rows[k].NewEave *= Factor;
		}
	}
	RefreshSuggestions();
}

void UHutongHeightsTool::RefreshSuggestions()
{
	Suggested = Suggestions(Rows);
	SuggestionNotes.Init(FString(), Rows.Num());
	const int32 Ref = ReferenceRow(Rows);
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		if (i == Ref) SuggestionNotes[i] = TEXT("reference: the others are ranked on this one");
		else if (Suggested[i] > 0.0)
		{
			SuggestionNotes[i] = FString::Printf(TEXT("%.2f × %s"), Suggested[i] / Rows[Ref].NewEave,
				*StaticEnum<EHutongCourtRole>()->GetDisplayNameTextByValue((int64)Rows[Ref].Role).ToString());
		}
		else SuggestionNotes[i] = TEXT("no ratio for this role");
	}

	// A gate's ridge stands the canon step over the building it is set into: the nearest selected
	// roofed building that is not a gate or a wall, at that building's New Eave.
	for (int32 g = 0; g < Rows.Num(); ++g)
	{
		const UHutongBuildingComponent* Gate = RowBuildings.IsValidIndex(g) ? RowBuildings[g].Get() : nullptr;
		if (!Gate || !Gate->IsA<UHutongGateHouseBuildingComponent>() || !Gate->GetOwner()) continue;
		FVector2D GQ[4];
		Gate->GetFootprintCorners(GQ);
		const FVector2D GL = 0.25 * (GQ[0] + GQ[1] + GQ[2] + GQ[3]);
		const FVector GateCentre = Gate->GetOwner()->GetActorTransform().TransformPosition(FVector(GL.X, GL.Y, 0.0));
		TArray<TPair<double, int32>> Near;
		for (int32 k = 0; k < Rows.Num(); ++k)
		{
			const UHutongBuildingComponent* B = RowBuildings.IsValidIndex(k) ? RowBuildings[k].Get() : nullptr;
			if (k == g || !B || !B->GetOwner() || Rows[k].RidgeAboveEave <= 0.0 || B->IsA<UHutongGateHouseBuildingComponent>()) continue;
			FVector2D Q[4];
			B->GetFootprintCorners(Q);
			const FVector L = B->GetOwner()->GetActorTransform().InverseTransformPosition(GateCentre);
			const FVector2D P(L.X, L.Y);
			Near.Add({ HutongFootprint::PointInQuad(Q, P) ? 0.0 : HutongFootprint::DistanceToQuadEdge(Q, P), k });
		}
		if (Near.Num() == 0)
		{
			Suggested[g] = -1.0;
			SuggestionNotes[g] = TEXT("select the row it stands in to suggest one");
			continue;
		}
		Near.Sort([](const TPair<double, int32>& A, const TPair<double, int32>& B) { return A.Key < B.Key; });
		// Both neighbours of a gate between two stretches of row; the taller decides.
		int32 Row = Near[0].Value;
		for (const TPair<double, int32>& N : Near)
		{
			if (N.Key > Near[0].Key + 100.0) break;
			if (Rows[N.Value].NewEave + Rows[N.Value].RidgeAboveEave > Rows[Row].NewEave + Rows[Row].RidgeAboveEave) Row = N.Value;
		}
		const double RowRidge = Rows[Row].NewEave + Rows[Row].RidgeAboveEave;
		Suggested[g] = Gate->EaveForRidge(RowRidge + HutongCanon::Gate::RidgeAboveRowCm, Rows[g].NewPreset);
		SuggestionNotes[g] = FString::Printf(TEXT("ridge %.0f cm over %s's"), HutongCanon::Gate::RidgeAboveRowCm, *Rows[Row].Label);
	}
}

TArray<FString> UHutongHeightsTool::GetPresetOptions(int32 Index) const
{
	const UHutongBuildingComponent* B = RowBuildings.IsValidIndex(Index) ? RowBuildings[Index].Get() : nullptr;
	return B ? B->GetPresetOptions() : TArray<FString>();
}

void UHutongHeightsTool::SetPreset(int32 Index, const FString& Preset)
{
	if (!Rows.IsValidIndex(Index)) return;
	Rows[Index].NewPreset = Preset;
	RefreshSuggestions();
}

void UHutongHeightsTool::SetRole(int32 Index, EHutongCourtRole Role)
{
	if (!Rows.IsValidIndex(Index)) return;
	Rows[Index].Role = Role;
	RefreshSuggestions();
}

void UHutongHeightsTool::UseSuggestion(int32 Index)
{
	if (Suggested.IsValidIndex(Index) && Suggested[Index] > 0.0 && Rows[Index].bEditable)
	{
		Rows[Index].NewEave = FMath::RoundToDouble(Suggested[Index]);
		RefreshSuggestions();
	}
}

void UHutongHeightsTool::UseSuggested()
{
	for (int32 i = 0; i < Rows.Num(); ++i)
	{
		if (Suggested.IsValidIndex(i) && Suggested[i] > 0.0 && Rows[i].bEditable) Rows[i].NewEave = FMath::RoundToDouble(Suggested[i]);
	}
	RefreshSuggestions();
}

void UHutongHeightsTool::Reset()
{
	RefreshRows(true);
}

void UHutongHeightsTool::SetCourtName(const FString& Name)
{
	if (!Settings) return;
	Settings->CourtName = Name;
	bCourtNameSuggested = false;
	SyncPanel();
}

void UHutongHeightsTool::OnPropertyModified(UObject* PropertySet, FProperty* Property)
{
	if (PropertySet == Settings && Property && Property->GetFName() == GET_MEMBER_NAME_CHECKED(UHutongHeightsToolProperties, CourtName))
	{
		bCourtNameSuggested = false;
	}
}

void UHutongHeightsTool::Apply()
{
	TArray<int32> Work;
	for (int32 i = 0; i < Rows.Num() && i < RowBuildings.Num(); ++i)
	{
		const UHutongBuildingComponent* B = RowBuildings[i].Get();
		if (B && (FMath::Abs(Rows[i].NewEave - Rows[i].CurrentEave) > 0.5 || Rows[i].Role != B->GetCourtRole()
			|| Rows[i].NewPreset != Rows[i].Preset)) Work.Add(i);
	}
	if (Work.Num() == 0)
	{
		NotifyHeights(LOCTEXT("NothingToApply", "No height, preset or role differs from what is built."));
		return;
	}

	FScopedSlowTask Task((float)Work.Num(), LOCTEXT("Applying", "Setting eaves…"));
	Task.MakeDialogDelayed(0.4f);
	int32 Set = 0;
	TArray<FString> Held;
	{
		const FScopedTransaction Transaction(LOCTEXT("ApplyEaves", "Set Building Eaves"));
		for (const int32 i : Work)
		{
			Task.EnterProgressFrame(1.0f);
			UHutongBuildingComponent* B = RowBuildings[i].Get();
			const FHutongEaveRow& Row = Rows[i];
			if (B->GetOwner()) B->GetOwner()->Modify();
			B->Modify();
			if (Row.Role != B->GetCourtRole()) B->CourtRole = Row.Role;
			bool bChanged = false;
			// Preset first: it replaces the parameters, the height included.
			if (Row.NewPreset != Row.Preset)
			{
				B->Preset = Row.NewPreset;
				B->ApplyPresetParams(Row.NewPreset);
				bChanged = true;
			}
			// The New column is what the building ends at, whatever the preset brought.
			if (FMath::Abs(Row.NewEave - B->GetEditHeight()) > 0.5 && B->SetEditHeight(FMath::RoundToDouble(Row.NewEave))) bChanged = true;
			if (!bChanged) continue;
			B->Rebuild();
			B->ApplyPlacementAttachments();
			++Set;
			// The doorway floor lifts an eave asked below walking clearance.
			if (B->GetEditHeight() > Row.NewEave + 0.5)
			{
				Held.Add(FString::Printf(TEXT("%s at %.0f cm"), *Row.Label, B->GetEditHeight()));
			}
		}
	}

	FString Message = FString::Printf(TEXT("Rebuilt %d building%s."), Set, Set == 1 ? TEXT("") : TEXT("s"));
	if (Held.Num() > 0) Message += TEXT(" Held up to keep the doorway passable: ") + FString::Join(Held, TEXT("; ")) + TEXT(".");
	NotifyHeights(FText::FromString(Message));
	RefreshRows(true);
}

void UHutongHeightsTool::SuggestCourtName()
{
	if (!Settings) return;
	Settings->CourtName = HutongCourts::SuggestName(GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld(), SelectedEaved());
	bCourtNameSuggested = true;
	SyncPanel();
}

void UHutongHeightsTool::AssignCourt()
{
	if (!Settings) return;
	const TArray<UHutongBuildingComponent*> Selected = HutongDetailOps::CollectSelected();
	const FString Name = Settings->CourtName.TrimStartAndEnd();
	const int32 Count = HutongCourts::Assign(Selected, Name);
	if (Count == 0) return;
	NotifyHeights(Name.IsEmpty()
		? FText::Format(LOCTEXT("CourtCleared", "Cleared the court of {0} building(s)."), Count)
		: FText::Format(LOCTEXT("CourtAssigned", "{0} building(s) now in court {1}."), Count, FText::FromString(Name)));
	RefreshRows(true);
}

void UHutongHeightsTool::SelectCourt()
{
	const TSet<FString> Courts = HutongCourts::CourtsOf(HutongDetailOps::CollectSelected());
	if (Courts.Num() == 0)
	{
		NotifyHeights(LOCTEXT("NoCourt", "No selected building belongs to a court: name one with Assign."));
		return;
	}
	HutongCourts::SelectCourts(GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld(), Courts, /*bAdd*/ true);
}

TArray<FString> UHutongHeightsTool::GetAllCourts() const
{
	return HutongCourts::AllCourts(GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld());
}

void UHutongHeightsTool::OpenCourt(const FString& Court)
{
	HutongCourts::SelectCourts(GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld(), { Court }, /*bAdd*/ false);
}

TArray<FText> UHutongHeightsTool::GetToolHelpLines() const
{
	using namespace HutongPresets;
	FNumberFormattingOptions Two;
	Two.SetMaximumFractionalDigits(2);
	TArray<FText> Lines;
	Lines.Add(LOCTEXT("HelpSelect",
		"Select buildings, type a New Eave in the Eave Heights window, then Apply."));
	Lines.Add(FText::Format(LOCTEXT("HelpRank",
		"Ranking: Main Hall (正房) highest; Side House (廂房) {0} of its eave; Front Row (倒座房) {1}; Rear Row (後罩房) {2}; Ear Room (耳房) {3}."),
		FText::AsNumber(EaveRatio(EHutongCourtRole::SideHouse), &Two), FText::AsNumber(EaveRatio(EHutongCourtRole::FrontRow), &Two),
		FText::AsNumber(EaveRatio(EHutongCourtRole::RearRow), &Two), FText::AsNumber(EaveRatio(EHutongCourtRole::EarRoom), &Two)));
	Lines.Add(LOCTEXT("HelpKeep",
		"Keep Proportions: changing one row scales every other row."));
	Lines.Add(LOCTEXT("HelpCourt",
		"Courtyard units (院落): Assign stores the name on the selection; Select Whole Court selects the unit again."));
	Lines.Add(FText::Format(LOCTEXT("HelpLimits",
		"A gate house (大門) is suggested {0} cm above its neighbour's ridge; pick its rank in the Preset column."),
		FText::AsNumber(HutongCanon::Gate::RidgeAboveRowCm)));
	Lines.Add(LOCTEXT("HelpEsc", "Esc puts the tool down and closes the window."));
	return Lines;
}

FText UHutongHeightsTool::GetStagePromptText() const
{
	return Rows.Num() == 0
		? LOCTEXT("PromptNone", "Select the buildings whose eaves to set: click, Shift+click to add, or drag a box.")
		: FText::Format(LOCTEXT("PromptSome", "{0} building(s) selected: set their eaves in the Eave Heights window, then Apply."), Rows.Num());
}

FText UHutongHeightsTool::GetKeyHintText() const
{
	return LOCTEXT("KeyHint", "Click select · Shift+click add · drag box · Esc put the tool down");
}

int32 UHutongHeightsTool::GetStageIndex() const
{
	return Rows.Num() > 0 ? 1 : 0;
}

TArray<FText> UHutongHeightsTool::GetStageNames() const
{
	return { LOCTEXT("StageSelect", "Select"), LOCTEXT("StageApply", "Edit and Apply") };
}

#undef LOCTEXT_NAMESPACE
