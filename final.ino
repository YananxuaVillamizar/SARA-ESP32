#include "biometric_sensor.h"
#include "wifi_backend.h"

String num_doc_usuario = "";
String nombre_usuario = "";

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
  mostrarMenu();
}

void loop() {
  if (Serial.available()) {
    String entrada = Serial.readStringUntil('\n');
    entrada.trim();

    if (entrada.length() == 0) return;

    if (entrada == "1") {
      flujoRegistro();
    } else if (entrada == "2") {
      flujoVerificacion();
    } else if (entrada == "?") {
      mostrarMenu();
    } else {
      // Asumir que es un documento
      buscarUsuario(entrada);
    }
  }
}

void mostrarMenu() {
  Serial.println("═══════════════════════════════════");
  Serial.println("1 -> REGISTRAR HUELLA");
  Serial.println("2 -> VERIFICAR HUELLA");
  Serial.println("? -> MOSTRAR ESTE MENÚ");
  Serial.println("═══════════════════════════════════\n");
}

void buscarUsuario(String documento) {
  Serial.println("Buscando usuario...");
  
  JsonDocument respuesta = buscarUsuarioPorDocumento(documento.c_str());

  if (respuesta.containsKey("error")) {
    Serial.println("✗ Error conectando al backend\n");
    return;
  }

  bool existe = respuesta["existe"];

  if (!existe) {
    Serial.println("✗ Usuario no encontrado\n");
    mostrarMenu();
    return;
  }

  num_doc_usuario = documento;
  nombre_usuario = respuesta["nombre_completo"].as<String>();

  Serial.print("✓ Bienvenido: ");
  Serial.println(nombre_usuario);
  Serial.println("\nElige opción:");
  Serial.println("1 -> Registrar");
  Serial.println("2 -> Verificar\n");
}

void flujoRegistro() {
  Serial.print("\n[REGISTRO]\n");
  Serial.print("Usuario: ");
  Serial.println(nombre_usuario);

  uint16_t id_sensor = (uint16_t)num_doc_usuario.toInt() % 1000;

  if (enrollFingerprintWithID(id_sensor)) {
    Serial.println("\n→ Notificando al servidor...");
    
    if (notificarRegistroExitoso(num_doc_usuario.c_str())) {
      Serial.println("✓ Registro completado!");
    } else {
      Serial.println("⚠ Huella guardada en sensor (servidor no respondió)");
    }
  } else {
    Serial.println("✗ Fallo el registro");
  }

  Serial.println("\n");
  mostrarMenu();
}

void flujoVerificacion() {
  Serial.print("\n[VERIFICACIÓN]\n");
  Serial.print("Usuario: ");
  Serial.println(nombre_usuario);

  uint16_t id_esperado = (uint16_t)num_doc_usuario.toInt() % 1000;

  if (searchFingerprintInSensor()) {
    int id_encontrado = getFingerprintID();

    if (id_encontrado == id_esperado) {
      Serial.println("\n✓ Verificación exitosa!");
      Serial.println("→ Registrando asistencia...");
      
      // Aquí iría el registro de asistencia
      // registrarAsistencia(num_doc_usuario.c_str(), ...);
      
    } else {
      Serial.print("✗ Huella no coincide. Encontrado ID: ");
      Serial.print(id_encontrado);
      Serial.print(" Esperado ID: ");
      Serial.println(id_esperado);
    }
  } else {
    Serial.println("✗ Huella no reconocida");
  }

  Serial.println("\n");
  mostrarMenu();
}