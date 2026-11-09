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
	struct FGalleryItem
	{
		FString Label;
		FVector2D Footprint = FVector2D(300.0, 300.0);
		TFunction<void(FDynamicMesh3&, EHutongDetail)> Build;
		TFunction<void(AStaticMeshActor*)> Attach;
		EHutongGalleryCategory Category = EHutongGalleryCategory::All;
		FString Cluster;
	};

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

	constexpr EHutongBaySide GalleryFacing = EHutongBaySide::MinusY;

	template <typename TComponent, typename TSetup>
	TFunction<void(AStaticMeshActor*)> MakeAttach(const FHutongPalette& Palette, TSetup&& Setup)
	{
		return [Palette, Setup](AStaticMeshActor* Actor)
		{
			TComponent* C = NewObject<TComponent>(Actor);
			if (!C) return;
			Setup(C);
			C->Palette = Palette;
			// AddInstanceComponent shows the component in Details and saves it.
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

		EHutongGalleryCategory Cat = EHutongGalleryCategory::Walls;
		FString Cluster;
		auto Add = [&](FGalleryItem&& It)
		{
			It.Category = Cat;
			It.Cluster = Cluster;
			Items.Add(MoveTemp(It));
		};

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

		Cat = EHutongGalleryCategory::Walls;
		{
			Cluster = TEXT("Walls (牆)");
			AddWall(TEXT("Wall (牆)"), [](FHutongWallParams&) {});
			AddWall(TEXT("Imperial City Wall (皇城牆)"), [](FHutongWallParams& P)
			{
				P.Role = EHutongWallRole::ImperialCity;
				P.bHasWindows = false;
			});

			auto AddCityWall = [&](const FString& Label, double Len, TFunction<void(FHutongCityWallParams&)> Tweak)
			{
				FHutongCityWallParams P;
				Tweak(P);
				const EHutongBaySide Outer = EHutongBaySide::MinusY;
				FGalleryItem It;
				It.Label = Label;
				It.Footprint = FVector2D(Len, P.GetBaseWidth());
				It.Build = [P, Len, Outer](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongCityWallBuildingComponent::BuildCityWallMesh(P, Len, Outer, M, D);
				};
				It.Attach = MakeAttach<UHutongCityWallBuildingComponent>(Palette,
					[P, Len, Outer](UHutongCityWallBuildingComponent* C)
					{
						C->Params = P;
						C->Length = Len;
						C->OuterSide = Outer;
					});
				Add(MoveTemp(It));
			};
			// Long enough for a bastion and a ramp: about 60 m for the inner city's.
			AddCityWall(TEXT("City Wall (城牆)"), 6000.0, [](FHutongCityWallParams& P) { P.bRamp = true; });
			AddCityWall(TEXT("City Wall (城牆) · Outer City (外城)"), 4000.0, [](FHutongCityWallParams& P)
			{
				P.Rank = EHutongCityWallRank::Outer;
				P.bRamp = true;
			});
			AddCityWall(TEXT("Palace Wall (紫禁城城牆)"), 4000.0, [](FHutongCityWallParams& P)
			{
				P.Rank = EHutongCityWallRank::Palace;
				P.bBastions = false;
				P.bLoopholes = false;
				P.bRamp = true;
			});

			for (const EHutongCityWallRank Rank : { EHutongCityWallRank::Inner, EHutongCityWallRank::Outer, EHutongCityWallRank::Palace })
			{
				FHutongCityWallParams P;
				P.ChooseRank(Rank);
				P.bBastions = false;
				P.bLoopholes = Rank != EHutongCityWallRank::Palace;
				UHutongWallCornerBuildingComponent* Probe = NewObject<UHutongWallCornerBuildingComponent>();
				Probe->Params = P;
				const double Platform = Probe->GetPlatformSize();
				FGalleryItem It;
				It.Label = Rank == EHutongCityWallRank::Palace ? FString(TEXT("Palace Wall Corner (紫禁城城角)"))
					: Rank == EHutongCityWallRank::Outer ? FString(TEXT("City Wall Corner (城角) · Outer City (外城)"))
					: FString(TEXT("City Wall Corner (城角)"));
				It.Footprint = FVector2D(Platform, Platform);
				It.Build = [P, Platform](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongWallCornerBuildingComponent::BuildCornerMesh(P, EHutongWallCornerShape::Platform, Platform, 0, M, D);
				};
				It.Attach = MakeAttach<UHutongWallCornerBuildingComponent>(Palette,
					[P](UHutongWallCornerBuildingComponent* C)
					{
						C->Params = P;
						C->OuterCorner = 0;
					});
				Add(MoveTemp(It));
			}

			{
				const FHutongArrowTowerParams P;
				FGalleryItem It;
				It.Label = TEXT("Arrow Tower (箭樓)");
				It.Footprint = UHutongArrowTowerBuildingComponent::FootprintFor(P, EHutongBaySide::MinusY);
				It.Build = [P](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongArrowTowerBuildingComponent::BuildArrowTowerMesh(P, EHutongBaySide::MinusY, M, D);
				};
				It.Attach = MakeAttach<UHutongArrowTowerBuildingComponent>(Palette,
					[P](UHutongArrowTowerBuildingComponent* C)
					{
						C->Params = P;
						C->Facade = EHutongBaySide::MinusY;
					});
				Add(MoveTemp(It));
			}

			{
				const FHutongCityGateParams P;
				FGalleryItem It;
				It.Label = TEXT("Barbican Gate (閘樓)");
				It.Footprint = UHutongCityGateBuildingComponent::FootprintFor(P, EHutongBaySide::MinusY);
				It.Build = [P](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongCityGateBuildingComponent::BuildCityGateMesh(P, EHutongBaySide::MinusY, M, D);
				};
				It.Attach = MakeAttach<UHutongCityGateBuildingComponent>(Palette,
					[P](UHutongCityGateBuildingComponent* C)
					{
						C->Params = P;
						C->Facade = EHutongBaySide::MinusY;
					});
				Add(MoveTemp(It));
			}
			{
				FHutongCityGateParams P;
				P.Tower = EHutongCityGateTower::Chenglou;
				FGalleryItem It;
				It.Label = TEXT("City Gate Tower (城樓)");
				It.Footprint = UHutongCityGateBuildingComponent::FootprintFor(P, EHutongBaySide::MinusY);
				It.Build = [P](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongCityGateBuildingComponent::BuildCityGateMesh(P, EHutongBaySide::MinusY, M, D);
				};
				It.Attach = MakeAttach<UHutongCityGateBuildingComponent>(Palette,
					[P](UHutongCityGateBuildingComponent* C)
					{
						C->Params = P;
						C->Facade = EHutongBaySide::MinusY;
					});
				Add(MoveTemp(It));
			}
			for (const EHutongCityWallRank Rank : { EHutongCityWallRank::Inner })
			{
				FHutongCityWallParams P;
				P.ChooseRank(Rank);
				const double Size = UHutongWallCornerBuildingComponent::SizeFor(P, EHutongWallCornerShape::Rounded, 0.0);
				FGalleryItem It;
				It.Label = TEXT("Rounded Wall Corner (甕城角)");
				It.Footprint = FVector2D(Size, Size);
				It.Build = [P, Size](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongWallCornerBuildingComponent::BuildCornerMesh(P, EHutongWallCornerShape::Rounded, Size, 0, M, D);
				};
				It.Attach = MakeAttach<UHutongWallCornerBuildingComponent>(Palette,
					[P](UHutongWallCornerBuildingComponent* C)
					{
						C->Params = P;
						C->Shape = EHutongWallCornerShape::Rounded;
						C->OuterCorner = 0;
					});
				Add(MoveTemp(It));
			}
			{
				FHutongCityWallParams P;
				P.bBastions = false;
				const double Size = UHutongWallCornerBuildingComponent::SizeFor(P, EHutongWallCornerShape::Platform, 0.0);
				FGalleryItem It;
				It.Label = TEXT("Corner Arrow Tower (角箭樓)");
				It.Footprint = FVector2D(Size, Size);
				It.Build = [P, Size](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongWallCornerBuildingComponent::BuildCornerMesh(P, EHutongWallCornerShape::Platform, Size, 0, M, D, EHutongCornerTower::ArrowTower);
				};
				It.Attach = MakeAttach<UHutongWallCornerBuildingComponent>(Palette,
					[P](UHutongWallCornerBuildingComponent* C)
					{
						C->Params = P;
						C->Tower = EHutongCornerTower::ArrowTower;
						C->OuterCorner = 0;
					});
				Add(MoveTemp(It));
			}
			{
				FHutongCityWallParams P;
				P.ChooseRank(EHutongCityWallRank::Palace);
				P.bBastions = false;
				P.bLoopholes = false;
				const double Size = UHutongWallCornerBuildingComponent::SizeFor(P, EHutongWallCornerShape::Platform, 0.0);
				FGalleryItem It;
				It.Label = TEXT("Palace Corner Tower (紫禁城角樓)");
				It.Footprint = FVector2D(Size, Size);
				It.Build = [P, Size](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongWallCornerBuildingComponent::BuildCornerMesh(P, EHutongWallCornerShape::Platform, Size, 0, M, D, EHutongCornerTower::PalaceTower);
				};
				It.Attach = MakeAttach<UHutongWallCornerBuildingComponent>(Palette,
					[P](UHutongWallCornerBuildingComponent* C)
					{
						C->Params = P;
						C->Tower = EHutongCornerTower::PalaceTower;
						C->OuterCorner = 0;
					});
				Add(MoveTemp(It));
			}

			Cluster = TEXT("Garden Doorways (牆門洞)");
			AddWall(TEXT("Moon Gate (月亮門)"), [](FHutongWallParams& P)
			{
				P.Role = EHutongWallRole::Courtyard;
				P.Doorway = EHutongWallDoorway::Moon;
			});
			AddWall(TEXT("Wall-Set Gate (隨牆門)"), [](FHutongWallParams& P)
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
					UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(P, GalleryFacing, 0, SX, SY, M, D);
				};
				It.Attach = MakeAttach<UHutongSiheyuanBuildingComponent>(Palette,
					[P, SX, SY, Name](UHutongSiheyuanBuildingComponent* C)
					{
						C->Params = P;
						C->Preset = Name;
						C->FootprintX = SX; C->FootprintY = SY; C->BaySide = GalleryFacing;
					});
				Add(MoveTemp(It));
			}

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
						UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(P, GalleryFacing, 0, SX, SY, M, D);
					};
					It.Attach = MakeAttach<UHutongSiheyuanBuildingComponent>(Palette,
						[P, SX, SY](UHutongSiheyuanBuildingComponent* C)
						{
							C->Params = P;
							C->FootprintX = SX; C->FootprintY = SY; C->BaySide = GalleryFacing;
						});
					Add(MoveTemp(It));

					FHutongSiheyuanParams F = P;
					F.RoofTile = EHutongRoofTile::He;
					F.bExposedFrame = true;
					FGalleryItem Fr;
					Fr.Label = FString::Printf(TEXT("%s · Exposed Frame (徹上明造)"), *Names[0]);
					Fr.Footprint = FVector2D(SX, SY);
					Fr.Build = [F, SX, SY](FDynamicMesh3& M, EHutongDetail D)
					{
						UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(F, GalleryFacing, 0, SX, SY, M, D);
					};
					Fr.Attach = MakeAttach<UHutongSiheyuanBuildingComponent>(Palette,
						[F, SX, SY](UHutongSiheyuanBuildingComponent* C)
						{
							C->Params = F;
							C->FootprintX = SX; C->FootprintY = SY; C->BaySide = GalleryFacing;
						});
					Add(MoveTemp(Fr));
				}

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
						UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(V, GalleryFacing, 0, SX, SY, M, D);
					};
					It.Attach = MakeAttach<UHutongSiheyuanBuildingComponent>(Palette,
						[V, SX, SY](UHutongSiheyuanBuildingComponent* C)
						{
							C->Params = V;
							C->FootprintX = SX; C->FootprintY = SY; C->BaySide = GalleryFacing;
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

		Cat = EHutongGalleryCategory::Gates;
		{
			Cluster = TEXT("Gate Houses (屋宇式門)");
			const EHutongGateStyle Styles[] = {
				EHutongGateStyle::Guangliang, EHutongGateStyle::Jinzhu,
				EHutongGateStyle::Manzi, EHutongGateStyle::Ruyi };
			const TCHAR* StyleNames[] = {
				TEXT("Wide-Hall Gate (廣亮大門)"), TEXT("Inner-Column Gate (金柱大門)"),
				TEXT("Flush Gate (蠻子門)"), TEXT("Ruyi Gate (如意門)") };

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
					UHutongGateHouseBuildingComponent::BuildGateHouseMesh(P, GalleryFacing, SX, SY, M, D);
				};
				It.Attach = MakeAttach<UHutongGateHouseBuildingComponent>(Palette,
					[P, SX, SY](UHutongGateHouseBuildingComponent* C)
					{
						C->Params = P;
						C->FootprintX = SX; C->FootprintY = SY; C->BaySide = GalleryFacing;
					});
				Add(MoveTemp(It));

				FGalleryItem Fr;
				Fr.Label = TEXT("Wide-Hall Gate (廣亮大門) · Splayed Screen Walls (反八字影壁)");
				Fr.Footprint = FVector2D(SX, SY);
				Fr.Build = [F, SX, SY](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongGateHouseBuildingComponent::BuildGateHouseMesh(F, GalleryFacing, SX, SY, M, D);
				};
				Fr.Attach = MakeAttach<UHutongGateHouseBuildingComponent>(Palette,
					[F, SX, SY](UHutongGateHouseBuildingComponent* C)
					{
						C->Params = F;
						C->FootprintX = SX; C->FootprintY = SY; C->BaySide = GalleryFacing;
					});
				Add(MoveTemp(Fr));
			};

			const int32 GateCount = bVar ? UE_ARRAY_COUNT(Styles) : 1;
			for (int32 i = 0; i < GateCount; ++i)
			{
				FHutongGateHouseParams P;
				P.Style = Styles[i];
				P.RandomSeed = S->RandomSeed + i;
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
						UHutongGateHouseBuildingComponent::BuildGateHouseMesh(Gate, GalleryFacing, SX, SY, M, D);
					};
					It.Attach = MakeAttach<UHutongGateHouseBuildingComponent>(Palette,
						[Gate, SX, SY](UHutongGateHouseBuildingComponent* C)
						{
							C->Params = Gate;
							C->FootprintX = SX; C->FootprintY = SY; C->BaySide = GalleryFacing;
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

			// 牆垣式門: gates standing in a wall (四合院建築及其構造 §5-1).
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
				// Several on one run; the middle one keeps the centre open for the collision test.
				AddWall(TEXT("Wall Gate (牆垣門) · Three Gates"), [](FHutongWallParams& P)
				{
					P.Length = 1800.0;
					P.bHasGate = true;
					P.GatePosition = 0.5;
					P.MoreGatePositions = { 0.18, 0.82 };
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
					UHutongInnerGateBuildingComponent::BuildInnerGateMesh(P, GalleryFacing, SX, SY, M, D);
				};
				It.Attach = MakeAttach<UHutongInnerGateBuildingComponent>(Palette,
					[P, SX, SY](UHutongInnerGateBuildingComponent* C)
					{
						C->Params = P;
						C->Width = SX; C->Depth = SY; C->BaySide = GalleryFacing;
					});
				Add(MoveTemp(It));
			}
		}

		Cat = EHutongGalleryCategory::Courtyard;
		{
			Cluster = TEXT("Covered Corridors (遊廊)");
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
			if (bVar) AddCorridor(TEXT("Encircling Corridor (抄手遊廊) · Closed Side"), true);

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
				if (!bVar && P.Passageway != EHutongEarPassage::AtEnd) continue;
				const double SX = P.GetSuggestedWidth() > 0.0 ? P.GetSuggestedWidth() : 760.0;
				const double SY = P.Room.GetSuggestedDepth() > 0.0 ? P.Room.GetSuggestedDepth() : 340.0;
				FGalleryItem It;
				It.Label = Name;
				It.Footprint = FVector2D(SX, SY);
				It.Build = [P, SX, SY](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongEarPassageBuildingComponent::BuildEarPassageMesh(P, GalleryFacing, SX, SY, M, D);
				};
				It.Attach = MakeAttach<UHutongEarPassageBuildingComponent>(Palette,
					[P, SX, SY, Name](UHutongEarPassageBuildingComponent* C)
					{
						C->Params = P;
						C->Preset = Name;
						C->FootprintX = SX; C->FootprintY = SY; C->BaySide = GalleryFacing;
					});
				Add(MoveTemp(It));
			}

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
					UHutongFrameBuildingComponent::BuildFrameMesh(Fp, GalleryFacing, 0, FX, FY, M, D);
				};
				Fr.Attach = MakeAttach<UHutongFrameBuildingComponent>(Palette,
					[Fp, FX, FY](UHutongFrameBuildingComponent* C)
					{
						C->Params = Fp;
						C->FootprintX = FX; C->FootprintY = FY; C->BaySide = GalleryFacing;
					});
				Add(MoveTemp(Fr));
			}
		}

		Cat = EHutongGalleryCategory::Street;
		{
			auto AddShop = [&](const FString& Label, int32 OpenBays, EHutongShopFront Front, EHutongShopScheme Scheme,
				bool bRaised = false)
			{
				FHutongShopfrontParams P;
				P.bRaisedPlatform = bRaised;
				P.bHasFence = bRaised;
				// Negative: the default, every bay open.
				P.bAllBaysOpen = OpenBays < 0;
				P.OpenBayCount = FMath::Max(OpenBays, 0);
				P.Front = Front;
				P.Scheme = Scheme;
				const double SX = 1000.0, SY = 520.0;

				FGalleryItem It;
				It.Label = Label;
				It.Footprint = FVector2D(SX, SY);
				It.Build = [P, SX, SY](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongShopfrontBuildingComponent::BuildShopfrontMesh(
						P, GalleryFacing, P.BayCountOverride, SX, SY, M, D);
				};
				It.Attach = MakeAttach<UHutongShopfrontBuildingComponent>(Palette,
					[P, SX, SY](UHutongShopfrontBuildingComponent* C)
					{
						C->Params = P;
						C->FootprintX = SX; C->FootprintY = SY; C->BaySide = GalleryFacing;
					});
				Add(MoveTemp(It));
			};
			Cluster = TEXT("Shopfronts (鋪面房)");
			AddShop(TEXT("Shopfront (鋪面房)"), -1, EHutongShopFront::Carved, EHutongShopScheme::Vermilion);
			AddShop(TEXT("Shopfront (鋪面房) · Lattice Shed Front (涼棚)"), -1, EHutongShopFront::Shed, EHutongShopScheme::Natural);
			AddShop(TEXT("Shopfront (鋪面房) · Platform Front (拍子式)"), -1, EHutongShopFront::Platform, EHutongShopScheme::Green);
			if (bVar)
			{
				AddShop(TEXT("Shopfront (鋪面房) · Plain Board Front"), -1, EHutongShopFront::Plain, EHutongShopScheme::Natural);
				AddShop(TEXT("Shopfront (鋪面房) · Raised, With Fence"), 2, EHutongShopFront::Plain, EHutongShopScheme::BlackGold, true);
				AddShop(TEXT("Shopfront (鋪面房) · Boarded Up"), 0, EHutongShopFront::Carved, EHutongShopScheme::BlueGreen);
			}

			Cluster = TEXT("Small Buildings (小房)");
			for (const TPair<const TCHAR*, FVector2D>& Small : {
					TPair<const TCHAR*, FVector2D>(TEXT("Small House (小房)"), FVector2D(400.0, 360.0)),
					TPair<const TCHAR*, FVector2D>(TEXT("Guard Post (堆撥房)"), FVector2D(560.0, 400.0)),
					TPair<const TCHAR*, FVector2D>(TEXT("Shed (棚)"), FVector2D(600.0, 300.0)),
					TPair<const TCHAR*, FVector2D>(TEXT("Shrine (土地廟)"), FVector2D(200.0, 200.0)) })
			{
				FHutongSmallBuildingParams P;
				if (!UHutongPresetLibrary::Get()->LoadPreset(TEXT("Small"), Small.Key, FHutongSmallBuildingParams::StaticStruct(), &P)) continue;
				const double SX = Small.Value.X, SY = Small.Value.Y;
				FGalleryItem It;
				It.Label = Small.Key;
				It.Footprint = FVector2D(SX, SY);
				It.Build = [P, SX, SY](FDynamicMesh3& M, EHutongDetail D)
				{
					UHutongSmallBuildingComponent::BuildSmallBuildingMesh(P, GalleryFacing, 0, SX, SY, M, D);
				};
				const FString Preset = Small.Key;
				It.Attach = MakeAttach<UHutongSmallBuildingComponent>(Palette,
					[P, SX, SY, Preset](UHutongSmallBuildingComponent* C)
					{
						C->Params = P;
						C->Preset = Preset;
						C->FootprintX = SX; C->FootprintY = SY; C->BaySide = GalleryFacing;
					});
				Add(MoveTemp(It));
			}

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
						P, GalleryFacing, P.BayCountOverride, SX, SY, M, D);
				};
				It.Attach = MakeAttach<UHutongStoreyBuildingComponent>(Palette,
					[P, SX, SY](UHutongStoreyBuildingComponent* C)
					{
						C->Params = P;
						C->FootprintX = SX; C->FootprintY = SY; C->BaySide = GalleryFacing;
					});
				Add(MoveTemp(It));
			};
			Cluster = TEXT("Storeyed Buildings (樓)");
			AddStorey(TEXT("Multi-Story Building (樓)"), true);
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
					UHutongHallBuildingComponent::BuildHallMesh(P, GalleryFacing, 0, SX, SY, M, D);
				};
				It.Attach = MakeAttach<UHutongHallBuildingComponent>(Palette,
					[P, SX, SY](UHutongHallBuildingComponent* C)
					{
						C->Params = P;
						C->FootprintX = SX; C->FootprintY = SY;
						C->BaySide = GalleryFacing;
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
					UHutongHallBuildingComponent::BuildHallMesh(P, GalleryFacing, 0, SX, SY, M, D);
				};
				It.Attach = MakeAttach<UHutongHallBuildingComponent>(Palette,
					[P, SX, SY](UHutongHallBuildingComponent* C)
					{
						C->Params = P;
						C->FootprintX = SX; C->FootprintY = SY; C->BaySide = GalleryFacing;
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

	struct FGalleryPlan
	{
		TArray<FVector2D> Origins;
		TArray<FBox2D> ClusterBoxes;
		TArray<FBox2D> CategoryBoxes;
		int32 ClusterCount = 0;
		double TotalX = 0.0, TotalY = 0.0;
	};

	FVector2D GalleryFacingOffset(const FVector2D& F, double Gap, int32 Quarter)
	{
		const double Half = 0.5 * Gap;
		return FVector2D((Quarter & 1) ? F.X + Half : 0.0, Quarter >= 2 ? FMath::Max(F.X, F.Y) + Half : 0.0);
	}
	FVector2D GalleryFacingSize(const FVector2D& F, int32 Quarter) { return (Quarter & 1) ? FVector2D(F.Y, F.X) : F; }
	FVector2D GalleryFacingsBlock(const FVector2D& F, double Gap)
	{
		return FVector2D(F.X + 0.5 * Gap + F.Y, 2.0 * FMath::Max(F.X, F.Y) + 0.5 * Gap);
	}

	bool TurnGalleryPiece(UHutongBuildingComponent* B, int32 Quarter)
	{
		// Corner offsets belong to the footprint's own corners; a turned twin would be another building.
		if (B->HasFootprintSkew()) return false;
		const FVector2D Size = B->GetFootprintSize();
		const FVector2D Turned = GalleryFacingSize(Size, Quarter);
		EHutongBaySide Side;
		if (B->GetFacade(Side))
		{
			if (!B->SetFacade((EHutongBaySide)(((int32)Side + Quarter) & 3))) return false;
			B->SetFootprintSize(Turned);
			return true;
		}
		const bool bWasAlongY = B->IsRunAlongY();
		if (Quarter != 1) return false;
		B->SetRunAlongY(!bWasAlongY);
		if (B->IsRunAlongY() == bWasAlongY) return false;
		B->SetFootprintSize(Turned);
		return true;
	}

	FGalleryPlan LayOut(const TArray<FGalleryItem>& Items, double Spacing, int32 PerRowIn, bool bFacings = false)
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
					const FVector2D F = bFacings ? GalleryFacingsBlock(Items[i].Footprint, Gap) : Items[i].Footprint;
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

bool HutongGallery::TurnPiece(UHutongBuildingComponent* Building, int32 Quarter)
{
	return Building && TurnGalleryPiece(Building, Quarter);
}

void HutongGallery::ForEachAttached(const UHutongGalleryToolProperties* Settings, UWorld* World,
	TFunctionRef<void(const FString&, AStaticMeshActor*)> Visit)
{
	if (!World) return;
	for (const FGalleryItem& It : MakeItems(Settings, FHutongPalette()))
	{
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
		if (!Actor) continue;
		if (It.Attach) It.Attach(Actor);
		Visit(It.Label, Actor);
		World->DestroyActor(Actor);
	}
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

			// Count distinct slots: IDs in an uncompacted mesh can skip.
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
	const FGalleryPlan G = LayOut(Items, Settings->Spacing, Settings->Columns, Settings->bAllFacings);
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
	// Set after the restore: every gallery tool shares this settings class.
	Settings->GalleryCategory = Category;
}


void UHutongGalleryTool::GetEffectiveRectBounds(
	double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const
{
	Super::GetEffectiveRectBounds(OutMinX, OutMinY, OutMaxX, OutMaxY);
	if (!Settings) return;

	const FGalleryPlan G = LayOut(MakeItems(Settings, FHutongPalette()), Settings->Spacing, Settings->Columns, Settings->bAllFacings);

	HoldExtentAtCursor(OutMinX, OutMaxX, G.TotalX);
	HoldExtentAtCursor(OutMinY, OutMaxY, G.TotalY);
}

void UHutongGalleryTool::SpawnFinalActor()
{
	if (!Settings) return;

	const FHutongPalette Palette = Appearance ? Appearance->Palette : FHutongPalette();
	const TArray<FGalleryItem> Items = MakeItems(Settings, Palette);
	if (Items.Num() == 0) return;

	const FGalleryPlan G = LayOut(Items, Settings->Spacing, Settings->Columns, Settings->bAllFacings);

	double MinX, MinY, MaxX, MaxY;
	GetEffectiveRectBounds(MinX, MinY, MaxX, MaxY);

	UWorld* World = GetToolManager()->GetContextQueriesAPI()->GetCurrentEditingWorld();
	if (!World) return;

	UInteractiveToolManager* ToolManager = GetToolManager();
	ToolManager->BeginUndoTransaction(
		NSLOCTEXT("HutongLayout", "PlaceGallery", "Place Hutong Gallery"));

	FScopedSlowTask Task((float)Items.Num(),
		NSLOCTEXT("HutongLayout", "BuildingGallery", "Building the gallery…"));
	Task.MakeDialog();

	for (int32 i = 0; i < Items.Num(); ++i)
	{
		const FGalleryItem& It = Items[i];
		Task.EnterProgressFrame(1.0f, FText::FromString(It.Label));

		TArray<FDynamicMesh3> LODs;
		HutongGen::FMeshTurn Turn;
		const int32 CollisionLOD = HutongGen::Detail::BuildPlacementLODs(IsPlanOnly(), It.Footprint.X, It.Footprint.Y,
			GetDetailLevel(), ShouldBuildLODChain(),
			[&It](FDynamicMesh3& M, EHutongDetail Level) { It.Build(M, Level); }, LODs, &Turn);
		const bool bPlanOnly = IsPlanOnly();
		if (!bPlanOnly && (LODs.Num() == 0 || LODs[0].TriangleCount() == 0)) continue;

		const FVector2D Origin = G.Origins[i];
		const FVector Loc = LocalRectToWorld(MinX + Origin.X, MinY + Origin.Y);
		const FTransform Xform(FRotator(0.0, PlacementYawDeg, 0.0),
			FVector(Loc.X, Loc.Y, StartWorld.Z));

		AStaticMeshActor* Actor = bPlanOnly
			? HutongGen::SpawnEmptyActor(World, Xform, GetActorNameBase())
			: HutongGen::SpawnStaticMeshActor(World, LODs, Xform, GetActorNameBase(), Palette, CollisionLOD, Turn);
		if (!Actor) continue;

		Actor->SetActorLabel(It.Label);
		if (!Settings->OutlinerFolder.IsNone())
		{
			Actor->SetFolderPath(FName(FString::Printf(TEXT("%s/%s/%s"),
				*Settings->OutlinerFolder.ToString(), *HutongGallery::CategoryName(It.Category), *It.Cluster)));
		}

		It.Attach(Actor);

		// Stamp sets bPlanOnly after the attach's own pass, so re-apply attachments or plan outlines are missing.
		if (UHutongBuildingComponent* B = Actor->FindComponentByClass<UHutongBuildingComponent>())
		{
			StampDetail(B);
			B->ApplyPlacementAttachments();
		}

		for (int32 Quarter = 1; Settings->bAllFacings && Quarter < 4; ++Quarter)
		{
			const FVector2D At = Origin + GalleryFacingOffset(It.Footprint, FMath::Max(Settings->Spacing, 50.0), Quarter);
			const FVector TurnedLoc = LocalRectToWorld(MinX + At.X, MinY + At.Y);
			AStaticMeshActor* Turned = HutongGen::SpawnEmptyActor(World,
				FTransform(FRotator(0.0, PlacementYawDeg, 0.0), FVector(TurnedLoc.X, TurnedLoc.Y, StartWorld.Z)), GetActorNameBase());
			if (!Turned) continue;
			It.Attach(Turned);
			UHutongBuildingComponent* B = Turned->FindComponentByClass<UHutongBuildingComponent>();
			if (!B || !TurnGalleryPiece(B, Quarter))
			{
				Turned->Destroy();
				continue;
			}
			Turned->SetActorLabel(FString::Printf(TEXT("%s · turned %d°"), *It.Label, 90 * Quarter));
			Turned->SetFolderPath(Actor->GetFolderPath());
			StampDetail(B);
			if (!B->bPlanOnly) B->Rebuild();
			B->ApplyPlacementAttachments();
		}
	}

	ToolManager->EndUndoTransaction();
}

FString UHutongGalleryTool::GetPlacementDetail() const
{
	if (!Settings) return FString();
	const TArray<FGalleryItem> Items = MakeItems(Settings, FHutongPalette());
	const FGalleryPlan G = LayOut(Items, Settings->Spacing, Settings->Columns, Settings->bAllFacings);
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
	const FGalleryPlan G = LayOut(Items, Settings->Spacing, Settings->Columns, Settings->bAllFacings);

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

	const double Gap = FMath::Max(Settings->Spacing, 50.0);
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		for (int32 Quarter = 0; Quarter < (Settings->bAllFacings ? 4 : 1); ++Quarter)
		{
			const FVector2D At = G.Origins[i] + GalleryFacingOffset(Items[i].Footprint, Gap, Quarter);
			DrawBox(FBox2D(At, At + GalleryFacingSize(Items[i].Footprint, Quarter)), 0.0, FLinearColor(0.35f, 0.75f, 1.0f, 1.0f), 3.0f);
		}
	}
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
