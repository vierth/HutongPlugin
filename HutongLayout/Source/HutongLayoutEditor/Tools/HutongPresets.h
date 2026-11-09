#pragma once

#include "CoreMinimal.h"
#include "InteractiveTool.h"
#include "HutongPresets.generated.h"

// Named parameter sets, stored per user in EditorPerProjectUserSettings.
UCLASS(config = EditorPerProjectUserSettings)
class UHutongPresetLibrary : public UObject
{
	GENERATED_BODY()

public:
	static UHutongPresetLibrary* Get();

	static void RegisterBuiltIn(FName ToolKey, const FString& Name,
		const UScriptStruct* Type, const void* Data);

	TArray<FString> GetPresetNames(FName ToolKey) const;
	void SavePreset(FName ToolKey, const FString& Name, const UScriptStruct* Type, const void* Data);
	bool LoadPreset(FName ToolKey, const FString& Name, const UScriptStruct* Type, void* OutData) const;
	void DeletePreset(FName ToolKey, const FString& Name);

	UPROPERTY(config)
	TMap<FString, FString> Presets;

private:
	static TMap<FString, FString>& BuiltIns();
};

// Save/load UI for a tool's params struct.
UCLASS()
class UHutongPresetProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	void Initialize(FName InToolKey, UInteractiveToolPropertySet* InOwnerSet, FName ParamsPropertyName);

	TFunction<void()> OnPresetLoaded;

	UPROPERTY(EditAnywhere, Category = "Preset", meta = (DisplayName = "Type / Preset", GetOptions = "GetPresetNames", ToolTip = "Existing preset to load or delete."))
	FString Preset;

	UPROPERTY(EditAnywhere, Category = "Preset", meta = (HutongAdvanced, ToolTip = "Name to save the current settings under."))
	FString SaveAs;

	// Buttons drawn by FHutongPresetCustomization: Save and Delete only in the advanced view.
	void SaveCurrentAsPreset();
	void LoadSelectedPreset();
	void DeleteSelectedPreset();

	UFUNCTION()
	TArray<FString> GetPresetNames() const;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	FName GetToolKey() const { return ToolKey; }

private:
	bool GetParams(const UScriptStruct*& OutType, void*& OutData) const;

	FName ToolKey;
	FName ParamsName;

	// TransientToolProperty, else RestoreProperties copies the previous tool's set from the CDO cache.
	UPROPERTY(meta=(TransientToolProperty))
	TWeakObjectPtr<UInteractiveToolPropertySet> OwnerSet;
};
