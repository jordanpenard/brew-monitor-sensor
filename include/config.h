
// Wi-Fi
#define SSID "<SSID>"
#define PASSWORD "<Password>"

// Server details
#define SERVER "<hostname>"
#define PORT <port>

// Sensor details
#define SENSOR_ID <ID>
#define SENSOR_SECRET <secret>

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
