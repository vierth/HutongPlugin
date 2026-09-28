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
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/Package.h"

namespace HutongGen
{
	const TCHAR* StarterMaterialPackagePath = TEXT("/Game/HutongLayout/Materials");

	namespace
	{
		// Procedural from mesh UVs (no shippable textures). Box-projected faces: 1 UV unit per metre;
		// roofs: 1 壟 per unit across, 1 tile row per unit down the slope.
		enum class EPattern { Flat, Brick, Tile, Grain, Speckle, Mottle, Courses, Moulded, Pavers };

		struct FStarterMaterial
		{
			int32 Slot;
			const TCHAR* AssetName;
			// Untextured roughness.
			float Roughness;
			EPattern Pattern;
		};

		// Brick and lime matte; lacquer the only shine.
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
			{ MatSlot_Finial,     TEXT("M_Hutong_Finial"),     0.75f, EPattern::Moulded },
			{ MatSlot_Floor,      TEXT("M_Hutong_Floor"),      0.80f, EPattern::Pavers },
		};

		// Friction and bounce; surface types (footstep sounds) are the project's to define.
		struct FStarterPhysics { const TCHAR* AssetName; float Friction; float Restitution; };

		FStarterPhysics PhysicsFor(int32 Slot)
		{
			switch (Slot)
			{
			case MatSlot_Body: case MatSlot_BaseCourse: return { TEXT("PM_Hutong_Brick"),   0.80f, 0.10f };
			case MatSlot_Roof: case MatSlot_Ridge: case MatSlot_Finial: return { TEXT("PM_Hutong_Tile"), 0.60f, 0.15f };
			case MatSlot_Floor:                         return { TEXT("PM_Hutong_Brick"),   0.80f, 0.10f };
			case MatSlot_Stone:                         return { TEXT("PM_Hutong_Stone"),   0.70f, 0.10f };
			case MatSlot_Paper:                         return { TEXT("PM_Hutong_Paper"),   0.50f, 0.05f };
			case MatSlot_Plaster:                       return { TEXT("PM_Hutong_Plaster"), 0.70f, 0.05f };
			case MatSlot_Earth:                         return { TEXT("PM_Hutong_Earth"),   0.90f, 0.00f };
			default:                                    return { TEXT("PM_Hutong_Wood"),    0.60f, 0.20f };
			}
		}

		// Found or created; saved with the materials.
		UPhysicalMaterial* StarterPhysics(const FString& PackagePath, int32 Slot, TArray<UPackage*>& PackagesToSave)
		{
			const FStarterPhysics Def = PhysicsFor(Slot);
			const FString PackageName = FString::Printf(TEXT("%s/%s"), *PackagePath, Def.AssetName);
			const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, Def.AssetName);
			if (UPhysicalMaterial* Found = FindObject<UPhysicalMaterial>(nullptr, *ObjectPath)) return Found;
			if (FPackageName::DoesPackageExist(PackageName)) return LoadObject<UPhysicalMaterial>(nullptr, *ObjectPath);

			UPackage* Package = CreatePackage(*PackageName);
			if (!Package) return nullptr;
			UPhysicalMaterial* Phys = NewObject<UPhysicalMaterial>(Package, FName(Def.AssetName), RF_Public | RF_Standalone | RF_Transactional);
			Phys->Friction = Def.Friction;
			Phys->Restitution = Def.Restitution;
			FAssetRegistryModule::AssetCreated(Phys);
			Package->MarkPackageDirty();
			PackagesToSave.AddUnique(Package);
			return Phys;
		}

		// Custom-node HLSL: UV, Color in; colour out plus tangent-space Normal and a Rough multiplier.
		// All Color-modulated, so Color stays the one retint knob.
		// Each pattern antialiases against fwidth(UV); features under a pixel are averaged, not drawn
		// (else moire arcs at grazing angles). `Detail` fades features to flat colour with distance.
		const TCHAR* PatternCode(EPattern Pattern)
		{
			switch (Pattern)
			{
			// 磨磚對縫: dressed 停泥磚, 24×6 cm faces, running bond, hairline joint.
			case EPattern::Brick:
			{
				static const FString Code = FString::Printf(TEXT("float2 P = UV / float2(%g, %g);\n"),
					HutongGen::BrickPattern::FaceLength / 100.0, HutongGen::BrickPattern::CourseHeight / 100.0)
					+ TEXT(
				"P.x += frac(floor(P.y) * 0.5);\n"
				"float2 F = frac(P);\n"
				"float2 W = max(fwidth(P), 1e-5);\n"
				"float Detail = saturate(1.0 - 0.7 * max(W.x, W.y));\n"
				// Joint edge at least a pixel wide: dissolves, not crawls.
				"float Jx = 1.0 - smoothstep(0.03 - W.x, 0.03 + W.x, F.x);\n"
				"float Jy = 1.0 - smoothstep(0.10 - W.y, 0.10 + W.y, F.y);\n"
				"float Joint = max(Jx, Jy);\n"
				// Below a pixel per brick, joints become average coverage.
				"Joint = lerp(0.13, Joint, Detail);\n"
				"float H = frac(sin(dot(floor(P), float2(12.9898, 78.233))) * 43758.5453);\n"
				"float3 Brick = Color.rgb * (0.88 + 0.22 * H * Detail);\n"
				// Joint is a groove: brick arrises fall into it, the bed does not.
				"float Nx = smoothstep(0.94, 1.0, F.x) - (1.0 - smoothstep(0.0, 0.06, F.x));\n"
				"float Ny = smoothstep(0.86, 1.0, F.y) - (1.0 - smoothstep(0.0, 0.16, F.y));\n"
				"Normal = normalize(float3(float2(Nx, Ny) * 0.55 * Detail, 1.0));\n"
				"Rough = lerp(1.0, 1.15, Joint);\n"
				"return lerp(Brick, Color.rgb * 0.62, Joint);\n");
				return *Code;
			}

			// 筒瓦 over 板瓦 (or 合瓦's 蓋瓦 over 底瓦): 壟 read at any distance; closer, 板瓦 channels
			// lapped 壓七露三 and 筒瓦 joints. u in 壟, cover course every half-unit; v in rows up to
			// the ridge. Laps fade before the 壟; each 壟 phased apart from its neighbours.
			case EPattern::Tile: return TEXT(
				"float2 W = max(fwidth(UV), 1e-5);\n"
				"float Detail = saturate(1.0 - 1.6 * max(W.x, W.y));\n"
				"float Fine = saturate(1.0 - 4.0 * W.y);\n"
				"float Fu = frac(UV.x);\n"
				"float X = abs(Fu - 0.5);\n"
				"float S = sign(Fu - 0.5);\n"
				"float T = saturate(X / 0.24);\n"
				"float Tube = sqrt(saturate(1.0 - T * T));\n"
				"float Cover = 1.0 - smoothstep(0.22, 0.25 + W.x, X);\n"
				"float P = saturate((X - 0.24) / 0.26);\n"
				"float Row = floor(UV.x + 0.5);\n"
				"float Phase = frac(sin(Row * 12.9898 + 547.631) * 43758.5453);\n"
				"float Lap = frac(UV.y * 3.0 + Phase);\n"
				"float Edge = 1.0 - smoothstep(0.0, 0.10 + 3.0 * W.y, Lap);\n"
				"float CJ = frac(UV.y / 1.5 + frac(sin(floor(UV.x) * 12.9898 + 234.699) * 43758.5453));\n"
				"float Joint = 1.0 - smoothstep(0.0, 0.05 + W.y / 1.5, CJ);\n"
				"float Pan = 0.70 + 0.10 * P + (0.08 * Lap - 0.16 * Edge) * Fine;\n"
				"float Cov = (0.72 + 0.38 * Tube) * (1.0 - 0.35 * Joint * Fine);\n"
				"float Shade = lerp(0.84, lerp(Pan, Cov, Cover), Detail);\n"
				"float HC = frac(sin(Row * 12.9898 + floor(UV.y / 1.5) * 78.233) * 43758.5453);\n"
				"float HP = frac(sin(Row * 12.9898 + floor(UV.y * 3.0 + Phase) * 78.233) * 43758.5453);\n"
				"float H = lerp(HP, HC, step(0.5, Cover));\n"
				// Across: tube rounds over, channel hollows. Down: each tile's lower edge stands proud.
				"float Nx = -S * (Cover * T * 0.8 + (1.0 - Cover) * (1.0 - P) * 0.55);\n"
				"float Ny = ((1.0 - Cover) * (0.2 * (Lap - 0.5) - 0.35 * Edge) - Cover * 0.5 * Joint) * Fine;\n"
				"Normal = normalize(float3(float2(Nx, Ny) * Detail, 1.0));\n"
				"Rough = lerp(1.06, 0.94, Cover * Detail);\n"
				"return Color.rgb * Shade * (0.93 + 0.14 * H * Detail);\n");

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

			// Dressed 青白石: soft value noise in octaves (30 cm clouding, 7 cm, 1.4 and 0.6 cm grain), sparse
			// small dark flecks, a faint relief from the grain's slope. Hard 2 cm hash cells read as pixels.
			// Prototyped offline against the old cells before porting.
			case EPattern::Speckle: return TEXT(
				"struct FStone\n"
				"{\n"
				"	float Hash(float2 C) { return frac(sin(dot(C, float2(12.9898, 78.233))) * 43758.5453); }\n"
				"	float Noise(float2 P)\n"
				"	{\n"
				"		float2 I = floor(P); float2 F = frac(P); float2 U = F * F * (3.0 - 2.0 * F);\n"
				"		return lerp(lerp(Hash(I), Hash(I + float2(1, 0)), U.x), lerp(Hash(I + float2(0, 1)), Hash(I + float2(1, 1)), U.x), U.y);\n"
				"	}\n"
				"	float Height(float2 P) { return 0.5 * Noise(P * 14.0) + 0.5 * Noise(P * 70.0); }\n"
				"};\n"
				"FStone S;\n"
				"float2 W = max(fwidth(UV), 1e-5);\n"
				"float Wm = max(W.x, W.y);\n"
				"float D14 = saturate(1.0 - 14.0 * Wm);\n"
				"float D70 = saturate(1.0 - 70.0 * Wm);\n"
				"float D160 = saturate(1.0 - 160.0 * Wm);\n"
				"float Fleck = saturate((S.Noise(UV * 110.0 + 7.3) - 0.88) / 0.05) * D160;\n"
				"float Shade = 0.95 + 0.07 * (S.Noise(UV * 3.0) - 0.5) + 0.035 * (S.Noise(UV * 14.0) - 0.5) * D14\n"
				"	+ 0.05 * (S.Noise(UV * 70.0) - 0.5) * D70 + 0.04 * (S.Noise(UV * 160.0 + 3.1) - 0.5) * D160 - 0.07 * Fleck;\n"
				"float E = 0.002;\n"
				"float H0 = S.Height(UV);\n"
				"float2 Slope = float2(H0 - S.Height(UV + float2(E, 0.0)), H0 - S.Height(UV + float2(0.0, E))) / E * 0.0015;\n"
				"Normal = normalize(float3(Slope * D70, 1.0));\n"
				"Rough = 1.0 + 0.05 * (Shade - 0.95) * 10.0 * D70;\n"
				"return Color.rgb * Shade;\n");

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

			// 花甎寶頂: moulded grey brick, each piece whole — soft clouding and grain, a faint relief, no courses
			// (the ridge's coursing made a 寶珠 a stack of plates).
			case EPattern::Moulded: return TEXT(
				"struct FClay\n"
				"{\n"
				"	float Hash(float2 C) { return frac(sin(dot(C, float2(12.9898, 78.233))) * 43758.5453); }\n"
				"	float Noise(float2 P)\n"
				"	{\n"
				"		float2 I = floor(P); float2 F = frac(P); float2 U = F * F * (3.0 - 2.0 * F);\n"
				"		return lerp(lerp(Hash(I), Hash(I + float2(1, 0)), U.x), lerp(Hash(I + float2(0, 1)), Hash(I + float2(1, 1)), U.x), U.y);\n"
				"	}\n"
				"	float Height(float2 P) { return 0.7 * Noise(P * 15.0) + 0.3 * Noise(P * 60.0); }\n"
				"};\n"
				"FClay S;\n"
				"float2 W = max(fwidth(UV), 1e-5);\n"
				"float Wm = max(W.x, W.y);\n"
				"float D15 = saturate(1.0 - 15.0 * Wm);\n"
				"float D60 = saturate(1.0 - 60.0 * Wm);\n"
				"float Shade = 0.95 + 0.06 * (S.Noise(UV * 4.0) - 0.5) + 0.05 * (S.Noise(UV * 15.0) - 0.5) * D15 + 0.03 * (S.Noise(UV * 60.0) - 0.5) * D60;\n"
				"float E = 0.002;\n"
				"float H0 = S.Height(UV);\n"
				"float2 Slope = float2(H0 - S.Height(UV + float2(E, 0.0)), H0 - S.Height(UV + float2(0.0, E))) / E * 0.001;\n"
				"Normal = normalize(float3(Slope * D15, 1.0));\n"
				"Rough = 1.0 + 0.5 * (Shade - 0.95) * D15;\n"
				"return Color.rgb * Shade;\n");

			// 尺二方磚墁地: 38.4 cm squares laid in a grid, hairline joints, each brick its own tone and a
			// soft mottle, the arrises eased into the joint.
			case EPattern::Pavers:
			{
				static const FString Code = FString(TEXT(
				"struct FPaver\n"
				"{\n"
				"	float Hash(float2 C) { return frac(sin(dot(C, float2(12.9898, 78.233))) * 43758.5453); }\n"
				"	float Noise(float2 P)\n"
				"	{\n"
				"		float2 I = floor(P); float2 F = frac(P); float2 U = F * F * (3.0 - 2.0 * F);\n"
				"		return lerp(lerp(Hash(I), Hash(I + float2(1, 0)), U.x), lerp(Hash(I + float2(0, 1)), Hash(I + float2(1, 1)), U.x), U.y);\n"
				"	}\n"
				"};\n"
				"FPaver S;\n"))
					+ FString::Printf(TEXT("float2 P = UV / %g;\n"), HutongGen::FloorPaverCm / 100.0)
					+ TEXT("float2 F = frac(P);\n"
				"float2 W = max(fwidth(P), 1e-5);\n"
				"float Detail = saturate(1.0 - 0.7 * max(W.x, W.y));\n"
				"float2 D = min(F, 1.0 - F);\n"
				"float Jx = 1.0 - smoothstep(0.012 - W.x, 0.012 + W.x, D.x);\n"
				"float Jy = 1.0 - smoothstep(0.012 - W.y, 0.012 + W.y, D.y);\n"
				"float Joint = lerp(0.05, max(Jx, Jy), Detail);\n"
				"float H = S.Hash(floor(P));\n"
				"float Mott = 0.6 * S.Noise(UV * 6.0) + 0.4 * S.Noise(UV * 25.0);\n"
				"float3 Brick = Color.rgb * (0.9 + (0.10 * H + 0.06 * (Mott - 0.5)) * Detail);\n"
				// The arris eases down into the joint.
				"float Nx = smoothstep(0.94, 1.0, F.x) - (1.0 - smoothstep(0.0, 0.06, F.x));\n"
				"float Ny = smoothstep(0.94, 1.0, F.y) - (1.0 - smoothstep(0.0, 0.06, F.y));\n"
				"Normal = normalize(float3(float2(Nx, Ny) * 0.35 * Detail, 1.0));\n"
				"Rough = lerp(1.0, 1.15, Joint);\n"
				"return lerp(Brick, Color.rgb * 0.7, Joint);\n");
				return *Code;
			}

			// 正脊: horizontal 4 cm 瓦條 courses, shadowed joint under each lip, butt joints every 40 cm.
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

		// Base colour = Albedo × pattern(Color); pattern Normal → Normal; Rough × Roughness.
		// Albedo defaults to white so the pattern shows alone.
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

			// Same parameter name as the tinted default instance.
			Color->ParameterName = TEXT("Color");
			Color->DefaultValue = FHutongPalette().GetSlotColor(Def.Slot);

			Roughness->ParameterName = TEXT("Roughness");
			Roughness->DefaultValue = Def.Roughness;

			// Pattern between colour and tint; Flat skips it (plain normal, scalar roughness).
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
					// Extra outputs: relief normal and roughness multiplier.
					Pattern->AdditionalOutputs.SetNum(2);
					Pattern->AdditionalOutputs[0].OutputName = TEXT("Normal");
					Pattern->AdditionalOutputs[0].OutputType = CMOT_Float3;
					Pattern->AdditionalOutputs[1].OutputName = TEXT("Rough");
					Pattern->AdditionalOutputs[1].OutputType = CMOT_Float1;
					// Setting AdditionalOutputs alone creates no pins; RebuildOutputs does.
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
		// Avoids a disk lookup per slot per spawn. Misses cached a few seconds, hits until the asset goes.
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

			// Never overwrite: users tune these. Only a missing physical material is added.
			if (FindPackage(nullptr, *PackageName) || FPackageName::DoesPackageExist(PackageName))
			{
				OutSkippedNames.Add(Def.AssetName);
				const FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, Def.AssetName);
				UMaterial* Existing = FindObject<UMaterial>(nullptr, *ObjectPath);
				if (!Existing) Existing = LoadObject<UMaterial>(nullptr, *ObjectPath);
				if (Existing && !Existing->PhysMaterial)
				{
					Existing->PhysMaterial = StarterPhysics(PackagePath, Def.Slot, PackagesToSave);
					Existing->MarkPackageDirty();
					PackagesToSave.AddUnique(Existing->GetPackage());
				}
				continue;
			}

			// No FullyLoad on a fileless package: the failed load GCs (a held factory came back with a
			// dead vtable). Plain NewObject, as the material factory does.
			UPackage* Package = CreatePackage(*PackageName);
			if (!Package) continue;

			UMaterial* Material = NewObject<UMaterial>(Package, FName(Def.AssetName),
				RF_Public | RF_Standalone | RF_Transactional);
			if (!Material) continue;

			BuildGraph(Material, Def);
			Material->PhysMaterial = StarterPhysics(PackagePath, Def.Slot, PackagesToSave);
			UMaterialEditingLibrary::RecompileMaterial(Material);

			FAssetRegistryModule::AssetCreated(Material);
			Package->MarkPackageDirty();
			PackagesToSave.AddUnique(Package);
			OutCreatedNames.Add(Def.AssetName);
		}

		// Save now: an unsaved material is a broken reference on reload.
		if (PackagesToSave.Num() > 0)
		{
			UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, /*bOnlyDirty*/ false);
		}

		return OutCreatedNames.Num();
	}
}
