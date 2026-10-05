#include "Tools/GalleryTool.h"
#include "Tools/HutongPresetDefaults.h"
#include "Tools/HutongPresets.h"
#include "Generation/HutongBuildingComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Engine/StaticMeshActor.h"
#include "InteractiveToolManager.h"
#include "ContextObjectStore.h"
#include "ToolContextInterfaces.h"
#include "SceneManagement.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/FileHelper.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"

using UE::Geometry::FDynamicMesh3;

namespace
{
	// One grid piece: label, footprint, mesh builder, and the component attach that rebuilds it later.
	struct FGalleryItem
	{
		FString Label;
		FVector2D Footprint = FVector2D(300.0, 300.0);
		TFunction<void(FDynamicMesh3&, EHutongDetail)> Build;
		TFunction<void(AStaticMeshActor*)> Attach;
		EHutongGalleryCategory Category = EHutongGalleryCategory::All;
		// Pieces of one kind, kept together on the ground and in the outliner.
		FString Cluster;
	};

	// Band order on the ground, front to back.
	constexpr EHutongGalleryCategory CategoryOrder[] = {
		EHutongGalleryCategory::Houses, EHutongGalleryCategory::Gates, EHutongGalleryCategory::Walls,
		EHutongGalleryCategory::Courtyard, EHutongGalleryCategory::Street, EHutongGalleryCategory::Temples };

	FString HouseCluster(const FString& Preset)
	{
		if (Preset.StartsWith(TEXT("Main Hall"))) return TEXT("Main Halls (正房)");
		if (Preset.StartsWith(TEXT("Side House"))) return TEXT("Side Houses (廂房)");
		if (Preset.StartsWith(TEXT("Front Row")) || Preset.StartsWith(TEXT("Rear Row"))) return TEXT("Front and Rear Rows (倒座房, 後罩房)");
		if (Preset.StartsWith(TEXT("Ear Room"))) return TEXT("Ear Rooms (耳房)");
		return Preset;
	}

	// Faced types are built facing -Y.
	constexpr EHutongBaySide Facing = EHutongBaySide::MinusY;

	template <typename TComponent, typename TSetup>
	TFunction<void(AStaticMeshActor*)> MakeAttach(const FHutongPalette& Palette, TSetup&& Setup)
	{
		return [Palette, Setup](AStaticMeshActor* Actor)
		{
			TComponent* C = NewObject<TComponent>(Actor);
			if (!C) return;
			Setup(C);
			C->Palette = Palette;
			// Without AddInstanceComponent the component is hidden in Details and not saved.
			Actor->AddInstanceComponent(C);
			C->RegisterComponent();
			C->ApplyPlacementAttachments();
		};
	}

	TArray<FGalleryItem> MakeItems(const UHutongGalleryToolProperties* S, const FHutongPalette& Palette)
	{
		TArray<FGalleryItem> Items;
		if (!S) return Items;

		const bool bVar = S->bIncludeVariants;

		// Every piece is stamped with the category and cluster current when it is added; the list is filtered
		// and grouped at the end, so a piece built by one generator can stand with another kind (a wall gate
		// is a 牆垣式門, with the gates).
		EHutongGalleryCategory Cat = EHutongGalleryCategory::Walls;
		FString Cluster;
		auto Add = [&](FGalleryItem&& It)
		{
			It.Category = Cat;
			It.Cluster = Cluster;
			Items.Add(MoveTemp(It));
		};

		// Walls, and the gates built as walls.
		auto AddWall = [&](const FString& Label, TFunction<void(FHutongWallParams&)> Tweak)
		{
			FHutongWallParams P;
			P.Length = 700.0;
			Tweak(P);
			FGalleryItem It;
			It.Label = Label;
			It.Footprint = FVector2D(P.Length, P.GetThickness());
			It.Build = [P](FDynamicMesh3& M, EHutongDetail D)
			{
				UHutongWallBuildingComponent::BuildWallMesh(P, P.Length, P.GetThickness(), false, M, D);
			};
			It.Attach = MakeAttach<UHutongWallBuildingComponent>(Palette,
				[P](UHutongWallBuildingComponent* C)
				{
					C->Params = P;
					C->Length = P.Length;
					C->bLengthAlongY = false;
				});
			Add(MoveTemp(It));
		};

		// --- 牆 ---
		Cat = EHutongGalleryCategory::Walls;
		{
			Cluster = TEXT("Walls (牆)");
			AddWall(TEXT("Wall (牆)"), [](FHutongWallParams&) {});

			// Garden openings.
			Cluster = TEXT("Garden Doorways (牆門洞)");
			AddWall(TEXT("Moon Gate (月亮門)"), [](FHutongWallParams& P)
			{
				P.Role = EHutongWallRole::Courtyard;
				P.Doorway = EHutongWallDoorway::Moon;
			});
			AddWall(TEXT("Plain Doorway (隨牆門)"), [](FHutongWallParams& P)
			{
				P.Role = EHutongWallRole::Courtyard;
				P.Doorway = EHutongWallDoorway::Rect;
			});

			if (bVar)
			{
				AddWall(TEXT("Arched Doorway (拱門)"), [](FHutongWallParams& P)
				{
					P.Role = EHutongWallRole::Courtyard;
					P.Doorway = EHutongWallDoorway::Arch;
				});
				AddWall(TEXT("Octagonal Doorway (八角門)"), [](FHutongWallParams& P)
				{
					P.Role = EHutongWallRole::Courtyard;
					P.Doorway = EHutongWallDoorway::Octagon;
				});
				AddWall(TEXT("Hexagonal Doorway (六角門)"), [](FHutongWallParams& P)
				{
					P.Role = EHutongWallRole::Courtyard;
					P.Doorway = EHutongWallDoorway::Hexagon;
				});
				AddWall(TEXT("Moon Gate (月亮門) · Hanging-Flower Dressing (垂花)"), [](FHutongWallParams& P)
				{
					P.Role = EHutongWallRole::Courtyard;
					P.Doorway = EHutongWallDoorway::Moon;
					P.bDoorwayChuihua = true;
				});
				// 什錦窗: marks a 隔牆 apart from a 院牆.
				Cluster = TEXT("Decorative Windows (什錦窗)");
				AddWall(TEXT("Decorative Windows (什錦窗) Wall · Round Window (圓窗)"), [](FHutongWallParams& P)
				{
					P.Role = EHutongWallRole::Courtyard;
					P.bHasWindows = true;
					P.WindowShape = EHutongWindowShape::Round;
				});
				AddWall(TEXT("Decorative Windows (什錦窗) Wall · Octagonal Window (八角窗)"), [](FHutongWallParams& P)
				{
					P.Role = EHutongWallRole::Courtyard;
					P.bHasWindows = true;
					P.WindowShape = EHutongWindowShape::Octagon;
				});
				AddWall(TEXT("Decorative Windows (什錦窗) Wall · Hexagonal Window (六角窗)"), [](FHutongWallParams& P)
				{
					P.Role = EHutongWallRole::Courtyard;
					P.bHasWindows = true;
					P.WindowShape = EHutongWindowShape::Hexagon;
				});
			}
		}

		// --- 正房 / 廂房 / 倒座房 / 後罩房 / 耳房 --- from the shipped presets.
		Cat = EHutongGalleryCategory::Houses;
		{
			TArray<FString> Names = HutongPresets::BuiltInSiheyuanNames();
			if (!bVar && Names.Num() > 1) Names.SetNum(1);

			for (const FString& Name : Names)
			{
				FHutongSiheyuanParams P;
				if (!UHutongPresetLibrary::Get()->LoadPreset(
						TEXT("Siheyuan"), Name, FHutongSiheyuanParams::StaticStruct(), &P))
				{
					continue;
				}
				const double SX = (P.SuggestedFrontage > 0.0) ? P.SuggestedFrontage : 900.0;
				const double SY = (P.GetSuggestedDepth() > 0.0) ? P.GetSuggestedDepth() : 500.0;

				Cluster = HouseCluster(Name);
				FGalleryItem It;
				It.Label = Name;
				It.Footprint = FVector2D(SX, SY);
				It.Build = [P, SX, SY](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(P, Facing, 0, SX, SY, M, D);
				};
				// The preset is named, so the piece reads as that preset and not as customized.
				It.Attach = MakeAttach<UHutongSiheyuanBuildingComponent>(Palette,
					[P, SX, SY, Name](UHutongSiheyuanBuildingComponent* C)
					{
						C->Params = P;
						C->Preset = Name;
						C->FootprintX = SX; C->FootprintY = SY; C->BaySide = Facing;
					});
				Add(MoveTemp(It));
			}

			// Sumptuary rule: same house in 合瓦 and in 筒瓦.
			if (bVar && Names.Num() > 0)
			{
				Cluster = HouseCluster(Names[0]);
				FHutongSiheyuanParams P;
				if (UHutongPresetLibrary::Get()->LoadPreset(
						TEXT("Siheyuan"), Names[0], FHutongSiheyuanParams::StaticStruct(), &P))
				{
					P.RoofTile = EHutongRoofTile::Tong;
					const double SX = (P.SuggestedFrontage > 0.0) ? P.SuggestedFrontage : 900.0;
					const double SY = (P.GetSuggestedDepth() > 0.0) ? P.GetSuggestedDepth() : 500.0;

					FGalleryItem It;
					It.Label = FString::Printf(TEXT("%s · Tube Tiles (筒瓦), Above Rank (逾制)"), *Names[0]);
					It.Footprint = FVector2D(SX, SY);
					It.Build = [P, SX, SY](FDynamicMesh3& M, EHutongDetail D)
					{
						UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(P, Facing, 0, SX, SY, M, D);
					};
					It.Attach = MakeAttach<UHutongSiheyuanBuildingComponent>(Palette,
						[P, SX, SY](UHutongSiheyuanBuildingComponent* C)
						{
							C->Params = P;
							C->FootprintX = SX; C->FootprintY = SY; C->BaySide = Facing;
						});
					Add(MoveTemp(It));

					// 徹上明造: the same hall with no ceiling, its frame open overhead.
					FHutongSiheyuanParams F = P;
					F.RoofTile = EHutongRoofTile::He;
					F.bExposedFrame = true;
					FGalleryItem Fr;
					Fr.Label = FString::Printf(TEXT("%s · Exposed Frame (徹上明造)"), *Names[0]);
					Fr.Footprint = FVector2D(SX, SY);
					Fr.Build = [F, SX, SY](FDynamicMesh3& M, EHutongDetail D)
					{
						UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(F, Facing, 0, SX, SY, M, D);
					};
					Fr.Attach = MakeAttach<UHutongSiheyuanBuildingComponent>(Palette,
						[F, SX, SY](UHutongSiheyuanBuildingComponent* C)
						{
							C->Params = F;
							C->FootprintX = SX; C->FootprintY = SY; C->BaySide = Facing;
						});
					Add(MoveTemp(Fr));
				}

				// A preset again with one thing changed, labelled "<preset> · <what>".
				auto AddVariant = [&](const FString& Preset, const TCHAR* What, TFunctionRef<void(FHutongSiheyuanParams&)> Change)
				{
					FHutongSiheyuanParams V;
					if (!UHutongPresetLibrary::Get()->LoadPreset(TEXT("Siheyuan"), Preset, FHutongSiheyuanParams::StaticStruct(), &V)) return;
					Change(V);
					const double SX = (V.SuggestedFrontage > 0.0) ? V.SuggestedFrontage : 900.0;
					const double SY = (V.GetSuggestedDepth() > 0.0) ? V.GetSuggestedDepth() : 500.0;
					Cluster = HouseCluster(Preset);
					FGalleryItem It;
					It.Label = FString::Printf(TEXT("%s · %s"), *Preset, What);
					It.Footprint = FVector2D(SX, SY);
					It.Build = [V, SX, SY](FDynamicMesh3& M, EHutongDetail D)
					{
						UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(V, Facing, 0, SX, SY, M, D);
					};
					It.Attach = MakeAttach<UHutongSiheyuanBuildingComponent>(Palette,
						[V, SX, SY](UHutongSiheyuanBuildingComponent* C)
						{
							C->Params = V;
							C->FootprintX = SX; C->FootprintY = SY; C->BaySide = Facing;
						});
					Add(MoveTemp(It));
				};
				// 檻牆 faces (p.90) and 封護檐 cornices (圖5-3-9) on the houses that show them.
				AddVariant(Names[0], TEXT("Framed Brick Pool Sill Walls (海棠池子)"),
					[](FHutongSiheyuanParams& V) { V.SillWallFinish = EHutongSillWall::Pool; });
				const FString Row = TEXT("Front Row (倒座房)");
				AddVariant(Row, TEXT("Rounded Rear Cornice (雞素子檐)"), [](FHutongSiheyuanParams& V) { V.RearCornice = EHutongSealedCornice::Rounded; });
				AddVariant(Row, TEXT("Drawer Rear Cornice (抽屜檐)"), [](FHutongSiheyuanParams& V) { V.RearCornice = EHutongSealedCornice::Drawer; });
				AddVariant(Row, TEXT("Seven-Course Rear Cornice (七層)"), [](FHutongSiheyuanParams& V) { V.RearCornice = EHutongSealedCornice::SevenCourse; });
			}
		}

		// --- 大門 and 垂花門 ---
		Cat = EHutongGalleryCategory::Gates;
		{
			Cluster = TEXT("Gate Houses (屋宇式門)");
			const EHutongGateStyle Styles[] = {
				EHutongGateStyle::Guangliang, EHutongGateStyle::Jinzhu,
				EHutongGateStyle::Manzi, EHutongGateStyle::Ruyi };
			const TCHAR* StyleNames[] = {
				TEXT("Wide-Hall Gate (廣亮大門)"), TEXT("Inner-Column Gate (金柱大門)"),
				TEXT("Flush Gate (蠻子門)"), TEXT("Ruyi Gate (如意門)") };

			// The 廣亮大門 again on 抱鼓石, and with its 反八字影壁; laid beside it.
			auto AddGuangliangVariants = [&]()
			{
				FHutongGateHouseParams P;
				P.Style = EHutongGateStyle::Guangliang;
				P.RandomSeed = S->RandomSeed + 11;
				P.DoorStones.Style = EHutongDoorStone::Drum;
				const FHutongGateHouseParams::FSizeRange R = P.GetSizeRange();
				const double SX = (R.FrontageMax > 0.0) ? 0.5 * (R.FrontageMin + R.FrontageMax) : 360.0;
				const double SY = (R.DepthMax > 0.0) ? 0.5 * (R.DepthMin + R.DepthMax) : 340.0;

				FGalleryItem It;
				It.Label = TEXT("Wide-Hall Gate (廣亮大門) · Drum Door Stone (抱鼓石)");
				FHutongGateHouseParams F = P;
				F.DoorStones.Style = EHutongDoorStone::Block;
				F.bSplayedScreens = true;
				It.Footprint = FVector2D(SX, SY);
				It.Build = [P, SX, SY](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongGateHouseBuildingComponent::BuildGateHouseMesh(P, Facing, SX, SY, M, D);
				};
				It.Attach = MakeAttach<UHutongGateHouseBuildingComponent>(Palette,
					[P, SX, SY](UHutongGateHouseBuildingComponent* C)
					{
						C->Params = P;
						C->FootprintX = SX; C->FootprintY = SY; C->BaySide = Facing;
					});
				Add(MoveTemp(It));

				// 反八字影壁 before the gate (圖5-1-2).
				FGalleryItem Fr;
				Fr.Label = TEXT("Wide-Hall Gate (廣亮大門) · Splayed Screen Walls (反八字影壁)");
				Fr.Footprint = FVector2D(SX, SY);
				Fr.Build = [F, SX, SY](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongGateHouseBuildingComponent::BuildGateHouseMesh(F, Facing, SX, SY, M, D);
				};
				Fr.Attach = MakeAttach<UHutongGateHouseBuildingComponent>(Palette,
					[F, SX, SY](UHutongGateHouseBuildingComponent* C)
					{
						C->Params = F;
						C->FootprintX = SX; C->FootprintY = SY; C->BaySide = Facing;
					});
				Add(MoveTemp(Fr));
			};

			const int32 GateCount = bVar ? UE_ARRAY_COUNT(Styles) : 1;
			for (int32 i = 0; i < GateCount; ++i)
			{
				FHutongGateHouseParams P;
				P.Style = Styles[i];
				P.RandomSeed = S->RandomSeed + i;
				// Middle of the style's size band, as the compound sizes its gate.
				const FHutongGateHouseParams::FSizeRange R = P.GetSizeRange();
				const double SX = (R.FrontageMax > 0.0) ? 0.5 * (R.FrontageMin + R.FrontageMax) : 360.0;
				const double SY = (R.DepthMax > 0.0) ? 0.5 * (R.DepthMin + R.DepthMax) : 340.0;

				auto AddGate = [&](const FHutongGateHouseParams& Gate, const FString& Label)
				{
					FGalleryItem It;
					It.Label = Label;
					It.Footprint = FVector2D(SX, SY);
					It.Build = [Gate, SX, SY](FDynamicMesh3& M, EHutongDetail D)
					{
						UHutongGateHouseBuildingComponent::BuildGateHouseMesh(Gate, Facing, SX, SY, M, D);
					};
					It.Attach = MakeAttach<UHutongGateHouseBuildingComponent>(Palette,
						[Gate, SX, SY](UHutongGateHouseBuildingComponent* C)
						{
							C->Params = Gate;
							C->FootprintX = SX; C->FootprintY = SY; C->BaySide = Facing;
						});
					Add(MoveTemp(It));
				};
				AddGate(P, StyleNames[i]);
				// 如意門 with its 門頭 left plain (素活), as the text allows (圖5-1-5.2).
				if (P.Style == EHutongGateStyle::Ruyi && bVar)
				{
					FHutongGateHouseParams Plain = P;
					Plain.bCarvedDoorHead = false;
					AddGate(Plain, TEXT("Ruyi Gate (如意門) · Plain Door Head (素活門頭)"));
				}
				if (i == 0 && bVar) AddGuangliangVariants();
			}

			// 牆垣式門: the gates that stand in a wall rather than a building (四合院建築及其構造 §5-1).
			Cluster = TEXT("Wall Gates (牆垣式門)");
			if (bVar)
			{
				AddWall(TEXT("Wall Gate (牆垣門) · Block Door Stone (方門墩)"), [](FHutongWallParams& P)
				{
					P.bHasGate = true;
					P.DoorStones.Style = EHutongDoorStone::Block;
				});
				AddWall(TEXT("Wall Gate (牆垣門) · Drum Door Stone (抱鼓石)"), [](FHutongWallParams& P)
				{
					P.bHasGate = true;
					P.DoorStones.Style = EHutongDoorStone::Drum;
				});
			}
			AddWall(TEXT("Wall-Mounted Inner Gate (牆垣式垂花門)"), [](FHutongWallParams& P)
			{
				P.Role = EHutongWallRole::Courtyard;
				P.Doorway = EHutongWallDoorway::Rect;
				P.bDoorwayChuihua = true;
			});

			Cluster = TEXT("Inner Gates (垂花門)");


			for (const int32 Variant : { 0, 1, 2, 3 })
			{
				const EHutongInnerGateStyle Style = (Variant % 2 == 0) ? EHutongInnerGateStyle::SinglePost : EHutongInnerGateStyle::OneHallOneRoll;
				const bool bFrame = Variant >= 2;
				if (bFrame && !bVar) continue;
				FHutongInnerGateParams P;
				P.Style = Style;
				P.bExposedFrame = bFrame;
				const FHutongInnerGateParams::FSizeRange R = P.GetSizeRange();
				const double SX = (R.FrontageMax > 0.0) ? 0.5 * (R.FrontageMin + R.FrontageMax) : 330.0;
				const double SY = (R.DepthMax > 0.0) ? 0.5 * (R.DepthMin + R.DepthMax) : 150.0;

				FGalleryItem It;
				It.Label = (Style == EHutongInnerGateStyle::SinglePost)
					? TEXT("Inner Gate (垂花門)") : TEXT("Inner Gate (垂花門) · Hall and Roll (一殿一卷)");
				if (bFrame) It.Label += TEXT(" · Exposed Frame (徹上明造)");
				It.Footprint = FVector2D(SX, SY);
				It.Build = [P, SX, SY](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongInnerGateBuildingComponent::BuildInnerGateMesh(P, Facing, SX, SY, M, D);
				};
				It.Attach = MakeAttach<UHutongInnerGateBuildingComponent>(Palette,
					[P, SX, SY](UHutongInnerGateBuildingComponent* C)
					{
						C->Params = P;
						C->Width = SX; C->Depth = SY; C->BaySide = Facing;
					});
				Add(MoveTemp(It));
			}
		}

		// --- 遊廊, 影壁, 甬路 ---
		Cat = EHutongGalleryCategory::Courtyard;
		{
			Cluster = TEXT("Covered Corridors (遊廊)");
			// Every 遊廊 is open to its frame (徹上明造), so no separate variant.
			auto AddCorridor = [&](const FString& Label, bool bClosed)
			{
				FHutongCorridorParams P;
				P.bClosedSide = bClosed;
				P.Length = 900.0;
				P.Width = 170.0;
				const double Dep = P.GetFootprintDepth();

				FGalleryItem It;
				It.Label = Label;
				It.Footprint = FVector2D(P.Length, Dep);
				It.Build = [P](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongCorridorBuildingComponent::BuildCorridorMesh(P, P.Length, false, false, M, D);
				};
				It.Attach = MakeAttach<UHutongCorridorBuildingComponent>(Palette,
					[P](UHutongCorridorBuildingComponent* C)
					{
						C->Params = P;
						C->Length = P.Length; C->Width = P.Width;
						C->bLengthAlongY = false; C->bFlipOpenSide = false;
					});
				Add(MoveTemp(It));
			};
			AddCorridor(TEXT("Covered Corridor (遊廊)"), false);
			if (bVar) AddCorridor(TEXT("Ring Corridor (抄手遊廊) · Closed Side"), true);

			Cluster = TEXT("Screen Walls (影壁)");
			{
				FHutongScreenWallParams P;
				P.Length = 460.0;
				FGalleryItem It;
				It.Label = TEXT("Screen Wall (影壁)");
				It.Footprint = FVector2D(P.Length, P.GetFootprintDepth());
				It.Build = [P](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongScreenWallBuildingComponent::BuildScreenWallMesh(P, P.Length, false, M, D);
				};
				It.Attach = MakeAttach<UHutongScreenWallBuildingComponent>(Palette,
					[P](UHutongScreenWallBuildingComponent* C)
					{
						C->Params = P;
						C->Length = P.Length; C->bLengthAlongY = false;
					});
				Add(MoveTemp(It));
			}

			Cluster = TEXT("Paths, Flower Beds and Water Jars (甬路, 花池, 魚缸)");
			{
				FHutongPathParams P;
				const double Len = 700.0;
				const double Wid = 150.0;
				FGalleryItem It;
				It.Label = TEXT("Paved Path (甬路)");
				It.Footprint = FVector2D(Len, Wid + 2.0 * P.GetKerbWidth());
				It.Build = [P, Len, Wid](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongPathBuildingComponent::BuildPathMesh(P, Len, Wid, false, M, D);
				};
				It.Attach = MakeAttach<UHutongPathBuildingComponent>(Palette,
					[P, Len, Wid](UHutongPathBuildingComponent* C)
					{
						C->Params = P;
						C->Length = Len; C->Width = Wid; C->bLengthAlongY = false;
					});
				Add(MoveTemp(It));
			}

			// 天棚魚缸石榴樹.
			{
				FHutongFlowerBedParams P;
				const double SX = 220.0, SY = 150.0;
				FGalleryItem It;
				It.Label = TEXT("Flower Bed (花池)");
				It.Footprint = FVector2D(SX, SY);
				It.Build = [P, SX, SY](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongFlowerBedBuildingComponent::BuildFlowerBedMesh(P, SX, SY, M, D);
				};
				It.Attach = MakeAttach<UHutongFlowerBedBuildingComponent>(Palette,
					[P, SX, SY](UHutongFlowerBedBuildingComponent* C)
					{
						C->Params = P;
						C->FootprintX = SX; C->FootprintY = SY;
					});
				Add(MoveTemp(It));
			}

			{
				FHutongWaterJarParams P;
				FGalleryItem It;
				It.Label = TEXT("Water Jar (魚缸)");
				const double Span = P.GetFootprint();
				It.Footprint = FVector2D(Span, Span);
				It.Build = [P](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongWaterJarBuildingComponent::BuildWaterJarMesh(P, M, D);
				};
				It.Attach = MakeAttach<UHutongWaterJarBuildingComponent>(Palette,
					[P](UHutongWaterJarBuildingComponent* C) { C->Params = P; });
				Add(MoveTemp(It));
			}
		}

		// --- 耳房, alone and with its 過道 --- from the ear room tool's shipped presets.
		Cat = EHutongGalleryCategory::Houses;
		{
			Cluster = HouseCluster(TEXT("Ear Room"));
			for (const FString& Name : HutongPresets::BuiltInEarRoomNames())
			{
				FHutongEarPassageParams P;
				if (!UHutongPresetLibrary::Get()->LoadPreset(
						TEXT("EarPassage"), Name, FHutongEarPassageParams::StaticStruct(), &P))
				{
					continue;
				}
				// Without variants, the one the compound builds.
				if (!bVar && P.Passageway != EHutongEarPassage::AtEnd) continue;
				const double SX = P.GetSuggestedWidth() > 0.0 ? P.GetSuggestedWidth() : 760.0;
				const double SY = P.Room.GetSuggestedDepth() > 0.0 ? P.Room.GetSuggestedDepth() : 340.0;
				FGalleryItem It;
				It.Label = Name;
				It.Footprint = FVector2D(SX, SY);
				It.Build = [P, SX, SY](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongEarPassageBuildingComponent::BuildEarPassageMesh(P, Facing, SX, SY, M, D);
				};
				It.Attach = MakeAttach<UHutongEarPassageBuildingComponent>(Palette,
					[P, SX, SY, Name](UHutongEarPassageBuildingComponent* C)
					{
						C->Params = P;
						C->Preset = Name;
						C->FootprintX = SX; C->FootprintY = SY; C->BaySide = Facing;
					});
				Add(MoveTemp(It));
			}

			// 過道: the roofed slot between a gable and the wall beside it, as the compound builds it.
			{
				FHutongPassageParams Pa;
				const double Run = 300.0, Clear = 200.0;
				FGalleryItem Ps;
				Ps.Label = TEXT("Passage (過道)");
				Pa.Width = Clear;
				Ps.Footprint = FVector2D(Run, Pa.GetRoofSpan());
				Ps.Build = [Pa, Run, Clear](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongPassageBuildingComponent::BuildPassageMesh(Pa, Run, Clear, false, M, D);
				};
				Ps.Attach = MakeAttach<UHutongPassageBuildingComponent>(Palette,
					[Pa, Run, Clear](UHutongPassageBuildingComponent* C)
					{
						C->Params = Pa;
						C->Length = Run; C->Width = Clear; C->bLengthAlongY = false;
					});
				Add(MoveTemp(Ps));
			}

			// 構架: the house's frame alone, as the frame tool stamps it; with its rafters as a variant.
			Cluster = TEXT("Timber Frames (構架)");
			for (const bool bRafters : { false, true })
			{
				if (bRafters && !bVar) continue;
				FHutongFrameParams Fp;
				Fp.bHasRafters = bRafters;
				const double FX = (Fp.House.SuggestedFrontage > 0.0) ? Fp.House.SuggestedFrontage : 1060.0;
				const double FY = (Fp.House.GetSuggestedDepth() > 0.0) ? Fp.House.GetSuggestedDepth() : 600.0;
				FGalleryItem Fr;
				Fr.Label = bRafters ? TEXT("Timber Frame (構架) · With Rafters (椽)") : TEXT("Timber Frame (構架)");
				Fr.Footprint = FVector2D(FX, FY);
				Fr.Build = [Fp, FX, FY](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongFrameBuildingComponent::BuildFrameMesh(Fp, Facing, 0, FX, FY, M, D);
				};
				Fr.Attach = MakeAttach<UHutongFrameBuildingComponent>(Palette,
					[Fp, FX, FY](UHutongFrameBuildingComponent* C)
					{
						C->Params = Fp;
						C->FootprintX = FX; C->FootprintY = FY; C->BaySide = Facing;
					});
				Add(MoveTemp(Fr));
			}
		}

		// --- 鋪面房 and 牌坊 ---
		Cat = EHutongGalleryCategory::Street;
		{
			auto AddShop = [&](const FString& Label, int32 OpenBays)
			{
				FHutongShopfrontParams P;
				P.OpenBayCount = OpenBays;
				const double SX = 1000.0, SY = 520.0;

				FGalleryItem It;
				It.Label = Label;
				It.Footprint = FVector2D(SX, SY);
				It.Build = [P, SX, SY](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongShopfrontBuildingComponent::BuildShopfrontMesh(
						P, Facing, P.BayCountOverride, SX, SY, M, D);
				};
				It.Attach = MakeAttach<UHutongShopfrontBuildingComponent>(Palette,
					[P, SX, SY](UHutongShopfrontBuildingComponent* C)
					{
						C->Params = P;
						C->FootprintX = SX; C->FootprintY = SY; C->BaySide = Facing;
					});
				Add(MoveTemp(It));
			};
			Cluster = TEXT("Shopfronts (鋪面房)");
			AddShop(TEXT("Shopfront (鋪面房)"), 1);
			if (bVar) AddShop(TEXT("Shopfront (鋪面房) · Boarded Up"), 0);

			auto AddStorey = [&](const FString& Label, bool bGallery)
			{
				FHutongStoreyParams P;
				P.bHasGallery = bGallery;
				P.bHasSkirtRoof = bGallery;
				const double SX = 1000.0, SY = 620.0;

				FGalleryItem It;
				It.Label = Label;
				It.Footprint = FVector2D(SX, SY);
				It.Build = [P, SX, SY](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongStoreyBuildingComponent::BuildStoreyMesh(
						P, Facing, P.BayCountOverride, SX, SY, M, D);
				};
				It.Attach = MakeAttach<UHutongStoreyBuildingComponent>(Palette,
					[P, SX, SY](UHutongStoreyBuildingComponent* C)
					{
						C->Params = P;
						C->FootprintX = SX; C->FootprintY = SY; C->BaySide = Facing;
					});
				Add(MoveTemp(It));
			};
			Cluster = TEXT("Storeyed Buildings (樓)");
			AddStorey(TEXT("Multi-Story Building (樓)"), true);
			// Without the storey gallery, for comparison.
			if (bVar) AddStorey(TEXT("Multi-Story Building (樓) · No Gallery"), false);

			auto AddPaifang = [&](const FString& Label, EHutongPaifangBays Bays, bool bRoofs, double Len)
			{
				FHutongPaifangParams P;
				P.BayCount = Bays;
				P.bHasRoofs = bRoofs;
				P.Length = Len;
				// Footprint depth must contain the 夾杆石, the deepest thing at ground level.
				const double Dep = FMath::Max(
					2.0 * (P.GetColumnRadius() + FMath::Max(P.PlinthSpread, 0.0)), 20.0);
				P.Depth = Dep;

				FGalleryItem It;
				It.Label = Label;
				It.Footprint = FVector2D(Len, Dep);
				It.Build = [P, Len, Dep](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongPaifangBuildingComponent::BuildPaifangMesh(P, Len, Dep, false, M, D);
				};
				It.Attach = MakeAttach<UHutongPaifangBuildingComponent>(Palette,
					[P, Len, Dep](UHutongPaifangBuildingComponent* C)
					{
						C->Params = P;
						C->Length = Len; C->Depth = Dep; C->bLengthAlongY = false;
					});
				Add(MoveTemp(It));
			};
			Cluster = TEXT("Memorial Arches (牌坊, 牌樓)");
			AddPaifang(TEXT("Roofed Memorial Arch (牌樓) · Three Bays, Four Posts (三間四柱)"), EHutongPaifangBays::Three, true, 1000.0);
			if (bVar)
			{
				AddPaifang(TEXT("Memorial Arch (牌坊) · One Bay, Two Posts (一間二柱) · Stone"), EHutongPaifangBays::One, false, 480.0);
			}
		}

		// --- 亭 and 殿: the roof types ---
		Cat = EHutongGalleryCategory::Temples;
		{
			auto AddPavilion = [&](const FString& Label, EHutongRoofType Roof, double Roll, bool bRound = false)
			{
				FHutongPavilionParams P;
				P.RoofType = Roof;
				P.RoofApexRoll = Roll;
				if (bRound)
				{
					P.Plan = EHutongPavilionPlan::Round;
					P.bHasFrieze = true;
				}
				// The 則例 figure's own 面闊, columns' outer faces.
				const double SX = HutongCanon::Pavilion::FigureBayCm * (1.0 + HutongCanon::Pavilion::ColumnPerBay), SY = SX;

				FGalleryItem It;
				It.Label = Label;
				It.Footprint = FVector2D(SX, SY);
				It.Build = [P, SX, SY](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongPavilionBuildingComponent::BuildPavilionMesh(P, SX, SY, M, D);
				};
				It.Attach = MakeAttach<UHutongPavilionBuildingComponent>(Palette,
					[P, SX, SY](UHutongPavilionBuildingComponent* C)
					{
						C->Params = P;
						C->Width = SX; C->Depth = SY;
					});
				Add(MoveTemp(It));
			};
			Cluster = TEXT("Pavilions (亭)");
			AddPavilion(TEXT("Pavilion (亭) · Pyramidal Roof (攢尖)"), EHutongRoofType::Cuanjian, 0.0);
			AddPavilion(TEXT("Pavilion (亭) · Round, Six Columns (六柱圓亭)"), EHutongRoofType::Cuanjian, 0.0, true);
			if (bVar)
			{
				AddPavilion(TEXT("Pavilion (亭) · Hipped Roof (廡殿)"), EHutongRoofType::Wudian, 0.0);
				AddPavilion(TEXT("Pavilion (亭) · Hip-and-Gable Roof (歇山)"), EHutongRoofType::Xieshan, 0.0);
				AddPavilion(TEXT("Pavilion (亭) · Rolled Hip-and-Gable Roof (捲棚歇山)"), EHutongRoofType::Xieshan, 0.35);
			}

			// 則例 卷二's 大式 hall at the figure's own size.
			Cluster = TEXT("Temple Halls (殿)");
			{
				FHutongHallParams P;
				P.Style = EHutongHallStyle::Grand;
				P.RoofTile = EHutongRoofTile::Tong;
				namespace GH = HutongCanon::GrandHall;
				const double SX = (GH::Frontage + GH::EaveColumn) * HutongCanon::Pavilion::FigureBayCm / 10.0;
				const double SY = (GH::Depth + GH::EaveColumn) * HutongCanon::Pavilion::FigureBayCm / 10.0;
				FGalleryItem It;
				It.Label = TEXT("Temple Hall (殿) · Grand, Nine Purlins (大式 九檁歇山)");
				It.Footprint = FVector2D(SX, SY);
				It.Build = [P, SX, SY](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongHallBuildingComponent::BuildHallMesh(P, Facing, 0, SX, SY, M, D);
				};
				It.Attach = MakeAttach<UHutongHallBuildingComponent>(Palette,
					[P, SX, SY](UHutongHallBuildingComponent* C)
					{
						C->Params = P;
						C->FootprintX = SX; C->FootprintY = SY;
						C->BaySide = Facing;
					});
				Add(MoveTemp(It));
			}

			auto AddHall = [&](const FString& Label, EHutongRoofType Roof, double Roll, bool bFrame = false)
			{
				FHutongHallParams P;
				P.RoofType = Roof;
				P.RoofApexRoll = Roll;
				P.bExposedFrame = bFrame;
				const double SX = 1000.0, SY = 660.0;

				FGalleryItem It;
				It.Label = Label;
				It.Footprint = FVector2D(SX, SY);
				It.Build = [P, SX, SY](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongHallBuildingComponent::BuildHallMesh(P, Facing, 0, SX, SY, M, D);
				};
				It.Attach = MakeAttach<UHutongHallBuildingComponent>(Palette,
					[P, SX, SY](UHutongHallBuildingComponent* C)
					{
						C->Params = P;
						C->FootprintX = SX; C->FootprintY = SY; C->BaySide = Facing;
					});
				Add(MoveTemp(It));
			};
			AddHall(TEXT("Temple Hall (殿) · Hip-and-Gable Roof (歇山)"), EHutongRoofType::Xieshan, 0.0);
			if (bVar)
			{
				AddHall(TEXT("Temple Hall (殿) · Hipped Roof (廡殿)"), EHutongRoofType::Wudian, 0.0);
				AddHall(TEXT("Temple Hall (殿) · Rolled Hip-and-Gable Roof (捲棚歇山)"), EHutongRoofType::Xieshan, 0.35);
				AddHall(TEXT("Temple Hall (殿) · Hip-and-Gable Roof (歇山) · Exposed Frame (徹上明造)"), EHutongRoofType::Xieshan, 0.0, true);
				AddHall(TEXT("Temple Hall (殿) · Hipped Roof (廡殿) · Exposed Frame (徹上明造)"), EHutongRoofType::Wudian, 0.0, true);
			}
		}

		// The categories asked for, in band order, each cluster together in the order it was first met.
		TArray<FGalleryItem> Out;
		for (const EHutongGalleryCategory C : CategoryOrder)
		{
			if (!S->Includes(C)) continue;
			TArray<FString> Clusters;
			for (const FGalleryItem& It : Items)
			{
				if (It.Category == C) Clusters.AddUnique(It.Cluster);
			}
			for (const FString& Name : Clusters)
			{
				for (const FGalleryItem& It : Items)
				{
					if (It.Category == C && It.Cluster == Name) Out.Add(It);
				}
			}
		}
		return Out;
	}

	// Where every piece goes: categories are bands front to back, clusters sit together inside them.
	struct FGalleryPlan
	{
		// Min corner of each piece (the corner its mesh is built around), in the gallery's frame.
		TArray<FVector2D> Origins;
		// Ground each cluster and each category covers, for the preview.
		TArray<FBox2D> ClusterBoxes;
		TArray<FBox2D> CategoryBoxes;
		int32 ClusterCount = 0;
		double TotalX = 0.0, TotalY = 0.0;
	};

	// Rows fill left to right. Small clusters share a row while its piece count stays within PerRow, a wider
	// gap between them; a larger cluster starts its own rows and wraps. Fronts (facing -Y) line up on a row.
	// Clusters stand twice the spacing apart, categories three times.
	FGalleryPlan LayOut(const TArray<FGalleryItem>& Items, double Spacing, int32 PerRowIn)
	{
		FGalleryPlan G;
		G.Origins.SetNum(Items.Num());
		const double Gap = FMath::Max(Spacing, 50.0);
		const double ClusterGap = 2.0 * Gap;
		const double CategoryGap = 3.0 * Gap;
		const int32 PerRow = FMath::Clamp(PerRowIn, 1, 16);

		double Y = Gap;
		int32 i = 0;
		while (i < Items.Num())
		{
			const EHutongGalleryCategory Cat = Items[i].Category;
			if (i > 0) Y += CategoryGap - Gap;
			FBox2D CatBox(ForceInit);

			double X = Gap, RowDepth = 0.0;
			int32 RowCount = 0;
			auto CloseRow = [&]()
			{
				if (RowCount == 0) return;
				Y += RowDepth + Gap;
				X = Gap; RowDepth = 0.0; RowCount = 0;
			};

			while (i < Items.Num() && Items[i].Category == Cat)
			{
				const FString ClusterName = Items[i].Cluster;
				int32 End = i;
				while (End < Items.Num() && Items[End].Category == Cat && Items[End].Cluster == ClusterName) ++End;
				if (RowCount > 0 && RowCount + (End - i) > PerRow) CloseRow();
				if (RowCount > 0) X += ClusterGap - Gap;

				FBox2D ClusterBox(ForceInit);
				for (; i < End; ++i)
				{
					if (RowCount >= PerRow) CloseRow();
					const FVector2D& F = Items[i].Footprint;
					G.Origins[i] = FVector2D(X, Y);
					ClusterBox += FBox2D(G.Origins[i], G.Origins[i] + F);
					X += F.X + Gap;
					RowDepth = FMath::Max(RowDepth, F.Y);
					++RowCount;
					G.TotalX = FMath::Max(G.TotalX, X);
				}
				G.ClusterBoxes.Add(ClusterBox);
				CatBox += ClusterBox;
				++G.ClusterCount;
			}
			CloseRow();
			G.CategoryBoxes.Add(CatBox);
		}
		G.TotalY = Y;
		return G;
	}
}

TArray<UClass*> HutongGallery::AttachedClasses(const UHutongGalleryToolProperties* Settings, UWorld* World)
{
	TArray<UClass*> Out;
	if (!World) return Out;
	for (const FGalleryItem& It : MakeItems(Settings, FHutongPalette()))
	{
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
		if (!Actor) continue;
		if (It.Attach) It.Attach(Actor);
		if (const UHutongBuildingComponent* C = Actor->FindComponentByClass<UHutongBuildingComponent>()) Out.AddUnique(C->GetClass());
		World->DestroyActor(Actor);
	}
	return Out;
}

void HutongGallery::ForEachBuilt(const UHutongGalleryToolProperties* Settings, EHutongDetail Level,
	TFunctionRef<void(const FString&, const FVector2D&, const FDynamicMesh3&)> Visit)
{
	for (const FGalleryItem& It : MakeItems(Settings, FHutongPalette()))
	{
		FDynamicMesh3 Mesh;
		It.Build(Mesh, Level);
		Visit(It.Label, It.Footprint, Mesh);
	}
}

void HutongGallery::WriteObj(const UHutongGalleryToolProperties* Settings, const FString& Path)
{
	FString Out;
	int32 Base = 1;
	double X = 0.0;
	for (const FGalleryItem& It : MakeItems(Settings, FHutongPalette()))
	{
		FDynamicMesh3 Mesh;
		It.Build(Mesh, EHutongDetail::Near);
		Out += FString::Printf(TEXT("# item %s %.0f\n"), *It.Label.Replace(TEXT(" "), TEXT("_")), X);
		TMap<int32, int32> Index;
		for (const int32 Vid : Mesh.VertexIndicesItr())
		{
			const FVector3d P = Mesh.GetVertex(Vid);
			Out += FString::Printf(TEXT("v %.2f %.2f %.2f\n"), P.X + X, P.Y, P.Z);
			Index.Add(Vid, Base + Index.Num());
		}
		const auto* Mat = Mesh.HasAttributes() ? Mesh.Attributes()->GetMaterialID() : nullptr;
		int32 Last = -1;
		for (const int32 Tid : Mesh.TriangleIndicesItr())
		{
			const int32 Slot = Mat ? Mat->GetValue(Tid) : 0;
			if (Slot != Last) { Out += FString::Printf(TEXT("usemtl s%d\n"), Slot); Last = Slot; }
			const UE::Geometry::FIndex3i T = Mesh.GetTriangle(Tid);
			Out += FString::Printf(TEXT("f %d %d %d\n"), Index[T.A], Index[T.B], Index[T.C]);
		}
		Base += Index.Num();
		X += 1500.0;
	}
	FFileHelper::SaveStringToFile(Out, *Path);
}

TArray<HutongGallery::FHutongCostRow> HutongGallery::BuildCostReport(
	const UHutongGalleryToolProperties* Settings)
{
	TArray<FHutongCostRow> Out;
	for (const FGalleryItem& It : MakeItems(Settings, FHutongPalette()))
	{
		FHutongCostRow Row;
		Row.Label = It.Label;

		for (int32 Level = 0; Level < FHutongCostRow::NumLevels; ++Level)
		{
			FDynamicMesh3 Mesh;
			const double Start = FPlatformTime::Seconds();
			It.Build(Mesh, (EHutongDetail)Level);
			Row.BuildMs[Level] = (FPlatformTime::Seconds() - Start) * 1000.0;

			Row.Triangles[Level] = Mesh.TriangleCount();
			Row.Vertices[Level] = Mesh.VertexCount();

			// Count distinct slots used; the mesh is not compacted.
			if (const UE::Geometry::FDynamicMeshMaterialAttribute* MatIDs =
				Mesh.HasAttributes() ? Mesh.Attributes()->GetMaterialID() : nullptr)
			{
				bool bSeen[HutongGen::MatSlot_Count] = {};
				for (const int32 tid : Mesh.TriangleIndicesItr())
				{
					bSeen[FMath::Clamp(MatIDs->GetValue(tid), 0, HutongGen::MatSlot_Count - 1)] = true;
				}
				for (const bool b : bSeen)
				{
					if (b) ++Row.MaterialSlots[Level];
				}
			}
		}

		Out.Add(MoveTemp(Row));
	}
	return Out;
}

bool UHutongGalleryToolProperties::Includes(EHutongGalleryCategory C) const
{
	if (GalleryCategory != EHutongGalleryCategory::All) return C == GalleryCategory;
	switch (C)
	{
	case EHutongGalleryCategory::Walls:     return bWalls;
	case EHutongGalleryCategory::Houses:    return bHouses;
	case EHutongGalleryCategory::Gates:     return bGates;
	case EHutongGalleryCategory::Courtyard: return bCourtyard;
	case EHutongGalleryCategory::Street:    return bStreet;
	case EHutongGalleryCategory::Temples:   return bRoofed;
	default:                                return false;
	}
}

FString HutongGallery::CategoryName(EHutongGalleryCategory Category)
{
	switch (Category)
	{
	case EHutongGalleryCategory::Walls:     return TEXT("Walls (牆)");
	case EHutongGalleryCategory::Houses:    return TEXT("Houses (房)");
	case EHutongGalleryCategory::Gates:     return TEXT("Gates (門)");
	case EHutongGalleryCategory::Courtyard: return TEXT("Courtyard (院)");
	case EHutongGalleryCategory::Street:    return TEXT("Street (街)");
	case EHutongGalleryCategory::Temples:   return TEXT("Temples and Pavilions (殿亭)");
	default:                                return TEXT("Gallery");
	}
}

TArray<HutongGallery::FPlacedPiece> HutongGallery::Plan(const UHutongGalleryToolProperties* Settings)
{
	TArray<FPlacedPiece> Out;
	if (!Settings) return Out;
	const TArray<FGalleryItem> Items = MakeItems(Settings, FHutongPalette());
	const FGalleryPlan G = LayOut(Items, Settings->Spacing, Settings->Columns);
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		FPlacedPiece& P = Out.AddDefaulted_GetRef();
		P.Label = Items[i].Label;
		P.Category = Items[i].Category;
		P.Cluster = Items[i].Cluster;
		P.Footprint = FBox2D(G.Origins[i], G.Origins[i] + Items[i].Footprint);
	}
	return Out;
}

UInteractiveTool* UHutongGalleryToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UHutongGalleryTool* Tool = NewObject<UHutongGalleryTool>(SceneState.ToolManager);
	Tool->Category = Category;
	return Tool;
}

void UHutongGalleryTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongGalleryToolProperties>(this);
	RegisterSettings(Settings);
	// After the restore: every gallery tool shares this settings class, so a restored category was the last
	// gallery's, and each tool placed whatever was opened first.
	Settings->GalleryCategory = Category;
}


void UHutongGalleryTool::GetEffectiveRectBounds(
	double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	Super::GetEffectiveRectBounds(OutMinX, OutMinY, OutMaxX, OutMaxY);
	if (!Settings) return;

	// Drag positions and turns the gallery; it does not size it.
	const FGalleryPlan G = LayOut(MakeItems(Settings, FHutongPalette()), Settings->Spacing, Settings->Columns);

	// Held under the cursor, not pinned to the anchor corner.
	HoldExtentAtCursor(OutMinX, OutMaxX, G.TotalX);
	HoldExtentAtCursor(OutMinY, OutMaxY, G.TotalY);
}

void UHutongGalleryTool::SpawnFinalActor()
{
	if (!Settings) return;

	const FHutongPalette Palette = Appearance ? Appearance->Palette : FHutongPalette();
	const TArray<FGalleryItem> Items = MakeItems(Settings, Palette);
	if (Items.Num() == 0) return;

	const FGalleryPlan G = LayOut(Items, Settings->Spacing, Settings->Columns);

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);

	UWorld* World = GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld();
	if (!World) return;

	// One transaction: the whole gallery is one Ctrl+Z.
	UInteractiveToolManager* ToolManager = GetToolManager();
	ToolManager->BeginUndoTransaction(
		NSLOCTEXT("HutongLayout", "PlaceGallery", "Place Hutong Gallery"));

	// Dozens of mesh bakes: show progress so it does not look hung.
	FScopedSlowTask Task((float)Items.Num(),
		NSLOCTEXT("HutongLayout", "BuildingGallery", "Building the gallery…"));
	Task.MakeDialog();

	for (int32 i = 0; i < Items.Num(); ++i)
	{
		const FGalleryItem& It = Items[i];
		Task.EnterProgressFrame(1.0f, FText::FromString(It.Label));

		// The item's build callable already takes a level, as the LOD chain wants.
		TArray<FDynamicMesh3> LODs;
		const int32 CollisionLOD = HutongGen::Detail::BuildPlacementLODs(IsPlanOnly(), It.Footprint.X, It.Footprint.Y,
			GetDetailLevel(), ShouldBuildLODChain(),
			[&It](FDynamicMesh3& M, EHutongDetail Level) { It.Build(M, Level); }, LODs);
		const bool bPlanOnly = IsPlanOnly();
		if (!bPlanOnly && (LODs.Num() == 0 || LODs[0].TriangleCount() == 0)) continue;

		const FVector2D Origin = G.Origins[i];
		const FVector Loc = LocalRectToWorld(MinX + Origin.X, MinY + Origin.Y);
		const FTransform Xform(FRotator(0.0, PlacementYawDeg, 0.0),
			FVector(Loc.X, Loc.Y, StartWorld.Z));

		AStaticMeshActor* Actor = bPlanOnly
			? HutongGen::SpawnEmptyActor(World, Xform, GetActorNameBase())
			: HutongGen::SpawnStaticMeshActor(World, LODs, Xform, GetActorNameBase(), Palette, CollisionLOD);
		if (!Actor) continue;

		Actor->SetActorLabel(It.Label);
		if (!Settings->OutlinerFolder.IsNone())
		{
			// Folder / category / cluster, as they stand on the ground.
			Actor->SetFolderPath(FName(FString::Printf(TEXT("%s/%s/%s"),
				*Settings->OutlinerFolder.ToString(), *HutongGallery::CategoryName(It.Category), *It.Cluster)));
		}

		// Each piece keeps its own building component, individually editable.
		It.Attach(Actor);

		// Stamp sets bPlanOnly after the attach's own pass, so re-apply attachments or plan outlines are missing.
		if (UHutongBuildingComponent* B = Actor->FindComponentByClass<UHutongBuildingComponent>())
		{
			StampDetail(B);
			B->ApplyPlacementAttachments();
		}
	}

	ToolManager->EndUndoTransaction();
}

FString UHutongGalleryTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	const TArray<FGalleryItem> Items = MakeItems(Settings, FHutongPalette());
	const FGalleryPlan G = LayOut(Items, Settings->Spacing, Settings->Columns);
	return FString::Printf(TEXT("%d pieces in %d clusters · %.0f x %.0f m"),
		Items.Num(), G.ClusterCount, G.TotalX * 0.01, G.TotalY * 0.01);
}

void UHutongGalleryTool::Render(IToolsContextRenderAPI* RenderAPI)
{
	Super::Render(RenderAPI);
	if (!bIsDragging || RenderAPI == nullptr || !Settings) return;
	FPrimitiveDrawInterface* PDI = RenderAPI->GetPrimitiveDrawInterface();
	if (!PDI) return;

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);

	const TArray<FGalleryItem> Items = MakeItems(Settings, FHutongPalette());
	const FGalleryPlan G = LayOut(Items, Settings->Spacing, Settings->Columns);

	auto DrawBox = [&](const FBox2D& Box, double Pad, const FLinearColor& Colour, float Thickness)
	{
		const double X0 = MinX + Box.Min.X - Pad, Y0 = MinY + Box.Min.Y - Pad;
		const double X1 = MinX + Box.Max.X + Pad, Y1 = MinY + Box.Max.Y + Pad;
		const FVector A = LocalRectToWorld(X0, Y0);
		const FVector B = LocalRectToWorld(X1, Y0);
		const FVector C = LocalRectToWorld(X1, Y1);
		const FVector D = LocalRectToWorld(X0, Y1);
		DrawPreviewLine(PDI, A, B, Colour, Thickness);
		DrawPreviewLine(PDI, B, C, Colour, Thickness);
		DrawPreviewLine(PDI, C, D, Colour, Thickness);
		DrawPreviewLine(PDI, D, A, Colour, Thickness);
	};

	// Each piece's footprint, each cluster round its pieces, each category round its clusters.
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		DrawBox(FBox2D(G.Origins[i], G.Origins[i] + Items[i].Footprint), 0.0, FLinearColor(0.35f, 0.75f, 1.0f, 1.0f), 3.0f);
	}
	const double Gap = FMath::Max(Settings->Spacing, 50.0);
	for (const FBox2D& Box : G.ClusterBoxes) DrawBox(Box, 0.5 * Gap, FLinearColor(0.35f, 0.75f, 1.0f, 0.5f), 1.5f);
	for (const FBox2D& Box : G.CategoryBoxes) DrawBox(Box, 1.5 * Gap, FLinearColor(1.0f, 0.75f, 0.3f, 1.0f), 2.0f);
}

TArray<FText> UHutongGalleryTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = (Category == EHutongGalleryCategory::All)
		? NSLOCTEXT("HutongGalleryTool", "HelpDrag", "Click to anchor, move to turn, click to place one of every type.")
		: FText::Format(NSLOCTEXT("HutongGalleryTool", "HelpDragCategory", "Click to anchor, move to turn, click to place every kind of {0}."),
			FText::FromString(HutongGallery::CategoryName(Category)));
	Lines.Insert(NSLOCTEXT("HutongGalleryTool", "HelpFolder",
		"Each piece is its own actor under the HutongGallery Outliner folder; delete the folder to clear it."), 1);
	Lines.Insert(NSLOCTEXT("HutongGalleryTool", "HelpFacing",
		"Turn off Include Variants for just one of each type."), 2);
	return Lines;
}
