#include "Generation/HutongCollision.h"
#include "Tools/GalleryTool.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "ShapeApproximation/SimpleShapeSet3.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongCollisionGalleryTest,
	"HutongLayout.Collision.Gallery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongCollisionGalleryTest::RunTest(const FString& Parameters)
{
	UHutongGalleryToolProperties* S = NewObject<UHutongGalleryToolProperties>();
	int32 Passages = 0, Solids = 0;
	HutongGallery::ForEachBuilt(S, EHutongDetail::Far,
		[this, &Passages, &Solids](const FString& Label, const FVector2D& Footprint, const UE::Geometry::FDynamicMesh3& Mesh)
		{
			FKAggregateGeom Geom;
			UE::Geometry::FSimpleShapeSet3d Shapes;
			HutongGen::FCollisionStats Stats;
			HutongGen::BuildSimpleCollision(Mesh, Geom, &Stats, &Shapes);
			int32 Blocked = 0;
			for (const double Z : { 60.0, 100.0, 150.0 })
			{
				Blocked += HutongGen::CollisionContains(Shapes, FVector3d(0.5 * Footprint.X, 0.5 * Footprint.Y, Z)) ? 1 : 0;
			}
			UE_LOG(LogTemp, Display, TEXT("collision: %-60s pieces %4d kept %4d boxes %4d hulls %4d centre %s"),
				*Label, Stats.Pieces, Stats.Kept, Stats.Boxes, Stats.Convexes, Blocked ? TEXT("blocked") : TEXT("open"));
			TestTrue(FString::Printf(TEXT("%s: has collision"), *Label), Stats.Kept == 0 || Geom.GetElementCount() > 0);
			// A way through stays a way through; a solid wall stays solid.
			const bool bPassage = Label.Contains(TEXT("Doorway")) || Label.Contains(TEXT("Moon Gate"))
				|| Label.Contains(TEXT("Corridor")) || Label.Contains(TEXT("Pavilion")) || Label.Contains(TEXT("Inner Gate ("))
				|| Label.Contains(TEXT("Wall Gate")) || Label.Contains(TEXT("Memorial Arch"));
			if (bPassage)
			{
				++Passages;
				TestEqual(FString::Printf(TEXT("%s: the way through is open"), *Label), Blocked, 0);
			}
			if (Label == TEXT("Wall (牆)") || Label.StartsWith(TEXT("Screen Wall")))
			{
				++Solids;
				TestEqual(FString::Printf(TEXT("%s: the wall is solid"), *Label), Blocked, 3);
			}
		});
	UE_LOG(LogTemp, Display, TEXT("collision: %d ways through, %d solid walls"), Passages, Solids);
	// The cases are picked by gallery label: a renamed label must not drop them silently.
	TestTrue(FString::Printf(TEXT("the gallery still offers ways through (%d)"), Passages), Passages >= 22);
	TestTrue(FString::Printf(TEXT("and solid walls (%d)"), Solids), Solids >= 2);
	return true;
}

#endif
