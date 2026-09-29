#include "Tools/CompoundTool.h"
#include "Tools/GalleryTool.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Not a check: if HUTONG_OBJ_DUMP names a file, writes the default compound at 近 as OBJ (one group
// per material slot) for offline inspection. Otherwise a no-op.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongMeshDumpTest,
	"HutongDev.CompoundObj",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongMeshDumpTest::RunTest(const FString& Parameters)
{
	const FString Path = FPlatformMisc::GetEnvironmentVariable(TEXT("HUTONG_OBJ_DUMP"));
	if (Path.IsEmpty()) return true;

	UHutongCompoundTool* Tool = NewObject<UHutongCompoundTool>();
	Tool->Settings = NewObject<UHutongCompoundToolProperties>(Tool);
	UHutongCompoundToolProperties* S = Tool->Settings;
	S->PlotSize = EHutongCompoundSize::Standard;
	S->ApplyCourtSize();
	S->Plan = EHutongCompoundPlan::ThreeCourtyards;
	S->CourtWalk = EHutongCourtWalk::Linked;

	double W = 0.0, D = 0.0;
	UHutongCompoundTool::GetStampedPlot(S, W, D);
	TArray<HutongCompound::FBuiltSlot> Built;
	Tool->BuildCompound(W, D, EHutongDetail::Near, Built);

	FString Out;
	int32 Base = 1;
	for (const HutongCompound::FBuiltSlot& B : Built)
	{
		const UE::Geometry::FDynamicMesh3& M = B.Mesh;
		Out += FString::Printf(TEXT("# slot %d %.0f %.0f %.0f %.0f\n"), int32(B.Slot.Piece),
			B.Slot.Min.X, B.Slot.Min.Y, B.Slot.Size.X, B.Slot.Size.Y);
		TMap<int32, int32> Index;
		for (const int32 Vid : M.VertexIndicesItr())
		{
			const FVector3d P = M.GetVertex(Vid);
			Out += FString::Printf(TEXT("v %.2f %.2f %.2f\n"), P.X, P.Y, P.Z);
			Index.Add(Vid, Base + Index.Num());
		}
		const auto* Mat = M.HasAttributes() ? M.Attributes()->GetMaterialID() : nullptr;
		int32 Last = -1;
		for (const int32 Tid : M.TriangleIndicesItr())
		{
			const int32 Slot = Mat ? Mat->GetValue(Tid) : 0;
			if (Slot != Last) { Out += FString::Printf(TEXT("usemtl s%d\n"), Slot); Last = Slot; }
			const UE::Geometry::FIndex3i T = M.GetTriangle(Tid);
			Out += FString::Printf(TEXT("f %d %d %d\n"), Index[T.A], Index[T.B], Index[T.C]);
		}
		Base += Index.Num();
	}
	FFileHelper::SaveStringToFile(Out, *Path);
	return true;
}

// Same with HUTONG_GALLERY_DUMP: every gallery placement, in a row.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongGalleryDumpTest,
	"HutongDev.GalleryObj",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongGalleryDumpTest::RunTest(const FString& Parameters)
{
	const FString Path = FPlatformMisc::GetEnvironmentVariable(TEXT("HUTONG_GALLERY_DUMP"));
	if (Path.IsEmpty()) return true;
	UHutongGalleryToolProperties* S = NewObject<UHutongGalleryToolProperties>();
	HutongGallery::WriteObj(S, Path);
	return true;
}

// With HUTONG_GALLERY_PLAN: where the whole gallery puts each piece, one CSV line each (category, cluster,
// label, footprint min and max in cm), to draw the layout offline.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongGalleryPlanDumpTest,
	"HutongDev.GalleryPlan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongGalleryPlanDumpTest::RunTest(const FString& Parameters)
{
	const FString Path = FPlatformMisc::GetEnvironmentVariable(TEXT("HUTONG_GALLERY_PLAN"));
	if (Path.IsEmpty()) return true;
	UHutongGalleryToolProperties* S = NewObject<UHutongGalleryToolProperties>();
	FString Out;
	for (const HutongGallery::FPlacedPiece& P : HutongGallery::Plan(S))
	{
		Out += FString::Printf(TEXT("%s\t%s\t%s\t%.0f\t%.0f\t%.0f\t%.0f\n"), *HutongGallery::CategoryName(P.Category),
			*P.Cluster, *P.Label, P.Footprint.Min.X, P.Footprint.Min.Y, P.Footprint.Max.X, P.Footprint.Max.Y);
	}
	FFileHelper::SaveStringToFile(Out, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	return true;
}

// With HUTONG_SLOT_DUMP: every gallery placement at every level, one line per piece of each triangle's
// material slot in order. Two dumps compare exactly across a refactor that must not move a slot.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHutongSlotDumpTest,
	"HutongDev.SlotSignature",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHutongSlotDumpTest::RunTest(const FString& Parameters)
{
	const FString Path = FPlatformMisc::GetEnvironmentVariable(TEXT("HUTONG_SLOT_DUMP"));
	if (Path.IsEmpty()) return true;
	UHutongGalleryToolProperties* S = NewObject<UHutongGalleryToolProperties>();
	FString Out;
	for (int32 Level = 0; Level < 4; ++Level)
	{
		HutongGallery::ForEachBuilt(S, (EHutongDetail)Level,
			[&](const FString& Label, const FVector2D&, const UE::Geometry::FDynamicMesh3& M)
			{
				const auto* Mat = M.HasAttributes() ? M.Attributes()->GetMaterialID() : nullptr;
				FString Sig;
				for (const int32 Tid : M.TriangleIndicesItr()) Sig.AppendChar(TCHAR('a' + (Mat ? Mat->GetValue(Tid) : 0)));
				Out += FString::Printf(TEXT("%d %s %d %s\n"), Level, *Label.Replace(TEXT(" "), TEXT("_")), M.TriangleCount(), *Sig);
			});
	}
	FFileHelper::SaveStringToFile(Out, *Path);
	return true;
}

#endif
