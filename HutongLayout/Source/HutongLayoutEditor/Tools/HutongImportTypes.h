#pragma once

#include "CoreMinimal.h"

namespace HutongExchange { struct FSceneFile; }

// What to build for a type the file names and this build does not have.
namespace HutongImportTypes
{
	// Asks once per unknown type, whatever the record count; else a file predating a split or rename
	// (院牆 and 隔牆 were one class) loses those buildings to a log line. Fills File.TypeRemap; returns
	// false to abandon the import. No dialog when nothing is unknown.
	bool ResolveUnknownTypes(HutongExchange::FSceneFile& File);

	// Headless variant: skips every unknown type instead of blocking on a modal.
	bool SkipUnknownTypes(HutongExchange::FSceneFile& File);
}
