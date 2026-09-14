#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInterface.h"
#include "HutongPalette.generated.h"

namespace HutongGen
{
	// 青磚: the cool medium grey of fired brick in Beijing hutong walls.
	extern const FLinearColor DefaultBrickColor;

	// Grey tile, a shade down from the brick.
	extern const FLinearColor DefaultRoofColor;

	// Dark oxblood for the woodwork.
	extern const FLinearColor DefaultWoodColor;

	// 青白石: the cool grey limestone a lane is built from.
	extern const FLinearColor DefaultStoneColor;

	// 彩畫: the blue-green of 旋子 and 蘇式 beam painting.
	extern const FLinearColor DefaultPaintColor;

	// 下鹼: the same clay as the wall above, better made — a shade darker and cooler.
	extern const FLinearColor DefaultBaseCourseColor;

	// 黑漆: the ordinary door lacquer.
	extern const FLinearColor DefaultDoorPaintColor;

	// 窗欞, a shade off the doors: lattice and leaves are painted in the same campaign but rarely out of the same pot.
	extern const FLinearColor DefaultLatticeColor;

	// 高麗紙, warm and off-white.
	extern const FLinearColor DefaultPaperColor;

	// 白灰: the lime plaster a room is finished in. Bare brick out, whitewash in.
	extern const FLinearColor DefaultPlasterColor;
	extern const FLinearColor DefaultEarthColor;
	extern const FLinearColor DefaultPartitionColor;

	// Material slots on the spawned mesh.
	enum EMaterialSlot : int32
	{
		MatSlot_Body = 0,
		MatSlot_Roof = 1,
		MatSlot_Wood = 2,
		MatSlot_Stone = 3,   // paifang plinths, 抱鼓石, 階條石
		MatSlot_Paint = 4,   // 彩畫 on 額枋 and 走馬板

		// 下鹼: better brick than the wall above, laid and dressed differently, reading a shade darker.
		MatSlot_BaseCourse = 5,

		// 板門 and 隔扇 leaves: the most saturated thing on the street, and the reason a hutong is not monochrome.
		MatSlot_DoorPaint = 6,

		// 窗欞: lattice and leaves are painted in the same campaign but rarely out of the same pot.
		MatSlot_Lattice = 7,

		// 窗紙: 高麗紙 over the lattice.
		MatSlot_Paper = 8,

		// 白灰: the inside of a room. Bare 青磚 outside, whitewash in.
		MatSlot_Plaster = 9,

		// The soil a 花池 retains and the swept ground of the courtyard.
		MatSlot_Earth = 10,

		// 板壁: plain boarding, waxed or oiled at best, of a piece with the floor rather than with the painted frame.
		MatSlot_Partition = 11,

		MatSlot_Count        // must stay last
	};

	// What the slot is called on the baked mesh.
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
		default:                 return TEXT("Unnamed");
		}
	}
}

// Every surface a spawned building wears: a colour per slot, and optionally a material instead.
USTRUCT(BlueprintType)
struct FHutongPalette
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance",
		meta = (DisplayName = "Body (青磚)", HideAlphaChannel, ToolTip="Colour of the brick body."))
	FLinearColor Body = HutongGen::DefaultBrickColor;

	// How much darker the roof and wall cap are than the body.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance",
		meta = (UIMin = "0", UIMax = "0.6", ClampMin = "0", ClampMax = "0.9", ToolTip="How much darker the roof and wall cap are than the body, from 0 to 0.9."))
	float RoofDarkening = 0.35f;

	// Columns and lintels.
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

	// Empty is the ordinary case: the slot gets a tinted BasicShapeMaterial instance in the colour above.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Materials",
		meta = (DisplayName = "Body Material (青磚)", ToolTip="Material for the brick body; replaces the body colour when set."))
	TObjectPtr<UMaterialInterface> BodyMaterial = nullptr;

	// Its own slot rather than derived from the body the way the colour is.
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
		meta = (DisplayName = "Paint Material (彩畫)", ToolTip="Material for the painted decoration (彩畫) on the beams; replaces the paint colour when set."))
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

	// Translucent, if you want the light through it.
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

	// The body scaled in linear space, alpha untouched.
	FLinearColor GetRoofColor() const
	{
		const float Scale = 1.0f - FMath::Clamp(RoofDarkening, 0.0f, 0.9f);
		return FLinearColor(Body.R * Scale, Body.G * Scale, Body.B * Scale, Body.A);
	}

	// The only place a slot maps to a colour.
	FLinearColor GetSlotColor(int32 Slot) const
	{
		switch (Slot)
		{
		case HutongGen::MatSlot_Roof:  return GetRoofColor();
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

	// The only place a slot maps to a material. Null means tint the default instead.
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
		default:                       return BodyMaterial;
		}
	}
};
