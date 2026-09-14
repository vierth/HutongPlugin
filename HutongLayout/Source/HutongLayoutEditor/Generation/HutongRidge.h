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
#include "Generation/PassageGenerator.h"
#include "Generation/PaifangGenerator.h"
#include "Generation/EarPassageGenerator.h"

// Where each roofed type's ridge lands: the fold of the roof, under whatever ridge course sits on
// it — or on a 捲棚 the rounded crown, which sits below the fold by the section's CrownFactor.
// Every answer goes through the params' own GetEaveHeight/GetRoofRise and the very section the
// generator builds with — an estimate that reads a different figure from the roof is how a gate
// was lifted "clear" of a row and built with its ridge below it. HutongLayout.Roofs.RidgeEstimates
// measures each of these against the built mesh.
namespace HutongGen::Ridge
{
	inline double Crown(EHutongPurlins Purlins, double HalfDepth, double Overhang, double Roll)
	{
		return Jiajia::MakeSection(Purlins, FMath::Max(HalfDepth, 0.5), FMath::Max(Overhang, 0.0), Roll).CrownFactor();
	}

	// Both accessors read Params.Width and Params.Depth, which are tool-driven and default to a
	// different building, so the footprint is filled in first.
	inline double House(FHutongSiheyuanParams P, double Frontage, double Depth)
	{
		P.Width = FMath::Max(Frontage, 1.0);
		P.Depth = FMath::Max(Depth, 1.0);
		return P.GetEaveHeight() + P.GetRoofRise() * P.GetRoofSection().CrownFactor();
	}

	inline double Gate(const FHutongGateHouseParams& P, double Depth)
	{
		return P.GetEaveHeight() + P.GetRoofRise(Depth) * Crown(P.GetPurlins(), 0.5 * Depth, P.RoofOverhang, P.RoofApexRoll);
	}

	// Both build their roof over Params.Depth with five purlins.
	inline double Shop(const FHutongShopfrontParams& P) { return P.GetEaveHeight() + P.GetRoofRise() * Crown(EHutongPurlins::Five, 0.5 * P.Depth, P.RoofOverhang, P.RoofApexRoll); }
	inline double Storey(const FHutongStoreyParams& P) { return P.GetEaveHeight() + P.GetRoofRise() * Crown(EHutongPurlins::Five, 0.5 * P.Depth, P.RoofOverhang, P.RoofApexRoll); }

	// The 翼角 lift at the corners is not the ridge.
	inline double Hall(const FHutongHallParams& P, double Depth) { return P.GetEaveHeight() + P.GetRoofRise(Depth) * Crown(P.Purlins, 0.5 * Depth, P.RoofOverhang, P.RoofApexRoll); }
	inline double Pavilion(const FHutongPavilionParams& P, double Depth) { return P.GetEaveHeight() + P.GetRoofRise(Depth) * Crown(P.Purlins, 0.5 * (Depth + 2.0 * FMath::Max(P.RoofOverhang, 0.0)), 0.0, P.RoofApexRoll); }

	// The corridor's roof spans its footprint depth; the inner gate's its Depth; the screen's its thickness. All three purlins.
	inline double Corridor(const FHutongCorridorParams& P) { return P.GetEaveHeight() + P.GetRoofRise() * Crown(EHutongPurlins::Three, 0.5 * P.GetFootprintDepth(), P.RoofOverhang, P.RoofApexRoll); }
	inline double InnerGate(const FHutongInnerGateParams& P) { return P.GetEaveHeight() + P.GetRoofRise() * Crown(EHutongPurlins::Three, 0.5 * P.Depth, P.RoofOverhang, P.RoofApexRoll); }
	inline double ScreenWall(const FHutongScreenWallParams& P) { return P.GetEaveHeight() + P.GetRoofRise() * Crown(EHutongPurlins::Three, 0.5 * P.Thickness, P.RoofOverhang, P.RoofApexRoll); }

	// Over the roof's span, which is the params' own Width.
	inline double Passage(const FHutongPassageParams& P) { return P.GetEaveHeight() + P.GetRoofRise(P.GetRoofSpan()); }

	// The central 樓's; the side 樓 sit a drop lower.
	inline double Paifang(const FHutongPaifangParams& P) { return P.GetRoofEaveZ() + P.GetRoofRise(); }
	// The taller of the room's ridge and the passage roof's, which is the room's in practice.
	inline double EarPassage(FHutongEarPassageParams P, double Frontage, double Depth)
	{
		P.Width = FMath::Max(Frontage, 1.0);
		P.Depth = FMath::Max(Depth, 1.0);
		const FHutongSiheyuanParams R = P.RoomParams();
		return FMath::Max(House(R, R.Width, R.Depth), Passage(P.PassageParams()));
	}
}
