#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Templates/Function.h"

namespace HutongGen
{
	// Shared by 鋪面房 and 樓 so the two agree on a boarded bay.
	namespace ShopBay
	{
		struct FFront
		{
			double FaceY = 0.0;

			double BoardWidth = 26.0;
			double BoardThickness = 5.0;

			double SillZ = 0.0;
			double HeadZ = 300.0;

			// Counted outward from the middle bay; zero boards up the whole front.
			int32 OpenBayCount = 1;

			enum class ECounter : uint8 { None, Inside, Street };
			ECounter Counter = ECounter::Inside;
			double InsideDepth = 400.0;
			double CounterHeight = 88.0;
			double CounterDepth = 52.0;
		};

		// Width of the way in past any counter: a walker with room to spare.
		inline constexpr double WalkGapCm = 110.0;
		// Customers' floor between the front and an inside counter.
		inline constexpr double InsideSetbackCm = 130.0;

		void OpenBayRange(int32 BayCount, int32 OpenBayCount, int32& OutLo, int32& OutHi);

		void AppendFront(
			UE::Geometry::FDynamicMesh3& Mesh,
			const TFunctionRef<double(int32)>& BoundaryX,
			int32 BayCount, double ColumnRadius, const FFront& F);
	}
}
