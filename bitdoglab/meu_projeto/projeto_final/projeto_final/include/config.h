#ifndef CONFIG_H
#define CONFIG_H

// WIFI
#define WIFI_SSID "GCNET-Alencar"
#define WIFI_PASS "11223344"

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

// OLED
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define OLED_ADDR 0x3C

// SENHA
#define PASSWORD_LENGTH 4

#endif