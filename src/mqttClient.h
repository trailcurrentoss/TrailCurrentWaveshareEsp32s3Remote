#pragma once
#include "globals.h"
#include <Preferences.h>

namespace mqttClient
{
    void loadSettings(Preferences &prefs);
    bool hasConfig();
    void connect();
    void disconnect();
    bool isConnected();
    void loop();
    void publishLightCommand(int id, int state);
}
