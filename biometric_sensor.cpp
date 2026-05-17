#include "biometric_sensor.h"

// =====================================================
// INSTANCIAS GLOBALES
// =====================================================
HardwareSerial mySerial(2);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&mySerial);

int lastFingerprintID = -1;
int lastFingerprintConfidence = -1;

// =====================================================
// INICIALIZAR SENSOR
// =====================================================
bool initFingerprintSensor() {
  Serial.print("Inicializando sensor en baudrate: ");
  Serial.println(SENSOR_BAUD);

  mySerial.begin(SENSOR_BAUD, SERIAL_8N1, RXD2, TXD2);
  delay(2000);

  finger.begin(SENSOR_BAUD);
  delay(2000);

  for (int i = 0; i < 5; i++) {
    if (finger.verifyPassword()) {
      Serial.println("✓ Sensor detectado correctamente.");
      finger.getParameters();
      Serial.print("Capacidad: ");
      Serial.println(finger.capacity);
      return true;
    }
    delay(1000);
  }

  Serial.println("✗ Error detectando sensor.");
  return false;
}

// =====================================================
// REGISTRAR HUELLA CON ID
// =====================================================
bool enrollFingerprintWithID(uint16_t id) {
  Serial.print("\n[REGISTRAR HUELLA] ID en sensor: ");
  Serial.println(id);
  
  int p = -1;

  Serial.println("→ Coloca el dedo...");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    delay(50);
  }

  Serial.println("→ Procesando primera captura...");
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    Serial.println("✗ Error en primera captura");
    return false;
  }

  Serial.println("→ Retira el dedo...");
  delay(2000);

  while (finger.getImage() != FINGERPRINT_NOFINGER) {
    delay(100);
  }

  Serial.println("→ Coloca el mismo dedo nuevamente...");
  p = -1;

  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    delay(50);
  }

  Serial.println("→ Procesando segunda captura...");
  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    Serial.println("✗ Error en segunda captura");
    return false;
  }

  Serial.println("→ Creando modelo...");
  if (finger.createModel() != FINGERPRINT_OK) {
    Serial.println("✗ Error creando modelo");
    return false;
  }

  Serial.println("→ Almacenando huella en sensor...");
  uint8_t result = finger.storeModel(id);
  
  if (result == FINGERPRINT_OK) {
    Serial.print("✓ Huella registrada exitosamente en ID: ");
    Serial.println(id);
    return true;
  } else {
    Serial.print("✗ Error almacenando huella. Código: ");
    Serial.println(result);
    return false;
  }
}

// =====================================================
// BUSCAR HUELLA EN SENSOR
// =====================================================
bool searchFingerprintInSensor() {
  Serial.print("\n[BUSCAR HUELLA]\n");
  Serial.println("→ Coloca el dedo en el sensor...");

  int p = FINGERPRINT_NOFINGER;
  unsigned long start = millis();

  while (millis() - start < 15000) {
    p = finger.getImage();

    if (p == FINGERPRINT_OK) {
      break;
    }

    delay(50);
  }

  if (p != FINGERPRINT_OK) {
    Serial.println("✗ No se detectó dedo.");
    return false;
  }

  Serial.println("→ Procesando huella...");
  if (finger.image2Tz() != FINGERPRINT_OK) {
    Serial.println("✗ Error procesando huella.");
    return false;
  }

  Serial.println("→ Buscando en el sensor...");
  p = finger.fingerFastSearch();

  if (p == FINGERPRINT_OK) {
    lastFingerprintID = finger.fingerID;
    lastFingerprintConfidence = finger.confidence;

    Serial.print("✓ Huella encontrada!");
    Serial.print(" | ID: ");
    Serial.print(lastFingerprintID);
    Serial.print(" | Confianza: ");
    Serial.println(lastFingerprintConfidence);

    return true;
  }

  Serial.println("✗ Huella no encontrada en el sensor.");
  lastFingerprintID = -1;
  lastFingerprintConfidence = -1;
  return false;
}

// =====================================================
// OBTENER ID
// =====================================================
int getFingerprintID() {
  return lastFingerprintID;
}

// =====================================================
// OBTENER CONFIANZA
// =====================================================
int getFingerprintConfidence() {
  return lastFingerprintConfidence;
}