#include "HutongLayoutCommands.h"

#define LOCTEXT_NAMESPACE "HutongLayoutCommands"

void FHutongLayoutCommands::RegisterCommands()
{
	UI_COMMAND(BeginWallTool, "Lane Wall",
		"Place a boundary wall (院牆) onto the lane, with a gate (牆垣式門).\n"
		"Click to start, click to end each leg, click the last end again to finish. G puts the gate on this leg, [ and ] slide it, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift, EKeys::One));

	UI_COMMAND(BeginCourtWallTool, "Court Wall",
		"Place a dividing wall (隔牆) inside the compound, with wall-set gates (隨牆門) and decorative windows (什錦窗).\n"
		"Click to start, click to end each leg, click the last end again to finish. G puts the doorway on this leg, [ and ] slide it, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::Eight));

	UI_COMMAND(BeginSiheyuanTool, "House",
		"Place one dwelling — main hall (正房), side house (廂房), front row (倒座房), rear row (後罩房) or ear room (耳房), chosen by preset.\n"
		"Click to anchor, move, click to fix the footprint, move to pick the facade side, click to place.\n"
		"Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift, EKeys::Two));

	UI_COMMAND(BeginEarPassageTool, "Ear Room",
		"Place an ear room (耳房): plain, with a covered passage (過道), or with no front door.\n"
		"Click to anchor, move, click to fix the footprint, move to pick the facade side, click to place. [ and ] set one to three bays; Shift+[ and ] swap the passage's end.\n"
		"Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginGateHouseTool, "Gate",
		"Place a courtyard gate house — wide-hall gate (廣亮大門), inner-column gate (金柱大門), flush gate (蠻子門) or ruyi gate (如意門).\n"
		"Click to anchor, move, click to fix the footprint, move to pick the side it faces, click to place.\n"
		"Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift, EKeys::Three));

	UI_COMMAND(BeginPaifangTool, "Paifang",
		"Place a memorial arch (牌坊) or roofed memorial arch (牌樓) across a street.\n"
		"Drag along the street to set the span; the depth is fixed. Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift, EKeys::Four));

	UI_COMMAND(BeginInnerGateTool, "Inner Gate",
		"Place an inner gate (垂花門) between the outer and inner courtyards.\n"
		"Click to anchor, move, click to fix the footprint, move to pick which way it faces, click to place.\n"
		"Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift, EKeys::Five));

	UI_COMMAND(BeginCorridorTool, "Corridor",
		"Place a covered corridor (遊廊) linking the buildings round a courtyard.\n"
		"Drag along the run; the width is fixed. [ and ] flip which side it opens onto.\n"
		"Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift, EKeys::Six));

	UI_COMMAND(BeginScreenWallTool, "Screen",
		"Place a screen wall (影壁) facing a gate.\n"
		"Drag to set the length; the depth is fixed. Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift, EKeys::Seven));

	UI_COMMAND(BeginShopfrontTool, "Shop",
		"Place a shopfront (鋪面房).\n"
		"Click to anchor, move, click to fix the footprint, move to pick the side facing the street, click to place.\n"
		"[ and ] change the bay count. Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift, EKeys::Eight));

	UI_COMMAND(BeginStoreyTool, "Multi-Story",
		"Place a two-story shop building (樓).\n"
		"Click to anchor, move, click to fix the footprint, move to pick the side facing the street, click to place.\n"
		"[ and ] change the bay count, - and = raise the upper story. Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::Nine));

	UI_COMMAND(BeginPavilionTool, "Pavilion",
		"Place a garden pavilion (亭) under a pyramidal roof (攢尖).\n"
		"Click to anchor, move, click to place. The plan is held roughly square.\n"
		"Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift, EKeys::Nine));

	UI_COMMAND(BeginPathTool, "Path",
		"Place a paved path (甬路) across a courtyard.\n"
		"Drag along the run; the width is held to a band. Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift, EKeys::Zero));

	// "Temple": "Hall" read as the plain house beside it.
	UI_COMMAND(BeginHallTool, "Temple",
		"Place a temple hall (殿).\n"
		"Click to anchor, move, click to fix the footprint, move to pick the side the facade faces, click to place.\n"
		"[ and ] change the bay count. Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::Two));

	UI_COMMAND(BeginStreetRowTool, "Street Row",
		"Lay out a row of houses (房) or shops (鋪面房) along a street, with gate houses (大門) on chosen bays.\n"
		"Click to anchor, click to size, pick the side the buildings face, click the gate bays, pick the side the gates face. Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::Zero));

	UI_COMMAND(BeginCompoundTool, "Compound",
		"Lay out a whole courtyard house (四合院) as separate buildings.\n"
		"Click to set the southeast corner, move to size the plot, click to lay it out. The orange edge is the street. Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::One));

	UI_COMMAND(BeginFrameTool, "Frame",
		"Place the timber frame (構架) of a main hall (正房), without walls or roof.\n"
		"Stamped at the preset's canonical size: click where it stands, move toward its front, click to place.\n"
		"Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginGalleryTool, "Gallery",
		"Place one of every type and variant on a grid.\n"
		"Click to anchor, move to position the set, click to place. Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::Three));

	UI_COMMAND(BeginGalleryWallsTool, "Wall Gallery",
		"Place every wall: plain, with garden doorways (moon gate 月亮門, wall-set gate 隨牆門, …) and with decorative windows (什錦窗).\n"
		"Click to anchor, move to position the set, click to place. Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginGalleryHousesTool, "House Gallery",
		"Place every house: main hall (正房), side house (廂房), front row (倒座房), rear row (後罩房), ear room (耳房), ear room with passage (耳房過道), covered passage (過道) and timber frames (構架).\n"
		"Click to anchor, move to position the set, click to place. Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginGalleryGatesTool, "Gate Gallery",
		"Place every gate: gate houses (屋宇式門) — wide-hall (廣亮大門), inner-column (金柱大門), flush (蠻子門), ruyi (如意門) — with variants, gates in walls (牆垣式門) and inner gates (垂花門).\n"
		"Click to anchor, move to position the set, click to place. Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginGalleryCourtyardTool, "Court Gallery",
		"Place every courtyard piece: covered corridors (遊廊), screen wall (影壁), paved path (甬路), flower bed (花池) and water jar (魚缸).\n"
		"Click to anchor, move to position the set, click to place. Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginGalleryStreetTool, "Street Gallery",
		"Place every street building: shopfronts (鋪面房), multi-story buildings (樓) and memorial arches (牌坊, roofed 牌樓).\n"
		"Click to anchor, move to position the set, click to place. Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginGalleryTemplesTool, "Temple Gallery",
		"Place every temple hall (殿) and pavilion (亭).\n"
		"Click to anchor, move to position the set, click to place. Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginFlowerBedTool, "Bed",
		"Place a kerbed flower bed (花池).\n"
		"Click to anchor, move, click again to place. Hold R to rotate, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::Five));

	UI_COMMAND(BeginWaterJarTool, "Jar",
		"Place a water jar (魚缸).\n"
		"Drag sets the belly diameter; the footprint is held square. - and = change its height.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::Six));

	UI_COMMAND(BeginImportTool, "Import",
		"Load an exported scene file and place the whole set.\n"
		"The blue outlines are the footprints it will lay down. Hold R to turn the set, Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::Seven));

	UI_COMMAND(BeginSmallBuildingTool, "Small",
		"Place a small freestanding building (小房): guard post, shed, lone room or shrine.\n"
		"Click to anchor, move, click again to place. F turns the front, [ and ] set the bays.",
		EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginImperialWallTool, "Imperial City Wall",
		"Place the wall round the imperial city (皇城牆): red plaster, battered, under a yellow glazed tile cap.\n"
		"Click to start, click to end each leg, click the last end again to finish. Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginPalaceWallTool, "Palace Wall",
		"Lay the Forbidden City's wall (紫禁城城牆): battered brick, crenellated outer parapet, red on the side facing the palace.\n"
		"Click to start, click to end each leg, click the last end to finish or the first point to close. F flips the battlements.",
		EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginWallCornerTool, "Wall Corner",
		"Place the corner of a city or palace wall (城角): the square platform under the corner tower, which wall runs join.\n"
		"Click the outside corner, move toward the inside of the city, click again. Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginArrowTowerTool, "Arrow Tower",
		"Place an arrow tower (箭樓) on its platform at the front of a barbican, as at Deshengmen (德勝門).\n"
		"Click the middle of the platform's front, move toward the inside of the city, click again. Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginCityGateTool, "City Gate",
		"Place a gate passage (門洞) through a city wall, with the barbican's gate tower (閘樓) over it.\n"
		"Click the middle of the gate's outer face, move toward the inside of the city, click again. Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginGateComplexTool, "Deshengmen",
		"Lay out a whole city gate as at Deshengmen (德勝門): gate tower (城樓), barbican (甕城), arrow tower (箭樓) and the barbican's gate (閘樓).\n"
		"Click the middle of the gate's outer face on the city wall, move toward the inside of the city, click again. Esc to cancel.",
		EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginCityWallTool, "City Wall",
		"Lay a city wall (城牆) on the Beijing inner city's pattern: battered brick, crenellated outer parapet (垛牆).\n"
		"Click to start, click to end each leg, click the last end to finish or the first point to close. F flips the battlements.",
		EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginUnknownTool, "Unknown",
		"Trace a footprint whose type is not known (未知): an outline with metadata, no geometry.\n"
		"Click to anchor, move, click again to place. Press T on it later to give it a type.",
		EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginMeasureTool, "Measure",
		"Measure a span, and calibrate a map image against it.\n"
		"Click, move, click. Reads out metres, paces (步), and what the span would be as a street width. Places nothing.",
		EUserInterfaceActionType::ToggleButton, FInputChord(EModifierKey::Shift | EModifierKey::Alt, EKeys::Four));

	UI_COMMAND(BeginHeightsTool, "Heights",
		"Set the eaves of several placed buildings at once.\n"
		"Click, Shift+click or box the buildings of a court; the panel lists each with its eave and a suggestion ranked on the Main Hall (正房). Apply sets them all. Places nothing.",
		EUserInterfaceActionType::ToggleButton, FInputChord());

	UI_COMMAND(BeginCourtsTool, "Courts",
		"Group placed buildings into courtyard units (院落) and file them in the Outliner.\n"
		"Select a court's buildings, keep or change the name suggested from the map tile, Assign. Places nothing.",
		EUserInterfaceActionType::ToggleButton, FInputChord());
}

#undef LOCTEXT_NAMESPACE
