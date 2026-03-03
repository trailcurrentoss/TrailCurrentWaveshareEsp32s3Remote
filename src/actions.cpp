#include <Arduino.h>
#include "globals.h"
#include "ui/ui.h"
#include "ui/actions.h"
#include "ui/screens.h"
#include "ui/styles.h"
#include "ui/vars.h"
#include "wifiHelper.h"
#include "mqttClient.h"

extern Preferences preferences;

static lv_obj_t *wifi_keyboard = NULL;
static lv_obj_t *server_keyboard = NULL;
static const char *MANUAL_ENTRY_LABEL = "Enter SSID...";
void setup_wifi_keyboard();
void setup_server_config_keyboard();
void prepare_server_config_screen();

void action_settings_selection_change(lv_event_t *e)
{
    int menuSelection = (int)lv_event_get_user_data(e);
    lv_obj_add_flag(objects.container_display_settings, LV_OBJ_FLAG_HIDDEN);     // Hide by default
    lv_obj_add_flag(objects.container_connectivity_options, LV_OBJ_FLAG_HIDDEN); // Hide by default
    lv_obj_add_flag(objects.container_demo_settings, LV_OBJ_FLAG_HIDDEN);        // Hide by default
    lv_obj_add_flag(objects.container_about_settings, LV_OBJ_FLAG_HIDDEN);       // Hide by default
    lv_obj_add_flag(objects.container_date_time_settings, LV_OBJ_FLAG_HIDDEN);   // Hide by default
    if (menuSelection == 0)
    {
        lv_obj_clear_flag(objects.container_display_settings, LV_OBJ_FLAG_HIDDEN);
    }
    else if (menuSelection == 1)
    {
        lv_obj_clear_flag(objects.container_connectivity_options, LV_OBJ_FLAG_HIDDEN);
    }
    else if (menuSelection == 2)
    {
        lv_obj_clear_flag(objects.container_demo_settings, LV_OBJ_FLAG_HIDDEN);
    }
    else if (menuSelection == 3)
    {
        lv_obj_clear_flag(objects.container_about_settings, LV_OBJ_FLAG_HIDDEN);
    }
    else if (menuSelection == 4)
    {
        lv_obj_clear_flag(objects.container_date_time_settings, LV_OBJ_FLAG_HIDDEN);
    }
}

void action_change_theme(lv_event_t *e)
{
    set_var_user_settings_changed(true);
    int themeIndex = (int)lv_event_get_user_data(e);
    set_var_selected_theme(themeIndex);
}

void action_change_screen(lv_event_t *e)
{
    debugf("[NAV] Changing screen to ID: %d\n", (int)lv_event_get_user_data(e));
    int screenData = (int)lv_event_get_user_data(e);
    /* Clear all toolbar button checked states for all home buttons */
    lv_obj_clear_state(objects.home_page_bottom_nav_bar__bottom_nav_bar_button_home, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.trailer_page_bottom_nav_bar__bottom_nav_bar_button_home, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.power_page_bottom_nav_bar__bottom_nav_bar_button_home, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.solar_page_bottom_nav_bar__bottom_nav_bar_button_home, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.air_page_bottom_nav_bar__bottom_nav_bar_button_home, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.setting_page_bottom_nav_bar__bottom_nav_bar_button_home, LV_STATE_CHECKED);
    /* Clear all toolbar button checked states for all trailer buttons */
    lv_obj_clear_state(objects.home_page_bottom_nav_bar__bottom_nav_bar_button_trailer, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.trailer_page_bottom_nav_bar__bottom_nav_bar_button_trailer, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.power_page_bottom_nav_bar__bottom_nav_bar_button_trailer, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.solar_page_bottom_nav_bar__bottom_nav_bar_button_trailer, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.air_page_bottom_nav_bar__bottom_nav_bar_button_trailer, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.setting_page_bottom_nav_bar__bottom_nav_bar_button_trailer, LV_STATE_CHECKED);
    /* Clear all toolbar button checked states for all power buttons */
    lv_obj_clear_state(objects.home_page_bottom_nav_bar__botttom_nav_bar_button_power, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.trailer_page_bottom_nav_bar__botttom_nav_bar_button_power, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.power_page_bottom_nav_bar__botttom_nav_bar_button_power, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.solar_page_bottom_nav_bar__botttom_nav_bar_button_power, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.air_page_bottom_nav_bar__botttom_nav_bar_button_power, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.setting_page_bottom_nav_bar__botttom_nav_bar_button_power, LV_STATE_CHECKED);
    /* Clear all toolbar button checked states for all solar buttons */
    lv_obj_clear_state(objects.home_page_bottom_nav_bar__botttom_nav_bar_button_solar, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.trailer_page_bottom_nav_bar__botttom_nav_bar_button_solar, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.power_page_bottom_nav_bar__botttom_nav_bar_button_solar, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.solar_page_bottom_nav_bar__botttom_nav_bar_button_solar, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.air_page_bottom_nav_bar__botttom_nav_bar_button_solar, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.setting_page_bottom_nav_bar__botttom_nav_bar_button_solar, LV_STATE_CHECKED);
    /* Clear all toolbar button checked states for all settings buttons */
    lv_obj_clear_state(objects.home_page_bottom_nav_bar__botttom_nav_bar_button_settings, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.trailer_page_bottom_nav_bar__botttom_nav_bar_button_settings, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.power_page_bottom_nav_bar__botttom_nav_bar_button_settings, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.solar_page_bottom_nav_bar__botttom_nav_bar_button_settings, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.air_page_bottom_nav_bar__botttom_nav_bar_button_settings, LV_STATE_CHECKED);
    lv_obj_clear_state(objects.setting_page_bottom_nav_bar__botttom_nav_bar_button_settings, LV_STATE_CHECKED);

    if (screenData == 0)
    {
        lv_scr_load(objects.home);
        /* Set toolbar icons on home to checked state */
        lv_obj_add_state(objects.home_page_bottom_nav_bar__bottom_nav_bar_button_home, LV_STATE_CHECKED);
    }
    else if (screenData == 1)
    {
        lv_scr_load(objects.trailer);
        /* Set toolbar icons on trailer to checked state */
        lv_obj_add_state(objects.trailer_page_bottom_nav_bar__bottom_nav_bar_button_trailer, LV_STATE_CHECKED);
    }
    else if (screenData == 2)
    {
        lv_scr_load(objects.power);
        /* Set toolbar icons on power to checked state */
        lv_obj_add_state(objects.power_page_bottom_nav_bar__botttom_nav_bar_button_power, LV_STATE_CHECKED);
    }
    else if (screenData == 3)
    {
        lv_scr_load(objects.solar);
        /* Set toolbar icons on solar to checked state */
        lv_obj_add_state(objects.solar_page_bottom_nav_bar__botttom_nav_bar_button_solar, LV_STATE_CHECKED);
    }
    else if (screenData == 4)
    {
        lv_scr_load(objects.air);
        /* Set toolbar icons on air to checked state */
        lv_obj_add_state(objects.air_page_bottom_nav_bar__botttom_nav_bar_button_fridge, LV_STATE_CHECKED);
    }
    else if (screenData == 5)
    {
        lv_scr_load(objects.settings);
        /* Set toolbar icons on settings to checked state */
        lv_obj_add_state(objects.setting_page_bottom_nav_bar__botttom_nav_bar_button_settings, LV_STATE_CHECKED);
    }
    else if (screenData == 6)
    {
        lv_scr_load(objects.wifi_setup);
        lv_obj_clear_flag(objects.wifi_setup, LV_OBJ_FLAG_SCROLLABLE);
        setup_wifi_keyboard();
    }
    else if (screenData == 7)
    {
        lv_scr_load(objects.server_config);
        lv_obj_clear_flag(objects.server_config, LV_OBJ_FLAG_SCROLLABLE);
        setup_server_config_keyboard();
        prepare_server_config_screen();
    }
}

void action_timeout_changed(lv_event_t *e)
{
    set_var_user_settings_changed(true);
    int currentTimeoutValue = get_var_screen_timeout_value();
    int eventValue = (int)lv_event_get_user_data(e);
    if (eventValue == 0)
    {
        currentTimeoutValue--;
    }
    else
    {
        currentTimeoutValue++;
    }
    set_var_screen_timeout_value(currentTimeoutValue);
}

void action_keep_on_while_driving_changed(lv_event_t *e)
{
    set_var_user_settings_changed(true);
    bool keepScreenOnWhileDriving = lv_obj_has_state(objects.check_box_keep_screen_on_while_in_motion, LV_STATE_CHECKED);
    set_var_keep_screen_on_while_driving(keepScreenOnWhileDriving);
}

/* NOTE: These have to match the order of the drop down items in the UI */
const char *timezoneItems[41] = {"ASKT9AKDT,M3.2.0/2:00:00,M11.1.0/2:00:00", "CST6CDT,M3.2.0/2:00:00,M11.1.0/2:00:00", "MST7MDT,M3.2.0/2:00:00,M11.1.0/2:00:00", "HST11HDT,M3.2.0/2:00:00,M11.1.0/2:00:00", "PST8PDT,M3.2.0/2:00:00,M11.1.0/2:00:00", "EST5EDT,M3.2.0/2:00:00,M11.1.0/2:00:00", "MST7"};
void action_timezone_change(lv_event_t *e)
{
    uint8_t selectedIdex = lv_dropdown_get_selected(objects.drop_down_selected_time_zone);
    debugln(selectedIdex);
    debugln(timezoneItems[selectedIdex]);
    set_var_user_settings_changed(true);
    set_var_current_time_zone_string(timezoneItems[selectedIdex]);
}

// --- WiFi Setup Actions ---

void action_wifi_scan(lv_event_t *e)
{
    int n = wifiHelper::scanNetworks();
    if (n > 0)
    {
        // Collect unique SSIDs, keeping the strongest RSSI for each
        String ssids[wifiHelper::MAX_SCAN_RESULTS];
        int rssis[wifiHelper::MAX_SCAN_RESULTS];
        int uniqueCount = 0;

        for (int i = 0; i < n && uniqueCount < wifiHelper::MAX_SCAN_RESULTS; i++)
        {
            String ssid = wifiHelper::getScanResult(i);
            if (ssid.length() == 0) continue;
            int rssi = wifiHelper::getScanRSSI(i);

            // Check for duplicate SSID
            bool found = false;
            for (int j = 0; j < uniqueCount; j++)
            {
                if (ssids[j] == ssid)
                {
                    found = true;
                    if (rssi > rssis[j]) rssis[j] = rssi;
                    break;
                }
            }
            if (!found)
            {
                ssids[uniqueCount] = ssid;
                rssis[uniqueCount] = rssi;
                uniqueCount++;
            }
        }

        // Sort by signal strength (strongest first)
        for (int i = 0; i < uniqueCount - 1; i++)
        {
            for (int j = i + 1; j < uniqueCount; j++)
            {
                if (rssis[j] > rssis[i])
                {
                    String tmpS = ssids[i]; ssids[i] = ssids[j]; ssids[j] = tmpS;
                    int tmpR = rssis[i]; rssis[i] = rssis[j]; rssis[j] = tmpR;
                }
            }
        }

        String options = MANUAL_ENTRY_LABEL;
        for (int i = 0; i < uniqueCount; i++)
        {
            options += "\n";
            options += ssids[i];
        }
        lv_roller_set_options(objects.roller_wifi_networks, options.c_str(), LV_ROLLER_MODE_NORMAL);
        char statusBuf[32];
        snprintf(statusBuf, sizeof(statusBuf), "Found %d networks", uniqueCount);
        set_var_connection_status_text(statusBuf);
    }
    else
    {
        lv_roller_set_options(objects.roller_wifi_networks, MANUAL_ENTRY_LABEL, LV_ROLLER_MODE_NORMAL);
        char diagBuf[64];
        snprintf(diagBuf, sizeof(diagBuf), "Scan returned %d (free: %lu)", n, (unsigned long)esp_get_free_heap_size());
        set_var_connection_status_text(diagBuf);
    }
    WiFi.scanDelete();
}

void action_wifi_connect(lv_event_t *e)
{
    char selectedSSID[64];
    lv_roller_get_selected_str(objects.roller_wifi_networks, selectedSSID, sizeof(selectedSSID));

    // If "Enter SSID..." is selected, use the manually typed SSID
    const char *ssid = selectedSSID;
    if (strcmp(selectedSSID, MANUAL_ENTRY_LABEL) == 0)
    {
        ssid = lv_textarea_get_text(objects.textarea_wifi_ssid);
        if (strlen(ssid) == 0)
        {
            set_var_connection_status_text("Enter an SSID first");
            return;
        }
    }

    const char *password = lv_textarea_get_text(objects.textarea_wifi_password);
    set_var_connection_status_text("Connecting...");

    if (wifiHelper::connectToNetwork(ssid, password, preferences))
    {
        set_var_connection_status_text("Connected!");
        lv_scr_load(objects.server_config);
        lv_obj_clear_flag(objects.server_config, LV_OBJ_FLAG_SCROLLABLE);
        setup_server_config_keyboard();
        prepare_server_config_screen();
    }
    else
    {
        set_var_connection_status_text("Connection failed. Try again.");
    }
}

static void wifi_keyboard_restore_layout()
{
    lv_obj_add_flag(wifi_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(objects.roller_wifi_networks, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(objects.btn_wifi_scan, LV_OBJ_FLAG_HIDDEN);
    // Restore SSID fields to EEZ-defined positions
    lv_obj_set_pos(objects.label_wifi_ssid, 280, 80);
    lv_obj_set_pos(objects.textarea_wifi_ssid, 280, 115);
    // Restore password fields to EEZ-defined positions
    lv_obj_set_pos(objects.label_wifi_password, 50, 300);
    lv_obj_set_pos(objects.textarea_wifi_password, 180, 295);
}

static void wifi_ta_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    lv_obj_t *kb = (lv_obj_t *)lv_event_get_user_data(e);
    if (code == LV_EVENT_FOCUSED)
    {
        lv_keyboard_set_textarea(kb, ta);
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
        // Hide roller and scan to make room above keyboard
        lv_obj_add_flag(objects.roller_wifi_networks, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.btn_wifi_scan, LV_OBJ_FLAG_HIDDEN);
        // Reposition both fields above the keyboard (keyboard is 200px at bottom)
        lv_obj_set_pos(objects.label_wifi_ssid, 50, 60);
        lv_obj_set_pos(objects.textarea_wifi_ssid, 150, 55);
        lv_obj_set_pos(objects.label_wifi_password, 50, 120);
        lv_obj_set_pos(objects.textarea_wifi_password, 150, 115);
    }
    if (code == LV_EVENT_DEFOCUSED)
    {
        lv_keyboard_set_textarea(kb, NULL);
        wifi_keyboard_restore_layout();
    }
}

static void wifi_kb_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL)
    {
        wifi_keyboard_restore_layout();
    }
}

void setup_wifi_keyboard()
{
    if (wifi_keyboard != NULL) return;

    wifi_keyboard = lv_keyboard_create(objects.wifi_setup);
    lv_obj_set_size(wifi_keyboard, 800, 200);
    lv_obj_align(wifi_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    add_style_keyboard_default(wifi_keyboard);
    lv_obj_add_flag(wifi_keyboard, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(objects.textarea_wifi_ssid, wifi_ta_event_cb, LV_EVENT_ALL, wifi_keyboard);
    lv_obj_add_event_cb(objects.textarea_wifi_password, wifi_ta_event_cb, LV_EVENT_ALL, wifi_keyboard);
    lv_obj_add_event_cb(wifi_keyboard, wifi_kb_event_cb, LV_EVENT_ALL, NULL);
}

// --- Server Config Keyboard ---

static void server_keyboard_restore_layout()
{
    lv_obj_add_flag(server_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(objects.btn_save_server_config, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(objects.btn_server_config_back, LV_OBJ_FLAG_HIDDEN);
    // Restore to EEZ-defined positions
    lv_obj_set_pos(objects.label_server_url, 50, 100);
    lv_obj_set_pos(objects.textarea_server_url, 200, 90);
    lv_obj_set_pos(objects.label_api_key, 50, 180);
    lv_obj_set_pos(objects.textarea_api_key, 200, 170);
}

static void server_ta_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    lv_obj_t *kb = (lv_obj_t *)lv_event_get_user_data(e);
    if (code == LV_EVENT_FOCUSED)
    {
        lv_keyboard_set_textarea(kb, ta);
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
        // Hide buttons that would be under the keyboard
        lv_obj_add_flag(objects.btn_save_server_config, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.btn_server_config_back, LV_OBJ_FLAG_HIDDEN);
        // Move fields above keyboard (keyboard at y=280)
        lv_obj_set_pos(objects.label_server_url, 50, 60);
        lv_obj_set_pos(objects.textarea_server_url, 200, 55);
        lv_obj_set_pos(objects.label_api_key, 50, 120);
        lv_obj_set_pos(objects.textarea_api_key, 200, 115);
    }
    if (code == LV_EVENT_DEFOCUSED)
    {
        lv_keyboard_set_textarea(kb, NULL);
        server_keyboard_restore_layout();
    }
}

static void server_kb_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL)
    {
        server_keyboard_restore_layout();
    }
}

void setup_server_config_keyboard()
{
    if (server_keyboard != NULL) return;

    server_keyboard = lv_keyboard_create(objects.server_config);
    lv_obj_set_size(server_keyboard, 800, 200);
    lv_obj_align(server_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    add_style_keyboard_default(server_keyboard);
    lv_obj_add_flag(server_keyboard, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(objects.textarea_server_url, server_ta_event_cb, LV_EVENT_ALL, server_keyboard);
    lv_obj_add_event_cb(objects.textarea_api_key, server_ta_event_cb, LV_EVENT_ALL, server_keyboard);
    lv_obj_add_event_cb(server_keyboard, server_kb_event_cb, LV_EVENT_ALL, NULL);
}

void prepare_server_config_screen()
{
    // Repurpose server config screen for MQTT host/password entry
    lv_label_set_text(objects.label_server_url, "MQTT Host:");
    lv_textarea_set_placeholder_text(objects.textarea_server_url, "trailcurrent01.local");
    lv_label_set_text(objects.label_api_key, "MQTT Pass:");
    lv_textarea_set_placeholder_text(objects.textarea_api_key, "password");

    // Pre-fill with stored MQTT host
    String mqttHost = preferences.getString("mqttHost", "");
    if (mqttHost.length() > 0)
    {
        lv_textarea_set_text(objects.textarea_server_url, mqttHost.c_str());
    }

    // Clear password textarea - never pre-fill for security
    lv_textarea_set_text(objects.textarea_api_key, "");
}

// --- MQTT Config Actions ---

void action_save_server_url(lv_event_t *e)
{
    const char *mqttHost = lv_textarea_get_text(objects.textarea_server_url);
    preferences.putString("mqttHost", mqttHost);
    set_var_server_url(mqttHost);
    set_var_user_settings_changed(true);
    debugf("[Settings] MQTT host saved: %s\n", mqttHost);
}

void action_save_api_key(lv_event_t *e)
{
    const char *mqttPass = lv_textarea_get_text(objects.textarea_api_key);
    preferences.putString("mqttPass", mqttPass);
    set_var_api_key_configured(strlen(mqttPass) > 0);
    set_var_user_settings_changed(true);
    debugln("[Settings] MQTT password saved");

    // Reconnect MQTT with new credentials
    if (wifiHelper::isConnected() && mqttClient::hasConfig())
    {
        mqttClient::disconnect();
        mqttClient::loadSettings(preferences);
        mqttClient::connect();
    }
}

void action_save_server_config(lv_event_t *e)
{
    const char *mqttHost = lv_textarea_get_text(objects.textarea_server_url);
    const char *mqttPass = lv_textarea_get_text(objects.textarea_api_key);

    preferences.putString("mqttHost", mqttHost);
    preferences.putString("mqttPass", mqttPass);
    set_var_server_url(mqttHost);
    set_var_api_key_configured(strlen(mqttPass) > 0);
    set_var_user_settings_changed(true);

    // Reload settings and connect MQTT if WiFi is available
    mqttClient::loadSettings(preferences);
    if (wifiHelper::isConnected() && mqttClient::hasConfig())
    {
        mqttClient::connect();
    }

    // Navigate to home screen
    lv_scr_load(objects.home);
    lv_obj_add_state(objects.home_page_bottom_nav_bar__bottom_nav_bar_button_home, LV_STATE_CHECKED);
}

// --- Light Control Actions ---
// ESP-IDF MQTT publish is thread-safe (queues to internal outbox),
// safe to call directly from the LVGL task.

void action_toggle_light(lv_event_t *e)
{
    int lightId = (int)lv_event_get_user_data(e);
    debugf("[Light] Toggling light ID: %d\n", lightId);

    int currentState = 0;
    switch (lightId)
    {
    case 1:
        currentState = get_var_pdm01_device01_status();
        break;
    case 2:
        currentState = get_var_pdm01_device02_status();
        break;
    case 3:
        currentState = get_var_pdm01_device03_status();
        break;
    case 4:
        currentState = get_var_pdm01_device04_status();
        break;
    case 5:
        currentState = get_var_pdm01_device05_status();
        break;
    case 6:
        currentState = get_var_pdm01_device06_status();
        break;
    case 7:
        currentState = get_var_pdm01_device07_status();
        break;
    case 8:
        currentState = get_var_pdm01_device08_status();
        break;
    }

    int newState = (currentState > 0) ? 0 : 1;
    mqttClient::publishLightCommand(lightId, newState);
}

void action_navigate_to_wifi_setup(lv_event_t *e)
{
    lv_scr_load(objects.wifi_setup);
    lv_obj_clear_flag(objects.wifi_setup, LV_OBJ_FLAG_SCROLLABLE);
    setup_wifi_keyboard();
}

// --- Wire light buttons (EEZ project has the action but didn't connect buttons) ---

void setup_light_buttons()
{
    lv_obj_add_event_cb(objects.btn_device01, action_toggle_light, LV_EVENT_CLICKED, (void *)1);
    lv_obj_add_event_cb(objects.btn_device02, action_toggle_light, LV_EVENT_CLICKED, (void *)2);
    lv_obj_add_event_cb(objects.btn_device03, action_toggle_light, LV_EVENT_CLICKED, (void *)3);
    lv_obj_add_event_cb(objects.btn_device04, action_toggle_light, LV_EVENT_CLICKED, (void *)4);
    lv_obj_add_event_cb(objects.btn_device05, action_toggle_light, LV_EVENT_CLICKED, (void *)5);
    lv_obj_add_event_cb(objects.btn_device06, action_toggle_light, LV_EVENT_CLICKED, (void *)6);
    lv_obj_add_event_cb(objects.btn_device07, action_toggle_light, LV_EVENT_CLICKED, (void *)7);
    lv_obj_add_event_cb(objects.btn_device08, action_toggle_light, LV_EVENT_CLICKED, (void *)8);
}
