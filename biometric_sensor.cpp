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

  // Reset robusto del UART
  mySerial.end();
  delay(1000);

  pinMode(RXD2, INPUT);
  pinMode(TXD2, OUTPUT);
  digitalWrite(TXD2, HIGH);
  delay(2000);

  mySerial.begin(SENSOR_BAUD, SERIAL_8N1, RXD2, TXD2);
  delay(3000);

  while (mySerial.available()) {
    mySerial.read();
  }

  finger.begin(SENSOR_BAUD);
  delay(3000);

  // Intentar 8 veces
  for (int i = 0; i < 8; i++) {
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
// REGISTRAR HUELLA CON ID (CON REINTENTOS)
// =====================================================
bool enrollFingerprintWithID(uint16_t id) {
  Serial.print("\n[REGISTRAR HUELLA] ID en sensor: ");
  Serial.println(id);
  
  int intentos = 0;
  int max_intentos = 3;

  while (intentos < max_intentos) {
    intentos++;
    
    Serial.print("\n→ Intento ");
    Serial.print(intentos);
    Serial.print("/");
    Serial.println(max_intentos);
    
    int p = -1;

    Serial.println("→ Coloca el dedo...");
    unsigned long start = millis();
    
    while (p != FINGERPRINT_OK && millis() - start < 8000) {
      p = finger.getImage();
      delay(100);
    }

    if (p != FINGERPRINT_OK) {
      Serial.println("✗ No se detectó dedo");
      
      if (intentos < max_intentos) {
        Serial.println("→ Esperando 3 segundos para reintentar...\n");
        delay(3000);
      }
      continue;
    }

    if (finger.image2Tz(1) != FINGERPRINT_OK) {
      Serial.println("✗ Error en primera captura");
      
      if (intentos < max_intentos) {
        Serial.println("→ Esperando 3 segundos para reintentar...\n");
        delay(3000);
      }
      continue;
    }

    Serial.println("→ Retira el dedo...");
    delay(2000);

    while (finger.getImage() != FINGERPRINT_NOFINGER) {
      delay(100);
    }

    delay(2000);

    Serial.println("→ Coloca el mismo dedo nuevamente...");
    p = -1;
    start = millis();

    while (p != FINGERPRINT_OK && millis() - start < 8000) {
      p = finger.getImage();
      delay(100);
    }

    if (p != FINGERPRINT_OK) {
      Serial.println("✗ No se detectó dedo en segundo intento");
      
      if (intentos < max_intentos) {
        Serial.println("→ Esperando 3 segundos para reintentar...\n");
        delay(3000);
      }
      continue;
    }

    if (finger.image2Tz(2) != FINGERPRINT_OK) {
      Serial.println("✗ Error en segunda captura");
      
      if (intentos < max_intentos) {
        Serial.println("→ Esperando 3 segundos para reintentar...\n");
        delay(3000);
      }
      continue;
    }

    Serial.println("→ Creando modelo...");
    if (finger.createModel() != FINGERPRINT_OK) {
      Serial.println("✗ Error creando modelo");
      
      if (intentos < max_intentos) {
        Serial.println("→ Esperando 3 segundos para reintentar...\n");
        delay(3000);
      }
      continue;
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
      
      if (intentos < max_intentos) {
        Serial.println("→ Esperando 3 segundos para reintentar...\n");
        delay(3000);
      }
      continue;
    }
  }

  Serial.println("\n✗ Fallo después de 3 intentos");
  return false;
}

// =====================================================
// BUSCAR HUELLA EN SENSOR (VERIFICACIÓN 1:1)
// =====================================================
bool searchFingerprintInSensorWithID(uint16_t sensor_id) {
  Serial.print("\n[BUSCAR HUELLA - ID: ");
  Serial.print(sensor_id);
  Serial.println("]");
  
  int intentos = 0;
  int max_intentos = 3;

  while (intentos < max_intentos) {
    intentos++;
    
    Serial.print("\n→ Intento ");
    Serial.print(intentos);
    Serial.print("/");
    Serial.println(max_intentos);
    
    Serial.println("→ Coloca el dedo en el sensor...");

    int p = FINGERPRINT_NOFINGER;
    unsigned long start = millis();

    while (millis() - start < 10000) {
      p = finger.getImage();

      if (p == FINGERPRINT_OK) {
        break;
      }

      delay(100);
    }

    if (p != FINGERPRINT_OK) {
      Serial.println("✗ No se detectó dedo");
      
      if (intentos < max_intentos) {
        Serial.println("→ Esperando 2 segundos para reintentar...\n");
        delay(2000);
      }
      continue;
    }

    Serial.println("→ Procesando huella...");
    if (finger.image2Tz() != FINGERPRINT_OK) {
      Serial.println("✗ Error procesando huella");
      
      if (intentos < max_intentos) {
        Serial.println("→ Esperando 2 segundos para reintentar...\n");
        delay(2000);
      }
      continue;
    }

    Serial.println("→ Comparando con huella almacenada...");
    
    // Hacer búsqueda directa sin cargar modelo primero
    int p_search = finger.fingerFastSearch();

    if (p_search == FINGERPRINT_OK) {
      int found_id = finger.fingerID;
      
      if (found_id == sensor_id) {
        // Match exitoso con el ID correcto
        lastFingerprintID = sensor_id;
        lastFingerprintConfidence = finger.confidence;

        Serial.print("✓ ¡Huella coincide!");
        Serial.print(" | Confianza: ");
        Serial.println(lastFingerprintConfidence);

        return true;
      } else {
        Serial.print("✗ Huella encontrada pero ID no coincide. Encontrado: ");
        Serial.print(found_id);
        Serial.print(" | Esperado: ");
        Serial.println(sensor_id);
        
        if (intentos < max_intentos) {
          Serial.println("→ Esperando 2 segundos para reintentar...\n");
          delay(2000);
        }
        continue;
      }
    } else {
      Serial.println("✗ Huella no encontrada en este intento");
      
      if (intentos < max_intentos) {
        Serial.println("→ Esperando 2 segundos para reintentar...\n");
        delay(2000);
      }
      continue;
    }
  }

  Serial.println("\n✗ Huella no encontrada después de 3 intentos");
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

// =====================================================
// BORRAR BASE DE DATOS DEL SENSOR
// =====================================================
bool clearDatabase() {
  int attempts = 0;
  uint8_t result;

  while (attempts < 5) {
    result = finger.emptyDatabase();

    if (result == FINGERPRINT_OK) {
      Serial.println("✓ Base de datos eliminada correctamente.");
      delay(1200);
      return true;
    }

    // Solo mostrar error si es diferente de 1 (que es normal en primeros intentos)
    if (result != 1) {
      Serial.print("Error eliminando base. Código: ");
      Serial.println(result);
    }

    attempts++;
    delay(1800);
  }

  Serial.println("✗ No se pudo eliminar la base.");
  return false;
}