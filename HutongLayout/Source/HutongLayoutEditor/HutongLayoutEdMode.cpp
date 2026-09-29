#include "HutongLayoutEdMode.h"
#include "Editor.h"
#include "TimerManager.h"
#include "UObject/ObjectSaveContext.h"
#include "InteractiveToolManager.h"
#include "EditorModeManager.h"
#include "HutongLayoutCommands.h"
#include "HutongLayoutEdModeToolkit.h"
#include "HutongLayoutModeSettings.h"
#include "Generation/HutongPlanOutlineComponent.h"
#include "Generation/HutongBuildingComponent.h"
#include "Selection.h"
#include "Tools/WallTool.h"
#include "Tools/SiheyuanTool.h"
#include "Tools/EarPassageTool.h"
#include "Tools/GateHouseTool.h"
#include "Tools/CorridorTool.h"
#include "Tools/InnerGateTool.h"
#include "Tools/PaifangTool.h"
#include "Tools/ScreenWallTool.h"
#include "Tools/CompoundTool.h"
#include "Tools/StreetRowTool.h"
#include "Tools/PathTool.h"
#include "Tools/FlowerBedTool.h"
#include "Tools/WaterJarTool.h"
#include "Tools/FrameTool.h"
#include "Tools/PavilionTool.h"
#include "Tools/ShopfrontTool.h"
#include "Tools/StoreyTool.h"
#include "Tools/HallTool.h"
#include "Tools/GalleryTool.h"
#include "Tools/MeasureTool.h"
#include "Tools/HeightsTool.h"
#include "Tools/CourtsTool.h"
#include "Tools/HutongImportTool.h"
#include "Tools/RectDragToolBase.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "InteractiveToolManager.h"
#include "InputCoreTypes.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"

class FHutongInputProcessor : public IInputProcessor
{
public:
	TWeakObjectPtr<UHutongLayoutEdMode> Owner;

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	URectDragToolBase* GetActiveRectTool() const
	{
		if (!Owner.IsValid()) return nullptr;
		UInteractiveToolManager* TM = Owner->GetToolManager();
		if (!TM) return nullptr;
		return Cast<URectDragToolBase>(TM->GetActiveTool(EToolSide::Left));
	}

	// Typing into a field (the heights window's eaves, a Details number): Esc, -, = and the letters
	// are the text's, not the tool's.
	static bool IsTyping(FSlateApplication& SlateApp)
	{
		const TSharedPtr<SWidget> Focused = SlateApp.GetKeyboardFocusedWidget();
		return Focused.IsValid() && Focused->GetType().ToString().Contains(TEXT("EditableText"));
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		URectDragToolBase* Tool = GetActiveRectTool();
		if (!Tool || IsTyping(SlateApp)) return false;

		const FKey Key = InKeyEvent.GetKey();
		if (Key == EKeys::Escape)
		{
			// Cancel what is in hand first (half-drawn rect or plan edit); with nothing in hand, put the
			// tool down so the preview stops following the cursor.
			if (Tool->IsPlacingActive() || Tool->IsEditingPlan())
			{
				Tool->CancelPlacement();
			}
			else if (Owner.IsValid())
			{
				Owner->PutToolDown();
			}
			return false;
		}
		if (Key == EKeys::Hyphen || Key == EKeys::Equals)
		{
			// Height edits the tool's persisted params, so unlike the bay keys it works before a placement starts.
			const FModifierKeysState& Mods = InKeyEvent.GetModifierKeys();
			const double Step = Mods.IsShiftDown() ? 5.0 : (Mods.IsControlDown() ? 100.0 : 20.0);
			Tool->AdjustHeight(Key == EKeys::Equals ? Step : -Step);
			return true;
		}
		if (Key == EKeys::LeftBracket || Key == EKeys::RightBracket)
		{
			// Repeats wanted: holding a bracket key walks the value.
			if (!Tool->IsPlacingActive())
			{
				// Nothing being placed: the keys turn the selected building's facade instead.
				if (InKeyEvent.IsRepeat()) return false;
				return Tool->TurnSelectedFacing(Key == EKeys::RightBracket ? 1 : -1);
			}
			const FModifierKeysState& Mods = InKeyEvent.GetModifierKeys();
			Tool->AdjustBracketValue(Key == EKeys::RightBracket ? 1 : -1,
				Mods.IsShiftDown(), Mods.IsControlDown());
			return true;
		}
		if (Key == EKeys::R)
		{
			// Hold R to rotate the in-progress placement around the first click point.
			if (!Tool->IsPlacingActive()) return false;
			if (!InKeyEvent.IsRepeat())
			{
				Tool->BeginRotateMode();
			}
			return true;
		}
		if (Key == EKeys::F)
		{
			if (InKeyEvent.IsRepeat()) return false;
			// Nothing being placed: flips the selected building's facade; with no such building the
			// viewport keeps F for focus.
			if (!Tool->IsPlacingActive()) return !Tool->IsEditingPlan() && Tool->FlipSelectedFacing();
			Tool->FlipFacing();
			return true;
		}
		if (Key == EKeys::G)
		{
			// Toggles the gate on the wall segment being drawn. Only mid-placement, so the viewport
			// keeps G for game view.
			if (!Tool->IsPlacingActive()) return false;
			if (!InKeyEvent.IsRepeat()) Tool->ToggleOpeningMark();
			return true;
		}
		return false;
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (IsTyping(SlateApp)) return false;
		if (InKeyEvent.GetKey() == EKeys::R)
		{
			if (URectDragToolBase* Tool = GetActiveRectTool())
			{
				Tool->EndRotateMode();
				return true;
			}
		}
		return false;
	}
};

#define LOCTEXT_NAMESPACE "HutongLayoutEdMode"

const FEditorModeID UHutongLayoutEdMode::EM_HutongLayoutModeId = TEXT("EM_HutongLayout");

UHutongLayoutEdMode::UHutongLayoutEdMode()
{
	Info = FEditorModeInfo(
		EM_HutongLayoutModeId,
		LOCTEXT("HutongLayoutModeName", "Hutong Layout"),
		FSlateIcon(),
		true,
		1000);

	// UEdMode creates the object, wraps it in LoadConfig/SaveConfig, and the toolkit slots its ModeDetailsView above the tool panel.
	SettingsClass = UHutongLayoutModeSettings::StaticClass();
}

void UHutongLayoutEdMode::Enter()
{
	Super::Enter();

	// Settings are recreated each Enter, so seed the plan-outline checkbox from the switch it mirrors.
	if (UHutongLayoutModeSettings* Settings = Cast<UHutongLayoutModeSettings>(SettingsObject))
	{
		Settings->bShowPlanOutlines = HutongPlanOutline::ArePlansVisible();
	}

	const FHutongLayoutCommands& Commands = FHutongLayoutCommands::Get();

	ToolCommandsById.Reset();
	auto RegisterTool = [this](TSharedPtr<FUICommandInfo> Command, const TCHAR* Id, UInteractiveToolBuilder* Builder)
	{
		ToolCommandsById.Add(Id, Command);
		UEdMode::RegisterTool(Command, Id, Builder);
	};

	RegisterTool(Commands.BeginWallTool, TEXT("HutongWallTool"),
		NewObject<UHutongLaneWallToolBuilder>(this));
	RegisterTool(Commands.BeginCourtWallTool, TEXT("HutongCourtWallTool"),
		NewObject<UHutongCourtWallToolBuilder>(this));
	RegisterTool(Commands.BeginSiheyuanTool, TEXT("HutongSiheyuanTool"),
		NewObject<UHutongSiheyuanToolBuilder>(this));
	RegisterTool(Commands.BeginEarPassageTool, TEXT("HutongEarPassageTool"),
		NewObject<UHutongEarPassageToolBuilder>(this));
	RegisterTool(Commands.BeginGateHouseTool, TEXT("HutongGateHouseTool"),
		NewObject<UHutongGateHouseToolBuilder>(this));
	RegisterTool(Commands.BeginPaifangTool, TEXT("HutongPaifangTool"),
		NewObject<UHutongPaifangToolBuilder>(this));
	RegisterTool(Commands.BeginInnerGateTool, TEXT("HutongInnerGateTool"),
		NewObject<UHutongInnerGateToolBuilder>(this));
	RegisterTool(Commands.BeginCorridorTool, TEXT("HutongCorridorTool"),
		NewObject<UHutongCorridorToolBuilder>(this));
	RegisterTool(Commands.BeginScreenWallTool, TEXT("HutongScreenWallTool"),
		NewObject<UHutongScreenWallToolBuilder>(this));
	RegisterTool(Commands.BeginShopfrontTool, TEXT("HutongShopfrontTool"),
		NewObject<UHutongShopfrontToolBuilder>(this));
	RegisterTool(Commands.BeginStoreyTool, TEXT("HutongStoreyTool"),
		NewObject<UHutongStoreyToolBuilder>(this));
	RegisterTool(Commands.BeginPavilionTool, TEXT("HutongPavilionTool"),
		NewObject<UHutongPavilionToolBuilder>(this));
	RegisterTool(Commands.BeginHallTool, TEXT("HutongHallTool"),
		NewObject<UHutongHallToolBuilder>(this));
	RegisterTool(Commands.BeginFrameTool, TEXT("HutongFrameTool"),
		NewObject<UHutongFrameToolBuilder>(this));
	RegisterTool(Commands.BeginPathTool, TEXT("HutongPathTool"),
		NewObject<UHutongPathToolBuilder>(this));
	RegisterTool(Commands.BeginFlowerBedTool, TEXT("HutongFlowerBedTool"),
		NewObject<UHutongFlowerBedToolBuilder>(this));
	RegisterTool(Commands.BeginWaterJarTool, TEXT("HutongWaterJarTool"),
		NewObject<UHutongWaterJarToolBuilder>(this));
	RegisterTool(Commands.BeginCompoundTool, TEXT("HutongCompoundTool"),
		NewObject<UHutongCompoundToolBuilder>(this));
	RegisterTool(Commands.BeginStreetRowTool, TEXT("HutongStreetRowTool"),
		NewObject<UHutongStreetRowToolBuilder>(this));
	RegisterTool(Commands.BeginGalleryTool, TEXT("HutongGalleryTool"),
		NewObject<UHutongGalleryToolBuilder>(this));
	// One gallery per category, the same tool with its builder's category.
	{
		UHutongGalleryToolBuilder* Builder = NewObject<UHutongGalleryToolBuilder>(this);
		Builder->Category = EHutongGalleryCategory::Walls;
		RegisterTool(Commands.BeginGalleryWallsTool, TEXT("HutongGalleryWallsTool"), Builder);
	}
	{
		UHutongGalleryToolBuilder* Builder = NewObject<UHutongGalleryToolBuilder>(this);
		Builder->Category = EHutongGalleryCategory::Houses;
		RegisterTool(Commands.BeginGalleryHousesTool, TEXT("HutongGalleryHousesTool"), Builder);
	}
	{
		UHutongGalleryToolBuilder* Builder = NewObject<UHutongGalleryToolBuilder>(this);
		Builder->Category = EHutongGalleryCategory::Gates;
		RegisterTool(Commands.BeginGalleryGatesTool, TEXT("HutongGalleryGatesTool"), Builder);
	}
	{
		UHutongGalleryToolBuilder* Builder = NewObject<UHutongGalleryToolBuilder>(this);
		Builder->Category = EHutongGalleryCategory::Courtyard;
		RegisterTool(Commands.BeginGalleryCourtyardTool, TEXT("HutongGalleryCourtyardTool"), Builder);
	}
	{
		UHutongGalleryToolBuilder* Builder = NewObject<UHutongGalleryToolBuilder>(this);
		Builder->Category = EHutongGalleryCategory::Street;
		RegisterTool(Commands.BeginGalleryStreetTool, TEXT("HutongGalleryStreetTool"), Builder);
	}
	{
		UHutongGalleryToolBuilder* Builder = NewObject<UHutongGalleryToolBuilder>(this);
		Builder->Category = EHutongGalleryCategory::Temples;
		RegisterTool(Commands.BeginGalleryTemplesTool, TEXT("HutongGalleryTemplesTool"), Builder);
	}
	RegisterTool(Commands.BeginMeasureTool, TEXT("HutongMeasureTool"),
		NewObject<UHutongMeasureToolBuilder>(this));
	RegisterTool(Commands.BeginHeightsTool, TEXT("HutongHeightsTool"),
		NewObject<UHutongHeightsToolBuilder>(this));
	RegisterTool(Commands.BeginCourtsTool, TEXT("HutongCourtsTool"),
		NewObject<UHutongCourtsToolBuilder>(this));
	RegisterTool(Commands.BeginImportTool, TEXT("HutongImportTool"),
		NewObject<UHutongImportToolBuilder>(this));

	// The default (UndoToExit) pushes an "Activate Tool" undo entry per activation, so Ctrl+Z
	// cancels the tool instead of undoing: with click-to-edit, a dead Ctrl+Z per click and a
	// Generate that could not be undone. Esc leaves a tool.
	GetToolManager()->ConfigureChangeTrackingMode(EToolChangeTrackingMode::NoChangeTracking);

	// The house is the usual first tool, so it starts active.
	GetToolManager()->SelectActiveToolType(EToolSide::Left, TEXT("HutongSiheyuanTool"));
	GetToolManager()->ActivateTool(EToolSide::Left);

	SelectionHandle = USelection::SelectionChangedEvent.AddUObject(this, &UHutongLayoutEdMode::OnEditorSelectionChanged);

	PreSaveHandle = FEditorDelegates::PreSaveWorldWithContext.AddWeakLambda(this,
		[this](UWorld*, FObjectPreSaveContext) { RememberToolBeforeSave(); });
	PostSaveHandle = FEditorDelegates::PostSaveWorldWithContext.AddWeakLambda(this,
		[this](UWorld*, FObjectPostSaveContext) { RestoreToolAfterSave(); });

	if (FSlateApplication::IsInitialized())
	{
		auto Processor = MakeShared<FHutongInputProcessor>();
		Processor->Owner = this;
		InputProcessor = Processor;
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
	}
}

void UHutongLayoutEdMode::Exit()
{
	USelection::SelectionChangedEvent.Remove(SelectionHandle);
	SelectionHandle.Reset();
	bFollowSelectionQueued = false;
	FEditorDelegates::PreSaveWorldWithContext.Remove(PreSaveHandle);
	FEditorDelegates::PostSaveWorldWithContext.Remove(PostSaveHandle);
	PreSaveHandle.Reset();
	PostSaveHandle.Reset();
	ToolBeforeSave.Reset();

	if (InputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
	InputProcessor.Reset();
	Super::Exit();
}

void UHutongLayoutEdMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FHutongLayoutEdModeToolkit);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> UHutongLayoutEdMode::GetModeCommands() const
{
	TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> Result;
	const FHutongLayoutCommands& Commands = FHutongLayoutCommands::Get();
	// Second list, required: a tool missing here loses its keyboard chord while its palette button still works.
	Result.Add(FName("Buildings"),
		{ Commands.BeginSiheyuanTool, Commands.BeginEarPassageTool, Commands.BeginShopfrontTool,
		  Commands.BeginStoreyTool, Commands.BeginHallTool, Commands.BeginPavilionTool,
		  Commands.BeginFrameTool, Commands.BeginStreetRowTool, Commands.BeginCompoundTool });
	Result.Add(FName("Gates"),
		{ Commands.BeginGateHouseTool, Commands.BeginInnerGateTool,
		  Commands.BeginPaifangTool, Commands.BeginScreenWallTool });
	Result.Add(FName("Enclosure"),
		{ Commands.BeginWallTool, Commands.BeginCourtWallTool,
		  Commands.BeginCorridorTool, Commands.BeginPathTool });
	Result.Add(FName("Courtyard"),
		{ Commands.BeginFlowerBedTool, Commands.BeginWaterJarTool });
	Result.Add(FName("Layout"),
		{ Commands.BeginGalleryTool, Commands.BeginGalleryWallsTool, Commands.BeginGalleryHousesTool, Commands.BeginGalleryGatesTool, Commands.BeginGalleryCourtyardTool, Commands.BeginGalleryStreetTool, Commands.BeginGalleryTemplesTool,
		  Commands.BeginMeasureTool, Commands.BeginHeightsTool, Commands.BeginCourtsTool, Commands.BeginImportTool });
	return Result;
}

UHutongLayoutEdMode* UHutongLayoutEdMode::GetActive()
{
	if (GEditor == nullptr) return nullptr;
	return Cast<UHutongLayoutEdMode>(
		GLevelEditorModeTools().GetActiveScriptableMode(EM_HutongLayoutModeId));
}

UHutongLayoutModeSettings* UHutongLayoutEdMode::GetActiveSettings()
{
	UHutongLayoutEdMode* Mode = GetActive();
	return Mode ? Cast<UHutongLayoutModeSettings>(Mode->SettingsObject) : nullptr;
}

bool UHutongLayoutEdMode::StartTool(const TCHAR* ToolIdentifier)
{
	UHutongLayoutEdMode* Mode = GetActive();
	UInteractiveToolManager* ToolManager = Mode ? Mode->GetToolManager() : nullptr;
	if (!ToolManager) return false;
	ToolManager->SelectActiveToolType(EToolSide::Left, ToolIdentifier);
	return ToolManager->ActivateTool(EToolSide::Left);
}

void UHutongLayoutEdMode::RememberToolBeforeSave()
{
	// Captured before the save: afterwards the tool manager has forgotten the tool. Empty when
	// none was up, so a save never opens one.
	UInteractiveToolManager* ToolManager = GetToolManager();
	ToolBeforeSave = ToolManager ? ToolManager->GetActiveToolName(EToolSide::Left) : FString();
}

void UHutongLayoutEdMode::RestoreToolAfterSave()
{
	const FString Wanted = MoveTemp(ToolBeforeSave);
	ToolBeforeSave.Reset();
	if (Wanted.IsEmpty() || !GEditor) return;

	// Next tick: not inside the save's broadcast, and the context has just shut a tool down.
	GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
		[this, Wanted]()
		{
			UInteractiveToolManager* ToolManager = GetToolManager();
			if (!ToolManager) return;
			// Not if the user picked another tool during the save.
			if (!ToolManager->GetActiveToolName(EToolSide::Left).IsEmpty()) return;
			ToolManager->SelectActiveToolType(EToolSide::Left, Wanted);
			ToolManager->ActivateTool(EToolSide::Left);
		}));
}

bool UHutongLayoutEdMode::IsToolOnPalette(const FString& ToolIdentifier, FName PaletteName) const
{
	const TSharedPtr<FUICommandInfo> Command = FindToolCommand(ToolIdentifier);
	if (!Command.IsValid()) return false;

	const TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> Palettes = GetModeCommands();
	const TArray<TSharedPtr<FUICommandInfo>>* Commands = Palettes.Find(PaletteName);
	return Commands && Commands->Contains(Command);
}

FName UHutongLayoutEdMode::PaletteOfTool(const FString& ToolIdentifier) const
{
	const TSharedPtr<FUICommandInfo> Command = FindToolCommand(ToolIdentifier);
	if (!Command.IsValid()) return NAME_None;
	for (const auto& Palette : GetModeCommands())
	{
		if (Palette.Value.Contains(Command)) return Palette.Key;
	}
	return NAME_None;
}

void UHutongLayoutEdMode::PutToolDown()
{
	UInteractiveToolManager* Manager = GetToolManager();
	if (!Manager || !Manager->HasActiveTool(EToolSide::Left)) return;

	// Next tick, never inside the key event: the tool being shut down would be handling it
	// (as in FollowSelection).
	if (!GEditor) return;
	GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		UInteractiveToolManager* TM = GetToolManager();
		if (TM && TM->HasActiveTool(EToolSide::Left))
		{
			TM->DeactivateTool(EToolSide::Left, EToolShutdownType::Cancel);
		}
	}));
}

FString UHutongLayoutEdMode::ToolIdentifierFor(const UHutongBuildingComponent* Building)
{
	if (!Building) return FString();
	if (const UHutongWallBuildingComponent* Wall = Cast<UHutongWallBuildingComponent>(Building))
	{
		return Wall->Params.Role == EHutongWallRole::Courtyard ? TEXT("HutongCourtWallTool") : TEXT("HutongWallTool");
	}
	static const TMap<UClass*, FString> Tools = {
		{ UHutongSiheyuanBuildingComponent::StaticClass(),   TEXT("HutongSiheyuanTool") },
		{ UHutongEarPassageBuildingComponent::StaticClass(), TEXT("HutongEarPassageTool") },
		{ UHutongGateHouseBuildingComponent::StaticClass(),  TEXT("HutongGateHouseTool") },
		{ UHutongPaifangBuildingComponent::StaticClass(),    TEXT("HutongPaifangTool") },
		{ UHutongInnerGateBuildingComponent::StaticClass(),  TEXT("HutongInnerGateTool") },
		{ UHutongCorridorBuildingComponent::StaticClass(),   TEXT("HutongCorridorTool") },
		{ UHutongScreenWallBuildingComponent::StaticClass(), TEXT("HutongScreenWallTool") },
		{ UHutongShopfrontBuildingComponent::StaticClass(),  TEXT("HutongShopfrontTool") },
		{ UHutongStoreyBuildingComponent::StaticClass(),     TEXT("HutongStoreyTool") },
		{ UHutongPavilionBuildingComponent::StaticClass(),   TEXT("HutongPavilionTool") },
		{ UHutongHallBuildingComponent::StaticClass(),       TEXT("HutongHallTool") },
		{ UHutongPathBuildingComponent::StaticClass(),       TEXT("HutongPathTool") },
		{ UHutongFlowerBedBuildingComponent::StaticClass(),  TEXT("HutongFlowerBedTool") },
		{ UHutongWaterJarBuildingComponent::StaticClass(),   TEXT("HutongWaterJarTool") },
		{ UHutongFrameBuildingComponent::StaticClass(),      TEXT("HutongFrameTool") },
	};
	for (const UClass* Class = Building->GetClass(); Class; Class = Class->GetSuperClass())
	{
		if (const FString* Found = Tools.Find(const_cast<UClass*>(Class))) return *Found;
	}
	return FString();
}

void UHutongLayoutEdMode::OnEditorSelectionChanged(UObject* Selection)
{
	if (!GEditor || Selection != GEditor->GetSelectedActors() || bFollowSelectionQueued) return;
	// Next tick: the broadcast may be our own tool's press selecting what it will drag; never swap
	// a tool out from under its own press.
	bFollowSelectionQueued = true;
	GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() { FollowSelection(); }));
}

void UHutongLayoutEdMode::FollowSelection()
{
	bFollowSelectionQueued = false;
	UInteractiveToolManager* ToolManager = GetToolManager();
	USelection* Selected = GEditor ? GEditor->GetSelectedActors() : nullptr;
	if (!ToolManager || !Selected || Selected->Num() != 1) return;

	const AActor* Actor = Cast<AActor>(Selected->GetSelectedObject(0));
	const UHutongBuildingComponent* Building = Actor ? Actor->FindComponentByClass<UHutongBuildingComponent>() : nullptr;
	const FString Wanted = ToolIdentifierFor(Building);
	if (Wanted.IsEmpty()) return;
	// An active tool stays: every tool edits any selected building, and a stray house click while
	// drawing walls must not drop the wall tool. Only an empty hand gets a tool.
	if (!ToolManager->GetActiveToolName(EToolSide::Left).IsEmpty()) return;

	// A drag in hand keeps its tool; asked again on release.
	if (const URectDragToolBase* Tool = Cast<URectDragToolBase>(ToolManager->GetActiveTool(EToolSide::Left)))
	{
		if (Tool->IsPlacingActive() || Tool->IsEditingPlan())
		{
			bFollowSelectionQueued = true;
			GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this, [this]() { FollowSelection(); }));
			return;
		}
	}

	ToolManager->SelectActiveToolType(EToolSide::Left, Wanted);
	if (!ToolManager->ActivateTool(EToolSide::Left)) return;
	// Tab after the tool is up: the toolkit closes a tool not on the tab it switches to.
	const FName Palette = PaletteOfTool(Wanted);
	if (!Palette.IsNone() && Toolkit.IsValid() && Toolkit->GetCurrentPalette() != Palette)
	{
		Toolkit->SetCurrentPalette(Palette);
	}
}

TSharedPtr<FUICommandInfo> UHutongLayoutEdMode::FindToolCommand(const FString& ToolIdentifier) const
{
	const TSharedPtr<FUICommandInfo>* Found = ToolCommandsById.Find(ToolIdentifier);
	return Found ? *Found : nullptr;
}

#undef LOCTEXT_NAMESPACE
