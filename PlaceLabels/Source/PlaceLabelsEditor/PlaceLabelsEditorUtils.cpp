#include "PlaceLabelsEditorUtils.h"

#include "PlaceLabelTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "PlaceLabelsEditorUtils"

namespace PlaceLabelsEditorUtils
{
	const TCHAR* StarterTypePackagePath = TEXT("/Game/PlaceLabels/Types");

	namespace
	{
		struct FStarterType
		{
			const TCHAR* AssetName;
			const TCHAR* TypeId;
			const TCHAR* Chinese;
			const TCHAR* Pinyin;
			const TCHAR* English;
			int32 Priority;
			// Comma-separated Type Ids, in preference order. First one that matches wins.
			const TCHAR* ParentTypes;
			EPlaceParentRelation Relation;
			FLinearColor OutlineColor;
		};

		// The hierarchy this plugin was built around.
		const FStarterType StarterTypes[] =
		{
			{ TEXT("DA_Type_District"), TEXT("district"), TEXT("城區"), TEXT("chengqu"), TEXT("District"),
			  10, TEXT(""), EPlaceParentRelation::Containing, FLinearColor(0.55f, 0.35f, 0.95f) },

			{ TEXT("DA_Type_Area"), TEXT("area"), TEXT("片區"), TEXT("pianqu"), TEXT("Area"),
			  20, TEXT("district"), EPlaceParentRelation::Containing, FLinearColor(0.35f, 0.55f, 0.95f) },

			{ TEXT("DA_Type_Avenue"), TEXT("avenue"), TEXT("大街"), TEXT("dajie"), TEXT("Avenue"),
			  30, TEXT("area,district"), EPlaceParentRelation::Containing, FLinearColor(0.25f, 0.85f, 0.85f) },

			{ TEXT("DA_Type_Hutong"), TEXT("hutong"), TEXT("胡同"), TEXT("hutong"), TEXT("Lane"),
			  40, TEXT("area,district"), EPlaceParentRelation::Containing, FLinearColor(0.35f, 0.9f, 0.45f) },

			// Adjacent, not Containing: a compound abuts the lane it is addressed on.
			{ TEXT("DA_Type_Compound"), TEXT("compound"), TEXT("院"), TEXT("yuan"), TEXT("Compound"),
			  50, TEXT("hutong,avenue"), EPlaceParentRelation::Adjacent, FLinearColor(1.0f, 0.85f, 0.2f) },

			{ TEXT("DA_Type_Temple"), TEXT("temple"), TEXT("寺"), TEXT("si"), TEXT("Temple"),
			  55, TEXT("hutong,avenue"), EPlaceParentRelation::Adjacent, FLinearColor(1.0f, 0.5f, 0.15f) },

			// Water, between the street types and the lane: a named watercourse is read at about the
			// scale of an avenue, and outranks it where the two outlines meet at a bank.
			{ TEXT("DA_Type_River"), TEXT("river"), TEXT("河"), TEXT("he"), TEXT("River"),
			  34, TEXT("area,district"), EPlaceParentRelation::Containing, FLinearColor(0.3f, 0.65f, 0.95f) },

			{ TEXT("DA_Type_Lake"), TEXT("lake"), TEXT("湖"), TEXT("hu"), TEXT("Lake"),
			  36, TEXT("area,district"), EPlaceParentRelation::Containing, FLinearColor(0.2f, 0.5f, 0.9f) },

			// 護城河 runs along the wall rather than inside anything, so Adjacent — and it is the
			// most specific of the three: a moat is one named stretch of water, not a kind of water.
			{ TEXT("DA_Type_Moat"), TEXT("moat"), TEXT("護城河"), TEXT("huchenghe"), TEXT("Moat"),
			  38, TEXT("district,area"), EPlaceParentRelation::Adjacent, FLinearColor(0.15f, 0.4f, 0.75f) },

			// Adjacent for the same reason a compound is, and a stronger one: a bridge carries the
			// street across the water, and the street's own outline usually stops at the bank. It
			// outranks what it stands in — standing on 銀錠橋 reads as the bridge, not as the lane.
			{ TEXT("DA_Type_Bridge"), TEXT("bridge"), TEXT("橋"), TEXT("qiao"), TEXT("Bridge"),
			  58, TEXT("avenue,hutong"), EPlaceParentRelation::Adjacent, FLinearColor(0.7f, 0.9f, 1.0f) },

			{ TEXT("DA_Type_Building"), TEXT("building"), TEXT("樓"), TEXT("lou"), TEXT("Building"),
			  60, TEXT("compound"), EPlaceParentRelation::Containing, FLinearColor(1.0f, 0.35f, 0.35f) },
		};
	}

	int32 CreateStarterTypeAssets(TArray<FString>& OutCreatedNames, TArray<FString>& OutSkippedNames)
	{
		OutCreatedNames.Reset();
		OutSkippedNames.Reset();

		TArray<UPackage*> PackagesToSave;

		for (const FStarterType& Def : StarterTypes)
		{
			const FString PackageName =
				FString::Printf(TEXT("%s/%s"), StarterTypePackagePath, Def.AssetName);

			// Never clobber an existing asset: the whole point of these is that you tune them.
			if (FindPackage(nullptr, *PackageName)
				|| FPackageName::DoesPackageExist(PackageName))
			{
				OutSkippedNames.Add(Def.AssetName);
				continue;
			}

			UPackage* Package = CreatePackage(*PackageName);
			if (!Package)
			{
				continue;
			}
			Package->FullyLoad();

			UPlaceLabelTypeAsset* Asset = NewObject<UPlaceLabelTypeAsset>(
				Package, UPlaceLabelTypeAsset::StaticClass(), FName(Def.AssetName),
				RF_Public | RF_Standalone | RF_Transactional);
			if (!Asset)
			{
				continue;
			}

			Asset->TypeId = FName(Def.TypeId);
			Asset->TypeLabel.Chinese = FText::FromString(Def.Chinese);
			Asset->TypeLabel.Pinyin = FText::FromString(Def.Pinyin);
			Asset->TypeLabel.English = FText::FromString(Def.English);
			Asset->DisplayPriority = Def.Priority;
			Asset->ParentRelation = Def.Relation;
			Asset->EditorOutlineColor = Def.OutlineColor;
			Asset->AccentColor = Def.OutlineColor;

			const FString ParentList(Def.ParentTypes);
			if (!ParentList.IsEmpty())
			{
				TArray<FString> Parts;
				ParentList.ParseIntoArray(Parts, TEXT(","));
				for (const FString& Part : Parts)
				{
					Asset->ParentTypes.Add(FName(*Part.TrimStartAndEnd()));
				}
			}

			FAssetRegistryModule::AssetCreated(Asset);
			Package->MarkPackageDirty();
			PackagesToSave.Add(Package);
			OutCreatedNames.Add(Def.AssetName);
		}

		// Save immediately rather than leaving them dirty.
		if (PackagesToSave.Num() > 0)
		{
			UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, /*bOnlyDirty*/ false);
		}

		return OutCreatedNames.Num();
	}
}

#undef LOCTEXT_NAMESPACE
