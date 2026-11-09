#include "Misc/AutomationTest.h"
#include "Generation/StoreyGenerator.h"
#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongDoor.h"
#include "DynamicMesh/DynamicMesh3.h"

#if WITH_DEV_AUTOMATION_TESTS

using UE::Geometry::FDynamicMesh3;

namespace
{
	double FurthestForwardBetween(const FDynamicMesh3& Mesh, double Z0, double Z1)
	{
		double Front = 0.0;
		for (int32 vid : Mesh.VertexIndicesItr())
		{
			const FVector3d V = Mesh.GetVertex(vid);
			if (V.Z < Z0 || V.Z > Z1) continue;
			Front = FMath::Min(Front, V.Y);
		}
		return Front;
	}

	double HighestVertex(const FDynamicMesh3& Mesh)
	{
		double Top = -1.0e9;
		for (int32 vid : Mesh.VertexIndicesItr()) Top = FMath::Max(Top, Mesh.GetVertex(vid).Z);
		return Top;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongStoreyTest, "HutongLayout.Storey.StoreyLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongStoreyTest::RunTest(const FString& Parameters)
{
	FHutongStoreyParams P;
	P.Width = 1000.0;
	P.Depth = 620.0;

	const double StoreyLine = P.GetStoreyLineHeight();
	const double Eave = P.GetEaveHeight();

	TestEqual(TEXT("the eave is the two storeys and the platform"),
		Eave, P.FloorHeight + P.LowerStoreyHeight + P.UpperStoreyHeight, 0.01);
	TestTrue(TEXT("a 樓 stands taller than the street around it"), Eave > 550.0);

	// The shop is enterable: the mesh is its own collision.
	TestTrue(TEXT("the shopfront's head clears a crouched body"),
		P.GetOpeningTopHeight() - P.FloorHeight >= HutongGen::Passage::MinClearHeight - 0.01);

	FDynamicMesh3 Full;
	HutongGen::BuildStorey(Full, P);
	if (!TestTrue(TEXT("the 樓 builds"), Full.TriangleCount() > 0)) return false;

	const double BandLow = StoreyLine - P.SkirtDrop - 1.0;
	const double BandHigh = StoreyLine + P.DeckThickness + P.RailHeight + 1.0;
	const double Reach = FurthestForwardBetween(Full, BandLow, BandHigh);
	TestTrue(TEXT("the skirt and gallery stand out in front of the facade"),
		Reach <= -0.5 * P.GalleryDepth);
	TestTrue(TEXT("and no further out than the skirt they sit on"),
		Reach >= -P.SkirtProjection - P.RailSection - 6.0);

	FHutongStoreyParams Plain = P;
	Plain.bHasSkirtRoof = false;
	Plain.bHasGallery = false;
	Plain.bHasSignboard = false;
	FDynamicMesh3 Tall;
	HutongGen::BuildStorey(Tall, Plain);

	TestTrue(TEXT("the storey line is most of what the type costs"),
		Full.TriangleCount() > Tall.TriangleCount());
	const double ColR = P.GetColumnRadiusFor(P.Width, P.Depth);
	TestTrue(TEXT("without it nothing but the columns reaches past the facade"),
		FurthestForwardBetween(Tall, BandLow, BandHigh) > -(ColR + 2.0));
	TestEqual(TEXT("and the building is the same height either way"),
		HighestVertex(Tall), HighestVertex(Full), 0.5);

	FHutongStoreyParams Taller = P;
	Taller.UpperStoreyHeight = P.UpperStoreyHeight + 100.0;
	FDynamicMesh3 TallerMesh;
	HutongGen::BuildStorey(TallerMesh, Taller);
	TestTrue(TEXT("raising the upper storey raises the ridge"),
		HighestVertex(TallerMesh) > HighestVertex(Full) + 90.0);

	FDynamicMesh3 Block;
	UHutongStoreyBuildingComponent::BuildStoreyMesh(P, EHutongBaySide::MinusY, 0,
		P.Width, P.Depth, Block, EHutongDetail::Massing);
	TestTrue(TEXT("the block builds"), Block.TriangleCount() > 0);
	TestTrue(TEXT("and reaches past the storey line"), HighestVertex(Block) > StoreyLine + 100.0);
	return true;
}

#endif
