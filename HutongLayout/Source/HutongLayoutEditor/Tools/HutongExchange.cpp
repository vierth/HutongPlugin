#include "Tools/HutongExchange.h"

#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongActorSpawn.h"
#include "Generation/HutongDetail.h"
#include "Tools/HutongDetailOps.h"
#include "Tools/HutongSnap.h"

#include "Editor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "JsonObjectConverter.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopedSlowTask.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "HutongExchange"

using UE::Geometry::FDynamicMesh3;

namespace HutongExchange
{

namespace
{
	// Every parameter, footprint field and palette colour is an Edit property; the siheyuan's two component pointers are the only UPROPERTYs that are not, and they are exactly what must not travel.
	constexpr int64 CheckFlags = CPF_Edit;
	constexpr int64 SkipFlags = CPF_Transient | CPF_Deprecated;

	TSharedPtr<FJsonObject> Vec2(const FVector2D& V)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("x"), V.X);
		O->SetNumberField(TEXT("y"), V.Y);
		return O;
	}

	FVector2D ReadVec2(const TSharedPtr<FJsonObject>& O)
	{
		if (!O.IsValid()) return FVector2D::ZeroVector;
		return FVector2D(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")));
	}

	TSharedPtr<FJsonObject> Vec3(const FVector& V)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("x"), V.X);
		O->SetNumberField(TEXT("y"), V.Y);
		O->SetNumberField(TEXT("z"), V.Z);
		return O;
	}

	FVector ReadVec3(const TSharedPtr<FJsonObject>& O)
	{
		if (!O.IsValid()) return FVector::ZeroVector;
		return FVector(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("y")),
			O->GetNumberField(TEXT("z")));
	}

	// Yaw about +Z on the XY plane, matching FRotator's own sense.
	FVector2D RotateXY(const FVector2D& V, double YawDeg)
	{
		const double R = FMath::DegreesToRadians(YawDeg);
		const double C = FMath::Cos(R), S = FMath::Sin(R);
		return FVector2D(V.X * C - V.Y * S, V.X * S + V.Y * C);
	}

	// The keys this plugin's own properties own, in the casing the converter writes them in.
	void GatherOwnKeys(const UClass* Class, TSet<FString>& OutKeys)
	{
		for (TFieldIterator<FProperty> It(Class); It; ++It)
		{
			const FProperty* Prop = *It;
			const UClass* Owner = Prop->GetOwnerClass();
			if (!Owner || !Owner->IsChildOf(UHutongBuildingComponent::StaticClass())) continue;
			OutKeys.Add(FJsonObjectConverter::StandardizeCase(Prop->GetAuthoredName()));
		}
	}

	// This plugin's own editable properties in one category, which is how a layout-only record
	// carries what a placement decided without carrying how the piece is built.
	bool IsInCategory(const FProperty* Prop, const TCHAR* Category)
	{
		if (!Prop->HasAnyPropertyFlags(CheckFlags)) return false;
		if (Prop->HasAnyPropertyFlags(SkipFlags)) return false;
		const UClass* Owner = Prop->GetOwnerClass();
		if (!Owner || !Owner->IsChildOf(UHutongBuildingComponent::StaticClass())) return false;
		return Prop->GetMetaData(TEXT("Category")) == Category;
	}

	TSharedPtr<FJsonObject> WriteCategory(const UHutongBuildingComponent* Component,
		const TCHAR* Category)
	{
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		for (TFieldIterator<FProperty> It(Component->GetClass()); It; ++It)
		{
			FProperty* Prop = *It;
			if (!IsInCategory(Prop, Category)) continue;
			const void* Value = Prop->ContainerPtrToValuePtr<void>(Component);
			TSharedPtr<FJsonValue> Json =
				FJsonObjectConverter::UPropertyToJsonValue(Prop, Value, CheckFlags, SkipFlags);
			if (Json.IsValid())
			{
				Out->SetField(FJsonObjectConverter::StandardizeCase(Prop->GetAuthoredName()), Json);
			}
		}
		if (Out->Values.Num() == 0) return nullptr;
		return TSharedPtr<FJsonObject>(Out);
	}

	void ReadCategory(const TSharedRef<FJsonObject>& Object, UHutongBuildingComponent* Component,
		const TCHAR* Category)
	{
		for (TFieldIterator<FProperty> It(Component->GetClass()); It; ++It)
		{
			FProperty* Prop = *It;
			if (!IsInCategory(Prop, Category)) continue;
			const FString Key = FJsonObjectConverter::StandardizeCase(Prop->GetAuthoredName());
			const TSharedPtr<FJsonValue> Value = Object->TryGetField(Key);
			if (!Value.IsValid()) continue;
			void* Target = Prop->ContainerPtrToValuePtr<void>(Component);
			FJsonObjectConverter::JsonValueToUProperty(Value, Prop, Target, CheckFlags, SkipFlags);
		}
	}

	// --- the delta ---
	// A record carries what a building was *changed to*, not what it is: the reference is a fresh
	// component of the same class carrying the same preset, and everything equal to it is dropped.
	// A file of a hundred buildings was otherwise a hundred copies of the shipped defaults, and
	// re-importing one pinned every field to whatever the canon said on the day it was written.
	bool JsonEquals(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		if (!A.IsValid() || !B.IsValid()) return A.IsValid() == B.IsValid();
		if (A->Type != B->Type) return false;
		switch (A->Type)
		{
		case EJson::Null:   return true;
		case EJson::Boolean: return A->AsBool() == B->AsBool();
		case EJson::Number:  return A->AsNumber() == B->AsNumber();
		case EJson::String:  return A->AsString() == B->AsString();
		case EJson::Array:
		{
			const TArray<TSharedPtr<FJsonValue>>& X = A->AsArray();
			const TArray<TSharedPtr<FJsonValue>>& Y = B->AsArray();
			if (X.Num() != Y.Num()) return false;
			for (int32 i = 0; i < X.Num(); ++i)
			{
				if (!JsonEquals(X[i], Y[i])) return false;
			}
			return true;
		}
		case EJson::Object:
		{
			const TSharedPtr<FJsonObject> X = A->AsObject();
			const TSharedPtr<FJsonObject> Y = B->AsObject();
			if (!X.IsValid() || !Y.IsValid()) return X.IsValid() == Y.IsValid();
			if (X->Values.Num() != Y->Values.Num()) return false;
			for (const auto& Field : X->Values)
			{
				if (!JsonEquals(Field.Value, Y->TryGetField(Field.Key))) return false;
			}
			return true;
		}
		default: return false;
		}
	}

	// Recurses into nested objects, so a params struct comes out as the two fields that were
	// touched rather than as all of it or none of it.
	void PruneEqual(const TSharedRef<FJsonObject>& Object, const TSharedRef<FJsonObject>& Reference)
	{
		TArray<FString> Keys;
		Keys.Reserve(Object->Values.Num());
		for (const auto& Field : Object->Values) Keys.Emplace(Field.Key);

		for (const FString& Key : Keys)
		{
			const TSharedPtr<FJsonValue> Mine = Object->TryGetField(Key);
			const TSharedPtr<FJsonValue> Theirs = Reference->TryGetField(Key);
			// A field the reference has no answer for is a field that has to travel.
			if (!Mine.IsValid() || !Theirs.IsValid()) continue;

			if (Mine->Type == EJson::Object && Theirs->Type == EJson::Object)
			{
				const TSharedPtr<FJsonObject> Inner = Mine->AsObject();
				const TSharedPtr<FJsonObject> Ref = Theirs->AsObject();
				if (Inner.IsValid() && Ref.IsValid())
				{
					PruneEqual(Inner.ToSharedRef(), Ref.ToSharedRef());
					if (Inner->Values.Num() == 0) Object->RemoveField(Key);
					continue;
				}
			}
			if (JsonEquals(Mine, Theirs)) Object->RemoveField(Key);
		}
	}

	// Case-insensitively, because a TMap<FString, ...> hashes that way and the file's own casing is cosmetic.
	void KeepOwnFieldsOnly(const TSharedRef<FJsonObject>& Object, const UClass* Class)
	{
		TSet<FString> Keep;
		GatherOwnKeys(Class, Keep);

		// The map's key type is the engine's, and 5.8 changed it out from under this; take the keys as FString.
		TArray<FString> Present;
		Present.Reserve(Object->Values.Num());
		for (const auto& Field : Object->Values)
		{
			Present.Emplace(Field.Key);
		}
		for (const FString& Key : Present)
		{
			if (!Keep.Contains(Key)) Object->RemoveField(Key);
		}
	}
}

FText FResult::Summarise() const
{
	if (!bSucceeded)
	{
		return Problems.Num() > 0
			? FText::FromString(Problems[0])
			: LOCTEXT("ExchangeFailed", "Failed.");
	}

	FString Line;
	if (Exported > 0)
	{
		Line = FString::Printf(TEXT("Exported %d building(s)."), Exported);
	}
	else
	{
		Line = FString::Printf(TEXT("%d placed, %d updated, %d skipped."), Created, Updated, Skipped);
	}
	if (bFromLayoutOnly)
	{
		Line += TEXT(" The file was layout only, so each building was rebuilt from its type's"
			" current defaults.");
	}
	if (Problems.Num() > 0)
	{
		Line += FString::Printf(TEXT(" %d problem(s) — see the Output Log."), Problems.Num());
	}
	return FText::FromString(Line);
}

void GatherBuildingComponentClasses(TMap<FName, UClass*>& OutClasses)
{
	OutClasses.Reset();
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		if (!Class->IsChildOf(UHutongBuildingComponent::StaticClass())) continue;
		if (Class == UHutongBuildingComponent::StaticClass()) continue;
		if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) continue;
		OutClasses.Add(Class->GetFName(), Class);
	}
}

void FindUnknownTypes(const FSceneFile& File, TMap<FName, int32>& OutCounts)
{
	OutCounts.Reset();

	TMap<FName, UClass*> Classes;
	GatherBuildingComponentClasses(Classes);
	for (const FRecord& R : File.Records)
	{
		if (Classes.Contains(R.ClassName)) continue;
		++OutCounts.FindOrAdd(R.ClassName);
	}
}

TSharedPtr<FJsonObject> WriteComponent(const UHutongBuildingComponent* Component)
{
	if (!Component) return nullptr;

	TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
	// A UClass is a UStruct and the converter takes the object as its own container — a supported path, not an accident.
	if (!FJsonObjectConverter::UStructToJsonObject(Component->GetClass(),
		Component, Out, CheckFlags, SkipFlags))
	{
		return nullptr;
	}
	KeepOwnFieldsOnly(Out, Component->GetClass());

	// Measured against a fresh one of the same type carrying the same preset, so what is written
	// is what somebody decided. Everything else follows the shipped defaults on the way back in,
	// which is what makes a re-import pick up a canon that has moved.
	if (UHutongBuildingComponent* Reference = NewObject<UHutongBuildingComponent>(
		GetTransientPackage(), Component->GetClass(), NAME_None, RF_Transient))
	{
		if (!Component->Preset.IsEmpty()) Reference->ApplyPresetParams(Component->Preset);

		TSharedRef<FJsonObject> RefBlob = MakeShared<FJsonObject>();
		if (FJsonObjectConverter::UStructToJsonObject(Reference->GetClass(), Reference, RefBlob,
			CheckFlags, SkipFlags))
		{
			KeepOwnFieldsOnly(RefBlob, Reference->GetClass());
			// Identity is not a parameter: the id is what a Sync import matches on, and a building
			// whose every field happens to sit at its type's default still has to be findable.
			RefBlob->RemoveField(FJsonObjectConverter::StandardizeCase(
				GET_MEMBER_NAME_STRING_CHECKED(UHutongBuildingComponent, BuildingId)));
			PruneEqual(Out, RefBlob);
		}
	}
	return Out;
}

bool ReadComponent(const TSharedRef<FJsonObject>& Blob, UHutongBuildingComponent* Component,
	FString& OutProblem)
{
	if (!Component)
	{
		OutProblem = TEXT("no component to read into");
		return false;
	}

	// Filtered on the way in as well as out.
	TSharedRef<FJsonObject> Filtered = MakeShared<FJsonObject>(*Blob);
	KeepOwnFieldsOnly(Filtered, Component->GetClass());

	// The preset first: the record holds the differences from it, not the whole of the building.
	// A preset that is not in this project is not fatal — the differences land on the shipped
	// defaults instead — but it is not silent either.
	FString PresetName;
	if (Filtered->TryGetStringField(TEXT("preset"), PresetName) && !PresetName.IsEmpty())
	{
		if (!Component->ApplyPresetParams(PresetName))
		{
			OutProblem = FString::Printf(
				TEXT("preset '%s' is not in this project; the recorded changes were applied to the "
					 "shipped defaults instead"), *PresetName);
		}
	}

	FText FailReason;
	// bStrictMode stays off: strict fails the whole record on a key with no matching property.
	const bool bOk = FJsonObjectConverter::JsonObjectToUStruct(Filtered, Component->GetClass(),
		Component, CheckFlags, SkipFlags, /*bStrictMode*/ false, &FailReason);
	if (!bOk)
	{
		OutProblem = FailReason.ToString();
	}
	return bOk;
}

void FootprintCornersInSetFrame(const FRecord& Record, FVector2D OutCorners[4])
{
	FVector2D Local[4];
	HutongFootprint::Corners(Record.Footprint, Record.Skew, Local);
	for (int32 i = 0; i < 4; ++i)
	{
		OutCorners[i] = Record.Offset + RotateXY(Local[i], Record.RelativeYawDeg);
	}
}

FVector2D NormaliseToBounds(TArray<FRecord>& Records, FVector2D& OutSize)
{
	OutSize = FVector2D::ZeroVector;
	if (Records.Num() == 0) return FVector2D::ZeroVector;

	FVector2D Min(TNumericLimits<double>::Max(), TNumericLimits<double>::Max());
	FVector2D Max(-TNumericLimits<double>::Max(), -TNumericLimits<double>::Max());

	for (const FRecord& R : Records)
	{
		FVector2D Corners[4];
		FootprintCornersInSetFrame(R, Corners);
		for (const FVector2D& C : Corners)
		{
			Min.X = FMath::Min(Min.X, C.X); Min.Y = FMath::Min(Min.Y, C.Y);
			Max.X = FMath::Max(Max.X, C.X); Max.Y = FMath::Max(Max.Y, C.Y);
		}
	}

	for (FRecord& R : Records)
	{
		R.Offset -= Min;
	}
	OutSize = Max - Min;
	return Min;
}

FTransform ComposeRecordTransform(const FRecord& Record, const FTransform& SetToWorld)
{
	const FVector Local(Record.Offset.X, Record.Offset.Y, Record.OffsetZ);
	const FVector Loc = SetToWorld.TransformPosition(Local);
	const double Yaw = SetToWorld.Rotator().Yaw + Record.RelativeYawDeg;
	return FTransform(FRotator(0.0, Yaw, 0.0), Loc);
}

bool Gather(const TArray<UHutongBuildingComponent*>& Buildings, UWorld* World,
	FSceneFile& OutFile, FResult& OutResult, bool bLayoutOnly)
{
	OutFile = FSceneFile();
	OutFile.LevelName = World ? World->GetMapName() : FString();
	OutFile.bLayoutOnly = bLayoutOnly;

	TArray<UHutongBuildingComponent*> Work;
	Work.Reserve(Buildings.Num());
	for (UHutongBuildingComponent* B : Buildings)
	{
		if (B && B->GetOwner()) Work.Add(B);
	}
	if (Work.Num() == 0)
	{
		OutResult.Problems.Add(TEXT("No Hutong buildings to export."));
		return false;
	}

	// The set frame takes the first building's own yaw.
	const FTransform First = Work[0]->GetOwner()->GetActorTransform();
	OutFile.SetYawDeg = First.Rotator().Yaw;
	const FVector Anchor = First.GetLocation();
	const FRotator SetRot(0.0, OutFile.SetYawDeg, 0.0);

	for (UHutongBuildingComponent* B : Work)
	{
		AActor* Actor = B->GetOwner();

		// A placement made before the id field existed has none.
		if (B->EnsureBuildingId())
		{
			B->Modify();
		}

		// A layout-only export writes no parameters at all; the type and the footprint are the record.
		TSharedPtr<FJsonObject> Blob;
		if (!bLayoutOnly)
		{
			Blob = WriteComponent(B);
			if (!Blob.IsValid())
			{
				OutResult.Problems.Add(FString::Printf(TEXT("Could not serialise %s; skipped."),
					*Actor->GetActorNameOrLabel()));
				++OutResult.Skipped;
				continue;
			}
		}

		FRecord Rec;
		Rec.Id = B->BuildingId;
		Rec.ClassName = B->GetClass()->GetFName();
		Rec.Label = Actor->GetActorLabel();
		Rec.Folder = Actor->GetFolderPath();
		Rec.Footprint = B->GetFootprintSize();
		Rec.Skew = B->FootprintSkew;
		Rec.bHasFacing = B->GetFacade(Rec.Facing);
		Rec.bRunAlongY = B->IsRunAlongY();
		Rec.Variant = B->GetTypeVariant();
		if (bLayoutOnly) Rec.FootprintFields = WriteCategory(B, TEXT("Footprint"));
		Rec.Detail = B->DetailLevel;
		Rec.bPlanOnly = B->bPlanOnly;
		Rec.Blob = Blob;

		const FTransform Xf = Actor->GetActorTransform();
		const FVector Rel = SetRot.UnrotateVector(Xf.GetLocation() - Anchor);
		Rec.Offset = FVector2D(Rel.X, Rel.Y);
		Rec.OffsetZ = Rel.Z;
		Rec.RelativeYawDeg = FRotator::NormalizeAxis(Xf.Rotator().Yaw - OutFile.SetYawDeg);

		OutFile.Records.Add(MoveTemp(Rec));
	}

	if (OutFile.Records.Num() == 0)
	{
		OutResult.Problems.Add(TEXT("Nothing could be serialised."));
		return false;
	}

	// After this the set frame's origin is the bounding rectangle's min corner.
	const FVector2D Shift = NormaliseToBounds(OutFile.Records, OutFile.BoundsSize);
	OutFile.SetOriginWorld = Anchor + SetRot.RotateVector(FVector(Shift.X, Shift.Y, 0.0));

	OutResult.Exported = OutFile.Records.Num();
	return true;
}

bool Write(const FSceneFile& File, const FString& FilePath, FResult& OutResult)
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("hutongSceneVersion"), FormatVersion);
	Root->SetStringField(TEXT("generator"), TEXT("HutongLayout"));
	Root->SetStringField(TEXT("coordinateSystem"), TEXT("unreal-world-centimetres"));
	Root->SetStringField(TEXT("level"), File.LevelName);
	Root->SetStringField(TEXT("exportedAt"), FDateTime::UtcNow().ToIso8601());
	// Stated in the file, because a reader has to know whether the absent parameters are missing
	// or deliberately absent.
	Root->SetBoolField(TEXT("layoutOnly"), File.bLayoutOnly);

	TSharedRef<FJsonObject> Set = MakeShared<FJsonObject>();
	Set->SetObjectField(TEXT("originWorld"), Vec3(File.SetOriginWorld));
	Set->SetNumberField(TEXT("yawDeg"), File.SetYawDeg);
	Set->SetObjectField(TEXT("size"), Vec2(File.BoundsSize));
	Root->SetObjectField(TEXT("set"), Set);

	TArray<TSharedPtr<FJsonValue>> Buildings;
	Buildings.Reserve(File.Records.Num());
	for (const FRecord& R : File.Records)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("id"), R.Id.ToString(EGuidFormats::DigitsWithHyphens));
		O->SetStringField(TEXT("class"), R.ClassName.ToString());
		O->SetStringField(TEXT("label"), R.Label);
		O->SetStringField(TEXT("folder"), R.Folder.IsNone() ? FString() : R.Folder.ToString());

		TSharedRef<FJsonObject> Off = MakeShared<FJsonObject>();
		Off->SetNumberField(TEXT("x"), R.Offset.X);
		Off->SetNumberField(TEXT("y"), R.Offset.Y);
		Off->SetNumberField(TEXT("z"), R.OffsetZ);
		O->SetObjectField(TEXT("offset"), Off);

		O->SetNumberField(TEXT("relativeYawDeg"), R.RelativeYawDeg);
		O->SetObjectField(TEXT("footprint"), Vec2(R.Footprint));
		if (!R.Skew.IsZero())
		{
			TArray<TSharedPtr<FJsonValue>> Offsets;
			for (int32 i = 0; i < 4; ++i) Offsets.Add(MakeShared<FJsonValueObject>(Vec2(R.Skew.Get(i))));
			O->SetArrayField(TEXT("cornerOffsets"), Offsets);
			O->SetStringField(TEXT("skewMode"),
				StaticEnum<EHutongSkewMode>()->GetNameStringByValue((int64)R.Skew.Mode));
		}

		// Only where there is no blob to carry them. All four are CPF_Edit UPROPERTYs, so a full
		// record already has them in `component`, and writing them twice would be two authorities
		// for one field with a precedence question between them. Written as names rather than
		// numbers: a layout file is meant to be read.
		if (R.Blob.IsValid())
		{
			O->SetObjectField(TEXT("component"), R.Blob);
		}
		else
		{
			if (R.bHasFacing)
			{
				O->SetStringField(TEXT("facing"),
					StaticEnum<EHutongBaySide>()->GetNameStringByValue((int64)R.Facing));
			}
			O->SetStringField(TEXT("detail"),
				StaticEnum<EHutongDetail>()->GetNameStringByValue((int64)R.Detail));
			O->SetBoolField(TEXT("planOnly"), R.bPlanOnly);
			// The footprint is two numbers and says nothing about which of them is the run.
			O->SetBoolField(TEXT("runAlongY"), R.bRunAlongY);
			// The kind within the class, where the class carries more than one kind.
			if (!R.Variant.IsNone()) O->SetStringField(TEXT("variant"), R.Variant.ToString());
			if (R.FootprintFields.IsValid())
			{
				O->SetObjectField(TEXT("footprintFields"), R.FootprintFields);
			}
		}

		Buildings.Add(MakeShared<FJsonValueObject>(O));
	}
	Root->SetArrayField(TEXT("buildings"), Buildings);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		OutResult.Problems.Add(TEXT("Could not serialise the scene."));
		return false;
	}

	// No BOM: RFC 8259 says JSON text is UTF-8 and a byte order mark is not part of it.
	if (!FFileHelper::SaveStringToFile(Output, *FilePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutResult.Problems.Add(FString::Printf(TEXT("Could not write %s."), *FilePath));
		return false;
	}
	return true;
}

bool Read(const FString& FilePath, FSceneFile& OutFile, FResult& OutResult)
{
	OutFile = FSceneFile();

	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FilePath))
	{
		OutResult.Problems.Add(FString::Printf(TEXT("Could not read %s."), *FilePath));
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutResult.Problems.Add(FString::Printf(TEXT("%s is not valid JSON."), *FilePath));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Buildings = nullptr;
	if (!Root->TryGetArrayField(TEXT("buildings"), Buildings) || !Buildings)
	{
		OutResult.Problems.Add(FString::Printf(TEXT("%s has no buildings array."), *FilePath));
		return false;
	}

	double Version = FormatVersion;
	Root->TryGetNumberField(TEXT("hutongSceneVersion"), Version);
	OutFile.Version = (int32)Version;
	if (OutFile.Version > FormatVersion)
	{
		// Read it anyway — the format only grows — but say so.
		OutResult.Problems.Add(FString::Printf(
			TEXT("%s was written by a newer version (%d against %d); unknown fields will be lost."),
			*FilePath, OutFile.Version, FormatVersion));
	}
	Root->TryGetStringField(TEXT("level"), OutFile.LevelName);
	Root->TryGetBoolField(TEXT("layoutOnly"), OutFile.bLayoutOnly);

	const TSharedPtr<FJsonObject>* Set = nullptr;
	if (Root->TryGetObjectField(TEXT("set"), Set) && Set)
	{
		const TSharedPtr<FJsonObject>* Origin = nullptr;
		if ((*Set)->TryGetObjectField(TEXT("originWorld"), Origin) && Origin)
		{
			OutFile.SetOriginWorld = ReadVec3(*Origin);
		}
		(*Set)->TryGetNumberField(TEXT("yawDeg"), OutFile.SetYawDeg);
	}

	for (const TSharedPtr<FJsonValue>& Value : *Buildings)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Obj) || !Obj)
		{
			OutResult.Problems.Add(TEXT("A building entry was not an object; skipped."));
			++OutResult.Skipped;
			continue;
		}

		FRecord Rec;

		FString IdText;
		if ((*Obj)->TryGetStringField(TEXT("id"), IdText))
		{
			FGuid::Parse(IdText, Rec.Id);
		}

		FString ClassText;
		if (!(*Obj)->TryGetStringField(TEXT("class"), ClassText) || ClassText.IsEmpty())
		{
			OutResult.Problems.Add(TEXT("A building entry names no class; skipped."));
			++OutResult.Skipped;
			continue;
		}
		Rec.ClassName = FName(*ClassText);

		(*Obj)->TryGetStringField(TEXT("label"), Rec.Label);
		FString FolderText;
		if ((*Obj)->TryGetStringField(TEXT("folder"), FolderText) && !FolderText.IsEmpty())
		{
			Rec.Folder = FName(*FolderText);
		}

		const TSharedPtr<FJsonObject>* Off = nullptr;
		if ((*Obj)->TryGetObjectField(TEXT("offset"), Off) && Off)
		{
			Rec.Offset = ReadVec2(*Off);
			(*Off)->TryGetNumberField(TEXT("z"), Rec.OffsetZ);
		}
		(*Obj)->TryGetNumberField(TEXT("relativeYawDeg"), Rec.RelativeYawDeg);

		const TSharedPtr<FJsonObject>* Foot = nullptr;
		if ((*Obj)->TryGetObjectField(TEXT("footprint"), Foot) && Foot)
		{
			Rec.Footprint = ReadVec2(*Foot);
		}
		const TArray<TSharedPtr<FJsonValue>>* Offsets = nullptr;
		if ((*Obj)->TryGetArrayField(TEXT("cornerOffsets"), Offsets) && Offsets && Offsets->Num() == 4)
		{
			for (int32 i = 0; i < 4; ++i)
			{
				const TSharedPtr<FJsonObject>* Corner = nullptr;
				if ((*Offsets)[i].IsValid() && (*Offsets)[i]->TryGetObject(Corner) && Corner)
				{
					Rec.Skew.Set(i, ReadVec2(*Corner));
				}
			}
			FString ModeName;
			if ((*Obj)->TryGetStringField(TEXT("skewMode"), ModeName))
			{
				const int64 ModeValue = StaticEnum<EHutongSkewMode>()->GetValueByNameString(ModeName);
				if (ModeValue != INDEX_NONE) Rec.Skew.Mode = (EHutongSkewMode)ModeValue;
			}
		}

		FString FacingName;
		if ((*Obj)->TryGetStringField(TEXT("facing"), FacingName))
		{
			const int64 FacingValue = StaticEnum<EHutongBaySide>()->GetValueByNameString(FacingName);
			if (FacingValue != INDEX_NONE)
			{
				Rec.Facing = (EHutongBaySide)FacingValue;
				Rec.bHasFacing = true;
			}
		}
		FString DetailName;
		if ((*Obj)->TryGetStringField(TEXT("detail"), DetailName))
		{
			const int64 DetailValue = StaticEnum<EHutongDetail>()->GetValueByNameString(DetailName);
			if (DetailValue != INDEX_NONE) Rec.Detail = (EHutongDetail)DetailValue;
		}
		(*Obj)->TryGetBoolField(TEXT("planOnly"), Rec.bPlanOnly);
		(*Obj)->TryGetBoolField(TEXT("runAlongY"), Rec.bRunAlongY);
		FString VariantText;
		if ((*Obj)->TryGetStringField(TEXT("variant"), VariantText) && !VariantText.IsEmpty())
		{
			Rec.Variant = FName(*VariantText);
		}
		const TSharedPtr<FJsonObject>* Fields = nullptr;
		if ((*Obj)->TryGetObjectField(TEXT("footprintFields"), Fields) && Fields)
		{
			Rec.FootprintFields = *Fields;
		}

		const TSharedPtr<FJsonObject>* Blob = nullptr;
		if ((*Obj)->TryGetObjectField(TEXT("component"), Blob) && Blob)
		{
			Rec.Blob = *Blob;
		}
		else if (!OutFile.bLayoutOnly)
		{
			OutResult.Problems.Add(FString::Printf(
				TEXT("%s carries no component data; skipped."),
				Rec.Label.IsEmpty() ? *ClassText : *Rec.Label));
			++OutResult.Skipped;
			continue;
		}

		OutFile.Records.Add(MoveTemp(Rec));
	}

	if (OutFile.Records.Num() == 0)
	{
		OutResult.Problems.Add(FString::Printf(TEXT("%s described no usable buildings."), *FilePath));
		return false;
	}

	// The file states the extent, but the records are the authority.
	FVector2D Size;
	const FVector2D Shift = NormaliseToBounds(OutFile.Records, Size);
	OutFile.BoundsSize = Size;
	const FRotator SetRot(0.0, OutFile.SetYawDeg, 0.0);
	OutFile.SetOriginWorld += SetRot.RotateVector(FVector(Shift.X, Shift.Y, 0.0));

	return true;
}

namespace
{
	// Every building in the world, keyed by its id, so a Sync import can find what it is updating.
	void MapExistingById(UWorld* World, TMap<FGuid, UHutongBuildingComponent*>& Out)
	{
		Out.Reset();
		for (UHutongBuildingComponent* B : HutongDetailOps::CollectLoaded(World))
		{
			if (B && B->BuildingId.IsValid()) Out.Add(B->BuildingId, B);
		}
	}

	// What a record says about the placement rather than about the building: where it stands, how
	// big it is, which way it faces and what it is built at. On a layout-only record this is the
	// whole of it, and the parameters stay whatever the component already has — the type's current
	// defaults on a fresh one, and what the user has tuned on a Sync match.
	void ApplyLayout(const FRecord& Record, UHutongBuildingComponent* Component)
	{
		if (!Component) return;
		// The kind first: a 隔牆 read back as a 院牆 is a different height, thickness and cap, and
		// its footprint means something else.
		if (!Record.Variant.IsNone()) Component->SetTypeVariant(Record.Variant);
		// Then the axis: a line-like piece reads its length off whichever of the two extents is
		// the run, so a footprint applied before this is a wall as long as it is thick.
		Component->SetRunAlongY(Record.bRunAlongY);
		// The placement's own fields verbatim where the file has them, which is everything the
		// footprint is made of rather than the two numbers it comes out as.
		if (Record.FootprintFields.IsValid())
		{
			ReadCategory(Record.FootprintFields.ToSharedRef(), Component, TEXT("Footprint"));
		}
		else if (Record.Footprint.X > 1.0 && Record.Footprint.Y > 1.0)
		{
			Component->SetFootprintSize(Record.Footprint);
		}
		if (Record.bHasFacing) Component->SetFacade(Record.Facing);
		Component->DetailLevel = Record.Detail;
		Component->bPlanOnly = Record.bPlanOnly;
	}

	// Everything a record says about a component: its parameters when it has them, its layout when
	// it does not. One path, so the fresh placement and the Sync update cannot disagree.
	bool Hydrate(const FRecord& Record, UHutongBuildingComponent* Component, FString& OutProblem,
		bool bForceLayout = false)
	{
		if (!Component)
		{
			OutProblem = TEXT("nothing to apply");
			return false;
		}
		if (Record.Blob.IsValid())
		{
			const bool bOk = ReadComponent(Record.Blob.ToSharedRef(), Component, OutProblem);
			// A record built for another type: only the fields both classes happen to share came
			// out of the blob, so the placement's own facts are applied on top of it.
			if (bOk && bForceLayout) ApplyLayout(Record, Component);
			return bOk;
		}
		ApplyLayout(Record, Component);
		return true;
	}

	// The same, plus the identity and the place the record puts the actor.
	bool ApplyRecord(const FRecord& Record, UHutongBuildingComponent* Component,
		const FTransform& Xform, const FGuid& Id, FString& OutProblem, bool bForceLayout = false)
	{
		if (!Hydrate(Record, Component, OutProblem, bForceLayout))
		{
			return false;
		}
		Component->BuildingId = Id;
		if (AActor* Actor = Component->GetOwner())
		{
			Actor->SetActorTransform(Xform);
		}
		return true;
	}
}

void Place(UWorld* World, const FSceneFile& File, const FTransform& SetToWorld,
	EMode Mode, FName FolderOverride, FResult& OutResult)
{
	if (!World)
	{
		OutResult.Problems.Add(TEXT("No world to place into."));
		return;
	}

	TMap<FName, UClass*> Classes;
	GatherBuildingComponentClasses(Classes);

	TMap<FGuid, UHutongBuildingComponent*> Existing;
	if (Mode == EMode::Sync)
	{
		MapExistingById(World, Existing);
	}

	ULevel* Level = World->GetCurrentLevel();
	if (!Level)
	{
		OutResult.Problems.Add(TEXT("The world has no current level."));
		return;
	}
	Level->Modify();

	FScopedSlowTask Task((float)File.Records.Num(),
		LOCTEXT("PlacingScene", "Placing the imported buildings…"));
	Task.MakeDialog();

	for (const FRecord& Record : File.Records)
	{
		const FString Name = Record.Label.IsEmpty() ? Record.ClassName.ToString() : Record.Label;
		Task.EnterProgressFrame(1.0f, FText::FromString(Name));

		UClass* const* Found = Classes.Find(Record.ClassName);

		// A type this build does not have. The file may predate a rename or a split — 院牆 and 隔牆
		// were one class once — so what to build instead is a question the caller has already put
		// to the user, once per unknown type rather than once per record.
		bool bRemapped = false;
		if (!Found || !*Found)
		{
			if (const FName* To = File.TypeRemap.Find(Record.ClassName))
			{
				if (To->IsNone())
				{
					// Deliberately dropped, so it is not a problem to report.
					++OutResult.Skipped;
					continue;
				}
				Found = Classes.Find(*To);
				bRemapped = (Found && *Found);
			}
		}
		if (!Found || !*Found)
		{
			OutResult.Problems.Add(FString::Printf(
				TEXT("%s: no building type called %s in this build; skipped."),
				*Name, *Record.ClassName.ToString()));
			++OutResult.Skipped;
			continue;
		}
		UClass* Class = *Found;

		FGuid Id = (Mode == EMode::Sync && Record.Id.IsValid())
			? Record.Id : FGuid::NewGuid();
		const FTransform Xform = ComposeRecordTransform(Record, SetToWorld);

		// --- the building is already here: reshape it rather than adding a second one ---
		if (Mode == EMode::Sync)
		{
			if (UHutongBuildingComponent** Hit = Existing.Find(Id))
			{
				UHutongBuildingComponent* B = *Hit;
				if (B && B->GetClass() == Class)
				{
					AActor* Actor = B->GetOwner();
					B->Modify();
					if (Actor) Actor->Modify();

					FString Problem;
					if (!ApplyRecord(Record, B, Xform, Id, Problem, bRemapped))
					{
						OutResult.Problems.Add(FString::Printf(TEXT("%s: %s; left as it was."),
							*Name, *Problem));
						++OutResult.Skipped;
						continue;
					}
					// Read, but with something worth saying about it.
					if (!Problem.IsEmpty())
					{
						OutResult.Problems.Add(FString::Printf(TEXT("%s: %s."), *Name, *Problem));
					}
					B->Rebuild();
					B->ApplyPlacementAttachments();
					if (Actor) OutResult.Placed.Add(Actor);
					++OutResult.Updated;
					continue;
				}
				// Same id, different type: the new building takes a fresh one. Left with the
				// record's, the level holds two components under one guid, and the next Sync keeps
				// whichever MapExistingById added last — actor-iteration order — so it can reshape
				// and teleport the unrelated older building.
				Id = FGuid::NewGuid();
				OutResult.Problems.Add(FString::Printf(
					TEXT("%s: a different type already carries this id; placed as a new building "
						 "under a new id."),
					*Name));
			}
		}

		// --- a fresh placement.
		UHutongBuildingComponent* Template = NewObject<UHutongBuildingComponent>(
			GetTransientPackage(), Class, NAME_None, RF_Transient);
		// A layout-only record builds from the type's shipped defaults at the recorded footprint.
		FString Problem;
		if (!Hydrate(Record, Template, Problem, bRemapped))
		{
			OutResult.Problems.Add(FString::Printf(TEXT("%s: %s; skipped."), *Name,
				Problem.IsEmpty() ? TEXT("could not read the component data") : *Problem));
			++OutResult.Skipped;
			continue;
		}

		TArray<FDynamicMesh3> LODs;
		Template->BuildLODs(LODs);
		if (!Template->bPlanOnly && (LODs.Num() == 0 || LODs[0].TriangleCount() == 0))
		{
			OutResult.Problems.Add(FString::Printf(
				TEXT("%s: built no geometry from its parameters; skipped."), *Name));
			++OutResult.Skipped;
			continue;
		}

		const FString SpawnName = bRemapped ? Class->GetName() : Record.ClassName.ToString();
		AStaticMeshActor* Actor = Template->bPlanOnly
			? HutongGen::SpawnEmptyActor(World, Xform, SpawnName)
			: HutongGen::SpawnStaticMeshActor(World, LODs, Xform, SpawnName, Template->Palette);
		if (!Actor)
		{
			OutResult.Problems.Add(FString::Printf(TEXT("%s: could not spawn an actor; skipped."), *Name));
			++OutResult.Skipped;
			continue;
		}

		if (!Record.Label.IsEmpty()) Actor->SetActorLabel(Record.Label);
		const FName Folder = FolderOverride.IsNone() ? Record.Folder : FolderOverride;
		if (!Folder.IsNone()) Actor->SetFolderPath(Folder);

		UHutongBuildingComponent* Building = NewObject<UHutongBuildingComponent>(
			Actor, Class, NAME_None, RF_Transactional);
		if (!Building || !ApplyRecord(Record, Building, Xform, Id, Problem, bRemapped))
		{
			OutResult.Problems.Add(FString::Printf(TEXT("%s: %s; placed without its parameters."),
				*Name, *Problem));
			++OutResult.Skipped;
			continue;
		}

		if (!Problem.IsEmpty())
		{
			OutResult.Problems.Add(FString::Printf(TEXT("%s: %s."), *Name, *Problem));
		}

		// AddInstanceComponent as well as RegisterComponent, or the component is invisible in the Details panel and is not saved with the actor.
		Actor->AddInstanceComponent(Building);
		Building->RegisterComponent();
		// What a placement hangs on the actor besides the mesh — the plan outline.
		Building->ApplyPlacementAttachments();

		OutResult.Placed.Add(Actor);
		++OutResult.Created;
	}

	// A whole street just arrived or moved; the snap cache has to see it.
	HutongSnap::Invalidate();
	OutResult.bFromLayoutOnly = File.bLayoutOnly;
	OutResult.bSucceeded = true;
}

namespace
{
	void ExportThese(const TArray<UHutongBuildingComponent*>& Buildings, UWorld* World,
		const FString& FilePath, FResult& OutResult, bool bLayoutOnly)
	{
		OutResult = FResult();

		// Gather may mint ids on placements that predate the field, which is a change to the level.
		const FScopedTransaction Transaction(LOCTEXT("ExportScene", "Export Hutong Scene"));

		FSceneFile File;
		if (!Gather(Buildings, World, File, OutResult, bLayoutOnly)) return;
		if (!Write(File, FilePath, OutResult)) return;

		OutResult.bSucceeded = true;
	}
}

void ExportLoaded(UWorld* World, const FString& FilePath, FResult& OutResult, bool bLayoutOnly)
{
	ExportThese(HutongDetailOps::CollectLoaded(World), World, FilePath, OutResult, bLayoutOnly);
}

void ExportSelection(UWorld* World, const FString& FilePath, FResult& OutResult, bool bLayoutOnly)
{
	ExportThese(HutongDetailOps::CollectSelected(), World, FilePath, OutResult, bLayoutOnly);
}

void ImportAtRecordedTransforms(UWorld* World, const FString& FilePath, EMode Mode,
	FName FolderOverride, FResult& OutResult, const FResolveFile& Resolve)
{
	OutResult = FResult();

	FSceneFile File;
	if (!Read(FilePath, File, OutResult)) return;

	// Between the read and the placement: what the file names that this build cannot build.
	if (Resolve && !Resolve(File))
	{
		OutResult.Problems.Add(TEXT("Import cancelled."));
		return;
	}

	// Straight back where it came from: the set frame's own recorded place in the world.
	const FTransform SetToWorld(FRotator(0.0, File.SetYawDeg, 0.0), File.SetOriginWorld);

	const FScopedTransaction Transaction(LOCTEXT("ImportScene", "Import Hutong Scene"));
	Place(World, File, SetToWorld, Mode, FolderOverride, OutResult);
}

} // namespace HutongExchange

#undef LOCTEXT_NAMESPACE
