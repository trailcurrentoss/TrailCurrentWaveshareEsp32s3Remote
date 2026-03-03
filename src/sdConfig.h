#pragma once
#include <ESP_IOExpander_Library.h>
#include <Preferences.h>

namespace sdConfig
{
    bool readConfigFromSD(ESP_IOExpander *expander, Preferences &prefs);
}
