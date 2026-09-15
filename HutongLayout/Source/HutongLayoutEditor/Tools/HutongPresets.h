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

	// Presets the plugin ships, held in code.
	static void RegisterBuiltIn(FName ToolKey, const FString& Name,
		const UScriptStruct* Type, const void* Data);

	TArray<FString> GetPresetNames(FName ToolKey) const;
	void SavePreset(FName ToolKey, const FString& Name, const UScriptStruct* Type, const void* Data);
	bool LoadPreset(FName ToolKey, const FString& Name, const UScriptStruct* Type, void* OutData) const;
	void DeletePreset(FName ToolKey, const FString& Name);

	UPROPERTY(config)
	TMap<FString, FString> Presets;

private:
	// Not a UPROPERTY: these are compiled in, never serialised.
	static TMap<FString, FString>& BuiltIns();
};

// Save/load UI for a tool's params struct.
UCLASS()
class UHutongPresetProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	// ToolKey namespaces the saved names so the wall and siheyuan lists stay separate.
	void Initialize(FName InToolKey, UInteractiveToolPropertySet* InOwnerSet, FName ParamsPropertyName);

	// Called after a preset is applied, so the tool can refresh the details panel.
	TFunction<void()> OnPresetLoaded;

	UPROPERTY(EditAnywhere, Category = "Preset", meta = (DisplayName = "Type / Preset", GetOptions = "GetPresetNames", ToolTip = "Existing preset to load or delete."))
	FString Preset;

	UPROPERTY(EditAnywhere, Category = "Preset", meta = (ToolTip = "Name to save the current settings under."))
	FString SaveAs;

	UFUNCTION(CallInEditor, Category = "Preset", meta = (DisplayName = "Save", ToolTip = "Saves the current settings under the Save As name."))
	void SaveCurrentAsPreset();

	UFUNCTION(CallInEditor, Category = "Preset", meta = (DisplayName = "Load", ToolTip = "Applies the selected preset to the current settings."))
	void LoadSelectedPreset();

	UFUNCTION(CallInEditor, Category = "Preset", meta = (DisplayName = "Delete", ToolTip = "Deletes the selected user preset."))
	void DeleteSelectedPreset();

	UFUNCTION()
	TArray<FString> GetPresetNames() const;

	// What this picker's names are namespaced under, for comparing against a component's own key.
	FName GetToolKey() const { return ToolKey; }

private:
	// Resolves the params struct on the owning set. Returns false if the owner has gone away.
	bool GetParams(const UScriptStruct*& OutType, void*& OutData) const;

	FName ToolKey;
	FName ParamsName;

	// TransientToolProperty, or RestoreProperties copies the previous tool's set back out of the
	// CDO cache and the preset buttons read another tool's parameters.
	UPROPERTY(meta=(TransientToolProperty))
	TWeakObjectPtr<UInteractiveToolPropertySet> OwnerSet;
};
