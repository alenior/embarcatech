#ifndef CONFIG_H
#define CONFIG_H

// WIFI
#define WIFI_SSID "GCNET-Alencar" // "Giovanna-4G" // 
#define WIFI_PASS "11223344" // "vintelitros" // 

// FIREBASE
#define FIREBASE_HOST "projeto-final-5eb42-default-rtdb.firebaseio.com"
#define FIREBASE_PORT 443

// Ex.: Legacy Database Secret (temporário/protótipo) OU idToken válido.
#define FIREBASE_AUTH_TOKEN "RIyKQ3tPMDn01Izrrbmo4MyUsmcpTc2OeRl633RX"

// DEVICE / FW
#define DEVICE_ID "bitdoglab-01"
#define FW_VERSION "1.0.0"

// FIREBASE PATHS
#define FB_ROOT "devices/" DEVICE_ID
#define FB_EVENTS_PATH "alarm"
#define FB_STATUS_PATH "alarm_status/current"
#define FB_CONTROL_PATH "devices/" DEVICE_ID "/control"

// PINOS
#define PIR_PIN 8
#define AUDIO_PIN 28

#define BTN_A 5
#define BTN_B 6

#define JOY_X 26
#define JOY_Y 27

#define LED_R 13
#define LED_G 11
#define LED_B 12
#define MATRIX_PIN 7
#define BUZZER_PIN 21
#define BUZZER_PIN2 10

// OLED
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define OLED_ADDR 0x3C

// AUDIO TUNING
#define AUDIO_TRIGGER_MIN 80
#define AUDIO_NOISE_MULTIPLIER 2
#define AUDIO_NOISE_MARGIN 11
#define AUDIO_TRIGGER_CONSECUTIVE 2
#define AUDIO_COOLDOWN_US 1200000
#define AUDIO_SAMPLE_INTERVAL_US 5000
#define AUDIO_BASELINE_SAMPLES 64
#define AUDIO_ARM_GRACE_US 2000000
#define AUDIO_DEBUG_LOG 1
#define AUDIO_DEBUG_LOG_INTERVAL_US 500000

// SENHA
#define PASSWORD_LENGTH 4

#endif