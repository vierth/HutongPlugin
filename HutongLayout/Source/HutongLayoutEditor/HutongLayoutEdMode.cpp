#include "HutongLayoutEdMode.h"
#include "Editor.h"
#include "TimerManager.h"
#include "UObject/ObjectSaveContext.h"
#include "InteractiveToolManager.h"
#include "EditorModeManager.h"
#include "Tools/EdModeInteractiveToolsContext.h"
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
#include "Tools/UnknownTool.h"
#include "Tools/CityWallTool.h"
#include "Tools/WallCornerTool.h"
#include "Tools/ArrowTowerTool.h"
#include "Tools/CityGateTool.h"
#include "Tools/GateComplexTool.h"
#include "Tools/SmallBuildingTool.h"
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
#include "Tools/HutongDetailOps.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

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

	// While typing into a field, Esc, -, = and the letters belong to the text.
	static bool IsTyping(FSlateApplication& SlateApp)
	{
		const TSharedPtr<SWidget> Focused = SlateApp.GetKeyboardFocusedWidget();
		return Focused.IsValid() && Focused->GetType().ToString().Contains(TEXT("EditableText"));
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		URectDragToolBase* Tool = GetActiveRectTool();
		if (IsTyping(SlateApp)) return false;
		if (!Tool) return false;

		const FKey Key = InKeyEvent.GetKey();
		const bool bIdle = !Tool->IsPlacingActive() && !Tool->IsEditingPlan();
		if (Key == EKeys::Escape && bIdle && Tool->CancelSelectedCycle()) return true;
		if (Key == EKeys::P || Key == EKeys::T)
		{
			// With no building selected the viewport keeps the key.
			const FModifierKeysState& Mods = InKeyEvent.GetModifierKeys();
			if (!bIdle || InKeyEvent.IsRepeat() || Mods.IsControlDown() || Mods.IsCommandDown() || Mods.IsAltDown()) return false;
			if (Mods.IsShiftDown()) return Key == EKeys::P ? Tool->CycleSelectedPreset(1) : Tool->CycleSelectedType(1);
			return Tool->OpenSelectedMenu(Key == EKeys::T, SlateApp.GetCursorPos());
		}
		if (Key == EKeys::Enter)
		{
			if (InKeyEvent.IsRepeat()) return false;
			if (Tool->IsPlacingActive()) return Tool->FinishPlacement();
			return bIdle && Tool->ConfirmSelectedCycle();
		}
		if (Key == EKeys::Escape && Tool->IsTurning() && !Tool->IsEditingPlan())
		{
			Tool->EndTurnMode();
			return true;
		}
		if (Key == EKeys::Escape)
		{
			// With nothing in hand, put the tool down so the preview stops following the cursor.
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
			// Height edits the tool's persisted params, so it works before a placement starts.
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
				if (InKeyEvent.IsRepeat() || Tool->IsEditingPlan()) return false;
				const int32 Delta = Key == EKeys::RightBracket ? 1 : -1;
				const FModifierKeysState& Mods = InKeyEvent.GetModifierKeys();
				if (Mods.IsControlDown()) return Tool->MoveSelectedDoorBay(Delta);
				return Mods.IsShiftDown() ? Tool->TurnSelectedFacing(Delta) : Tool->AdjustSelectedBays(Delta);
			}
			const FModifierKeysState& Mods = InKeyEvent.GetModifierKeys();
			Tool->AdjustBracketValue(Key == EKeys::RightBracket ? 1 : -1,
				Mods.IsShiftDown(), Mods.IsControlDown());
			return true;
		}
		if (Key == EKeys::R)
		{
			// Nothing being placed: turn mode on the selection (with nothing to turn the editor keeps R).
			if (!Tool->IsPlacingActive())
			{
				// Mid plan drag: swallowed, so the editor's scale gizmo stays out.
				if (Tool->IsEditingPlan()) return true;
				if (InKeyEvent.IsRepeat()) return Tool->IsTurning();
				const FModifierKeysState& Mods = InKeyEvent.GetModifierKeys();
				if (Mods.IsControlDown() || Mods.IsCommandDown() || Mods.IsAltDown()) return false;
				return Tool->ToggleTurnMode();
			}
			if (!InKeyEvent.IsRepeat())
			{
				Tool->BeginRotateMode();
			}
			return true;
		}
		if (Key == EKeys::F)
		{
			if (InKeyEvent.IsRepeat()) return false;
			// With no such building the viewport keeps F for focus.
			if (!Tool->IsPlacingActive()) return !Tool->IsEditingPlan() && Tool->FlipSelectedFacing();
			Tool->FlipFacing();
			return true;
		}
		if (Key == EKeys::G)
		{
			// With nothing placed and no wall selected the viewport keeps G for game view.
			if (!Tool->IsPlacingActive())
			{
				if (InKeyEvent.IsRepeat() || Tool->IsEditingPlan()) return false;
				return InKeyEvent.GetModifierKeys().IsShiftDown() ? Tool->AddSelectedGate() : Tool->ToggleSelectedGate();
			}
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
		HutongPlanOutline::SetPlansOverBuildings(Settings->bPlansOverBuildings);
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
	RegisterTool(Commands.BeginSmallBuildingTool, TEXT("HutongSmallBuildingTool"),
		NewObject<UHutongSmallBuildingToolBuilder>(this));
	RegisterTool(Commands.BeginCityWallTool, TEXT("HutongCityWallTool"),
		NewObject<UHutongCityWallToolBuilder>(this));
	RegisterTool(Commands.BeginPalaceWallTool, TEXT("HutongPalaceWallTool"),
		NewObject<UHutongPalaceWallToolBuilder>(this));
	RegisterTool(Commands.BeginImperialWallTool, TEXT("HutongImperialWallTool"),
		NewObject<UHutongImperialWallToolBuilder>(this));
	RegisterTool(Commands.BeginWallCornerTool, TEXT("HutongWallCornerTool"),
		NewObject<UHutongWallCornerToolBuilder>(this));
	RegisterTool(Commands.BeginArrowTowerTool, TEXT("HutongArrowTowerTool"),
		NewObject<UHutongArrowTowerToolBuilder>(this));
	RegisterTool(Commands.BeginCityGateTool, TEXT("HutongCityGateTool"),
		NewObject<UHutongCityGateToolBuilder>(this));
	RegisterTool(Commands.BeginGateComplexTool, TEXT("HutongGateComplexTool"),
		NewObject<UHutongGateComplexToolBuilder>(this));
	RegisterTool(Commands.BeginUnknownTool, TEXT("HutongUnknownTool"),
		NewObject<UHutongUnknownToolBuilder>(this));
	RegisterTool(Commands.BeginCompoundTool, TEXT("HutongCompoundTool"),
		NewObject<UHutongCompoundToolBuilder>(this));
	RegisterTool(Commands.BeginStreetRowTool, TEXT("HutongStreetRowTool"),
		NewObject<UHutongStreetRowToolBuilder>(this));
	RegisterTool(Commands.BeginGalleryTool, TEXT("HutongGalleryTool"),
		NewObject<UHutongGalleryToolBuilder>(this));
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

	// The default (UndoToExit) pushes an "Activate Tool" undo entry per activation that Ctrl+Z would pop.
	GetToolManager()->ConfigureChangeTrackingMode(EToolChangeTrackingMode::NoChangeTracking);

	GetToolManager()->SelectActiveToolType(EToolSide::Left, TEXT("HutongSiheyuanTool"));
	GetToolManager()->ActivateTool(EToolSide::Left);

	SelectionHandle = USelection::SelectionChangedEvent.AddUObject(this, &UHutongLayoutEdMode::OnEditorSelectionChanged);

	if (UEditorInteractiveToolsContext* Context = GetInteractiveToolsContext(EToolsContextScope::Editor))
	{
		bContextEndedToolsOnSave = Context->GetDeactivateToolsOnSaveWorld();
		Context->SetDeactivateToolsOnSaveWorld(false);
	}

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
	if (UEditorInteractiveToolsContext* Context = GetInteractiveToolsContext(EToolsContextScope::Editor))
	{
		Context->SetDeactivateToolsOnSaveWorld(bContextEndedToolsOnSave);
	}

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
		  Commands.BeginFrameTool, Commands.BeginSmallBuildingTool, Commands.BeginStreetRowTool, Commands.BeginCompoundTool });
	Result.Add(FName("Gates"),
		{ Commands.BeginGateHouseTool, Commands.BeginInnerGateTool,
		  Commands.BeginPaifangTool, Commands.BeginScreenWallTool });
	Result.Add(FName("Enclosure"),
		{ Commands.BeginWallTool, Commands.BeginCourtWallTool, Commands.BeginCityWallTool,
		  Commands.BeginImperialWallTool, Commands.BeginPalaceWallTool, Commands.BeginWallCornerTool, Commands.BeginArrowTowerTool, Commands.BeginCityGateTool, Commands.BeginGateComplexTool,
		  Commands.BeginCorridorTool, Commands.BeginPathTool });
	Result.Add(FName("Courtyard"),
		{ Commands.BeginFlowerBedTool, Commands.BeginWaterJarTool });
	Result.Add(FName("Layout"),
		{ Commands.BeginGalleryTool, Commands.BeginGalleryWallsTool, Commands.BeginGalleryHousesTool, Commands.BeginGalleryGatesTool, Commands.BeginGalleryCourtyardTool, Commands.BeginGalleryStreetTool, Commands.BeginGalleryTemplesTool,
		  Commands.BeginUnknownTool, Commands.BeginMeasureTool, Commands.BeginHeightsTool, Commands.BeginCourtsTool, Commands.BeginImportTool });
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

	// Next tick: inside the key event the tool being shut down would be handling it.
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
		switch (Wall->Params.Role)
		{
		case EHutongWallRole::Courtyard:    return TEXT("HutongCourtWallTool");
		case EHutongWallRole::ImperialCity: return TEXT("HutongImperialWallTool");
		default:                            return TEXT("HutongWallTool");
		}
	}
	if (const UHutongCityWallBuildingComponent* City = Cast<UHutongCityWallBuildingComponent>(Building))
	{
		if (City->Params.Rank == EHutongCityWallRank::Palace) return TEXT("HutongPalaceWallTool");
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
		{ UHutongUnknownBuildingComponent::StaticClass(),    TEXT("HutongUnknownTool") },
		{ UHutongCityWallBuildingComponent::StaticClass(),   TEXT("HutongCityWallTool") },
		{ UHutongWallCornerBuildingComponent::StaticClass(), TEXT("HutongWallCornerTool") },
		{ UHutongArrowTowerBuildingComponent::StaticClass(), TEXT("HutongArrowTowerTool") },
		{ UHutongCityGateBuildingComponent::StaticClass(),   TEXT("HutongCityGateTool") },
		{ UHutongSmallBuildingComponent::StaticClass(),      TEXT("HutongSmallBuildingTool") },
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
	// Next tick: the broadcast may be our own tool's press selecting what it will drag.
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
	if (Toolkit.IsValid() && Toolkit->GetCurrentPalette() == FName(TEXT("Scene"))) return;
	// Every tool edits any selected building, so an active tool stays; only an empty hand gets one.
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
	// Tab after the tool is up: the toolkit closes a tool missing from the tab it switches to.
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
