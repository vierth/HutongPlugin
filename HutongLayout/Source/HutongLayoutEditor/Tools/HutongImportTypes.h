#pragma once

#include "CoreMinimal.h"

namespace HutongExchange { struct FSceneFile; }

namespace HutongImportTypes
{
	// Asks once per type, so a file predating a class split or rename keeps those buildings.
	bool ResolveUnknownTypes(HutongExchange::FSceneFile& File);

	bool SkipUnknownTypes(HutongExchange::FSceneFile& File);
}
