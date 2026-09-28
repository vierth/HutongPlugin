#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInterface.h"
#include "HutongPalette.generated.h"

namespace HutongGen
{
	// Starter Brick pattern unit: one 停泥磚 face, running bond. Other brick sizes (博縫 方磚) scale against it.
	namespace BrickPattern
	{
		inline constexpr double FaceLength = 24.0;
		inline constexpr double CourseHeight = 6.4;
	}

	// 青磚: cool medium grey.
	extern const FLinearColor DefaultBrickColor;

	// Grey tile, a shade below the brick.
	extern const FLinearColor DefaultRoofColor;

	// Dark oxblood woodwork.
	extern const FLinearColor DefaultWoodColor;

	// 青白石: cool grey limestone.
	extern const FLinearColor DefaultStoneColor;

	// 彩畫: 旋子 / 蘇式 blue-green.
	extern const FLinearColor DefaultPaintColor;

	// 下鹼: a shade darker and cooler than the wall.
	extern const FLinearColor DefaultBaseCourseColor;

	// 黑漆 door lacquer.
	extern const FLinearColor DefaultDoorPaintColor;

	// 窗欞: a shade off the doors.
	extern const FLinearColor DefaultLatticeColor;

	// 高麗紙: warm off-white.
	extern const FLinearColor DefaultPaperColor;

	// 白灰: interior lime plaster.
	extern const FLinearColor DefaultPlasterColor;
	extern const FLinearColor DefaultEarthColor;
	extern const FLinearColor DefaultPartitionColor;
	// 方磚: floor paving, a shade lighter than the wall's brick.
	extern const FLinearColor DefaultFloorColor;
	// 尺二方磚: the Floor pattern's brick, which a generator authoring floor UVs lays out in.
	inline constexpr double FloorPaverCm = 38.4;

	// Material slots on the spawned mesh.
	enum EMaterialSlot : int32
	{
		MatSlot_Body = 0,
		MatSlot_Roof = 1,
		MatSlot_Wood = 2,
		MatSlot_Stone = 3,   // paifang plinths, 抱鼓石, 階條石
		MatSlot_Paint = 4,   // 彩畫 on 額枋 and 走馬板

		// 下鹼: finer brick, a shade darker than the wall.
		MatSlot_BaseCourse = 5,

		// 板門 and 隔扇 leaves.
		MatSlot_DoorPaint = 6,

		// 窗欞: separate from door paint so it can sit a shade off.
		MatSlot_Lattice = 7,

		// 窗紙 over the lattice.
		MatSlot_Paper = 8,

		// 白灰: room interiors.
		MatSlot_Plaster = 9,

		// 花池 soil and courtyard ground.
		MatSlot_Earth = 10,

		// 板壁: plain unpainted boarding.
		MatSlot_Partition = 11,

		// 正脊: stacked 瓦條 and brick, coursed rather than in 壟.
		MatSlot_Ridge = 12,

		// 花甎寶頂: moulded brick finials, whole pieces rather than courses.
		MatSlot_Finial = 13,

		// 尺二方磚墁地: the square floor paving of a 臺明.
		MatSlot_Floor = 14,

		MatSlot_Count        // must stay last
	};

	// Slot name on the baked mesh.
	inline FName MaterialSlotName(int32 Slot)
	{
		switch (Slot)
		{
		case MatSlot_Body:       return TEXT("Body (青磚)");
		case MatSlot_Roof:       return TEXT("Roof (瓦)");
		case MatSlot_Wood:       return TEXT("Wood (木作)");
		case MatSlot_Stone:      return TEXT("Stone (石作)");
		case MatSlot_Paint:      return TEXT("Paint (彩畫)");
		case MatSlot_BaseCourse: return TEXT("Base Course (下鹼)");
		case MatSlot_DoorPaint:  return TEXT("Door Lacquer (門漆)");
		case MatSlot_Lattice:    return TEXT("Window Lattice (窗欞)");
		case MatSlot_Paper:      return TEXT("Window Paper (窗紙)");
		case MatSlot_Plaster:    return TEXT("Interior Plaster (白灰)");
		case MatSlot_Earth:      return TEXT("Bare Earth (素土)");
		case MatSlot_Partition:  return TEXT("Partitions (板壁)");
		case MatSlot_Ridge:      return TEXT("Ridge (正脊)");
		case MatSlot_Finial:     return TEXT("Finial (寶頂)");
		case MatSlot_Floor:      return TEXT("Floor Paving (方磚)");
		default:                 return TEXT("Unnamed");
		}
	}
}

// Colour per material slot, optionally overridden by a material.
USTRUCT(BlueprintType)
struct FHutongPalette
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance",
		meta = (DisplayName = "Body (青磚)", HideAlphaChannel, ToolTip="Colour of the brick body."))
	FLinearColor Body = HutongGen::DefaultBrickColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance",
		meta = (UIMin = "0", UIMax = "0.6", ClampMin = "0", ClampMax = "0.9", ToolTip="How much darker the roof and wall cap are than the body, from 0 to 0.9."))
	float RoofDarkening = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance",
		meta = (DisplayName = "Wood (木作)", HideAlphaChannel, ToolTip="Colour of the columns, lintels and other woodwork."))
	FLinearColor Wood = HutongGen::DefaultWoodColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance",
		meta = (DisplayName = "Stone (石作)", HideAlphaChannel, ToolTip="Colour of the stonework."))
	FLinearColor Stone = HutongGen::DefaultStoneColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance",
		meta = (DisplayName = "Paint (彩畫)", HideAlphaChannel, ToolTip="Colour of the painted decoration (彩畫) on the beams."))
	FLinearColor Paint = HutongGen::DefaultPaintColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance",
		meta = (DisplayName = "Base Course (下鹼)", HideAlphaChannel, ToolTip="Colour of the base course (下鹼)."))
	FLinearColor BaseCourse = HutongGen::DefaultBaseCourseColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance",
		meta = (DisplayName = "Door Lacquer (門漆)", HideAlphaChannel, ToolTip="Colour of the door leaves."))
	FLinearColor DoorPaint = HutongGen::DefaultDoorPaintColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance",
		meta = (DisplayName = "Window Lattice (窗欞)", HideAlphaChannel, ToolTip="Colour of the window lattice."))
	FLinearColor Lattice = HutongGen::DefaultLatticeColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance",
		meta = (DisplayName = "Window Paper (窗紙)", HideAlphaChannel, ToolTip="Colour of the window paper."))
	FLinearColor Paper = HutongGen::DefaultPaperColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance",
		meta = (DisplayName = "Interior Plaster (白灰)", HideAlphaChannel, ToolTip="Colour of the interior plaster."))
	FLinearColor Plaster = HutongGen::DefaultPlasterColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance",
		meta = (DisplayName = "Bare Earth (素土)", HideAlphaChannel, ToolTip="Colour of bare earth in beds and on the courtyard ground."))
	FLinearColor Earth = HutongGen::DefaultEarthColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance",
		meta = (DisplayName = "Partitions (板壁)", HideAlphaChannel, ToolTip="Colour of the interior timber partitions."))
	FLinearColor Partition = HutongGen::DefaultPartitionColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance",
		meta = (DisplayName = "Floor Paving (方磚)", HideAlphaChannel, ToolTip="Colour of the square floor paving."))
	FLinearColor Floor = HutongGen::DefaultFloorColor;

	// Null (usual): slot gets a BasicShapeMaterial instance tinted with the colour.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Materials",
		meta = (DisplayName = "Body Material (青磚)", ToolTip="Material for the brick body; replaces the body colour when set."))
	TObjectPtr<UMaterialInterface> BodyMaterial = nullptr;

	// Own material, though the roof colour derives from Body.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Materials",
		meta = (DisplayName = "Roof Material (瓦)", ToolTip="Material for the roof tiles; replaces the derived roof colour when set."))
	TObjectPtr<UMaterialInterface> RoofMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Materials",
		meta = (DisplayName = "Wood Material (木作)", ToolTip="Material for the woodwork; replaces the wood colour when set."))
	TObjectPtr<UMaterialInterface> WoodMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Materials",
		meta = (DisplayName = "Stone Material (石作)", ToolTip="Material for the stonework; replaces the stone colour when set."))
	TObjectPtr<UMaterialInterface> StoneMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Materials",
		meta = (DisplayName = "Paint Material (彩畫)", ToolTip="Material for the painted decoration (彩畫); replaces the paint colour when set."))
	TObjectPtr<UMaterialInterface> PaintMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Materials",
		meta = (DisplayName = "Base Course Material (下鹼)", ToolTip="Material for the base course (下鹼); replaces its colour when set."))
	TObjectPtr<UMaterialInterface> BaseCourseMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Materials",
		meta = (DisplayName = "Door Lacquer Material (門漆)", ToolTip="Material for the door leaves; replaces the door lacquer colour when set."))
	TObjectPtr<UMaterialInterface> DoorPaintMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Materials",
		meta = (DisplayName = "Lattice Material (窗欞)", ToolTip="Material for the window lattice; replaces the lattice colour when set."))
	TObjectPtr<UMaterialInterface> LatticeMaterial = nullptr;

	// Use a translucent material to let light through.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Materials",
		meta = (DisplayName = "Window Paper Material (窗紙)", ToolTip="Material for the window paper; replaces the paper colour when set."))
	TObjectPtr<UMaterialInterface> PaperMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Materials",
		meta = (DisplayName = "Interior Plaster Material (白灰)", ToolTip="Material for the interior plaster; replaces the plaster colour when set."))
	TObjectPtr<UMaterialInterface> PlasterMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Materials",
		meta = (DisplayName = "Bare Earth Material (素土)", ToolTip="Material for bare earth; replaces the earth colour when set."))
	TObjectPtr<UMaterialInterface> EarthMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Materials",
		meta = (DisplayName = "Partition Material (板壁)", ToolTip="Material for the interior partitions; replaces the partition colour when set."))
	TObjectPtr<UMaterialInterface> PartitionMaterial = nullptr;

	// Roof colour but own material: the ridge is coursed, not in 壟.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Materials",
		meta = (DisplayName = "Ridge Material (正脊)", ToolTip="Material for the main ridge (正脊); replaces the derived roof colour there when set."))
	TObjectPtr<UMaterialInterface> RidgeMaterial = nullptr;

	// Roof colour, own material: moulded 花甎, not coursed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Materials",
		meta = (DisplayName = "Finial Material (寶頂)", ToolTip="Material for the roof finials (寶頂); replaces the derived roof colour there when set."))
	TObjectPtr<UMaterialInterface> FinialMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Materials",
		meta = (DisplayName = "Floor Paving Material (方磚)", ToolTip="Material for the floor paving; replaces the paving colour when set."))
	TObjectPtr<UMaterialInterface> FloorMaterial = nullptr;

	// Body scaled in linear space, alpha kept.
	FLinearColor GetRoofColor() const
	{
		const float Scale = 1.0f - FMath::Clamp(RoofDarkening, 0.0f, 0.9f);
		return FLinearColor(Body.R * Scale, Body.G * Scale, Body.B * Scale, Body.A);
	}

	// Sole slot → colour map.
	FLinearColor GetSlotColor(int32 Slot) const
	{
		switch (Slot)
		{
		case HutongGen::MatSlot_Roof:
		case HutongGen::MatSlot_Ridge:
		case HutongGen::MatSlot_Finial: return GetRoofColor();
		case HutongGen::MatSlot_Floor: return Floor;
		case HutongGen::MatSlot_Wood:  return Wood;
		case HutongGen::MatSlot_Stone: return Stone;
		case HutongGen::MatSlot_Paint: return Paint;
		case HutongGen::MatSlot_BaseCourse: return BaseCourse;
		case HutongGen::MatSlot_DoorPaint:  return DoorPaint;
		case HutongGen::MatSlot_Lattice:    return Lattice;
		case HutongGen::MatSlot_Paper:      return Paper;
		case HutongGen::MatSlot_Plaster:    return Plaster;
		case HutongGen::MatSlot_Earth:      return Earth;
		case HutongGen::MatSlot_Partition:  return Partition;
		default:                       return Body;
		}
	}

	// Sole slot → material map. Null = tint the default.
	UMaterialInterface* GetSlotMaterial(int32 Slot) const
	{
		switch (Slot)
		{
		case HutongGen::MatSlot_Roof:  return RoofMaterial;
		case HutongGen::MatSlot_Wood:  return WoodMaterial;
		case HutongGen::MatSlot_Stone: return StoneMaterial;
		case HutongGen::MatSlot_Paint: return PaintMaterial;
		case HutongGen::MatSlot_BaseCourse: return BaseCourseMaterial;
		case HutongGen::MatSlot_DoorPaint:  return DoorPaintMaterial;
		case HutongGen::MatSlot_Lattice:    return LatticeMaterial;
		case HutongGen::MatSlot_Paper:      return PaperMaterial;
		case HutongGen::MatSlot_Plaster:    return PlasterMaterial;
		case HutongGen::MatSlot_Earth:      return EarthMaterial;
		case HutongGen::MatSlot_Partition:  return PartitionMaterial;
		case HutongGen::MatSlot_Ridge:      return RidgeMaterial;
		case HutongGen::MatSlot_Finial:     return FinialMaterial;
		case HutongGen::MatSlot_Floor:      return FloorMaterial;
		default:                       return BodyMaterial;
		}
	}
};
