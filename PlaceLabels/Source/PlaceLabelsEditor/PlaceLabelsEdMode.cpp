#include "PlaceLabelsEdMode.h"

#include "PlaceLabelsCommands.h"
#include "PlaceLabelsEdModeToolkit.h"
#include "PlaceRegionComponent.h"
#include "Tools/PlaceRegionEditTool.h"
#include "Tools/PlaceRegionPenTool.h"
#include "Tools/PlaceRegionSelectTool.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "InteractiveToolManager.h"
#include "InputCoreTypes.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
	// Is the user typing into a text field right now?
	bool IsTypingIntoTextField()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return false;
		}

		TSharedPtr<SWidget> Focused = FSlateApplication::Get().GetKeyboardFocusedWidget();
		while (Focused.IsValid())
		{
			if (Focused->GetType().ToString().Contains(TEXT("EditableText")))
			{
				return true;
			}
			Focused = Focused->GetParentWidget();
		}
		return false;
	}
}

// Global key handling for the Place Labels tools.
class FPlaceLabelsInputProcessor : public IInputProcessor
{
public:
	TWeakObjectPtr<UPlaceLabelsEdMode> Owner;

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp,
		TSharedRef<ICursor> Cursor) override
	{
	}

	UInteractiveTool* GetActiveTool() const
	{
		if (!Owner.IsValid())
		{
			return nullptr;
		}
		UInteractiveToolManager* ToolManager = Owner->GetToolManager();
		return ToolManager ? ToolManager->GetActiveTool(EToolSide::Left) : nullptr;
	}

	UPlaceRegionPenTool* GetActivePenTool() const
	{
		return Cast<UPlaceRegionPenTool>(GetActiveTool());
	}

	UPlaceRegionEditTool* GetActiveEditTool() const
	{
		return Cast<UPlaceRegionEditTool>(GetActiveTool());
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		// Before anything else.
		if (IsTypingIntoTextField())
		{
			return false;
		}

		const FKey Key = InKeyEvent.GetKey();

		if (UPlaceRegionPenTool* Pen = GetActivePenTool())
		{
			if (!Pen->IsActive())
			{
				return false;
			}

			if (Key == EKeys::Escape)
			{
				Pen->CancelDrawing();
				// Deliberately not consumed, so the tool system sees the Escape too.
				return false;
			}
			if (Key == EKeys::BackSpace)
			{
				Pen->UndoLastPoint();
				return true;
			}
			if (Key == EKeys::Delete)
			{
				// Removes whichever corner is under the cursor.
				Pen->DeleteHoveredPoint();
				return true;
			}
			if (Key == EKeys::Enter)
			{
				// Enter means "finish the current stage": first it closes the outline, then it commits the named region.
				if (Pen->IsAwaitingConfirm())
				{
					Pen->ConfirmRegion();
				}
				else
				{
					Pen->CloseOutline();
				}
				return true;
			}
			return false;
		}

		if (UPlaceRegionEditTool* Edit = GetActiveEditTool())
		{
			// Everything below is conditional on the edit tool actually holding a region.
			if (!Edit->HasTarget())
			{
				return false;
			}

			if (Key == EKeys::Escape)
			{
				Edit->ClearTarget();
				return false;
			}
			if (Key == EKeys::Delete || Key == EKeys::BackSpace)
			{
				if (!Edit->HasCornerSelection())
				{
					return false;
				}
				Edit->DeleteSelectedCorners();
				return true;
			}
			if (Key == EKeys::A && InKeyEvent.IsControlDown())
			{
				Edit->SelectAllCorners();
				return true;
			}
		}

		return false;
	}
};

#define LOCTEXT_NAMESPACE "PlaceLabelsEdMode"

const FEditorModeID UPlaceLabelsEdMode::EM_PlaceLabelsModeId = TEXT("EM_PlaceLabels");

UPlaceLabelsEdMode::UPlaceLabelsEdMode()
{
	Info = FEditorModeInfo(
		EM_PlaceLabelsModeId,
		LOCTEXT("PlaceLabelsModeName", "Place Labels"),
		FSlateIcon(),
		true,
		// Just after Hutong Layout's 1000, so the two sit together in the mode bar.
		1010);
}

void UPlaceLabelsEdMode::Enter()
{
	Super::Enter();

	// Draw every region while the mode is up, filled, so which ground is already labelled is
	// obvious without clicking anything. Nothing is drawn once the mode is down.
	UPlaceRegionComponent::SetEditorDrawingVisible(true);

	const FPlaceLabelsCommands& Commands = FPlaceLabelsCommands::Get();

	RegisterTool(Commands.BeginSelectTool, TEXT("PlaceRegionSelectTool"),
		NewObject<UPlaceRegionSelectToolBuilder>(this));
	RegisterTool(Commands.BeginPenTool, TEXT("PlaceRegionPenTool"),
		NewObject<UPlaceRegionPenToolBuilder>(this));
	RegisterTool(Commands.BeginEditTool, TEXT("PlaceRegionEditTool"),
		NewObject<UPlaceRegionEditToolBuilder>(this));

	// The mode opens ready to look at what is already down rather than ready to draw over it.
	GetToolManager()->SelectActiveToolType(EToolSide::Left, TEXT("PlaceRegionSelectTool"));

	if (FSlateApplication::IsInitialized())
	{
		TSharedRef<FPlaceLabelsInputProcessor> Processor = MakeShared<FPlaceLabelsInputProcessor>();
		Processor->Owner = this;
		InputProcessor = Processor;
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
	}
}

void UPlaceLabelsEdMode::Exit()
{
	UPlaceRegionComponent::SetEditorDrawingVisible(false);

	if (InputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
	InputProcessor.Reset();

	Super::Exit();
}

void UPlaceLabelsEdMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FPlaceLabelsEdModeToolkit);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> UPlaceLabelsEdMode::GetModeCommands() const
{
	TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> Result;
	const FPlaceLabelsCommands& Commands = FPlaceLabelsCommands::Get();

	// This key has to match the toolkit's GetToolPaletteNames.
	Result.Add(FName("Tools"), { Commands.BeginPenTool, Commands.BeginEditTool });
	return Result;
}

#undef LOCTEXT_NAMESPACE
