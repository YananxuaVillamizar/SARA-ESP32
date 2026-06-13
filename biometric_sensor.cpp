#include "biometric_sensor.h"
#include "display.h"

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
  logPrint("Inicializando sensor en baudrate: ");
  logPrintln(SENSOR_BAUD);

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
      logPrintln("✓ Sensor detectado correctamente.");
      finger.getParameters();
      logPrint("Capacidad: ");
      logPrintln(finger.capacity);
      return true;
    }
    delay(1000);
  }

  logPrintln("✗ Error detectando sensor.");
  return false;
}

// =====================================================
// REGISTRAR HUELLA CON ID (CON REINTENTOS)
// =====================================================
bool enrollFingerprintWithID(uint16_t id) {
  logPrint("\n[REGISTRAR HUELLA] ID en sensor: ");
  logPrintln(id);
  
  int intentos = 0;
  int max_intentos = 3;

  while (intentos < max_intentos) {
    intentos++;
    
    logPrint("\n→ Intento ");
    logPrint(intentos);
    logPrint("/");
    logPrintln(max_intentos);
    
    int p = -1;

    logPrintln("→ Coloca el dedo...");
    unsigned long start = millis();
    
    while (p != FINGERPRINT_OK && millis() - start < 8000) {
      p = finger.getImage();
      delay(100);
    }

    if (p != FINGERPRINT_OK) {
      logPrintln("✗ No se detectó dedo");
      
      if (intentos < max_intentos) {
        logPrintln("→ Esperando 3 segundos para reintentar...\n");
        delay(3000);
      }
      continue;
    }

    if (finger.image2Tz(1) != FINGERPRINT_OK) {
      logPrintln("✗ Error en primera captura");
      
      if (intentos < max_intentos) {
        logPrintln("→ Esperando 3 segundos para reintentar...\n");
        delay(3000);
      }
      continue;
    }

    logPrintln("→ Retira el dedo...");
    delay(2000);

    while (finger.getImage() != FINGERPRINT_NOFINGER) {
      delay(100);
    }

    delay(2000);

    logPrintln("→ Coloca el mismo dedo nuevamente...");
    p = -1;
    start = millis();

    while (p != FINGERPRINT_OK && millis() - start < 8000) {
      p = finger.getImage();
      delay(100);
    }

    if (p != FINGERPRINT_OK) {
      logPrintln("✗ No se detectó dedo en segundo intento");
      
      if (intentos < max_intentos) {
        logPrintln("→ Esperando 3 segundos para reintentar...\n");
        delay(3000);
      }
      continue;
    }

    if (finger.image2Tz(2) != FINGERPRINT_OK) {
      logPrintln("✗ Error en segunda captura");
      
      if (intentos < max_intentos) {
        logPrintln("→ Esperando 3 segundos para reintentar...\n");
        delay(3000);
      }
      continue;
    }

    logPrintln("→ Creando modelo...");
    if (finger.createModel() != FINGERPRINT_OK) {
      logPrintln("✗ Error creando modelo");
      
      if (intentos < max_intentos) {
        logPrintln("→ Esperando 3 segundos para reintentar...\n");
        delay(3000);
      }
      continue;
    }

    logPrintln("→ Almacenando huella en sensor...");
    uint8_t result = finger.storeModel(id);
    
    if (result == FINGERPRINT_OK) {
      logPrint("✓ Huella registrada exitosamente en ID: ");
      logPrintln(id);
      return true;
    } else {
      logPrint("✗ Error almacenando huella. Código: ");
      logPrintln(result);
      
      if (intentos < max_intentos) {
        logPrintln("→ Esperando 3 segundos para reintentar...\n");
        delay(3000);
      }
      continue;
    }
  }

  logPrintln("\n✗ Fallo después de 3 intentos");
  return false;
}

// =====================================================
// BUSCAR HUELLA EN SENSOR (VERIFICACIÓN 1:1)
// =====================================================
bool searchFingerprintOnce(uint16_t sensor_id) {
  logPrintln("→ Procesando huella...");

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
    logPrintln("✗ No se detectó dedo");
    return false;
  }

  if (finger.image2Tz() != FINGERPRINT_OK) {
    logPrintln("✗ Error procesando huella");
    return false;
  }

  logPrintln("→ Comparando con huella almacenada...");
  
  int p_search = finger.fingerFastSearch();

  if (p_search == FINGERPRINT_OK) {
    int found_id = finger.fingerID;
    
    if (found_id == sensor_id) {
      lastFingerprintID = sensor_id;
      lastFingerprintConfidence = finger.confidence;
      return true;
    } else {
      logPrint("✗ Huella encontrada pero ID no coincide. Encontrado: ");
      logPrint(found_id);
      logPrint(" | Esperado: ");
      logPrintln(sensor_id);
      return false;
    }
  } else {
    logPrintln("✗ Huella no encontrada en este intento");
    return false;
  }
}

bool searchFingerprintInSensorWithID(uint16_t sensor_id) {
  logPrint("\n[BUSCAR HUELLA - ID: ");
  logPrint(sensor_id);
  logPrintln("]");
  
  int intentos = 0;
  int max_intentos = 3;

  while (intentos < max_intentos) {
    intentos++;
    
    logPrint("\n→ Intento ");
    logPrint(intentos);
    logPrint("/");
    logPrintln(max_intentos);
    
    logPrintln("→ Coloca el dedo en el sensor...");

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
      logPrintln("✗ No se detectó dedo");
      
      if (intentos < max_intentos) {
        logPrintln("→ Esperando 2 segundos para reintentar...\n");
        delay(2000);
      }
      continue;
    }

    logPrintln("→ Procesando huella...");
    if (finger.image2Tz() != FINGERPRINT_OK) {
      logPrintln("✗ Error procesando huella");
      
      if (intentos < max_intentos) {
        logPrintln("→ Esperando 2 segundos para reintentar...\n");
        delay(2000);
      }
      continue;
    }

    logPrintln("→ Comparando con huella almacenada...");
    
    // Hacer búsqueda directa sin cargar modelo primero
    int p_search = finger.fingerFastSearch();

    if (p_search == FINGERPRINT_OK) {
      int found_id = finger.fingerID;
      
      if (found_id == sensor_id) {
        // Match exitoso con el ID correcto
        lastFingerprintID = sensor_id;
        lastFingerprintConfidence = finger.confidence;

        logPrint("✓ ¡Huella coincide!");
        logPrint(" | Confianza: ");
        logPrintln(lastFingerprintConfidence);

        return true;
      } else {
        logPrint("✗ Huella encontrada pero ID no coincide. Encontrado: ");
        logPrint(found_id);
        logPrint(" | Esperado: ");
        logPrintln(sensor_id);
        
        if (intentos < max_intentos) {
          logPrintln("→ Esperando 2 segundos para reintentar...\n");
          delay(2000);
        }
        continue;
      }
    } else {
      logPrintln("✗ Huella no encontrada en este intento");
      
      if (intentos < max_intentos) {
        logPrintln("→ Esperando 2 segundos para reintentar...\n");
        delay(2000);
      }
      continue;
    }
  }

  logPrintln("\n✗ Huella no encontrada después de 3 intentos");
  lastFingerprintID = -1;
  lastFingerprintConfidence = -1;
  return false;
}

int searchFingerprintWithRetries(uint16_t sensor_id, bool &permitir_supervisado) {
  permitir_supervisado = false;
  int intentos = 0;
  const int MAX_INTENTOS = 3;

  while (intentos < MAX_INTENTOS) {
    intentos++;
    logPrint("→ Intento ");
    logPrint(intentos);
    logPrint("/");
    logPrintln(MAX_INTENTOS);
    logPrintln("→ Coloca el dedo en el sensor...");
    
    // ★ LLAMAR A LA FUNCIÓN SIN REINTENTOS INTERNOS
    if (searchFingerprintOnce(sensor_id)) {
      int id = getFingerprintID();
      return id;
    }
    
    logPrintln(); // Una sola línea vacía entre intentos
    
    if (intentos < MAX_INTENTOS) {
      delay(2000);
    }
  }

  // Si llegamos aquí, los 3 intentos fallaron
  permitir_supervisado = true;
  logPrintln("\n✗ No se pudo verificar la huella después de 3 intentos");
  return -1;
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
      logPrintln("✓ Base de datos eliminada correctamente.");
      delay(1200);
      return true;
    }

    // Solo mostrar error si es diferente de 1 (que es normal en primeros intentos)
    if (result != 1) {
      logPrint("Error eliminando base. Código: ");
      logPrintln(result);
    }

    attempts++;
    delay(1800);
  }

  logPrintln("✗ No se pudo eliminar la base.");
  return false;
}

bool deleteFingerprintById(uint16_t id) {
  // ★ BORRAR UNA SOLA HUELLA POR ID
  int attempts = 0;
  uint8_t result;

  logPrint("→ Borrando huella con ID: ");
  logPrintln(id);

  while (attempts < 5) {
    result = finger.deleteModel(id);

    if (result == FINGERPRINT_OK) {
      logPrint("✓ Huella ");
      logPrint(id);
      logPrintln(" eliminada correctamente.");
      delay(800);
      return true;
    }

    logPrint("[Intento ");
    logPrint(attempts + 1);
    logPrint("/5] Error eliminando huella. Código: ");
    logPrintln(result);

    attempts++;
    delay(1000);
  }

  logPrint("✗ No se pudo eliminar la huella ");
  logPrintln(id);
  return false;
}