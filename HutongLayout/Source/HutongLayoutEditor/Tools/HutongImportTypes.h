#pragma once

#include "CoreMinimal.h"

namespace HutongExchange { struct FSceneFile; }

// What to build for a type the file names and this build does not have.
namespace HutongImportTypes
{
	// Puts the question to the user once per unknown type, whatever the record count: a file
	// written before a type was split or renamed — 院牆 and 隔牆 were one class — otherwise loses
	// every one of those buildings to a line in the log. Fills File.TypeRemap and returns false if
	// the import should be abandoned. A file with nothing unknown in it never shows a dialog.
	bool ResolveUnknownTypes(HutongExchange::FSceneFile& File);

	// The same question with no user in the room: everything unknown is skipped. Headless callers
	// and tests take this rather than blocking on a modal that cannot be answered.
	bool SkipUnknownTypes(HutongExchange::FSceneFile& File);
}
