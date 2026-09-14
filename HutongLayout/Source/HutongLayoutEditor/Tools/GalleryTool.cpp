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

using UE::Geometry::FDynamicMesh3;

namespace
{
	// One thing on the grid: a label, the footprint it occupies, how to build its mesh and how to attach the component that rebuilds it later.
	struct FGalleryItem
	{
		FString Label;
		FVector2D Footprint = FVector2D(300.0, 300.0);
		TFunction<void(FDynamicMesh3&, EHutongDetail)> Build;
		TFunction<void(AStaticMeshActor*)> Attach;
	};

	// Every faced type is built facing -Y.
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
			// AddInstanceComponent as well as RegisterComponent, or the component is invisible in the Details panel and is not saved with the actor.
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

		// --- 牆 ---
		if (S->bWalls)
		{
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
				Items.Add(MoveTemp(It));
			};

			AddWall(TEXT("Wall (牆)"), [](FHutongWallParams&) {});

			// The garden openings, and they are the reason a wall is worth walking round.
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
			AddWall(TEXT("Wall-Mounted Inner Gate (牆垣式垂花門)"), [](FHutongWallParams& P)
			{
				P.Role = EHutongWallRole::Courtyard;
				P.Doorway = EHutongWallDoorway::Rect;
				P.bDoorwayChuihua = true;
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
				// 什錦窗, the other half of what tells a 隔牆 from a 院牆 at a glance.
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
		}

		// --- 正房 / 廂房 / 倒座房 / 後罩房 / 耳房 --- Walked out of the shipped presets.
		if (S->bHouses)
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

				FGalleryItem It;
				It.Label = Name;
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
				Items.Add(MoveTemp(It));
			}

			// The sumptuary rule, side by side: the same house in 合瓦 and in 筒瓦.
			if (bVar && Names.Num() > 0)
			{
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
					Items.Add(MoveTemp(It));
				}
			}
		}

		// --- 大門 and 垂花門 ---
		if (S->bGates)
		{
			const EHutongGateStyle Styles[] = {
				EHutongGateStyle::Guangliang, EHutongGateStyle::Jinzhu,
				EHutongGateStyle::Manzi, EHutongGateStyle::Ruyi };
			const TCHAR* StyleNames[] = {
				TEXT("Wide-Hall Gate (廣亮大門)"), TEXT("Inner-Column Gate (金柱大門)"),
				TEXT("Flush Gate (蠻子門)"), TEXT("Ruyi Gate (如意門)") };

			const int32 GateCount = bVar ? UE_ARRAY_COUNT(Styles) : 1;
			for (int32 i = 0; i < GateCount; ++i)
			{
				FHutongGateHouseParams P;
				P.Style = Styles[i];
				P.RandomSeed = S->RandomSeed + i;
				// The middle of the style's own band, as the compound sizes its gate.
				const FHutongGateHouseParams::FSizeRange R = P.GetSizeRange();
				const double SX = (R.FrontageMax > 0.0) ? 0.5 * (R.FrontageMin + R.FrontageMax) : 360.0;
				const double SY = (R.DepthMax > 0.0) ? 0.5 * (R.DepthMin + R.DepthMax) : 340.0;

				FGalleryItem It;
				It.Label = StyleNames[i];
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
				Items.Add(MoveTemp(It));
			}

			// The 門墩 rank rule, visible: the same gate with the drum its rank allows.
			if (bVar)
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
				Items.Add(MoveTemp(It));
			}

			{
				FHutongInnerGateParams P;
				const FHutongInnerGateParams::FSizeRange R = P.GetSizeRange();
				const double SX = (R.FrontageMax > 0.0) ? 0.5 * (R.FrontageMin + R.FrontageMax) : 330.0;
				const double SY = (R.DepthMax > 0.0) ? 0.5 * (R.DepthMin + R.DepthMax) : 150.0;

				FGalleryItem It;
				It.Label = TEXT("Inner Gate (垂花門)");
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
				Items.Add(MoveTemp(It));
			}
		}

		// --- 遊廊, 影壁, 甬路 ---
		if (S->bCourtyard)
		{
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
				Items.Add(MoveTemp(It));
			};
			AddCorridor(TEXT("Covered Corridor (遊廊)"), false);
			if (bVar) AddCorridor(TEXT("Ring Corridor (抄手遊廊) · Closed Side"), true);

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
				Items.Add(MoveTemp(It));
			}

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
				Items.Add(MoveTemp(It));
			}

			// 天棚魚缸石榴樹: the courtyard's own furnishing.
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
				Items.Add(MoveTemp(It));
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
				Items.Add(MoveTemp(It));
			}
		}

		// --- 耳房 with its 過道 ---
		if (S->bHouses)
		{
			FHutongEarPassageParams P;
			const double SX = 760.0, SY = 340.0;
			FGalleryItem It;
			It.Label = TEXT("Ear Room With Passage (耳房過道)");
			It.Footprint = FVector2D(SX, SY);
			It.Build = [P, SX, SY](FDynamicMesh3& M, EHutongDetail D)
			{
				UHutongEarPassageBuildingComponent::BuildEarPassageMesh(P, Facing, SX, SY, M, D);
			};
			It.Attach = MakeAttach<UHutongEarPassageBuildingComponent>(Palette,
				[P, SX, SY](UHutongEarPassageBuildingComponent* C)
				{
					C->Params = P;
					C->FootprintX = SX; C->FootprintY = SY; C->BaySide = Facing;
				});
			Items.Add(MoveTemp(It));
		}

		// --- 鋪面房 and 牌坊 ---
		if (S->bStreet)
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
				Items.Add(MoveTemp(It));
			};
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
				Items.Add(MoveTemp(It));
			};
			AddStorey(TEXT("Multi-Story Building (樓)"), true);
			// Without the storey line, which is the whole of what the type is: worth seeing beside it.
			if (bVar) AddStorey(TEXT("Multi-Story Building (樓) · No Gallery"), false);

			auto AddPaifang = [&](const FString& Label, EHutongPaifangBays Bays, bool bRoofs, double Len)
			{
				FHutongPaifangParams P;
				P.BayCount = Bays;
				P.bHasRoofs = bRoofs;
				P.Length = Len;
				// The 夾杆石 spread either side of a column is the deepest thing at ground level, and so is what the footprint has to contain.
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
				Items.Add(MoveTemp(It));
			};
			AddPaifang(TEXT("Roofed Memorial Arch (牌樓) · Three Bays, Four Posts (三間四柱)"), EHutongPaifangBays::Three, true, 1000.0);
			if (bVar)
			{
				AddPaifang(TEXT("Memorial Arch (牌坊) · One Bay, Two Posts (一間二柱) · Stone"), EHutongPaifangBays::One, false, 480.0);
			}
		}

		// --- 亭 and 殿, which is where the roof types live ---
		if (S->bRoofed)
		{
			auto AddPavilion = [&](const FString& Label, EHutongRoofType Roof, double Roll)
			{
				FHutongPavilionParams P;
				P.RoofType = Roof;
				P.RoofApexRoll = Roll;
				const double SX = 380.0, SY = 380.0;

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
				Items.Add(MoveTemp(It));
			};
			AddPavilion(TEXT("Pavilion (亭) · Pyramidal Roof (攢尖)"), EHutongRoofType::Cuanjian, 0.0);
			if (bVar)
			{
				AddPavilion(TEXT("Pavilion (亭) · Hipped Roof (廡殿)"), EHutongRoofType::Wudian, 0.0);
				AddPavilion(TEXT("Pavilion (亭) · Hip-and-Gable Roof (歇山)"), EHutongRoofType::Xieshan, 0.0);
				AddPavilion(TEXT("Pavilion (亭) · Rolled Hip-and-Gable Roof (捲棚歇山)"), EHutongRoofType::Xieshan, 0.35);
			}

			auto AddHall = [&](const FString& Label, EHutongRoofType Roof, double Roll)
			{
				FHutongHallParams P;
				P.RoofType = Roof;
				P.RoofApexRoll = Roll;
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
				Items.Add(MoveTemp(It));
			};
			AddHall(TEXT("Temple Hall (殿) · Hip-and-Gable Roof (歇山)"), EHutongRoofType::Xieshan, 0.0);
			if (bVar)
			{
				AddHall(TEXT("Temple Hall (殿) · Hipped Roof (廡殿)"), EHutongRoofType::Wudian, 0.0);
				AddHall(TEXT("Temple Hall (殿) · Rolled Hip-and-Gable Roof (捲棚歇山)"), EHutongRoofType::Xieshan, 0.35);
			}
		}

		return Items;
	}

	// The grid.
	struct FGrid
	{
		double CellX = 0.0, CellY = 0.0;
		int32 Columns = 1, Rows = 1;
		double TotalX = 0.0, TotalY = 0.0;
	};

	FGrid LayOut(const TArray<FGalleryItem>& Items, double Spacing, int32 Columns)
	{
		FGrid G;
		G.Columns = FMath::Clamp(Columns, 1, 16);
		G.Rows = FMath::Max(1, FMath::DivideAndRoundUp(Items.Num(), G.Columns));

		double MaxX = 0.0, MaxY = 0.0;
		for (const FGalleryItem& It : Items)
		{
			MaxX = FMath::Max(MaxX, It.Footprint.X);
			MaxY = FMath::Max(MaxY, It.Footprint.Y);
		}
		const double Gap = FMath::Max(Spacing, 50.0);
		G.CellX = MaxX + Gap;
		G.CellY = MaxY + Gap;
		G.TotalX = G.Columns * G.CellX + Gap;
		G.TotalY = G.Rows * G.CellY + Gap;
		return G;
	}

	// Where item i's own origin — the min corner its mesh is built around — lands in the grid.
	FVector2D ItemOrigin(const FGrid& G, const FGalleryItem& It, int32 Index, double Spacing)
	{
		const double Gap = FMath::Max(Spacing, 50.0);
		const int32 Col = Index % G.Columns;
		const int32 Row = Index / G.Columns;
		// Centred in its cell, so a small piece is not shoved into a corner of it.
		return FVector2D(
			Gap + Col * G.CellX + 0.5 * (G.CellX - Gap - It.Footprint.X),
			Gap + Row * G.CellY + 0.5 * (G.CellY - Gap - It.Footprint.Y));
	}
}

TArray<TPair<FString, int32>> HutongGallery::BuildAll(const UHutongGalleryToolProperties* Settings)
{
	TArray<TPair<FString, int32>> Out;
	for (const FGalleryItem& It : MakeItems(Settings, FHutongPalette()))
	{
		FDynamicMesh3 Mesh;
		It.Build(Mesh, EHutongDetail::Near);
		Out.Emplace(It.Label, Mesh.TriangleCount());
	}
	return Out;
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

			// Counted rather than compacted.
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

UInteractiveTool* UHutongGalleryToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	return NewObject<UHutongGalleryTool>(SceneState.ToolManager);
}

void UHutongGalleryTool::RegisterToolSettings()
{
	Settings = NewObject<UHutongGalleryToolProperties>(this);
	RegisterSettings(Settings);
}

void UHutongGalleryTool::GetEffectiveRectBounds(
	double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	Super::GetEffectiveRectBounds(OutMinX, OutMinY, OutMaxX, OutMaxY);
	if (!Settings) return;

	// The drag positions and turns the gallery; it does not size it.
	const FGrid G = LayOut(MakeItems(Settings, FHutongPalette()), Settings->Spacing, Settings->Columns);

	// Carried under the cursor rather than pinned to the anchor's corner.
	HoldExtentAtCursor(OutMinX, OutMaxX, G.TotalX);
	HoldExtentAtCursor(OutMinY, OutMaxY, G.TotalY);
}

void UHutongGalleryTool::SpawnFinalActor()
{
	if (!Settings) return;

	const FHutongPalette Palette = Appearance ? Appearance->Palette : FHutongPalette();
	const TArray<FGalleryItem> Items = MakeItems(Settings, Palette);
	if (Items.Num() == 0) return;

	const FGrid G = LayOut(Items, Settings->Spacing, Settings->Columns);

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);

	UWorld* World = GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld();
	if (!World) return;

	// One transaction round the lot, so a gallery is one Ctrl+Z rather than two dozen.
	UInteractiveToolManager* ToolManager = GetToolManager();
	ToolManager->BeginUndoTransaction(
		NSLOCTEXT("HutongLayout", "PlaceGallery", "Place Hutong Gallery"));

	// A gallery is two dozen static mesh bakes, which is long enough that a silent editor reads as a hang.
	FScopedSlowTask Task((float)Items.Num(),
		NSLOCTEXT("HutongLayout", "BuildingGallery", "Building the gallery…"));
	Task.MakeDialog();

	for (int32 i = 0; i < Items.Num(); ++i)
	{
		const FGalleryItem& It = Items[i];
		Task.EnterProgressFrame(1.0f, FText::FromString(It.Label));

		// The item's own two callables are already the shape the chain wants: one build, asked for a level.
		TArray<FDynamicMesh3> LODs;
		HutongGen::Detail::BuildPlacementLODs(IsPlanOnly(), It.Footprint.X, It.Footprint.Y,
			GetDetailLevel(), ShouldBuildLODChain(),
			[&It](FDynamicMesh3& M, EHutongDetail Level) { It.Build(M, Level); }, LODs);
		const bool bPlanOnly = IsPlanOnly();
		if (!bPlanOnly && (LODs.Num() == 0 || LODs[0].TriangleCount() == 0)) continue;

		const FVector2D Origin = ItemOrigin(G, It, i, Settings->Spacing);
		const FVector Loc = LocalRectToWorld(MinX + Origin.X, MinY + Origin.Y);
		const FTransform Xform(FRotator(0.0, PlacementYawDeg, 0.0),
			FVector(Loc.X, Loc.Y, StartWorld.Z));

		AStaticMeshActor* Actor = bPlanOnly
			? HutongGen::SpawnEmptyActor(World, Xform, GetActorNameBase())
			: HutongGen::SpawnStaticMeshActor(World, LODs, Xform, GetActorNameBase(), Palette);
		if (!Actor) continue;

		// The label is the whole point of a gallery.
		Actor->SetActorLabel(It.Label);
		if (!Settings->OutlinerFolder.IsNone())
		{
			Actor->SetFolderPath(Settings->OutlinerFolder);
		}

		// Every piece keeps its own building component and stays individually editable.
		It.Attach(Actor);

		// Stamped after the attach rather than threaded through MakeAttach — and the attachments
		// applied again after it: the stamp is what sets bPlanOnly, and the attach's own pass ran
		// before it, so a laid-out gallery had components and no outlines.
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
	const FGrid G = LayOut(Items, Settings->Spacing, Settings->Columns);
	return FString::Printf(TEXT("%d pieces · %d x %d · %.0f x %.0f m"),
		Items.Num(), G.Columns, G.Rows, G.TotalX * 0.01, G.TotalY * 0.01);
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
	const FGrid G = LayOut(Items, Settings->Spacing, Settings->Columns);

	// Each piece's own footprint, not the cells.
	const FLinearColor Plan(0.35f, 0.75f, 1.0f, 1.0f);
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		const FVector2D O = ItemOrigin(G, Items[i], i, Settings->Spacing);
		const double X0 = MinX + O.X, Y0 = MinY + O.Y;
		const double X1 = X0 + Items[i].Footprint.X, Y1 = Y0 + Items[i].Footprint.Y;
		const FVector A = LocalRectToWorld(X0, Y0);
		const FVector B = LocalRectToWorld(X1, Y0);
		const FVector C = LocalRectToWorld(X1, Y1);
		const FVector D = LocalRectToWorld(X0, Y1);
		DrawPreviewLine(PDI, A, B, Plan, 3.0f);
		DrawPreviewLine(PDI, B, C, Plan, 3.0f);
		DrawPreviewLine(PDI, C, D, Plan, 3.0f);
		DrawPreviewLine(PDI, D, A, Plan, 3.0f);
	}
}

TArray<FText> UHutongGalleryTool::GetToolHelpLines() const
{
	TArray<FText> Lines = Super::GetToolHelpLines();
	Lines[0] = NSLOCTEXT("HutongGalleryTool", "HelpDrag",
		"Click to anchor, move, click to place one of everything. The drag only positions and turns the gallery — its size is the layout's own.");
	Lines.Insert(NSLOCTEXT("HutongGalleryTool", "HelpFolder",
		"Every piece is a separate, editable actor, labelled with its type and dropped in the HutongGallery outliner folder — delete that folder to clear the set."), 1);
	Lines.Insert(NSLOCTEXT("HutongGalleryTool", "HelpFacing",
		"Everything faces the same way, so one walk along a row shows every front. Turn off Include Variants for one of each type instead of all of them."), 2);
	return Lines;
}
