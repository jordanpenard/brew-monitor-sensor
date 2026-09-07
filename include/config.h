
// Status LED (blue LED on GPIO15 for the ESP32-C6 Super Mini; use GPIO_NUM_13
// if your board differs). The BOOT button pin is predefined by the core
// (GPIO9 on ESP32-C6).
#define STATUS_LED_PIN GPIO_NUM_15
#define LED_ACTIVE_HIGH 1  // 1 = high turns the LED on, 0 = high turns it off

// Capture window after every boot/wake during which the LED is on and the BOOT
// button can be pressed to open the config portal (ms)
#define BOOT_PRESS_WINDOW_MS 3000

// Config portal settings
#define CONFIG_PORTAL_TIMEOUT_S 300
#define CONFIG_AP_SSID_PREFIX "BrewMonitor"

// Storage namespace for Preferences
#define PreferencesNamespace "brewmon"

// Accelerometer (I2C pins - adjust to match your wiring)
#define MPU 0x68
#define MPU_SCL GPIO_NUM_20
#define MPU_SDA GPIO_NUM_19
#define MPU_1G 16384.0
#define G_FILTER 0.01

// Battery level: midpoint of the 100k voltage divider.
#define BATTERY_PIN GPIO_NUM_4

// Number of measurement taken from the sensor for averaging
#define NB_MEASURE 10

// Sleep time : 10min
#define SLEEP_TIME 60000000*10
