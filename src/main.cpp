#include <Arduino.h>
#include "globals.h"
#include <lvgl.h>
#include <ESP_Panel_Library.h>
#include <ESP_IOExpander_Library.h>
#include "ui/ui.h"
#include "ui/vars.h"
#include "wifiHelper.h"
#include "mqttClient.h"
#include "sdConfig.h"
#include <Preferences.h>
Preferences preferences; // NVS storage for user settings
extern void setup_wifi_keyboard();
extern void setup_server_config_keyboard();
extern void prepare_server_config_screen();
extern void setup_light_buttons();
static ESP_IOExpander *expander = NULL;
unsigned long int previousuStatusCheckMillis = 0;
long uStatusCheckInterval = 33;

// Extend IO Pin define
#define TP_RST 1
#define LCD_BL 2
#define LCD_RST 3
#define SD_CS 4
#define USB_SEL 5

// I2C Pin define
#define I2C_MASTER_NUM 0
#define I2C_MASTER_SDA_IO 8
#define I2C_MASTER_SCL_IO 9

/* LVGL porting configurations */
#define LVGL_TICK_PERIOD_MS (2)
#define LVGL_TASK_MAX_DELAY_MS (500)
#define LVGL_TASK_MIN_DELAY_MS (1)
#define LVGL_TASK_STACK_SIZE (4 * 1024)
#define LVGL_TASK_PRIORITY (2)
#define LVGL_BUF_SIZE (ESP_PANEL_LCD_H_RES * 50)

ESP_Panel *panel = NULL;
SemaphoreHandle_t lvgl_mux = NULL; // LVGL mutex

#if ESP_PANEL_LCD_BUS_TYPE == ESP_PANEL_BUS_TYPE_RGB
/* Display flushing */
void lvgl_port_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    panel->getLcd()->drawBitmap(area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_p);
    lv_disp_flush_ready(disp);
}
#else
/* Display flushing */
void lvgl_port_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    panel->getLcd()->drawBitmap(area->x1, area->y1, area->x2 + 1, area->y2 + 1, color_p);
}

bool notify_lvgl_flush_ready(void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}
#endif /* ESP_PANEL_LCD_BUS_TYPE */

#if ESP_PANEL_USE_LCD_TOUCH
/* Read the touchpad */
void lvgl_port_tp_read(lv_indev_drv_t *indev, lv_indev_data_t *data)
{
    panel->getLcdTouch()->readData();

    bool touched = panel->getLcdTouch()->getTouchState();
    if (!touched)
    {
        data->state = LV_INDEV_STATE_REL;
    }
    else
    {
        TouchPoint point = panel->getLcdTouch()->getPoint();

        data->state = LV_INDEV_STATE_PR;
        /*Set the coordinates*/
        data->point.x = point.x;
        data->point.y = point.y;
    }
}
#endif

void lvgl_port_lock(int timeout_ms)
{
    const TickType_t timeout_ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    xSemaphoreTakeRecursive(lvgl_mux, timeout_ticks);
}

void lvgl_port_unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_mux);
}

void lvgl_port_task(void *arg)
{
    debugln("Starting LVGL task");

    uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    while (1)
    {
        // Lock the mutex due to the LVGL APIs are not thread-safe
        lvgl_port_lock(-1);
        task_delay_ms = lv_timer_handler();
        // Release the mutex
        lvgl_port_unlock();
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS)
        {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        }
        else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS)
        {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

void setup()
{
    Serial.begin(115200);  /* USB CDC debug output */
    Serial0.begin(115200); /* UART0 debug output */

    String LVGL_Arduino = "Hello LVGL! ";
    LVGL_Arduino += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

    debugln(LVGL_Arduino);
    debugln("I am ESP32_Display_Panel");

    panel = new ESP_Panel();

    /* Initialize LVGL core */
    lv_init();

    /* Initialize LVGL buffers */
    static lv_disp_draw_buf_t draw_buf;
    /* Using double buffers is more faster than single buffer */
    /* Use PSRAM for display buffers to leave internal SRAM available for WiFi */
    uint8_t *buf = (uint8_t *)heap_caps_calloc(1, LVGL_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    uint8_t *buf2 = (uint8_t *)heap_caps_calloc(1, LVGL_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    assert(buf);
    lv_disp_draw_buf_init(&draw_buf, buf, buf2, LVGL_BUF_SIZE);

    /* Initialize the display device */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    /* Change the following line to your display resolution */
    disp_drv.hor_res = ESP_PANEL_LCD_H_RES;
    disp_drv.ver_res = ESP_PANEL_LCD_V_RES;
    disp_drv.flush_cb = lvgl_port_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

#if ESP_PANEL_USE_LCD_TOUCH
    /* Initialize the input device */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_port_tp_read;
    lv_indev_drv_register(&indev_drv);
#endif
    /* Initialize bus and device of panel */
    panel->init();
#if ESP_PANEL_LCD_BUS_TYPE != ESP_PANEL_BUS_TYPE_RGB
    /* Register a function to notify LVGL when the panel is ready to flush */
    /* This is useful for refreshing the screen using DMA transfers */
    panel->getLcd()->setCallback(notify_lvgl_flush_ready, &disp_drv);
#endif

    debugln("Initialize IO expander");
    /* Initialize IO expander */
    expander = new ESP_IOExpander_CH422G(I2C_MASTER_NUM, ESP_IO_EXPANDER_I2C_CH422G_ADDRESS_000);
    expander->init();
    expander->begin();
    expander->multiPinMode(TP_RST | LCD_BL | LCD_RST | SD_CS | USB_SEL, OUTPUT);
    expander->multiDigitalWrite(TP_RST | LCD_BL | LCD_RST | SD_CS, HIGH);

    expander->digitalWrite(USB_SEL, LOW);
    /* Add into panel */
    panel->addIOExpander(expander);

    /* Start panel */
    panel->begin();

    /* Create a task to run the LVGL task periodically */
    lvgl_mux = xSemaphoreCreateRecursiveMutex();
    xTaskCreate(lvgl_port_task, "lvgl", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL);

    /* Lock the mutex due to the LVGL APIs are not thread-safe */
    lvgl_port_lock(-1);

    ui_init();
    setup_light_buttons();

    /* Release the mutex */
    lvgl_port_unlock();

    // Initialize NVS preferences
    preferences.begin("user_settings", false);

    // Try reading config from SD card (writes values to NVS if found)
    sdConfig::readConfigFromSD(expander, preferences);

    // Initialize WiFi (picks up any fresh values from SD card)
    wifiHelper::initialize(preferences);

    // Load MQTT settings from NVS (picks up any fresh values from SD card)
    mqttClient::loadSettings(preferences);

    // Load saved UI settings from NVS
    int savedTheme = preferences.getInt("selectedTheme", 0);
    int screenTimeout = preferences.getInt("screenTimeout", 0);
    bool keepScreenOnWhileDriving = preferences.getBool("onWhileDriving", true);
    String savedTimezone = preferences.getString("timeZone", "ASKT9AKDT,M3.2.0/2:00:00,M11.1.0/2:00:00");
    set_var_selected_theme(savedTheme);
    set_var_screen_timeout_value(screenTimeout);
    set_var_keep_screen_on_while_driving(keepScreenOnWhileDriving);
    const char *timezoneItemsList[7] = {"ASKT9AKDT,M3.2.0/2:00:00,M11.1.0/2:00:00", "CST6CDT,M3.2.0/2:00:00,M11.1.0/2:00:00", "MST7MDT,M3.2.0/2:00:00,M11.1.0/2:00:00", "HST11HDT,M3.2.0/2:00:00,M11.1.0/2:00:00", "PST8PDT,M3.2.0/2:00:00,M11.1.0/2:00:00", "EST5EDT,M3.2.0/2:00:00,M11.1.0/2:00:00", "MST7"};
    int selectedTimezone = 0;
    savedTimezone.trim();
    const char *savedTimezoneChar = savedTimezone.c_str();
    for (int i = 0; i < 7; i++)
    {
        if (strcmp(timezoneItemsList[i], savedTimezoneChar) == 0)
        {
            debugln("Found selected timezone");
            debugf("[Settings] Timezone: %s\n", timezoneItemsList[i]);
            selectedTimezone = i;
        }
    }
    lv_dropdown_set_selected(objects.drop_down_selected_time_zone, selectedTimezone);
    set_var_current_time_zone_string(savedTimezoneChar);
    // Set the version number label
    lv_label_set_text(objects.label_version_number, CURRENT_VERSION);

    // Determine which screen to show on boot
    if (!wifiHelper::hasCredentials() && !mqttClient::hasConfig())
    {
        // No config at all - show "insert SD card" message
        debugln("[Boot] No configuration found, showing SD card prompt");
        lv_scr_load(objects.wifi_setup);
        lv_obj_clear_flag(objects.wifi_setup, LV_OBJ_FLAG_SCROLLABLE);
        // Hide all interactive widgets - just show a message
        lv_obj_add_flag(objects.roller_wifi_networks, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.textarea_wifi_ssid, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.label_wifi_ssid, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.textarea_wifi_password, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.label_wifi_password, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.btn_wifi_connect, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.btn_wifi_skip, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(objects.btn_wifi_scan, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(objects.label_wifi_setup_title, "Configuration Required");
        set_var_connection_status_text("Insert SD card with config.env and ca.crt then restart");
    }
    else if (wifiHelper::isConnected() && mqttClient::hasConfig())
    {
        // Everything configured and WiFi connected - connect MQTT
        debugln("[Boot] Fully configured, connecting MQTT");
        mqttClient::connect();
    }
    else
    {
        debugln("[Boot] Partial config - waiting for WiFi or reconnect");
    }

    debugln("Setup done");
}

void loop()
{
    // Process queued MQTT messages (with LVGL mutex for thread safety)
    mqttClient::loop();

    // If settings have changed we need to persist them
    if (get_var_user_settings_changed())
    {
        int selectedTheme = get_var_selected_theme();
        int screenTimeout = get_var_screen_timeout_value();
        bool keepScreenOnWhileDriving = get_var_keep_screen_on_while_driving();
        preferences.putInt("selectedTheme", selectedTheme);
        preferences.putInt("screenTimeout", screenTimeout);
        preferences.putBool("onWhileDriving", keepScreenOnWhileDriving);
        preferences.putString("timeZone", get_var_current_time_zone_string());
        set_var_user_settings_changed(false);
    }

    // Check WiFi reconnection
    wifiHelper::checkReconnect();

    // If WiFi just connected and MQTT hasn't been started yet, connect
    if (wifiHelper::isConnected() && !mqttClient::isConnected() && mqttClient::hasConfig())
    {
        static unsigned long lastMqttConnectAttempt = 0;
        unsigned long now = millis();
        if (now - lastMqttConnectAttempt >= 5000)
        {
            lastMqttConnectAttempt = now;
            debugln("[Loop] WiFi connected, starting MQTT...");
            mqttClient::connect();
        }
    }

    unsigned long currentStatusCheckMillis = millis();
    if (currentStatusCheckMillis - previousuStatusCheckMillis >= uStatusCheckInterval)
    {
        lvgl_port_lock(-1);
        if ((get_var_pdm01_device01_status() > 0) || (get_var_pdm01_device02_status() > 0) || (get_var_pdm01_device03_status() > 0) || (get_var_pdm01_device04_status() > 0) || (get_var_pdm01_device05_status() > 0) || (get_var_pdm01_device08_status() > 0))
        {
            lv_obj_add_state(objects.label_warning_icon_lights, LV_STATE_CHECKED);
            lv_obj_add_state(objects.label_warning_text_lights, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(objects.label_warning_icon_lights, LV_STATE_CHECKED);
            lv_obj_clear_state(objects.label_warning_text_lights, LV_STATE_CHECKED);
        }
        if (get_var_pdm01_device07_status() > 0)
        {
            lv_obj_add_state(objects.label_warning_icon_water, LV_STATE_CHECKED);
            lv_obj_add_state(objects.label_warning_text_water, LV_STATE_CHECKED);
        }
        else
        {
            lv_obj_clear_state(objects.label_warning_icon_water, LV_STATE_CHECKED);
            lv_obj_clear_state(objects.label_warning_text_water, LV_STATE_CHECKED);
        }
        lvgl_port_unlock();
        previousuStatusCheckMillis = currentStatusCheckMillis;
    }
    delay(5);
}
