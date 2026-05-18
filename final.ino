#include "biometric_sensor.h"
#include "wifi_backend.h"

String num_doc_usuario = "";
String nombre_usuario = "";
String usuario_id = "";

void setup() {
  Serial.begin(115200);
  delay(3000);

  Serial.println("\n=== SARA - SISTEMA BIOMÉTRICO ===\n");

  if (!initFingerprintSensor()) {
    Serial.println("FATAL: Sensor no detectado");
    while (1);
  }

  if (!conectarWiFi()) {
    Serial.println("FATAL: WiFi no disponible");
    while (1);
  }

  Serial.println("\n✓ Sistema listo\n");
  mostrarMenuPrincipal();
}

void loop() {
  if (Serial.available()) {
    String entrada = Serial.readStringUntil('\n');
    entrada.trim();

    if (entrada.length() == 0) return;

    if (entrada == "1") {
      iniciarFlujoPedirDocumento("REGISTRO");
    } else if (entrada == "2") {
      iniciarFlujoPedirDocumento("VERIFICACION");
    } else if (entrada == "3") {
      borrarTodasLasHuellas();
    } else if (entrada == "?") {
      mostrarMenuPrincipal();
    }else {
      // Si estamos esperando documento en un flujo
      if (num_doc_usuario.length() == 0) {
        procesarDocumento(entrada, "");
      }
    }
  }
}

void mostrarMenuPrincipal() {
  Serial.println("═══════════════════════════════════");
  Serial.println("1 -> REGISTRAR HUELLA");
  Serial.println("2 -> VERIFICAR HUELLA");
  Serial.println("3 -> BORRAR TODAS LAS HUELLAS");
  Serial.println("? -> MOSTRAR MENÚ");
  Serial.println("═══════════════════════════════════\n");
}

void iniciarFlujoPedirDocumento(String tipo_flujo) {
  Serial.println("\n═══════════════════════════════════");
  Serial.print("[ ");
  Serial.print(tipo_flujo);
  Serial.println(" ]");
  Serial.println("═══════════════════════════════════");
  Serial.println("\nIngresa el documento del usuario:");
  Serial.println("(Ej: 1234567890)\n");
  
  // Esperar documento
  unsigned long timeout = millis();
  while (millis() - timeout < 30000) {
    if (Serial.available()) {
      String documento = Serial.readStringUntil('\n');
      documento.trim();
      
      if (documento.length() > 0) {
        procesarDocumento(documento, tipo_flujo);
        return;
      }
    }
    delay(100);
  }

  Serial.println("✗ Timeout. Regresando al menú...\n");
  mostrarMenuPrincipal();
}

void procesarDocumento(String documento, String tipo_flujo) {
  Serial.print("\n→ Buscando usuario: ");
  Serial.println(documento);
  
  JsonDocument respuesta = buscarUsuarioPorDocumento(documento.c_str());

  if (respuesta.containsKey("error")) {
    Serial.println("✗ Error conectando al backend\n");
    mostrarMenuPrincipal();
    return;
  }

  bool existe = respuesta["existe"];

  if (!existe) {
    // El backend retorna un mensaje específico
    String mensaje = respuesta["mensaje"].as<String>();
    
    Serial.print("✗ ");
    Serial.println(mensaje);
    Serial.println();
    mostrarMenuPrincipal();
    return;
  }

  // Usuario encontrado
  num_doc_usuario = documento;
  nombre_usuario = respuesta["nombre_completo"].as<String>();
  usuario_id = respuesta["id"].as<String>();

  Serial.print("✓ Bienvenido: ");
  Serial.println(nombre_usuario);

  if (tipo_flujo == "REGISTRO") {
    flujoRegistro();
  } else if (tipo_flujo == "VERIFICACION") {
    flujoVerificacion();
  } else {
    mostrarMenuPrincipal();
  }
}

void flujoRegistro() {
  Serial.print("\n[REGISTRO]\n");
  Serial.print("Usuario: ");
  Serial.println(nombre_usuario);
  Serial.print("Documento: ");
  Serial.println(num_doc_usuario);

  // Verificar si el usuario ya tiene huella registrada
  Serial.println("\n→ Verificando si usuario ya tiene huella...");
  
  JsonDocument datos_usuario = obtenerDatosUsuario(num_doc_usuario.c_str());
  
  uint16_t id_sensor = -1;
  
  if (datos_usuario["existe"] && datos_usuario["sensor_id"] != -1) {
    // Usuario ya tiene huella, usar el mismo ID
    id_sensor = datos_usuario["sensor_id"];
    Serial.print("→ Usuario tiene huella anterior. Usando ID: ");
    Serial.println(id_sensor);
    Serial.println("→ Se actualizará la huella existente.\n");
  } else {
    // Usuario no tiene huella, obtener nuevo ID
    Serial.println("\n→ Solicitando ID disponible al servidor...");

    WiFiClient cliente;
    if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
      Serial.println("✗ Error conectando al backend");
      mostrarMenuPrincipal();
      return;
    }

    String request = String("GET /hardware/sensor/id-disponible HTTP/1.1\r\n");
    request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
    request += "Connection: close\r\n";
    request += "\r\n";

    cliente.print(request);
    delay(500);

    String respuesta_str = "";
    while (cliente.available()) {
      respuesta_str += (char)cliente.read();
    }
    cliente.stop();

    JsonDocument respuesta_json;
    int json_inicio = respuesta_str.indexOf("{");
    if (json_inicio != -1) {
      String json_str = respuesta_str.substring(json_inicio);
      deserializeJson(respuesta_json, json_str);
    }

    if (!respuesta_json["disponible"]) {
      Serial.println("✗ No hay IDs disponibles en el sensor");
      mostrarMenuPrincipal();
      return;
    }

    id_sensor = respuesta_json["sensor_id"];

    Serial.print("✓ ID asignado: ");
    Serial.println(id_sensor);
  }

  Serial.println("\nPresiona cualquier tecla para continuar...");

  // Esperar que el usuario presione algo
  while (!Serial.available()) {
    delay(100);
  }
  Serial.readStringUntil('\n');

  if (enrollFingerprintWithID(id_sensor)) {
    Serial.println("\n→ Notificando al servidor...");
    
    if (notificarRegistroExitoso(num_doc_usuario.c_str(), id_sensor)) {
      Serial.println("✓ Registro completado exitosamente!");
    } else {
      Serial.println("⚠ Huella guardada en sensor (servidor no respondió)");
    }
  } else {
    Serial.println("✗ Fallo el registro");
  }

  num_doc_usuario = "";
  nombre_usuario = "";
  usuario_id = "";

  Serial.println("\nPresiona cualquier tecla para volver al menú...");
  while (!Serial.available()) {
    delay(100);
  }
  Serial.readStringUntil('\n');

  Serial.println();
  mostrarMenuPrincipal();
}

void flujoVerificacion() {
  Serial.print("\n[VERIFICACIÓN]\n");
  Serial.print("Usuario: ");
  Serial.println(nombre_usuario);
  Serial.print("Documento: ");
  Serial.println(num_doc_usuario);

  // Obtener sensor_id desde el backend
  Serial.println("\n→ Obteniendo sensor_id del usuario...");

  JsonDocument datos = obtenerDatosUsuario(num_doc_usuario.c_str());

  if (!datos["existe"]) {
    Serial.println("✗ Error obteniendo datos del usuario");
    mostrarMenuPrincipal();
    return;
  }

  if (datos["sensor_id"] == -1) {
    Serial.println("✗ El usuario no tiene huella registrada");
    mostrarMenuPrincipal();
    return;
  }

  uint16_t id_esperado = datos["sensor_id"];

  Serial.print("✓ Sensor ID encontrado: ");
  Serial.println(id_esperado);
  Serial.println("\nPresiona cualquier tecla para continuar...");

  // Esperar que el usuario presione algo
  while (!Serial.available()) {
    delay(100);
  }
  Serial.readStringUntil('\n');

  if (searchFingerprintInSensorWithID(id_esperado)) {
    int confianza = getFingerprintConfidence();

    Serial.println("\n✓ ¡Verificación exitosa!");
    Serial.print("Confianza: ");
    Serial.println(confianza);
    
    Serial.println("\n→ Registrando asistencia...");
    // Aquí irá registro de asistencia en el futuro
    Serial.println("✓ Asistencia registrada");
      
  } else {
    Serial.println("\n✗ Huella no reconocida");
  }

  num_doc_usuario = "";
  nombre_usuario = "";
  usuario_id = "";

  Serial.println("\nPresiona cualquier tecla para volver al menú...");
  while (!Serial.available()) {
    delay(100);
  }
  Serial.readStringUntil('\n');

  Serial.println();
  mostrarMenuPrincipal();
}

void borrarTodasLasHuellas() {
  Serial.println("\n═══════════════════════════════════");
  Serial.println("[BORRAR TODAS LAS HUELLAS]");
  Serial.println("═══════════════════════════════════");
  Serial.println("\n⚠ ¡ADVERTENCIA!");
  Serial.println("Esto eliminará TODAS las huellas del sensor.");
  Serial.println("¿Estás seguro? (S/N)\n");

  unsigned long timeout = millis();
  while (millis() - timeout < 10000) {
    if (Serial.available()) {
      char respuesta = Serial.read();
      
      if (respuesta == 'S' || respuesta == 's') {
        Serial.println("\n→ Borrando todas las huellas...\n");
        
        // Limpiar buffer
        while (Serial.available()) {
          Serial.read();
        }
        
        // Borrar base de datos del sensor
        if (clearDatabase()) {
          Serial.println("✓ Todas las huellas han sido eliminadas.");
        } else {
          Serial.println("✗ Error borrando las huellas.");
        }
        
        Serial.println("\nPresiona cualquier tecla para volver al menú...");
        while (!Serial.available()) {
          delay(100);
        }
        Serial.readStringUntil('\n');
        
        Serial.println();
        mostrarMenuPrincipal();
        return;
      } else {
        Serial.println("Operación cancelada.\n");
        mostrarMenuPrincipal();
        return;
      }
    }
    delay(100);
  }

  Serial.println("Timeout. Cancelado.\n");
  mostrarMenuPrincipal();
}