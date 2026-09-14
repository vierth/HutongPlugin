#include "PlaceLabelSettings.h"

#include "PlaceLabelHUDWidget.h"

UPlaceLabelSettings::UPlaceLabelSettings()
{
	// Defaults to the plain built-in readout so the plugin does something the moment it is enabled.
	HUDWidgetClass = UPlaceLabelDefaultHUDWidget::StaticClass();
}
