#include "Tools/GalleryTool.h"
#include "Generation/HutongPalette.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// First triangle a vertical ray from From meets going Dir (+1 up, -1 down); -1 if none.
	int32 FirstHitVertical(const UE::Geometry::FDynamicMesh3& M, const FVector3d& From, double Dir)
	{
		int32 Best = -1;
		double BestT = BIG_NUMBER;
		for (const int32 Tid : M.TriangleIndicesItr())
		{
			FVector3d A, B, C;
			M.GetTriVertices(Tid, A, B, C);
			const double D = (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
			if (FMath::Abs(D) < 1e-9) continue;
			const double U = ((From.X - A.X) * (C.Y - A.Y) - (From.Y - A.Y) * (C.X - A.X)) / D;
			const double V = ((B.X - A.X) * (From.Y - A.Y) - (B.Y - A.Y) * (From.X - A.X)) / D;
			if (U < 0.0 || V < 0.0 || U + V > 1.0) continue;
			const double Z = A.Z + U * (B.Z - A.Z) + V * (C.Z - A.Z);
			const double T = (Z - From.Z) * Dir;
			if (T > 0.0 && T < BestT) { BestT = T; Best = Tid; }
		}
		return Best;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongExposedFrameTest,
	"HutongLayout.Roofs.ExposedFrame",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongExposedFrameTest::RunTest(const FString& Parameters)
{
	// 徹上明造: from inside, looking up always meets timber (the shell's boards, the frame, the rafters),
	// never sky or tile; from above, the roof comes first (no member through it).
	UHutongGalleryToolProperties* S = NewObject<UHutongGalleryToolProperties>();
	int32 Pieces = 0;
	HutongGallery::ForEachBuilt(S, EHutongDetail::Near,
		[&](const FString& Label, const FVector2D& Footprint, const UE::Geometry::FDynamicMesh3& M)
		{
			// Corridors carry it by default, unlabelled.
			if (!Label.Contains(TEXT("Exposed Frame")) && !Label.Contains(TEXT("遊廊")) && !Label.Contains(TEXT("亭) · Pyramidal")) && !Label.Contains(TEXT("圓亭"))) return;
			++Pieces;
			const auto* Mat = M.Attributes()->GetMaterialID();
			int32 Sky = 0, Tile = 0, Timber = 0, Poke = 0;
			for (int32 i = 1; i <= 5; ++i)
			{
				for (int32 j = 1; j <= 3; ++j)
				{
					const double X = Footprint.X * (0.1 + 0.8 * i / 6.0);
					const double Y = Footprint.Y * (0.15 + 0.7 * j / 4.0);
					const int32 Up = FirstHitVertical(M, FVector3d(X, Y, 150.0), 1.0);
					if (Up < 0) { ++Sky; continue; }
					const int32 Slot = Mat->GetValue(Up);
					if (Slot == HutongGen::MatSlot_Roof || Slot == HutongGen::MatSlot_Ridge) ++Tile;
					else ++Timber;
					const int32 Down = FirstHitVertical(M, FVector3d(X, Y, 5000.0), -1.0);
					const int32 Top = (Down >= 0) ? Mat->GetValue(Down) : -1;
					if (Top != HutongGen::MatSlot_Roof && Top != HutongGen::MatSlot_Ridge && Top != HutongGen::MatSlot_Finial) ++Poke;
				}
			}
			TestTrue(FString::Printf(TEXT("%s: overhead is timber (%d timber, %d tile, %d sky)"), *Label, Timber, Tile, Sky),
				Timber > 0 && Tile == 0 && Sky == 0);
			TestEqual(FString::Printf(TEXT("%s: nothing stands through the roof"), *Label), Poke, 0);
		});
	TestTrue(TEXT("the gallery shows exposed frames"), Pieces >= 5);
	return true;
}

#endif
