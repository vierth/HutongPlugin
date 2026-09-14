#include "Tools/HutongPresets.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

namespace
{
	FString MakeKey(FName ToolKey, const FString& Name)
	{
		return FString::Printf(TEXT("%s/%s"), *ToolKey.ToString(), *Name);
	}
}

UHutongPresetLibrary* UHutongPresetLibrary::Get()
{
	// The CDO loads its config on class load and is what SaveConfig writes back.
	return GetMutableDefault<UHutongPresetLibrary>();
}

TMap<FString, FString>& UHutongPresetLibrary::BuiltIns()
{
	static TMap<FString, FString> Map;
	return Map;
}

void UHutongPresetLibrary::RegisterBuiltIn(FName ToolKey, const FString& Name,
	const UScriptStruct* Type, const void* Data)
{
	if (Name.IsEmpty() || !Type || !Data) return;

	// Serialised through the same converter user presets go through.
	FString Json;
	if (!FJsonObjectConverter::UStructToJsonObjectString(Type, Data, Json)) return;

	BuiltIns().Add(MakeKey(ToolKey, Name), Json);
}

TArray<FString> UHutongPresetLibrary::GetPresetNames(FName ToolKey) const
{
	const FString Prefix = MakeKey(ToolKey, FString());
	TSet<FString> Unique;
	for (const TPair<FString, FString>& Pair : BuiltIns())
	{
		if (Pair.Key.StartsWith(Prefix)) Unique.Add(Pair.Key.RightChop(Prefix.Len()));
	}
	for (const TPair<FString, FString>& Pair : Presets)
	{
		if (Pair.Key.StartsWith(Prefix)) Unique.Add(Pair.Key.RightChop(Prefix.Len()));
	}
	TArray<FString> Names = Unique.Array();
	Names.Sort();
	return Names;
}

void UHutongPresetLibrary::SavePreset(FName ToolKey, const FString& Name, const UScriptStruct* Type, const void* Data)
{
	if (Name.IsEmpty() || !Type || !Data) return;

	FString Json;
	if (!FJsonObjectConverter::UStructToJsonObjectString(Type, Data, Json)) return;

	Presets.Add(MakeKey(ToolKey, Name), Json);
	SaveConfig();
}

bool UHutongPresetLibrary::LoadPreset(FName ToolKey, const FString& Name, const UScriptStruct* Type, void* OutData) const
{
	// User first, so saving over a built-in's name shadows it rather than being ignored.
	const FString Key = MakeKey(ToolKey, Name);
	const FString* Json = Presets.Find(Key);
	if (!Json) Json = BuiltIns().Find(Key);
	if (!Json || !Type || !OutData) return false;

	TSharedPtr<FJsonObject> JsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(*Json);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid()) return false;

	return FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), Type, OutData);
}

void UHutongPresetLibrary::DeletePreset(FName ToolKey, const FString& Name)
{
	// Only ever removes the user's copy.
	if (Presets.Remove(MakeKey(ToolKey, Name)) > 0)
	{
		SaveConfig();
	}
}

void UHutongPresetProperties::Initialize(FName InToolKey, UInteractiveToolPropertySet* InOwnerSet, FName ParamsPropertyName)
{
	ToolKey = InToolKey;
	OwnerSet = InOwnerSet;
	ParamsName = ParamsPropertyName;
}

bool UHutongPresetProperties::GetParams(const UScriptStruct*& OutType, void*& OutData) const
{
	UInteractiveToolPropertySet* Owner = OwnerSet.Get();
	if (!Owner) return false;

	FStructProperty* StructProp = FindFProperty<FStructProperty>(Owner->GetClass(), ParamsName);
	if (!StructProp) return false;

	OutType = StructProp->Struct;
	OutData = StructProp->ContainerPtrToValuePtr<void>(Owner);
	return OutType != nullptr && OutData != nullptr;
}

TArray<FString> UHutongPresetProperties::GetPresetNames() const
{
	return UHutongPresetLibrary::Get()->GetPresetNames(ToolKey);
}

void UHutongPresetProperties::SaveCurrentAsPreset()
{
	const UScriptStruct* Type = nullptr;
	void* Data = nullptr;
	// Fall back to the selected preset name so the Save button still does the obvious thing when the user is overwriting.
	const FString Name = SaveAs.IsEmpty() ? Preset : SaveAs;
	if (Name.IsEmpty() || !GetParams(Type, Data)) return;

	UHutongPresetLibrary::Get()->SavePreset(ToolKey, Name, Type, Data);
	Preset = Name;
	SaveAs.Reset();
}

void UHutongPresetProperties::LoadSelectedPreset()
{
	const UScriptStruct* Type = nullptr;
	void* Data = nullptr;
	if (Preset.IsEmpty() || !GetParams(Type, Data)) return;

	if (UHutongPresetLibrary::Get()->LoadPreset(ToolKey, Preset, Type, Data) && OnPresetLoaded)
	{
		OnPresetLoaded();
	}
}

void UHutongPresetProperties::DeleteSelectedPreset()
{
	if (Preset.IsEmpty()) return;
	UHutongPresetLibrary::Get()->DeletePreset(ToolKey, Preset);
	Preset.Reset();
}
