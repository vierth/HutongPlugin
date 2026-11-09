#pragma once

#include "CoreMinimal.h"
#include "Generation/SiheyuanGenerator.h"
#include "Generation/GateHouseGenerator.h"
#include "Generation/ShopfrontGenerator.h"
#include "Generation/StoreyGenerator.h"
#include "Generation/HallGenerator.h"
#include "Generation/PavilionGenerator.h"
#include "Generation/CorridorGenerator.h"
#include "Generation/InnerGateGenerator.h"
#include "Generation/ScreenWallGenerator.h"
#include "Generation/SmallBuildingGenerator.h"
#include "Generation/PassageGenerator.h"
#include "Generation/PaifangGenerator.h"
#include "Generation/EarPassageGenerator.h"

// Reads the params' own GetRoofBaseHeight/GetRoofRise and the generator's section, as the built roof does.
namespace HutongGen::Ridge
{
	inline double Crown(EHutongPurlins Purlins, double HalfDepth, double Overhang, double Roll)
	{
		return Jiajia::MakeSection(Purlins, FMath::Max(HalfDepth, 0.5), FMath::Max(Overhang, 0.0), Roll).CrownFactor();
	}

	// Accessors read tool-driven Width/Depth; fill the footprint first.
	inline double House(FHutongSiheyuanParams P, double Frontage, double Depth)
	{
		P.Width = FMath::Max(Frontage, 1.0);
		P.Depth = FMath::Max(Depth, 1.0);
		return P.GetRoofBaseHeight() + P.GetRoofRise() * P.GetRoofSection().CrownFactor();
	}

	inline double Gate(const FHutongGateHouseParams& P, double Depth)
	{
		return P.GetRoofBaseHeight() + P.GetRoofRise(Depth) * Crown(P.GetPurlins(), 0.5 * Depth, P.GetRoofOverhang(), P.RoofApexRoll);
	}

	inline double Shop(const FHutongShopfrontParams& P) { return P.GetRoofBaseHeight() + P.GetRoofRise() * Crown(EHutongPurlins::Five, 0.5 * P.Depth, P.RoofOverhang, P.RoofApexRoll); }
	inline double Storey(const FHutongStoreyParams& P) { return P.GetRoofBaseHeight() + P.GetRoofRise() * Crown(EHutongPurlins::Five, 0.5 * P.Depth, P.RoofOverhang, P.RoofApexRoll); }

	// Excludes the 翼角 corner lift.
	inline double Hall(FHutongHallParams P, double Frontage, double Depth)
	{
		P.Width = FMath::Max(Frontage, 1.0);
		P.Depth = FMath::Max(Depth, 1.0);
		return P.GetRoofBaseHeight() + P.GetRoofRise(Depth) * P.GetRoofSection(Depth).CrownFactor();
	}
	inline double Pavilion(FHutongPavilionParams P, double Width, double Depth)
	{
		P.Width = FMath::Max(Width, 1.0);
		P.Depth = FMath::Max(Depth, 1.0);
		return P.GetRoofBaseHeight() + P.GetRoofRise() * P.GetRoofSection().CrownFactor();
	}

	inline double Corridor(const FHutongCorridorParams& P) { return P.GetRoofBaseHeight() + P.GetRoofRise() * Crown(EHutongPurlins::Three, 0.5 * P.GetFootprintDepth(), P.RoofOverhang, P.GetRoofRoll()); }
	// 一殿一卷: the front 殿's ridge; the 卷's crown is level with it.
	inline double InnerGate(const FHutongInnerGateParams& P)
	{
		if (P.IsHallAndRoll()) return P.GetRoofBaseHeight() + P.GetRoofRise();
		return P.GetRoofBaseHeight() + P.GetRoofRise() * Crown(EHutongPurlins::Three, 0.5 * P.Depth, P.RoofOverhang, P.RoofApexRoll);
	}
	inline double ScreenWall(const FHutongScreenWallParams& P) { return P.GetEaveHeight() + P.GetRoofRise() * Crown(EHutongPurlins::Three, 0.5 * P.Thickness, P.RoofOverhang, P.RoofApexRoll); }

	inline double Passage(const FHutongPassageParams& P) { return P.GetEaveHeight() + P.GetRoofRise(P.GetRoofSpan()); }

	inline double Paifang(const FHutongPaifangParams& P) { return P.GetRoofEaveZ() + P.GetRoofRise(); }
	inline double Small(const FHutongSmallBuildingParams& P)
	{
		const double D = P.GetDepth();
		if (P.Roof == EHutongSmallRoof::LeanTo)
		{
			return P.GetEaveHeight() + P.GetRoofRise() * (D + FHutongSmallBuildingParams::LeanToRearOverhang) / D + FHutongSmallBuildingParams::LeanToSlab;
		}
		return P.GetRoofBaseHeight() + P.GetRoofRise() * Crown(EHutongPurlins::Three, 0.5 * D, P.GetRoofOverhang(), 0.0);
	}
	// The room's ridge in practice; a passage under its own roof over the whole frontage has only that one.
	inline double EarPassage(FHutongEarPassageParams P, double Frontage, double Depth)
	{
		P.Width = FMath::Max(Frontage, 1.0);
		P.Depth = FMath::Max(Depth, 1.0);
		const FHutongSiheyuanParams R = P.RoomParams();
		if (!P.HasPassage()) return House(R, R.Width, R.Depth);
		if (!P.HasRoom() && !P.bRoofOverPassage) return Passage(P.PassageParams());
		return FMath::Max(House(R, R.Width, R.Depth), Passage(P.PassageParams()));
	}
}
