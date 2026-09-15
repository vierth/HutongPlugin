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
		// No overhang either side: the roof is carried on the two walls it runs between, and its edges are buried in them.
		Roof.FrontOverhang = 0.0;
		Roof.RearOverhang = 0.0;
		Roof.FasciaDepth = 0.0;
		Roof.FasciaWidth = 0.0;
		Roof.RafterSection = 0.0;

		// 硬山, flush at both ends: the south end sits over the 隔牆 that closes the passage and the north end stops on the building across the back court.
		Roof.GableOverhang = 0.0;

		// 三檁 — one 步架 a side, so a single straight slope each way, which is all a span this narrow wants.
		Roof.Section = Jiajia::MakeSection(EHutongPurlins::Three, 0.5 * Span, 0.0, 0.0);
		Roof.Rise = P.GetRoofRise(Span);
		Roof.Tile = EHutongRoofTile::He;

		// AppendGableRoof owns its own material tagging, so the passage cannot come out brick.
		// The roof bears into a wall at each side: no 山牆, no rake.
		Roof.RakeDepth = 0.0;
		Shell::AppendGableRoof(Mesh, Run, Span, Eave, Roof);
	}
}
