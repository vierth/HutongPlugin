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
#include "Tools/PavilionTool.h"
#include "Tools/ShopfrontTool.h"
#include "Tools/StoreyTool.h"
#include "Tools/HallTool.h"
#include "Tools/GalleryTool.h"
#include "Tools/MeasureTool.h"
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

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		URectDragToolBase* Tool = GetActiveRectTool();
		if (!Tool) return false;

		const FKey Key = InKeyEvent.GetKey();
		if (Key == EKeys::Escape)
		{
			Tool->CancelPlacement();
			return false;
		}
		if (Key == EKeys::Hyphen || Key == EKeys::Equals)
		{
			// Height edits the tool's persisted params, so unlike the bay keys they work before a placement starts too.
			const FModifierKeysState& Mods = InKeyEvent.GetModifierKeys();
			const double Step = Mods.IsShiftDown() ? 5.0 : (Mods.IsControlDown() ? 100.0 : 20.0);
			Tool->AdjustHeight(Key == EKeys::Equals ? Step : -Step);
			return true;
		}
		if (Key == EKeys::LeftBracket || Key == EKeys::RightBracket)
		{
			// Repeats are wanted here — holding a bracket key walks the value.
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
		if (Key == EKeys::G)
		{
			// Hold G on the click that ends a wall segment: that segment gets the gate. Taken
			// only mid-placement, so the viewport keeps G for its game view otherwise.
			if (!Tool->IsPlacingActive()) return false;
			Tool->SetOpeningKeyHeld(true);
			return true;
		}
		return false;
	}

	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() == EKeys::R)
		{
			if (URectDragToolBase* Tool = GetActiveRectTool())
			{
				Tool->EndRotateMode();
				return true;
			}
		}
		if (InKeyEvent.GetKey() == EKeys::G)
		{
			if (URectDragToolBase* Tool = GetActiveRectTool())
			{
				const bool bWasHeld = Tool->bOpeningKeyHeld;
				Tool->SetOpeningKeyHeld(false);
				return bWasHeld;
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

	// UEdMode makes the object, calls LoadConfig/SaveConfig round it and hands it to the toolkit, which already builds ModeDetailsView and slots it above the tool's panel.
	SettingsClass = UHutongLayoutModeSettings::StaticClass();
}

void UHutongLayoutEdMode::Enter()
{
	Super::Enter();

	// The settings object is made fresh on every Enter, so its plan-outline checkbox has to be
	// told what the switch it mirrors is currently set to.
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
	RegisterTool(Commands.BeginMeasureTool, TEXT("HutongMeasureTool"),
		NewObject<UHutongMeasureToolBuilder>(this));
	RegisterTool(Commands.BeginImportTool, TEXT("HutongImportTool"),
		NewObject<UHutongImportToolBuilder>(this));

	// The tool manager's default (UndoToExit) pushes an "Activate Tool" entry onto the undo stack
	// on every activation, and Ctrl+Z pops it — cancelling the tool instead of undoing the last
	// edit. With a click on a building starting its tool, that was one dead Ctrl+Z per click, and
	// a Generate could not be undone at all. Nothing here needs undo to leave a tool: Esc does.
	GetToolManager()->ConfigureChangeTrackingMode(EToolChangeTrackingMode::NoChangeTracking);

	// The house is the tool most sessions start with, so it is the one that is already up.
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
	// Second list, and not optional: a tool left out here loses its keyboard chord while its palette button keeps working.
	Result.Add(FName("Buildings"),
		{ Commands.BeginSiheyuanTool, Commands.BeginEarPassageTool, Commands.BeginShopfrontTool,
		  Commands.BeginStoreyTool, Commands.BeginHallTool, Commands.BeginPavilionTool,
		  Commands.BeginStreetRowTool, Commands.BeginCompoundTool });
	Result.Add(FName("Gates"),
		{ Commands.BeginGateHouseTool, Commands.BeginInnerGateTool,
		  Commands.BeginPaifangTool, Commands.BeginScreenWallTool });
	Result.Add(FName("Enclosure"),
		{ Commands.BeginWallTool, Commands.BeginCourtWallTool,
		  Commands.BeginCorridorTool, Commands.BeginPathTool });
	Result.Add(FName("Courtyard"),
		{ Commands.BeginFlowerBedTool, Commands.BeginWaterJarTool });
	Result.Add(FName("Layout"),
		{ Commands.BeginGalleryTool, Commands.BeginMeasureTool, Commands.BeginImportTool });
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
	// Captured on the way in, because by the time the save is over the tool manager has forgotten
	// which tool it had. Empty when nothing was up, so a save never opens a tool of its own.
	UInteractiveToolManager* ToolManager = GetToolManager();
	ToolBeforeSave = ToolManager ? ToolManager->GetActiveToolName(EToolSide::Left) : FString();
}

void UHutongLayoutEdMode::RestoreToolAfterSave()
{
	const FString Wanted = MoveTemp(ToolBeforeSave);
	ToolBeforeSave.Reset();
	if (Wanted.IsEmpty() || !GEditor) return;

	// Next tick: the save's own broadcast is no place to be starting a tool, and the context has
	// only just finished shutting one down.
	GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,
		[this, Wanted]()
		{
			UInteractiveToolManager* ToolManager = GetToolManager();
			if (!ToolManager) return;
			// Not if something else is up: the user may have picked a tool while the save ran.
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
	// Next tick: the broadcast may be our own tool's press selecting what it is about to drag,
	// and a tool is not swapped out from under its own press.
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
	// A tool already up stays up: every tool edits any selected building, and a stray click on a
	// house while drawing walls must not take the wall tool away. Only an empty hand gets a tool.
	if (!ToolManager->GetActiveToolName(EToolSide::Left).IsEmpty()) return;

	// A drag in hand keeps its tool; asked again when it lets go.
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
	// Its tab too, after the tool is up: the toolkit closes a tool that is not on the tab it switches to.
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
