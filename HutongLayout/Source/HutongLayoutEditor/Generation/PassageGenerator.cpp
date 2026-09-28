#include "Generation/PassageGenerator.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongShell.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	void BuildPassage(FDynamicMesh3& Mesh, const FHutongPassageParams& P)
	{
		const double Run = FMath::Max(P.Length, 1.0);
		const double Span = P.GetRoofSpan();
		const double Eave = P.GetEaveHeight();

		Shell::FRoofParams Roof;
		// No side overhang: the roof bears on the two walls, edges buried in them.
		Roof.FrontOverhang = 0.0;
		Roof.RearOverhang = 0.0;
		Roof.FasciaDepth = 0.0;
		Roof.FasciaWidth = 0.0;
		Roof.RafterSection = 0.0;

		// 硬山 flush both ends: south over the closing 隔牆, north on the building across the back court.
		Roof.GableOverhang = 0.0;

		// 三檁: one 步架 a side, a straight slope each way.
		Roof.Section = Jiajia::MakeSection(EHutongPurlins::Three, 0.5 * Span, 0.0, 0.0);
		Roof.Rise = P.GetRoofRise(Span);
		Roof.Tile = EHutongRoofTile::He;

		// AppendGableRoof tags its own material; no 山牆, no rake: it bears into a wall each side.
		Roof.RakeDepth = 0.0;
		Roof.bTileRuns = P.bHasTileRuns;
		Shell::AppendGableRoof(Mesh, Run, Span, Eave, Roof);
	}
}
