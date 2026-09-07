/*
 *  brew-monitor Hardware
 *  ESP32-C6 Super Mini - WiFiManager config portal + NVS settings
 *
 *  Behavior:
 *  - If no valid config is stored, the WiFiManager portal opens directly on boot.
 *  - If config exists, a short LED capture window runs after boot/wake; pressing
 *    the BOOT button (GPIO9) during that window opens the portal.
 *  - While the portal is open the LED flashes; a save persists everything to NVS
 *    and the device restarts into the normal measure/report/sleep cycle.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <Wire.h>
#include "esp_sleep.h"
#include "config.h"

// ---------------------------------------------------------------------------
// Settings (persisted in NVS via Preferences)
// ---------------------------------------------------------------------------
struct SystemSettings {
    char ssid[32];
    char password[64];
    char server[64];
    int  port;
    char sensor_id[32];
    char sensor_secret[64];
};

SystemSettings settings;
bool settingsLoaded = false;

// ---------------------------------------------------------------------------
// WiFiManager state
// ---------------------------------------------------------------------------
WiFiManager wm;
bool configSaved = false;

// Custom portal parameters (globals: read by the save callback later)
WiFiManagerParameter wmServerParam("server", "Server hostname", "", 64);
WiFiManagerParameter wmPortParam("port", "Server port", "", 6);
WiFiManagerParameter wmSensorIdParam("sensor_id", "Sensor ID", "", 32);
WiFiManagerParameter wmSensorSecretParam("sensor_secret", "Sensor secret", "", 64);

// Simple GPIO LED helper (LED_ACTIVE_HIGH / LED_ACTIVE_LOW via config.h)
void setLed(bool on) {
#if LED_ACTIVE_HIGH
    digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
#else
    digitalWrite(STATUS_LED_PIN, on ? LOW : HIGH);
#endif
}

// ---------------------------------------------------------------------------
// NVS persistence (Preferences)
// ---------------------------------------------------------------------------
bool loadSettings() {
    Preferences prefs;
    if (!prefs.begin(PreferencesNamespace, true)) {
        Serial.println("Error: cannot open Preferences (read)");
        return false;
    }

    memset(&settings, 0, sizeof(settings));

    String ssid         = prefs.getString("ssid", "");
    String password     = prefs.getString("password", "");
    String server       = prefs.getString("server", "");
    int      port       = prefs.getInt("port", 0);
    String sensor_id    = prefs.getString("sensor_id", "");
    String sensor_secret = prefs.getString("sensor_secret", "");

    if (ssid.length() > 0 && server.length() > 0 && port > 0 &&
        sensor_id.length() > 0 && sensor_secret.length() > 0) {
        strncpy(settings.ssid, ssid.c_str(), sizeof(settings.ssid) - 1);
        strncpy(settings.password, password.c_str(), sizeof(settings.password) - 1);
        strncpy(settings.server, server.c_str(), sizeof(settings.server) - 1);
        settings.port = port;
        strncpy(settings.sensor_id, sensor_id.c_str(), sizeof(settings.sensor_id) - 1);
        strncpy(settings.sensor_secret, sensor_secret.c_str(), sizeof(settings.sensor_secret) - 1);
        settingsLoaded = true;
        Serial.println("Settings loaded from NVS");
    } else {
        Serial.println("No complete config found in NVS");
    }

    prefs.end();
    return settingsLoaded;
}

bool saveSettings() {
    Preferences prefs;
    if (!prefs.begin(PreferencesNamespace, false)) {
        Serial.println("Error: could not open Preferences (write)");
        return false;
    }

    prefs.putString("ssid", settings.ssid);
    prefs.putString("password", settings.password);
    prefs.putString("server", settings.server);
    prefs.putInt("port", settings.port);
    prefs.putString("sensor_id", settings.sensor_id);
    prefs.putString("sensor_secret", settings.sensor_secret);
    prefs.end();

    settingsLoaded = true;
    Serial.println("Settings saved to NVS");
    return true;
}

bool isConfigured() {
    return settingsLoaded &&
           strlen(settings.ssid) > 0 &&
           strlen(settings.password) > 0 &&
           strlen(settings.server) > 0 &&
           settings.port > 0 &&
           strlen(settings.sensor_id) > 0 &&
           strlen(settings.sensor_secret) > 0;
}

// ---------------------------------------------------------------------------
// BOOT button capture window (config already exists -> look for a press)
// LED is solid while waiting; flash acknowledgement on press.
// ---------------------------------------------------------------------------
bool captureBootButton() {
    pinMode(BOOT_PIN, INPUT_PULLUP);

    unsigned long start = millis();
    setLed(true); // LED on during the whole capture window

    while (millis() - start < (unsigned long)BOOT_PRESS_WINDOW_MS) {
        if (digitalRead(BOOT_PIN) == LOW) {
            delay(50); // debounce
            if (digitalRead(BOOT_PIN) == LOW) {
                // Acknowledge with a fast blink, then the portal loop keeps flashing
                for (int i = 0; i < 3; i++) {
                    setLed(true);  delay(60);
                    setLed(false); delay(60);
                }
                setLed(true);
                Serial.println("BOOT button pressed - opening portal");
                return true;
            }
        }
        delay(10);
    }

    setLed(false); // window closed without a press
    return false;
}

// ---------------------------------------------------------------------------
// Save callback - fires after a SUCCESSFUL WiFi connect inside the portal.
// Custom params were already committed into the WiFiManagerParameter objects
// by the portal, so read everything (WiFi + params) and persist to NVS.
// ---------------------------------------------------------------------------
void saveConfigCallback() {
    Serial.println("[portal] save confirmed - persisting settings");

    String ssid = wm.getWiFiSSID();
    String pass = wm.getWiFiPass();

    strncpy(settings.ssid, ssid.c_str(), sizeof(settings.ssid) - 1);
    strncpy(settings.password, pass.c_str(), sizeof(settings.password) - 1);
    strncpy(settings.server, wmServerParam.getValue(), sizeof(settings.server) - 1);
    settings.port = atoi(wmPortParam.getValue());
    strncpy(settings.sensor_id, wmSensorIdParam.getValue(), sizeof(settings.sensor_id) - 1);
    strncpy(settings.sensor_secret, wmSensorSecretParam.getValue(), sizeof(settings.sensor_secret) - 1);

    saveSettings();
    configSaved = true;
}

// ---------------------------------------------------------------------------
// Config portal (non-blocking; LED flashes while open).
// Returns after a save, an exit, or CONFIG_PORTAL_TIMEOUT_S.
// ---------------------------------------------------------------------------
void openConfigPortal() {
    // Prefill the custom params with the current / last known values
    wmServerParam.setValue(settings.server, sizeof(settings.server));
    wmPortParam.setValue(String((uint32_t)settings.port).c_str(), 6);
    wmSensorIdParam.setValue(settings.sensor_id, sizeof(settings.sensor_id));
    wmSensorSecretParam.setValue(settings.sensor_secret, sizeof(settings.sensor_secret));

    wm.addParameter(&wmServerParam);
    wm.addParameter(&wmPortParam);
    wm.addParameter(&wmSensorIdParam);
    wm.addParameter(&wmSensorSecretParam);

    // Non-blocking portal: startConfigPortal() returns immediately, we pump it
    wm.setConfigPortalBlocking(false);
    // Auto-close the portal once WiFi + params are saved successfully
    wm.setDisableConfigPortal(true);
    // Timeout is only cosmetically set here (ignored while clients are
    // connected); the loop below enforces the hard CONFIG_PORTAL_TIMEOUT_S cap.
    wm.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT_S);
    wm.setSaveConfigCallback(saveConfigCallback);

    String apName = String(CONFIG_AP_SSID_PREFIX) + "-" + String((uint32_t)(ESP.getEfuseMac() >> 24), HEX);

    if (settingsLoaded && strlen(settings.ssid) > 0) {
        wm.preloadWiFi(settings.ssid, settings.password);
    }

    Serial.println("Starting config portal: " + apName);
    configSaved = false;
    wm.startConfigPortal(apName.c_str(), NULL);

    unsigned long portalStart = millis();
    while (wm.getConfigPortalActive()) {
        wm.process();

        // Flash the LED while the portal is open
        setLed((millis() / 400) % 2 == 0);

        if (millis() - portalStart > (unsigned long)CONFIG_PORTAL_TIMEOUT_S * 1000UL) {
            Serial.println("[portal] timeout - closing");
            wm.stopConfigPortal();
            break;
        }
        delay(10);
    }

    setLed(false);
    Serial.println("[portal] closed");
}

// ---------------------------------------------------------------------------
// WiFi connection using stored settings (max 20 attempts x 500 ms)
// ---------------------------------------------------------------------------
void connect_to_wifi() {
    if (!settingsLoaded) {
        Serial.println("No settings - cannot connect to WiFi");
        return;
    }

    Serial.print("Connecting to WiFi: ");
    Serial.println(settings.ssid);

    WiFi.persistent(false);
    WiFi.disconnect(true);

    WiFi.begin(settings.ssid, settings.password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        attempts++;
        if (attempts > 20) {
            Serial.println("\nWiFi connect timeout");
            return;
        }
    }

    Serial.println(" connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
}

// ---------------------------------------------------------------------------
// HTTPS POST (insecure client, same as the original sketch)
// ---------------------------------------------------------------------------
void http_post_request(const char *hostname, uint16_t port, String request, String data) {
    WiFiClientSecure client;
    client.setInsecure();

    Serial.print("Connecting to ");
    Serial.print(hostname);
    Serial.print("...");

    int attempts = 0;
    while (!client.connect(hostname, port)) {
        delay(500);
        Serial.print(".");
        attempts++;
        if (attempts > 20) {
            Serial.println("\nHTTP connect timeout");
            return;
        }
    }
    Serial.println(" connected");

    client.print("POST /" + request + " HTTP/1.1\r\n" +
                 "Host: " + String(hostname) + "\r\n" +
                 "Content-Type: application/json\r\n" +
                 "Content-Length: " + String(data.length()) + "\r\n" +
                 "User-Agent: ESP32-C6\r\n" +
                 "\r\n" + data);

    client.stop();
    Serial.println("[request sent]");
}

// ---------------------------------------------------------------------------
// Filter / averaging helper (unchanged)
// ---------------------------------------------------------------------------
float filter(float data[NB_MEASURE]) {
    float out = 0;
    int count = 0;
    float tmp;

    for (int i = 0; i < NB_MEASURE; i++) {
        for (int j = i + 1; j < NB_MEASURE; j++) {
            if (data[j] < data[i]) {
                tmp = data[j];
                data[j] = data[i];
                data[i] = tmp;
            }
        }
    }

    for (int i = 0; i < NB_MEASURE; i++) {
        if ((data[i] <= (data[NB_MEASURE / 2] + G_FILTER)) &&
            (data[i] >= (data[NB_MEASURE / 2] - G_FILTER))) {
            out += data[i];
            count++;
        }
    }

    return out / count;
}

// ---------------------------------------------------------------------------
// setup: decide between config portal and normal operation
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println("=== Brew Monitor ===");

    pinMode(STATUS_LED_PIN, OUTPUT);
    setLed(false);

    Wire.begin(MPU_SDA, MPU_SCL);

    loadSettings();

    bool needConfig = !isConfigured();
    bool buttonPressed = false;

    // Only run the BOOT capture window when a config already exists
    if (!needConfig) {
        buttonPressed = captureBootButton();
    }

    if (needConfig || buttonPressed) {
        openConfigPortal();

        if (configSaved) {
            Serial.println("Config saved - restarting");
            ESP.restart();
        }
        if (!settingsLoaded) {
            // First boot, nothing saved -> sleep; portal reopens on next boot
            Serial.println("Not configured - going to deep sleep");
            esp_deep_sleep(SLEEP_TIME);
        }
        // Config existed, portal closed without changes -> continue below
    }

    setLed(false);
    Serial.println("Setup complete - starting measurement cycle");
}

// ---------------------------------------------------------------------------
// loop: measure, report, sleep (runs once per boot, then deep sleep)
// ---------------------------------------------------------------------------
void loop() {
    float AcX[NB_MEASURE];
    float AcY[NB_MEASURE];
    float AcZ[NB_MEASURE];
    float X, Y, Z, Tilt, Temp = 0;

    // Wake up the MPU-6050
    delay(100);
    Wire.beginTransmission(MPU);
    Wire.write(0x6B); // PWR_MGMT_1 register
    Wire.write(0);    // wake
    Wire.endTransmission(true);

    for (int i = 0; i < NB_MEASURE; i++) {
        delay(100);
        Wire.beginTransmission(MPU);
        Wire.write(0x3B); // ACCEL_XOUT_H
        Wire.endTransmission(false);
        Wire.requestFrom((uint8_t)MPU, (size_t)8, true);

        AcX[i] = (int16_t)(Wire.read() << 8 | Wire.read()) / MPU_1G;
        AcY[i] = (int16_t)(Wire.read() << 8 | Wire.read()) / MPU_1G;
        AcZ[i] = (int16_t)(Wire.read() << 8 | Wire.read()) / MPU_1G;
        Temp += ((int16_t)(Wire.read() << 8 | Wire.read()) / 340.00) + 36.53;
    }

    // MPU-6050 sleep
    Wire.beginTransmission(MPU);
    Wire.write(0x6B);
    Wire.write(0x40);
    Wire.endTransmission(true);

    X = filter(AcX);
    Y = filter(AcY);
    Z = filter(AcZ);
    Tilt = atan2(Y, sqrt(X * X + Z * Z)) * RAD_TO_DEG;
    Temp = Temp / NB_MEASURE;

    float battery = analogReadMilliVolts(BATTERY_PIN) * 2.0 / 1000.0;

    Serial.println("MPU-6050:");
    Serial.print("  X = "); Serial.println(X);
    Serial.print("  Y = "); Serial.println(Y);
    Serial.print("  Z = "); Serial.println(Z);
    Serial.print("  Tilt = "); Serial.println(Tilt);
    Serial.print("  Temp = "); Serial.println(Temp);
    Serial.print("  Battery = "); Serial.println(battery);

    connect_to_wifi();

    if (WiFi.status() == WL_CONNECTED) {
        http_post_request(settings.server, settings.port,
            "storage/sensor/add_data",
            String("{\"sensor_id\": \"") + settings.sensor_id +
            String("\", \"secret\": \"") + settings.sensor_secret +
            String("\", \"angle\": \"") + Tilt +
            String("\", \"temperature\": \"") + Temp +
            String("\", \"battery\": \"") + battery + String("\"}"));
    }

    Serial.println("Deep sleep");
    esp_deep_sleep(SLEEP_TIME);
}
