#include "Generation/HutongStarterMaterials.h"
#include "Generation/HutongPalette.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "FileHelpers.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

namespace HutongGen
{
	const TCHAR* StarterMaterialPackagePath = TEXT("/Game/HutongLayout/Materials");

	namespace
	{
		// What the surface is made of, drawn from the UVs the mesh authors rather than from a
		// texture the plugin cannot ship. Box-projected faces carry one UV unit per metre; roofs
		// carry one 壟 per unit across and one tile row per unit down the slope.
		enum class EPattern { Flat, Brick, Tile, Grain, Speckle, Mottle };

		struct FStarterMaterial
		{
			int32 Slot;
			const TCHAR* AssetName;
			// How matte the surface reads before anyone puts a texture on it.
			float Roughness;
			EPattern Pattern;
		};

		// Fired brick and lime are matte; lacquer is the one thing on the street that shines.
		const FStarterMaterial StarterMaterials[] =
		{
			{ MatSlot_Body,       TEXT("M_Hutong_Body"),       0.85f, EPattern::Brick },
			{ MatSlot_Roof,       TEXT("M_Hutong_Roof"),       0.75f, EPattern::Tile },
			{ MatSlot_Wood,       TEXT("M_Hutong_Wood"),       0.55f, EPattern::Grain },
			{ MatSlot_Stone,      TEXT("M_Hutong_Stone"),      0.70f, EPattern::Speckle },
			{ MatSlot_Paint,      TEXT("M_Hutong_Paint"),      0.60f, EPattern::Flat },
			{ MatSlot_BaseCourse, TEXT("M_Hutong_BaseCourse"), 0.80f, EPattern::Brick },
			{ MatSlot_DoorPaint,  TEXT("M_Hutong_DoorPaint"),  0.35f, EPattern::Grain },
			{ MatSlot_Lattice,    TEXT("M_Hutong_Lattice"),    0.50f, EPattern::Flat },
			{ MatSlot_Paper,      TEXT("M_Hutong_Paper"),      0.90f, EPattern::Mottle },
			{ MatSlot_Plaster,    TEXT("M_Hutong_Plaster"),    0.95f, EPattern::Mottle },
			{ MatSlot_Earth,      TEXT("M_Hutong_Earth"),      1.00f, EPattern::Mottle },
			{ MatSlot_Partition,  TEXT("M_Hutong_Partition"),  0.60f, EPattern::Grain },
		};

		// HLSL for the custom node: UV in, Color in, float3 out. Every one is Color modulated,
		// so the Color parameter stays the one knob a whole street is retinted by.
		const TCHAR* PatternCode(EPattern Pattern)
		{
			switch (Pattern)
			{
			// 磨磚對縫: dressed 停泥磚, 24 by 6 cm faces in running bond with a hairline joint.
			case EPattern::Brick: return TEXT(
				"float2 p = UV / float2(0.24, 0.064);\n"
				"p.x += frac(floor(p.y) * 0.5);\n"
				"float2 cell = floor(p);\n"
				"float2 f = frac(p);\n"
				"float joint = (f.x < 0.03 || f.y < 0.1) ? 1.0 : 0.0;\n"
				"float h = frac(sin(dot(cell, float2(12.9898, 78.233))) * 43758.5453);\n"
				"float3 brick = Color.rgb * (0.88 + 0.22 * h);\n"
				"return lerp(brick, Color.rgb * 0.62, joint);\n");

			// 筒瓦 over 板瓦: what a Beijing roof reads as from any distance is the run of 壟 down the
			// slope — a rounded 筒瓦 lit on top with a dark seam at each foot — and the rows only as
			// faint scallops under it. Rows and 壟 given equal weight make a grid.
			case EPattern::Tile: return TEXT(
				"float2 f = frac(UV);\n"
				"float x = abs(f.x - 0.5);\n"
				"float t = saturate(x / 0.24);\n"
				"float tube = sqrt(saturate(1.0 - t * t));\n"
				"float foot = 1.0 - smoothstep(0.2, 0.34, x);\n"
				"float channel = 0.72 + 0.16 * smoothstep(0.3, 0.5, x);\n"
				"float shade = lerp(channel, 0.7 + 0.45 * tube, foot);\n"
				"shade *= 1.0 - 0.18 * (1.0 - smoothstep(0.0, 0.07, f.y));\n"
				"float h = frac(sin(floor(UV.x) * 12.9898 + floor(UV.y * 0.5) * 78.233) * 43758.5453);\n"
				"return Color.rgb * shade * (0.95 + 0.1 * h);\n");

			// Long grain with a slow wander.
			case EPattern::Grain: return TEXT(
				"float g = sin(UV.x * 110.0 + sin(UV.y * 6.0) * 2.5) * 0.5 + 0.5;\n"
				"float h = frac(sin(dot(floor(UV * float2(120.0, 4.0)), float2(12.9898, 78.233))) * 43758.5453);\n"
				"return Color.rgb * (0.9 + 0.1 * g + 0.06 * h);\n");

			// Fine even speckle, 2 cm cells.
			case EPattern::Speckle: return TEXT(
				"float h = frac(sin(dot(floor(UV * 50.0), float2(12.9898, 78.233))) * 43758.5453);\n"
				"return Color.rgb * (0.92 + 0.12 * h);\n");

			// Soft blotches at half a metre with a finer one under them.
			case EPattern::Mottle: return TEXT(
				"float a = frac(sin(dot(floor(UV * 2.0), float2(12.9898, 78.233))) * 43758.5453);\n"
				"float b = frac(sin(dot(floor(UV * 17.0), float2(39.3468, 11.135))) * 24634.6345);\n"
				"return Color.rgb * (0.9 + 0.08 * a + 0.05 * b);\n");

			default: return nullptr;
			}
		}
		static_assert(UE_ARRAY_COUNT(StarterMaterials) == MatSlot_Count, "one starter material per slot");

		const FStarterMaterial* Definition(int32 Slot)
		{
			for (const FStarterMaterial& Def : StarterMaterials)
			{
				if (Def.Slot == Slot) return &Def;
			}
			return nullptr;
		}

		FString PackageNameFor(int32 Slot)
		{
			return FString::Printf(TEXT("%s/%s"), StarterMaterialPackagePath, *StarterMaterialAssetName(Slot));
		}

		FString ObjectPathFor(int32 Slot)
		{
			const FString Name = StarterMaterialAssetName(Slot);
			return FString::Printf(TEXT("%s/%s.%s"), StarterMaterialPackagePath, *Name, *Name);
		}

		// Albedo texture × pattern(Color) into base colour, Roughness scalar into roughness. The
		// texture defaults to white so the pattern shows on its own; a real texture drops in over it.
		void BuildGraph(UMaterial* Material, const FStarterMaterial& Def)
		{
			UTexture* White = LoadObject<UTexture2D>(nullptr,
				TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));

			auto Make = [&](UClass* Class, int32 X, int32 Y)
			{
				return UMaterialEditingLibrary::CreateMaterialExpression(Material, Class, X, Y);
			};
			auto* Albedo = Cast<UMaterialExpressionTextureSampleParameter2D>(
				Make(UMaterialExpressionTextureSampleParameter2D::StaticClass(), -700, -250));
			auto* Color = Cast<UMaterialExpressionVectorParameter>(
				Make(UMaterialExpressionVectorParameter::StaticClass(), -1000, 50));
			auto* Tint = Cast<UMaterialExpressionMultiply>(
				Make(UMaterialExpressionMultiply::StaticClass(), -300, 0));
			auto* Roughness = Cast<UMaterialExpressionScalarParameter>(
				Make(UMaterialExpressionScalarParameter::StaticClass(), -300, 250));
			if (!Albedo || !Color || !Tint || !Roughness) return;

			Albedo->ParameterName = TEXT("Albedo");
			Albedo->Texture = White;
			Albedo->SamplerType = SAMPLERTYPE_Color;

			// The same parameter name the tinted default instance carries.
			Color->ParameterName = TEXT("Color");
			Color->DefaultValue = FHutongPalette().GetSlotColor(Def.Slot);

			Roughness->ParameterName = TEXT("Roughness");
			Roughness->DefaultValue = Def.Roughness;

			// The pattern sits between the colour and the tint; a flat surface skips it.
			UMaterialExpression* Surface = Color;
			if (const TCHAR* Code = PatternCode(Def.Pattern))
			{
				auto* UV = Cast<UMaterialExpressionTextureCoordinate>(
					Make(UMaterialExpressionTextureCoordinate::StaticClass(), -1000, -100));
				auto* Pattern = Cast<UMaterialExpressionCustom>(
					Make(UMaterialExpressionCustom::StaticClass(), -650, 0));
				if (UV && Pattern)
				{
					Pattern->Description = TEXT("Pattern");
					Pattern->OutputType = CMOT_Float3;
					Pattern->Code = Code;
					Pattern->Inputs.SetNum(2);
					Pattern->Inputs[0].InputName = TEXT("UV");
					Pattern->Inputs[1].InputName = TEXT("Color");
					UMaterialEditingLibrary::ConnectMaterialExpressions(UV, TEXT(""), Pattern, TEXT("UV"));
					UMaterialEditingLibrary::ConnectMaterialExpressions(Color, TEXT(""), Pattern, TEXT("Color"));
					Surface = Pattern;
				}
			}

			UMaterialEditingLibrary::ConnectMaterialExpressions(Albedo, TEXT("RGB"), Tint, TEXT("A"));
			UMaterialEditingLibrary::ConnectMaterialExpressions(Surface, TEXT(""), Tint, TEXT("B"));
			UMaterialEditingLibrary::ConnectMaterialProperty(Tint, TEXT(""), MP_BaseColor);
			UMaterialEditingLibrary::ConnectMaterialProperty(Roughness, TEXT(""), MP_Roughness);
		}
	}

	FString StarterMaterialAssetName(int32 Slot)
	{
		const FStarterMaterial* Def = Definition(Slot);
		return Def ? FString(Def->AssetName) : FString();
	}

	UMaterialInterface* FindStarterMaterial(int32 Slot)
	{
		// A street spawns hundreds of pieces of a dozen slots each; asking the disk for every one
		// of them is what this cache is for. A miss is believed for a few seconds, a hit until the
		// asset goes away.
		struct FEntry
		{
			TWeakObjectPtr<UMaterialInterface> Material;
			double LastMiss = -1.0e9;
		};
		static FEntry Cache[MatSlot_Count];
		if (Slot < 0 || Slot >= MatSlot_Count) return nullptr;

		FEntry& Entry = Cache[Slot];
		if (UMaterialInterface* Cached = Entry.Material.Get()) return Cached;

		const double Now = FPlatformTime::Seconds();
		if (Now - Entry.LastMiss < 5.0) return nullptr;

		const FString ObjectPath = ObjectPathFor(Slot);
		UMaterialInterface* Found = FindObject<UMaterialInterface>(nullptr, *ObjectPath);
		if (!Found && FPackageName::DoesPackageExist(PackageNameFor(Slot)))
		{
			Found = LoadObject<UMaterialInterface>(nullptr, *ObjectPath);
		}

		Entry.Material = Found;
		if (!Found) Entry.LastMiss = Now;
		return Found;
	}

	int32 CreateStarterMaterials(TArray<FString>& OutCreatedNames, TArray<FString>& OutSkippedNames)
	{
		return CreateStarterMaterialsAt(StarterMaterialPackagePath, OutCreatedNames, OutSkippedNames);
	}

	int32 CreateStarterMaterialsAt(const FString& PackagePath, TArray<FString>& OutCreatedNames, TArray<FString>& OutSkippedNames)
	{
		OutCreatedNames.Reset();
		OutSkippedNames.Reset();

		TArray<UPackage*> PackagesToSave;

		for (const FStarterMaterial& Def : StarterMaterials)
		{
			const FString PackageName = FString::Printf(TEXT("%s/%s"), *PackagePath, Def.AssetName);

			// Never clobber an existing asset: the whole point of these is that you tune them.
			if (FindPackage(nullptr, *PackageName) || FPackageName::DoesPackageExist(PackageName))
			{
				OutSkippedNames.Add(Def.AssetName);
				continue;
			}

			// No FullyLoad on a package that has no file yet: it runs a load that fails and
			// collects garbage on the way out, which is how a transient factory held across it
			// came back with a dead vtable. Straight NewObject is what the material factory does.
			UPackage* Package = CreatePackage(*PackageName);
			if (!Package) continue;

			UMaterial* Material = NewObject<UMaterial>(Package, FName(Def.AssetName),
				RF_Public | RF_Standalone | RF_Transactional);
			if (!Material) continue;

			BuildGraph(Material, Def);
			UMaterialEditingLibrary::RecompileMaterial(Material);

			FAssetRegistryModule::AssetCreated(Material);
			Package->MarkPackageDirty();
			PackagesToSave.Add(Package);
			OutCreatedNames.Add(Def.AssetName);
		}

		// Saved at once rather than left dirty: an unsaved material is a broken reference on reload.
		if (PackagesToSave.Num() > 0)
		{
			UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, /*bOnlyDirty*/ false);
		}

		return OutCreatedNames.Num();
	}
}
