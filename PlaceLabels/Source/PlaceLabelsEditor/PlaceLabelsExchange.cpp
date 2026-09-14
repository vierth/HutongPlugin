#include "PlaceLabelsExchange.h"

#include "PlaceLabelGeometry.h"
#include "PlaceLabelTypes.h"
#include "PlaceRegionActor.h"
#include "PlaceRegionComponent.h"
#include "Tools/PlaceRegionEditCore.h"
#include "Algo/Reverse.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#define LOCTEXT_NAMESPACE "PlaceLabelsExchange"

namespace PlaceLabelsExchange
{

namespace
{
	// Column order for the CSV.
	const TCHAR* CsvHeader =
		TEXT("regionId,actorLabel,typeId,chinese,pinyin,english,note,source,confidence,corners,areaM2");

	FString EscapeCsv(const FString& In)
	{
		// Quote whenever the value could otherwise break the row, and double any quote inside it.
		if (!In.Contains(TEXT(",")) && !In.Contains(TEXT("\"")) && !In.Contains(TEXT("\n"))
			&& !In.Contains(TEXT("\r")))
		{
			return In;
		}
		FString Escaped = In;
		Escaped.ReplaceInline(TEXT("\""), TEXT("\"\""));
		return FString::Printf(TEXT("\"%s\""), *Escaped);
	}

	// Splits one CSV record into fields, honouring quotes and embedded newlines.
	bool ParseCsvRecord(const FString& Text, int32& Cursor, TArray<FString>& OutFields)
	{
		OutFields.Reset();
		if (Cursor >= Text.Len())
		{
			return false;
		}

		FString Field;
		bool bInQuotes = false;

		while (Cursor < Text.Len())
		{
			const TCHAR C = Text[Cursor];

			if (bInQuotes)
			{
				if (C == TEXT('"'))
				{
					// A doubled quote inside a quoted field is a literal quote.
					if (Cursor + 1 < Text.Len() && Text[Cursor + 1] == TEXT('"'))
					{
						Field.AppendChar(TEXT('"'));
						Cursor += 2;
						continue;
					}
					bInQuotes = false;
					++Cursor;
					continue;
				}
				Field.AppendChar(C);
				++Cursor;
				continue;
			}

			if (C == TEXT('"'))
			{
				bInQuotes = true;
				++Cursor;
				continue;
			}
			if (C == TEXT(','))
			{
				OutFields.Add(Field);
				Field.Reset();
				++Cursor;
				continue;
			}
			if (C == TEXT('\r') || C == TEXT('\n'))
			{
				// Swallow CRLF as one terminator.
				if (C == TEXT('\r') && Cursor + 1 < Text.Len() && Text[Cursor + 1] == TEXT('\n'))
				{
					++Cursor;
				}
				++Cursor;
				OutFields.Add(Field);
				return true;
			}

			Field.AppendChar(C);
			++Cursor;
		}

		OutFields.Add(Field);
		return true;
	}

	void GatherRegionComponents(UWorld* World, TArray<UPlaceRegionComponent*>& Out)
	{
		Out.Reset();
		TArray<TWeakObjectPtr<UPlaceRegionComponent>> Weak;
		PlaceLabelsEdit::GatherRegions(World, Weak);
		for (const TWeakObjectPtr<UPlaceRegionComponent>& W : Weak)
		{
			if (UPlaceRegionComponent* Region = W.Get())
			{
				Out.Add(Region);
			}
		}
	}

	FString TextOrEmpty(const FText& In)
	{
		return In.IsEmpty() ? FString() : In.ToString();
	}

	FText FieldToText(const FString& In)
	{
		return In.IsEmpty() ? FText::GetEmpty() : FText::FromString(In);
	}

	// Confidence travels as its own number, 1 to 5. An empty cell leaves the region's answer
	// alone: a spreadsheet is edited a column at a time, and a blank there means "not my column"
	// rather than "no evidence".
	EPlaceConfidence ParseConfidence(const FString& Field, EPlaceConfidence Current)
	{
		const FString Trimmed = Field.TrimStartAndEnd();
		if (Trimmed.IsEmpty() || !Trimmed.IsNumeric())
		{
			return Current;
		}
		const int32 Value = FCString::Atoi(*Trimmed);
		return (Value >= 1 && Value <= 5) ? static_cast<EPlaceConfidence>(Value) : Current;
	}

	// Writes the outline as a GeoJSON linear ring.
	TArray<TSharedPtr<FJsonValue>> MakeRing(const TArray<FVector2D>& World)
	{
		TArray<TSharedPtr<FJsonValue>> Ring;
		Ring.Reserve(World.Num() + 1);

		auto AddPoint = [&Ring](const FVector2D& P)
		{
			TArray<TSharedPtr<FJsonValue>> Pair;
			Pair.Add(MakeShared<FJsonValueNumber>(P.X));
			Pair.Add(MakeShared<FJsonValueNumber>(P.Y));
			Ring.Add(MakeShared<FJsonValueArray>(Pair));
		};

		for (const FVector2D& P : World)
		{
			AddPoint(P);
		}
		if (World.Num() > 0)
		{
			AddPoint(World[0]);
		}
		return Ring;
	}

	UPlaceRegionComponent* FindByRegionId(const TArray<UPlaceRegionComponent*>& Regions,
		const FGuid& Id)
	{
		if (!Id.IsValid())
		{
			return nullptr;
		}
		for (UPlaceRegionComponent* Region : Regions)
		{
			if (Region && Region->RegionId == Id)
			{
				return Region;
			}
		}
		return nullptr;
	}

	// Applies a world-space outline to a region, re-centring the actor on it.
	void ApplyWorldOutline(UPlaceRegionComponent* Region, const TArray<FVector2D>& World, double Z)
	{
		if (!Region || World.Num() < 3)
		{
			return;
		}

		FVector2D Centre = FVector2D::ZeroVector;
		for (const FVector2D& P : World)
		{
			Centre += P;
		}
		Centre /= static_cast<double>(World.Num());

		AActor* Owner = Region->GetOwner();
		if (Owner)
		{
			Owner->Modify();
			// Actor origin at the outline's centre keeps LocalPoints small, which matters for float precision a long way from the world origin, and gives the gizmo something to grab.
			Owner->SetActorLocation(FVector(Centre.X, Centre.Y, Z));
		}

		Region->Modify();
		Region->LocalPoints.Reset(World.Num());

		const FTransform& Xf = Region->GetComponentTransform();
		for (const FVector2D& P : World)
		{
			const FVector Local = Xf.InverseTransformPosition(FVector(P.X, P.Y, Z));
			Region->LocalPoints.Emplace(Local.X, Local.Y);
		}

		// Normalise winding on the way in.
		if (PlaceLabelsGeo::SignedArea2D(Region->LocalPoints) < 0.0)
		{
			Algo::Reverse(Region->LocalPoints);
		}

		Region->RebuildCache();
		Region->UpdateBounds();
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
		Line = FString::Printf(TEXT("Exported %d region(s)."), Exported);
	}
	else
	{
		Line = FString::Printf(TEXT("%d created, %d updated, %d skipped."), Created, Updated, Skipped);
	}
	if (Problems.Num() > 0)
	{
		Line += FString::Printf(TEXT(" %d problem(s) — see the Output Log."), Problems.Num());
	}
	return FText::FromString(Line);
}

void GatherTypeAssets(TMap<FName, UPlaceLabelTypeAsset*>& OutTypes)
{
	OutTypes.Reset();

	const FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	FARFilter Filter;
	Filter.ClassPaths.Add(UPlaceLabelTypeAsset::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;

	TArray<FAssetData> Assets;
	AssetRegistryModule.Get().GetAssets(Filter, Assets);

	for (const FAssetData& Asset : Assets)
	{
		if (UPlaceLabelTypeAsset* Type = Cast<UPlaceLabelTypeAsset>(Asset.GetAsset()))
		{
			if (!Type->TypeId.IsNone())
			{
				// First wins.
				OutTypes.FindOrAdd(Type->TypeId, Type);
			}
		}
	}
}

void ExportGeoJson(UWorld* World, const FString& FilePath, FResult& OutResult)
{
	OutResult = FResult();

	TArray<UPlaceRegionComponent*> Regions;
	GatherRegionComponents(World, Regions);

	TArray<TSharedPtr<FJsonValue>> Features;
	Features.Reserve(Regions.Num());

	for (UPlaceRegionComponent* Region : Regions)
	{
		const TArray<FVector2D>& WorldPoints = Region->GetWorldPoints2D();
		if (WorldPoints.Num() < 3)
		{
			OutResult.Problems.Add(FString::Printf(TEXT("%s: fewer than three corners, not exported."),
				Region->GetOwner() ? *Region->GetOwner()->GetActorLabel() : TEXT("<unnamed>")));
			continue;
		}

		// Assign an id to anything drawn before the field existed.
		Region->EnsureRegionId();

		TArray<TSharedPtr<FJsonValue>> Rings;
		Rings.Add(MakeShared<FJsonValueArray>(MakeRing(WorldPoints)));

		TSharedRef<FJsonObject> Geometry = MakeShared<FJsonObject>();
		Geometry->SetStringField(TEXT("type"), TEXT("Polygon"));
		Geometry->SetArrayField(TEXT("coordinates"), Rings);

		TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
		Properties->SetStringField(TEXT("regionId"), Region->RegionId.ToString(EGuidFormats::DigitsWithHyphens));
		Properties->SetStringField(TEXT("actorLabel"),
			Region->GetOwner() ? Region->GetOwner()->GetActorLabel() : FString());
		Properties->SetStringField(TEXT("typeId"), Region->GetTypeId().ToString());
		Properties->SetStringField(TEXT("chinese"), TextOrEmpty(Region->Name.Chinese));
		Properties->SetStringField(TEXT("pinyin"), TextOrEmpty(Region->Name.Pinyin));
		Properties->SetStringField(TEXT("english"), TextOrEmpty(Region->Name.English));
		Properties->SetStringField(TEXT("note"), TextOrEmpty(Region->Note));

		// Metadata travels as the number, which is the scale itself — a display name would be a
		// translation of it and would not survive a round trip through another language.
		Properties->SetStringField(TEXT("source"), TextOrEmpty(Region->Source));
		Properties->SetNumberField(TEXT("confidence"), static_cast<int32>(Region->Confidence));
		Properties->SetNumberField(TEXT("z"), Region->GetComponentLocation().Z);
		Properties->SetBoolField(TEXT("useHeightBounds"), Region->bUseHeightBounds);
		if (Region->bUseHeightBounds)
		{
			Properties->SetNumberField(TEXT("minZ"), Region->MinZ);
			Properties->SetNumberField(TEXT("maxZ"), Region->MaxZ);
		}
		Properties->SetNumberField(TEXT("areaM2"), Region->GetWorldArea() / 10000.0);

		// The parent is written as a label for a human reading the file.
		if (const UPlaceRegionComponent* Parent = Region->GetEffectiveParent())
		{
			Properties->SetStringField(TEXT("parentLabel"),
				Parent->GetOwner() ? Parent->GetOwner()->GetActorLabel() : FString());
		}

		TSharedRef<FJsonObject> Feature = MakeShared<FJsonObject>();
		Feature->SetStringField(TEXT("type"), TEXT("Feature"));
		Feature->SetObjectField(TEXT("geometry"), Geometry);
		Feature->SetObjectField(TEXT("properties"), Properties);

		Features.Add(MakeShared<FJsonValueObject>(Feature));
		++OutResult.Exported;
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("type"), TEXT("FeatureCollection"));
	Root->SetNumberField(TEXT("placeLabelsVersion"), FormatVersion);

	Root->SetStringField(TEXT("coordinateSystem"), TEXT("unreal-world-centimetres"));
	Root->SetStringField(TEXT("level"), World ? World->GetMapName() : FString());
	Root->SetArrayField(TEXT("features"), Features);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		OutResult.Problems.Add(TEXT("Could not serialise the region data."));
		return;
	}

	// No BOM: RFC 8259 says JSON text is UTF-8 and a byte order mark is not part of it, and some strict readers choke on one.
	if (!FFileHelper::SaveStringToFile(Output, *FilePath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		OutResult.Problems.Add(FString::Printf(TEXT("Could not write %s."), *FilePath));
		return;
	}

	OutResult.bSucceeded = true;
}

void ExportCsv(UWorld* World, const FString& FilePath, FResult& OutResult)
{
	OutResult = FResult();

	TArray<UPlaceRegionComponent*> Regions;
	GatherRegionComponents(World, Regions);

	FString Output = CsvHeader;
	Output += LINE_TERMINATOR;

	for (UPlaceRegionComponent* Region : Regions)
	{
		Region->EnsureRegionId();

		TArray<FString> Fields;
		Fields.Add(Region->RegionId.ToString(EGuidFormats::DigitsWithHyphens));
		Fields.Add(Region->GetOwner() ? Region->GetOwner()->GetActorLabel() : FString());
		Fields.Add(Region->GetTypeId().ToString());
		Fields.Add(TextOrEmpty(Region->Name.Chinese));
		Fields.Add(TextOrEmpty(Region->Name.Pinyin));
		Fields.Add(TextOrEmpty(Region->Name.English));
		Fields.Add(TextOrEmpty(Region->Note));
		Fields.Add(TextOrEmpty(Region->Source));
		Fields.Add(FString::FromInt(static_cast<int32>(Region->Confidence)));
		Fields.Add(FString::FromInt(Region->LocalPoints.Num()));
		Fields.Add(FString::Printf(TEXT("%.1f"), Region->GetWorldArea() / 10000.0));

		for (int32 i = 0; i < Fields.Num(); ++i)
		{
			if (i > 0)
			{
				Output += TEXT(",");
			}
			Output += EscapeCsv(Fields[i]);
		}
		Output += LINE_TERMINATOR;
		++OutResult.Exported;
	}

	// UTF-8 *with* a BOM here, unlike the JSON.
	if (!FFileHelper::SaveStringToFile(Output, *FilePath,
			FFileHelper::EEncodingOptions::ForceUTF8))
	{
		OutResult.Problems.Add(FString::Printf(TEXT("Could not write %s."), *FilePath));
		return;
	}

	OutResult.bSucceeded = true;
}

void ImportGeoJson(UWorld* World, const FString& FilePath, FResult& OutResult)
{
	OutResult = FResult();

	if (!World)
	{
		OutResult.Problems.Add(TEXT("No editor world."));
		return;
	}

	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FilePath))
	{
		OutResult.Problems.Add(FString::Printf(TEXT("Could not read %s."), *FilePath));
		return;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutResult.Problems.Add(TEXT("That file is not valid JSON."));
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Features = nullptr;
	if (!Root->TryGetArrayField(TEXT("features"), Features) || !Features)
	{
		OutResult.Problems.Add(TEXT("No 'features' array — is this a GeoJSON file?"));
		return;
	}

	TArray<UPlaceRegionComponent*> Existing;
	GatherRegionComponents(World, Existing);

	TMap<FName, UPlaceLabelTypeAsset*> Types;
	GatherTypeAssets(Types);

	ULevel* Level = World->GetCurrentLevel();
	if (!Level)
	{
		OutResult.Problems.Add(TEXT("No current level."));
		return;
	}
	Level->Modify();

	int32 FeatureIndex = -1;
	for (const TSharedPtr<FJsonValue>& FeatureValue : *Features)
	{
		++FeatureIndex;

		const TSharedPtr<FJsonObject>* Feature = nullptr;
		if (!FeatureValue.IsValid() || !FeatureValue->TryGetObject(Feature) || !Feature)
		{
			OutResult.Problems.Add(FString::Printf(TEXT("Feature %d is not an object."), FeatureIndex));
			++OutResult.Skipped;
			continue;
		}

		const TSharedPtr<FJsonObject>* Geometry = nullptr;
		if (!(*Feature)->TryGetObjectField(TEXT("geometry"), Geometry) || !Geometry)
		{
			OutResult.Problems.Add(FString::Printf(TEXT("Feature %d has no geometry."), FeatureIndex));
			++OutResult.Skipped;
			continue;
		}

		FString GeometryType;
		(*Geometry)->TryGetStringField(TEXT("type"), GeometryType);
		if (GeometryType != TEXT("Polygon"))
		{
			OutResult.Problems.Add(FString::Printf(
				TEXT("Feature %d is a %s; only Polygon is supported."), FeatureIndex, *GeometryType));
			++OutResult.Skipped;
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>* Rings = nullptr;
		if (!(*Geometry)->TryGetArrayField(TEXT("coordinates"), Rings) || !Rings || Rings->Num() == 0)
		{
			OutResult.Problems.Add(FString::Printf(TEXT("Feature %d has no coordinates."), FeatureIndex));
			++OutResult.Skipped;
			continue;
		}

		// Outer ring only.
		const TArray<TSharedPtr<FJsonValue>>* Ring = nullptr;
		if (!(*Rings)[0]->TryGetArray(Ring) || !Ring)
		{
			++OutResult.Skipped;
			continue;
		}
		if (Rings->Num() > 1)
		{
			OutResult.Problems.Add(FString::Printf(
				TEXT("Feature %d has %d rings; only the outer one was used (holes are not supported)."),
				FeatureIndex, Rings->Num()));
		}

		TArray<FVector2D> Points;
		Points.Reserve(Ring->Num());
		for (const TSharedPtr<FJsonValue>& PointValue : *Ring)
		{
			const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
			if (!PointValue.IsValid() || !PointValue->TryGetArray(Pair) || !Pair || Pair->Num() < 2)
			{
				continue;
			}
			Points.Emplace((*Pair)[0]->AsNumber(), (*Pair)[1]->AsNumber());
		}

		// GeoJSON repeats the first point to close the ring; the component implies the closing edge.
		if (Points.Num() >= 2 && Points[0].Equals(Points.Last(), 0.01))
		{
			Points.Pop();
		}
		if (Points.Num() < 3)
		{
			OutResult.Problems.Add(FString::Printf(
				TEXT("Feature %d has fewer than three distinct corners."), FeatureIndex));
			++OutResult.Skipped;
			continue;
		}

		const TSharedPtr<FJsonObject>* PropertiesPtr = nullptr;
		TSharedPtr<FJsonObject> Properties = (*Feature)->TryGetObjectField(TEXT("properties"), PropertiesPtr)
			&& PropertiesPtr
			? *PropertiesPtr
			: MakeShared<FJsonObject>();

		FString IdString;
		Properties->TryGetStringField(TEXT("regionId"), IdString);
		FGuid Id;
		FGuid::Parse(IdString, Id);

		double Z = 0.0;
		Properties->TryGetNumberField(TEXT("z"), Z);

		UPlaceRegionComponent* Region = FindByRegionId(Existing, Id);
		const bool bIsNew = (Region == nullptr);

		if (bIsNew)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.OverrideLevel = Level;
			SpawnParams.ObjectFlags = RF_Transactional;

			APlaceRegionActor* Actor = World->SpawnActor<APlaceRegionActor>(
				APlaceRegionActor::StaticClass(), FTransform(FVector(0, 0, Z)), SpawnParams);
			if (!Actor || !Actor->Region)
			{
				OutResult.Problems.Add(FString::Printf(
					TEXT("Feature %d: could not spawn a region actor."), FeatureIndex));
				++OutResult.Skipped;
				continue;
			}
			Region = Actor->Region;

			// Keep the file's id rather than the one OnComponentCreated just minted, or a second import of the same file makes a second copy of every region.
			Region->Modify();
			Region->RegionId = Id.IsValid() ? Id : FGuid::NewGuid();
			Existing.Add(Region);
		}

		ApplyWorldOutline(Region, Points, Z);

		Region->Modify();

		FString Field;
		if (Properties->TryGetStringField(TEXT("chinese"), Field)) { Region->Name.Chinese = FieldToText(Field); }
		if (Properties->TryGetStringField(TEXT("pinyin"), Field))  { Region->Name.Pinyin  = FieldToText(Field); }
		if (Properties->TryGetStringField(TEXT("english"), Field)) { Region->Name.English = FieldToText(Field); }
		if (Properties->TryGetStringField(TEXT("note"), Field))    { Region->Note         = FieldToText(Field); }
		if (Properties->TryGetStringField(TEXT("source"), Field))  { Region->Source       = FieldToText(Field); }

		double ConfidenceNumber = 0.0;
		if (Properties->TryGetNumberField(TEXT("confidence"), ConfidenceNumber))
		{
			Region->Confidence = ParseConfidence(FString::FromInt(FMath::RoundToInt(ConfidenceNumber)),
				Region->Confidence);
		}

		if (Properties->TryGetStringField(TEXT("typeId"), Field) && !Field.IsEmpty()
			&& Field != TEXT("None"))
		{
			if (UPlaceLabelTypeAsset** Found = Types.Find(FName(*Field)))
			{
				Region->Type = *Found;
			}
			else
			{
				OutResult.Problems.Add(FString::Printf(
					TEXT("Feature %d refers to type '%s', which this project has no asset for."),
					FeatureIndex, *Field));
			}
		}

		bool bHeightBounds = false;
		if (Properties->TryGetBoolField(TEXT("useHeightBounds"), bHeightBounds) && bHeightBounds)
		{
			Region->bUseHeightBounds = true;
			Properties->TryGetNumberField(TEXT("minZ"), Region->MinZ);
			Properties->TryGetNumberField(TEXT("maxZ"), Region->MaxZ);
		}

		if (AActor* Owner = Region->GetOwner())
		{
			Owner->Modify();
			Owner->SetActorLabel(Owner->GetDefaultActorLabel());
		}

		bIsNew ? ++OutResult.Created : ++OutResult.Updated;
	}

	// One hierarchy sweep at the end.
	for (UPlaceRegionComponent* Region : Existing)
	{
		if (Region && Region->bAutoParent && !Region->ExplicitParent)
		{
			Region->RecomputeDerivedParent();
		}
	}

	OutResult.bSucceeded = true;
}

void ImportCsv(UWorld* World, const FString& FilePath, FResult& OutResult)
{
	OutResult = FResult();

	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FilePath))
	{
		OutResult.Problems.Add(FString::Printf(TEXT("Could not read %s."), *FilePath));
		return;
	}

	int32 Cursor = 0;
	TArray<FString> Header;
	if (!ParseCsvRecord(Text, Cursor, Header))
	{
		OutResult.Problems.Add(TEXT("That file is empty."));
		return;
	}

	// Resolve columns by name, so reordering them in a spreadsheet.
	TMap<FString, int32> Columns;
	for (int32 i = 0; i < Header.Num(); ++i)
	{
		Columns.Add(Header[i].TrimStartAndEnd().ToLower(), i);
	}

	const int32* IdColumn = Columns.Find(TEXT("regionid"));
	if (!IdColumn)
	{
		OutResult.Problems.Add(
			TEXT("No 'regionId' column — export a CSV first and fill that one in, so each row "
				 "knows which region it belongs to."));
		return;
	}

	TArray<UPlaceRegionComponent*> Existing;
	GatherRegionComponents(World, Existing);

	TMap<FName, UPlaceLabelTypeAsset*> Types;
	GatherTypeAssets(Types);

	auto FieldAt = [](const TArray<FString>& Row, const int32* Column) -> const FString*
	{
		return (Column && Row.IsValidIndex(*Column)) ? &Row[*Column] : nullptr;
	};

	const int32* TypeColumn = Columns.Find(TEXT("typeid"));
	const int32* ChineseColumn = Columns.Find(TEXT("chinese"));
	const int32* PinyinColumn = Columns.Find(TEXT("pinyin"));
	const int32* EnglishColumn = Columns.Find(TEXT("english"));
	const int32* NoteColumn = Columns.Find(TEXT("note"));
	const int32* SourceColumn = Columns.Find(TEXT("source"));
	const int32* ConfidenceColumn = Columns.Find(TEXT("confidence"));

	int32 RowNumber = 1;
	TArray<FString> Row;
	while (ParseCsvRecord(Text, Cursor, Row))
	{
		++RowNumber;

		// A trailing newline produces one empty record; that is not a problem worth reporting.
		if (Row.Num() == 0 || (Row.Num() == 1 && Row[0].TrimStartAndEnd().IsEmpty()))
		{
			continue;
		}

		const FString* IdField = FieldAt(Row, IdColumn);
		FGuid Id;
		if (!IdField || !FGuid::Parse(IdField->TrimStartAndEnd(), Id))
		{
			OutResult.Problems.Add(FString::Printf(TEXT("Row %d: no usable regionId."), RowNumber));
			++OutResult.Skipped;
			continue;
		}

		UPlaceRegionComponent* Region = FindByRegionId(Existing, Id);
		if (!Region)
		{
			// Deliberately not spawned.
			OutResult.Problems.Add(FString::Printf(
				TEXT("Row %d: no region in this level with id %s."), RowNumber, *Id.ToString()));
			++OutResult.Skipped;
			continue;
		}

		Region->Modify();

		if (const FString* Field = FieldAt(Row, ChineseColumn)) { Region->Name.Chinese = FieldToText(*Field); }
		if (const FString* Field = FieldAt(Row, PinyinColumn))  { Region->Name.Pinyin  = FieldToText(*Field); }
		if (const FString* Field = FieldAt(Row, EnglishColumn)) { Region->Name.English = FieldToText(*Field); }
		if (const FString* Field = FieldAt(Row, NoteColumn))    { Region->Note         = FieldToText(*Field); }
		if (const FString* Field = FieldAt(Row, SourceColumn))  { Region->Source       = FieldToText(*Field); }
		if (const FString* Field = FieldAt(Row, ConfidenceColumn))
		{
			Region->Confidence = ParseConfidence(*Field, Region->Confidence);
		}

		if (const FString* Field = FieldAt(Row, TypeColumn))
		{
			const FString Trimmed = Field->TrimStartAndEnd();
			if (!Trimmed.IsEmpty() && Trimmed != TEXT("None"))
			{
				if (UPlaceLabelTypeAsset** Found = Types.Find(FName(*Trimmed)))
				{
					Region->Type = *Found;
				}
				else
				{
					OutResult.Problems.Add(FString::Printf(
						TEXT("Row %d refers to type '%s', which this project has no asset for."),
						RowNumber, *Trimmed));
				}
			}
		}

		if (AActor* Owner = Region->GetOwner())
		{
			Owner->Modify();
			Owner->SetActorLabel(Owner->GetDefaultActorLabel());
		}

		// The type drives parenting and the outline colour, and both may just have changed.
		Region->MarkRenderStateDirty();
		++OutResult.Updated;
	}

	for (UPlaceRegionComponent* Region : Existing)
	{
		if (Region && Region->bAutoParent && !Region->ExplicitParent)
		{
			Region->RecomputeDerivedParent();
		}
	}

	OutResult.bSucceeded = true;
}

} // namespace PlaceLabelsExchange

#undef LOCTEXT_NAMESPACE
