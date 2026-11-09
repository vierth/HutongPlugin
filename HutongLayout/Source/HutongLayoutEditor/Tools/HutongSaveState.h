#pragma once

#include "CoreMinimal.h"

class UHutongBuildingComponent;

// The editor's default autosave writes a backup copy elsewhere and leaves the package unsaved, so it is reported apart.
namespace HutongSaveState
{
	enum class EState : uint8
	{
		Saved,
		Unsaved,
		// Unsaved, but an autosave has backed up every change since.
		BackedUp,
	};

	struct FStatus
	{
		FText Text;
		FLinearColor Colour = FLinearColor::White;
	};

	EState StateOf(const UHutongBuildingComponent* Building);
	FLinearColor ColourOf(EState State);
	// English only: the viewport's bitmap font.
	FString LabelOf(EState State);

	FStatus Describe(const TArray<UHutongBuildingComponent*>& Buildings);

	const FStatus& SceneStatus();

	class FPolled
	{
	public:
		explicit FPolled(TFunction<TArray<UHutongBuildingComponent*>()> InSource) : Source(MoveTemp(InSource)) {}
		const FStatus& Get() const;

	private:
		TFunction<TArray<UHutongBuildingComponent*>()> Source;
		mutable uint64 Frame = MAX_uint64;
		mutable FStatus Status;
	};

	void Start();
	void Stop();
}
