#ifndef BIOMETRIC_SENSOR_H
#define BIOMETRIC_SENSOR_H

#include <Adafruit_Fingerprint.h>

// =====================================================
// CONFIGURACIÓN DEL SENSOR
// =====================================================
#define RXD2 16
#define TXD2 17
#define SENSOR_BAUD 57600
#define MAX_FINGERPRINTS 127

// =====================================================
// VARIABLES GLOBALES
// =====================================================
extern HardwareSerial mySerial;
extern Adafruit_Fingerprint finger;

// =====================================================
// PROTOTIPOS DE FUNCIONES
// =====================================================
bool initFingerprintSensor();
bool enrollFingerprintWithID(uint16_t id);
bool searchFingerprintInSensor();
int getFingerprintID();
int getFingerprintConfidence();

#endif // BIOMETRIC_SENSOR_H