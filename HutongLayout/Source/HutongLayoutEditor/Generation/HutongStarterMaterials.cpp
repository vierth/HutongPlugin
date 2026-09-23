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
		enum class EPattern { Flat, Brick, Tile, Grain, Speckle, Mottle, Courses };

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
			{ MatSlot_Ridge,      TEXT("M_Hutong_Ridge"),      0.80f, EPattern::Courses },
		};

		// HLSL for the custom node: UV and Color in; the surface colour out, with a tangent-space
		// Normal and a Rough multiplier beside it. Every one is Color modulated, so the Color
		// parameter stays the one knob a whole street is retinted by.
		//
		// Each pattern antialiases itself against fwidth(UV), the footprint of one pixel in UV: a
		// joint or a 壟 narrower than that cannot be drawn, only averaged, and drawing it anyway is
		// what put arcs across brick seen at a grazing angle. `Detail` fades every feature out as
		// the footprint grows, leaving the flat colour a distant wall should have.
		const TCHAR* PatternCode(EPattern Pattern)
		{
			switch (Pattern)
			{
			// 磨磚對縫: dressed 停泥磚, 24 by 6 cm faces in running bond with a hairline joint.
			case EPattern::Brick: return TEXT(
				"float2 P = UV / float2(0.24, 0.064);\n"
				"P.x += frac(floor(P.y) * 0.5);\n"
				"float2 F = frac(P);\n"
				"float2 W = max(fwidth(P), 1e-5);\n"
				"float Detail = saturate(1.0 - 0.7 * max(W.x, W.y));\n"
				// The joint's edge is never narrower than a pixel, so it dissolves rather than crawls.
				"float Jx = 1.0 - smoothstep(0.03 - W.x, 0.03 + W.x, F.x);\n"
				"float Jy = 1.0 - smoothstep(0.10 - W.y, 0.10 + W.y, F.y);\n"
				"float Joint = max(Jx, Jy);\n"
				// Past a pixel per brick, what is left of the joints is their average coverage.
				"Joint = lerp(0.13, Joint, Detail);\n"
				"float H = frac(sin(dot(floor(P), float2(12.9898, 78.233))) * 43758.5453);\n"
				"float3 Brick = Color.rgb * (0.88 + 0.22 * H * Detail);\n"
				// The joint is a groove: the brick's arrises fall away into it, its bed does not.
				"float Nx = smoothstep(0.94, 1.0, F.x) - (1.0 - smoothstep(0.0, 0.06, F.x));\n"
				"float Ny = smoothstep(0.86, 1.0, F.y) - (1.0 - smoothstep(0.0, 0.16, F.y));\n"
				"Normal = normalize(float3(float2(Nx, Ny) * 0.55 * Detail, 1.0));\n"
				"Rough = lerp(1.0, 1.15, Joint);\n"
				"return lerp(Brick, Color.rgb * 0.62, Joint);\n");

			// 筒瓦 over 板瓦: what a Beijing roof reads as from any distance is the run of 壟 down the
			// slope — a rounded 筒瓦 lit on top with a dark seam at each foot — and the rows only as
			// faint scallops under it. Rows and 壟 given equal weight make a grid.
			case EPattern::Tile: return TEXT(
				"float2 F = frac(UV);\n"
				"float2 W = max(fwidth(UV), 1e-5);\n"
				"float Detail = saturate(1.0 - 1.6 * max(W.x, W.y));\n"
				"float X = abs(F.x - 0.5);\n"
				"float T = saturate(X / 0.24);\n"
				"float Tube = sqrt(saturate(1.0 - T * T));\n"
				"float Foot = 1.0 - smoothstep(0.2, 0.34 + W.x, X);\n"
				"float Channel = 0.72 + 0.16 * smoothstep(0.3, 0.5, X);\n"
				"float Shade = lerp(Channel, 0.7 + 0.45 * Tube, Foot);\n"
				"float Seam = 1.0 - smoothstep(0.0, 0.07 + W.y, F.y);\n"
				"Shade *= 1.0 - 0.18 * Seam;\n"
				"Shade = lerp(0.88, Shade, Detail);\n"
				"float H = frac(sin(floor(UV.x) * 12.9898 + floor(UV.y * 0.5) * 78.233) * 43758.5453);\n"
				// Across the 壟 the tube turns over; down it the rows step at every seam.
				"float Nx = -sign(F.x - 0.5) * T * Foot * 1.1;\n"
				"float Ny = -Seam * 0.45;\n"
				"Normal = normalize(float3(float2(Nx, Ny) * Detail, 1.0));\n"
				"Rough = lerp(1.06, 0.94, Foot * Detail);\n"
				"return Color.rgb * Shade * (0.95 + 0.1 * H * Detail);\n");

			// Long grain with a slow wander.
			case EPattern::Grain: return TEXT(
				"float2 W = max(fwidth(UV), 1e-5);\n"
				"float Detail = saturate(1.0 - 110.0 * W.x);\n"
				"float Phase = UV.x * 110.0 + sin(UV.y * 6.0) * 2.5;\n"
				"float G = sin(Phase) * 0.5 + 0.5;\n"
				"float H = frac(sin(dot(floor(UV * float2(120.0, 4.0)), float2(12.9898, 78.233))) * 43758.5453);\n"
				"Normal = normalize(float3(cos(Phase) * 0.1 * Detail, 0.0, 1.0));\n"
				"Rough = 1.0 + 0.06 * (G - 0.5) * Detail;\n"
				"return Color.rgb * (0.9 + (0.1 * G + 0.06 * H) * Detail);\n");

			// Fine even speckle, 2 cm cells.
			case EPattern::Speckle: return TEXT(
				"float2 W = max(fwidth(UV), 1e-5);\n"
				"float Detail = saturate(1.0 - 50.0 * max(W.x, W.y));\n"
				"float2 C = floor(UV * 50.0);\n"
				"float H  = frac(sin(dot(C, float2(12.9898, 78.233))) * 43758.5453);\n"
				"float Hx = frac(sin(dot(C + float2(1.0, 0.0), float2(12.9898, 78.233))) * 43758.5453);\n"
				"float Hy = frac(sin(dot(C + float2(0.0, 1.0), float2(12.9898, 78.233))) * 43758.5453);\n"
				"Normal = normalize(float3(float2(H - Hx, H - Hy) * 0.3 * Detail, 1.0));\n"
				"Rough = 1.0 + 0.05 * (H - 0.5) * Detail;\n"
				"return Color.rgb * (0.92 + 0.12 * H * Detail);\n");

			// Soft blotches at half a metre with a finer one under them.
			case EPattern::Mottle: return TEXT(
				"float2 W = max(fwidth(UV), 1e-5);\n"
				"float Detail = saturate(1.0 - 17.0 * max(W.x, W.y));\n"
				"float2 C = floor(UV * 17.0);\n"
				"float A = frac(sin(dot(floor(UV * 2.0), float2(12.9898, 78.233))) * 43758.5453);\n"
				"float B  = frac(sin(dot(C, float2(39.3468, 11.135))) * 24634.6345);\n"
				"float Bx = frac(sin(dot(C + float2(1.0, 0.0), float2(39.3468, 11.135))) * 24634.6345);\n"
				"float By = frac(sin(dot(C + float2(0.0, 1.0), float2(39.3468, 11.135))) * 24634.6345);\n"
				"Normal = normalize(float3(float2(B - Bx, B - By) * 0.18 * Detail, 1.0));\n"
				"Rough = 1.0 + 0.04 * (B - 0.5) * Detail;\n"
				"return Color.rgb * (0.9 + 0.08 * A + 0.05 * B * Detail);\n");

			// 正脊: 瓦條 courses along the ridge, each a 4 cm band with a shadowed joint under its lip,
			// and the long pieces butted every 40 cm — horizontal, where the roof's 壟 run down.
			case EPattern::Courses: return TEXT(
				"float2 P = UV / float2(0.40, 0.04);\n"
				"P.x += frac(floor(P.y) * 0.37);\n"
				"float2 F = frac(P);\n"
				"float2 W = max(fwidth(P), 1e-5);\n"
				"float Detail = saturate(1.0 - 0.7 * max(W.x, W.y));\n"
				"float Jy = 1.0 - smoothstep(0.14 - W.y, 0.14 + W.y, F.y);\n"
				"float Jx = 1.0 - smoothstep(0.01 - W.x, 0.01 + W.x, F.x);\n"
				"float Joint = lerp(0.12, max(Jy, 0.6 * Jx), Detail);\n"
				"float H = frac(sin(floor(P.y) * 78.233 + floor(P.x) * 12.9898) * 43758.5453);\n"
				"float3 Course = Color.rgb * (0.9 + 0.16 * H * Detail);\n"
				// Each course's lip overhangs the joint below it.
				"float Ny = smoothstep(0.8, 1.0, F.y) - (1.0 - smoothstep(0.0, 0.2, F.y));\n"
				"Normal = normalize(float3(0.0, Ny * 0.6 * Detail, 1.0));\n"
				"Rough = lerp(1.0, 1.12, Joint);\n"
				"return lerp(Course, Color.rgb * 0.55, Joint);\n");

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

		// Albedo texture × pattern(Color) into base colour, the pattern's own normal into Normal and
		// its Rough multiplier against the Roughness scalar. The texture defaults to white so the
		// pattern shows on its own; a real texture drops in over it.
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

			// The pattern sits between the colour and the tint; a flat surface skips it and keeps a
			// plain normal and the scalar roughness on its own.
			UMaterialExpression* Surface = Color;
			UMaterialExpressionCustom* Relief = nullptr;
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
					// Beside the colour it returns: the relief the pattern describes, and how much
					// rougher or smoother that relief is than the surface's own figure.
					Pattern->AdditionalOutputs.SetNum(2);
					Pattern->AdditionalOutputs[0].OutputName = TEXT("Normal");
					Pattern->AdditionalOutputs[0].OutputType = CMOT_Float3;
					Pattern->AdditionalOutputs[1].OutputName = TEXT("Rough");
					Pattern->AdditionalOutputs[1].OutputType = CMOT_Float1;
					// The pins are built from AdditionalOutputs, not read off it: set them and the
					// node still has only its default output, so nothing could be connected to Normal.
					Pattern->RebuildOutputs();
					UMaterialEditingLibrary::ConnectMaterialExpressions(UV, TEXT(""), Pattern, TEXT("UV"));
					UMaterialEditingLibrary::ConnectMaterialExpressions(Color, TEXT(""), Pattern, TEXT("Color"));
					Surface = Pattern;
					Relief = Pattern;
				}
			}

			UMaterialEditingLibrary::ConnectMaterialExpressions(Albedo, TEXT("RGB"), Tint, TEXT("A"));
			UMaterialEditingLibrary::ConnectMaterialExpressions(Surface, TEXT(""), Tint, TEXT("B"));
			UMaterialEditingLibrary::ConnectMaterialProperty(Tint, TEXT(""), MP_BaseColor);

			UMaterialExpression* RoughnessOut = Roughness;
			if (Relief)
			{
				UMaterialEditingLibrary::ConnectMaterialProperty(Relief, TEXT("Normal"), MP_Normal);
				if (auto* Scaled = Cast<UMaterialExpressionMultiply>(
						Make(UMaterialExpressionMultiply::StaticClass(), -300, 400)))
				{
					UMaterialEditingLibrary::ConnectMaterialExpressions(Roughness, TEXT(""), Scaled, TEXT("A"));
					UMaterialEditingLibrary::ConnectMaterialExpressions(Relief, TEXT("Rough"), Scaled, TEXT("B"));
					RoughnessOut = Scaled;
				}
			}
			UMaterialEditingLibrary::ConnectMaterialProperty(RoughnessOut, TEXT(""), MP_Roughness);
		}
	}

	FString StarterMaterialAssetName(int32 Slot)
	{
		const FStarterMaterial* Def = Definition(Slot);
		return Def ? FString(Def->AssetName) : FString();
	}

	bool StarterMaterialHasRelief(int32 Slot)
	{
		const FStarterMaterial* Def = Definition(Slot);
		return Def && PatternCode(Def->Pattern) != nullptr;
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
