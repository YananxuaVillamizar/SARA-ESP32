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
    } else if (entrada == "4") {
      iniciarFlujoPedirDocumento("ASISTENCIA");
    } else if (entrada == "?") {
      mostrarMenuPrincipal();
    } else {
      // Si estamos esperando documento en un flujo
      if (num_doc_usuario.length() == 0) {
        procesarDocumento(entrada, "");
      }
    }
  }
}

void mostrarMenuPrincipal() {
  Serial.println("\n═══════════════════════════════════════════════════");
  Serial.println("           SISTEMA SARA - MENÚ PRINCIPAL");
  Serial.println("═══════════════════════════════════════════════════\n");
  
  Serial.println("┌─ GESTIÓN DE HUELLAS DACTILARES ─────────────────┐");
  Serial.println("│ 1 -> Registrar huella                            │");
  Serial.println("│ 2 -> Verificar huella                            │");
  Serial.println("│ 3 -> Borrar todas las huellas                    │");
  Serial.println("└────────────────────────────────────────────────────┘\n");
  
  Serial.println("┌─ REGISTRO DE ASISTENCIA ────────────────────────┐");
  Serial.println("│ 4 -> Registrar asistencia (entrada/salida)       │");
  Serial.println("└────────────────────────────────────────────────────┘\n");
  
  Serial.println("? -> Mostrar este menú");
  Serial.println("═══════════════════════════════════════════════════\n");
}

void iniciarFlujoPedirDocumento(String tipo_flujo) {
  // Limpiar buffer serial completamente
  while (Serial.available()) {
    Serial.read();
  }
  delay(300);

  Serial.println("\nIngresa el documento del usuario:");
  Serial.println("(Ej: 1234567890)\n");

  String num_doc = "";
  unsigned long timeout = millis();

  while (millis() - timeout < 30000) {
    if (Serial.available()) {
      char c = Serial.read();
      
      // Solo agregar caracteres válidos
      if ((c >= '0' && c <= '9') || c == '\n' || c == '\r') {
        if (c == '\n' || c == '\r') {
          if (num_doc.length() > 0) {
            break;
          }
        } else {
          num_doc += c;
        }
      }
    }
    delay(50);
  }

  num_doc.trim();

  if (num_doc.length() == 0) {
    Serial.println("✗ Timeout");
    mostrarMenuPrincipal();
    return;
  }

  Serial.print("→ Buscando usuario: ");
  Serial.println(num_doc);

  procesarDocumento(num_doc, tipo_flujo);
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
  } else if (tipo_flujo == "ASISTENCIA") {
    flujoAsistencia();
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

void flujoAsistencia() {
  Serial.print("\n[ASISTENCIA]\n");
  Serial.print("Usuario: ");
  Serial.println(nombre_usuario);
  Serial.print("Documento: ");
  Serial.println(num_doc_usuario);

  // Paso 1: Obtener asignaturas del usuario
  Serial.println("\n→ Obteniendo programas y asignaturas...");
  
  JsonDocument respuesta = obtenerAsignaturasUsuario(num_doc_usuario.c_str());

  if (respuesta.containsKey("error")) {
    Serial.println("✗ Error obteniendo asignaturas");
    mostrarMenuPrincipal();
    return;
  }

  if (!respuesta["existe"]) {
    Serial.println("✗ No se encontraron datos");
    mostrarMenuPrincipal();
    return;
  }

  String rol = respuesta["rol"].as<String>();
  bool requiere_selector = respuesta["requiere_selector_programa"];
  
  String horario_id = "";
  String nombre_asignatura = "";
  String asignatura_id = "";

  // Si requiere selector de programa
  if (requiere_selector) {
    JsonArray programas = respuesta["programas"].as<JsonArray>();

    Serial.println("\n→ Selecciona un programa:\n");
    
    if (rol == "Docente") {
      Serial.println("(Tienes múltiples programas asignados.)\n");
    } else {
      Serial.println("(Tienes múltiples programas matriculados.)\n");
    }

    for (size_t i = 0; i < programas.size(); i++) {
      String codigo = programas[i]["codigo"].as<String>();
      String nombre = programas[i]["nombre"].as<String>();
      
      if (codigo.length() > 0 && nombre.length() > 0) {
        Serial.print(i + 1);
        Serial.print(". ");
        Serial.print(nombre);
        Serial.print(" (");
        Serial.print(codigo);
        Serial.println(")");
      }
    }

    Serial.print("\nOpción (1-");
    Serial.print(programas.size());
    Serial.println("):\n");

    // Esperar selección de programa
    unsigned long timeout = millis();
    int opcion_programa = -1;

    while (millis() - timeout < 30000) {
      if (Serial.available()) {
        String entrada = Serial.readStringUntil('\n');
        entrada.trim();
        
        int opcion = entrada.toInt();
        
        if (opcion > 0 && opcion <= (int)programas.size()) {
          opcion_programa = opcion - 1;
          break;
        } else {
          Serial.println("✗ Opción inválida. Intenta de nuevo:");
        }
      }
      delay(100);
    }

    if (opcion_programa == -1) {
      Serial.println("✗ Timeout");
      mostrarMenuPrincipal();
      return;
    }

    // Obtener asignaturas del programa seleccionado
    Serial.println("\n→ Obteniendo asignaturas del programa...");
    String programa_id = programas[opcion_programa]["programa_id"].as<String>();
    String nombre_programa = programas[opcion_programa]["nombre"].as<String>();
    
    JsonDocument respuesta_asignaturas = obtenerAsignaturasPrograma(num_doc_usuario.c_str(), programa_id.c_str());

    if (respuesta_asignaturas.containsKey("error")) {
      Serial.println("✗ Error obteniendo asignaturas");
      mostrarMenuPrincipal();
      return;
    }

    JsonArray asignaturas = respuesta_asignaturas["asignaturas"].as<JsonArray>();

    if (asignaturas.size() == 0) {
      Serial.println("✗ No hay asignaturas en este programa");
      mostrarMenuPrincipal();
      return;
    }

    // Mostrar asignaturas del programa seleccionado
    Serial.print("\n✓ Programa: ");
    Serial.println(nombre_programa);
    Serial.println("\n→ Asignaturas disponibles:\n");
    
    for (size_t i = 0; i < asignaturas.size(); i++) {
      Serial.print(i + 1);
      Serial.print(". ");
      Serial.print(asignaturas[i]["nombre"].as<String>());
      Serial.print(" (");
      Serial.print(asignaturas[i]["codigo"].as<String>());
      Serial.print(") - Grupo ");
      Serial.println(asignaturas[i]["grupo"].as<String>());
    }

    Serial.print("\nSelecciona una asignatura (1-");
    Serial.print(asignaturas.size());
    Serial.println("):\n");

    // Esperar selección de asignatura
    timeout = millis();
    int opcion_asignatura = -1;

    while (millis() - timeout < 30000) {
      if (Serial.available()) {
        String entrada = Serial.readStringUntil('\n');
        entrada.trim();
        
        int opcion = entrada.toInt();
        
        if (opcion > 0 && opcion <= (int)asignaturas.size()) {
          opcion_asignatura = opcion - 1;
          break;
        } else {
          Serial.println("✗ Opción inválida. Intenta de nuevo:");
        }
      }
      delay(100);
    }

    if (opcion_asignatura == -1) {
      Serial.println("✗ Timeout");
      mostrarMenuPrincipal();
      return;
    }

    // Guardar datos de la asignatura seleccionada
    asignatura_id = asignaturas[opcion_asignatura]["asignatura_id"].as<String>();
    nombre_asignatura = asignaturas[opcion_asignatura]["nombre"].as<String>();
    
    Serial.print("\n✓ Asignatura seleccionada: ");
    Serial.println(nombre_asignatura);

    // ★ SELECTOR DE HORARIOS
    if (rol == "Docente") {
      Serial.println("\n→ Horarios disponibles para esta asignatura:\n");
      
      JsonDocument horarios_resp = obtenerHorariosAsignatura(
        num_doc_usuario.c_str(),
        asignatura_id.c_str()
      );

      if (horarios_resp.containsKey("error")) {
        Serial.println("✗ Error obteniendo horarios");
        mostrarMenuPrincipal();
        return;
      }

      JsonArray horarios = horarios_resp["horarios"].as<JsonArray>();

      if (horarios.size() == 0) {
        Serial.println("✗ No hay horarios disponibles");
        mostrarMenuPrincipal();
        return;
      }

      for (size_t i = 0; i < horarios.size(); i++) {
        Serial.print(i + 1);
        Serial.print(". ");
        Serial.print(horarios[i]["dia_semana"].as<String>());
        Serial.print(" ");
        Serial.print(horarios[i]["hora_inicio"].as<String>());
        Serial.print(" - ");
        Serial.println(horarios[i]["hora_fin"].as<String>());
      }

      Serial.print("\nSelecciona un horario (1-");
      Serial.print(horarios.size());
      Serial.println("):\n");

      timeout = millis();
      int opcion_horario = -1;

      while (millis() - timeout < 30000) {
        if (Serial.available()) {
          String entrada = Serial.readStringUntil('\n');
          entrada.trim();
          
          int opcion = entrada.toInt();
          
          if (opcion > 0 && opcion <= (int)horarios.size()) {
            opcion_horario = opcion - 1;
            break;
          } else {
            Serial.println("✗ Opción inválida. Intenta de nuevo:");
          }
        }
        delay(100);
      }

      if (opcion_horario == -1) {
        Serial.println("✗ Timeout");
        mostrarMenuPrincipal();
        return;
      }

      horario_id = horarios[opcion_horario]["horario_id"].as<String>();

      Serial.print("\n✓ Horario seleccionado: ");
      Serial.print(horarios[opcion_horario]["dia_semana"].as<String>());
      Serial.print(" ");
      Serial.print(horarios[opcion_horario]["hora_inicio"].as<String>());
      Serial.print(" - ");
      Serial.println(horarios[opcion_horario]["hora_fin"].as<String>());
    } else {
      // Para estudiantes: obtener horario_id de la asignatura
      horario_id = asignaturas[opcion_asignatura]["horario_id"].as<String>();
    }

  } else {
    // No requiere selector, procesar asignaturas directamente
    JsonArray asignaturas = respuesta["asignaturas"].as<JsonArray>();

    if (asignaturas.size() == 0) {
      Serial.println("✗ El usuario no tiene asignaturas asignadas");
      mostrarMenuPrincipal();
      return;
    }

    // Mostrar asignaturas disponibles
    Serial.println("\n→ Asignaturas disponibles:\n");
    
    for (size_t i = 0; i < asignaturas.size(); i++) {
      Serial.print(i + 1);
      Serial.print(". ");
      Serial.print(asignaturas[i]["nombre"].as<String>());
      Serial.print(" (");
      Serial.print(asignaturas[i]["codigo"].as<String>());
      Serial.print(") - Grupo ");
      Serial.println(asignaturas[i]["grupo"].as<String>());
    }

    Serial.print("\nSelecciona una asignatura (1-");
    Serial.print(asignaturas.size());
    Serial.println("):\n");

    // Esperar selección
    unsigned long timeout = millis();
    int opcion_seleccionada = -1;

    while (millis() - timeout < 30000) {
      if (Serial.available()) {
        String entrada = Serial.readStringUntil('\n');
        entrada.trim();
        
        int opcion = entrada.toInt();
        
        if (opcion > 0 && opcion <= (int)asignaturas.size()) {
          opcion_seleccionada = opcion - 1;
          break;
        } else {
          Serial.println("✗ Opción inválida. Intenta de nuevo:");
        }
      }
      delay(100);
    }

    if (opcion_seleccionada == -1) {
      Serial.println("✗ Timeout");
      mostrarMenuPrincipal();
      return;
    }

    // Obtener datos de la asignatura seleccionada
    JsonObject asignatura_seleccionada = asignaturas[opcion_seleccionada];
    horario_id = asignatura_seleccionada["horario_id"].as<String>();
    nombre_asignatura = asignatura_seleccionada["nombre"].as<String>();
    asignatura_id = asignatura_seleccionada["asignatura_id"].as<String>();

    Serial.print("\n✓ Asignatura seleccionada: ");
    Serial.println(nombre_asignatura);

    // Para estudiantes con un solo programa: mostrar horarios disponibles
    if (rol == "Estudiante") {
      Serial.println("\n→ Horarios disponibles para esta asignatura:\n");
      
      JsonDocument horarios_resp = obtenerHorariosAsignatura(
        num_doc_usuario.c_str(),
        asignatura_id.c_str()
      );

      if (horarios_resp.containsKey("error")) {
        Serial.println("✗ Error obteniendo horarios");
        mostrarMenuPrincipal();
        return;
      }

      JsonArray horarios = horarios_resp["horarios"].as<JsonArray>();

      if (horarios.size() == 0) {
        Serial.println("✗ No hay horarios disponibles");
        mostrarMenuPrincipal();
        return;
      }

      for (size_t i = 0; i < horarios.size(); i++) {
        Serial.print(i + 1);
        Serial.print(". ");
        Serial.print(horarios[i]["dia_semana"].as<String>());
        Serial.print(" ");
        Serial.print(horarios[i]["hora_inicio"].as<String>());
        Serial.print(" - ");
        Serial.println(horarios[i]["hora_fin"].as<String>());
      }

      Serial.print("\nSelecciona un horario (1-");
      Serial.print(horarios.size());
      Serial.println("):\n");

      unsigned long timeout = millis();
      int opcion_horario = -1;

      while (millis() - timeout < 30000) {
        if (Serial.available()) {
          String entrada = Serial.readStringUntil('\n');
          entrada.trim();
          
          int opcion = entrada.toInt();
          
          if (opcion > 0 && opcion <= (int)horarios.size()) {
            opcion_horario = opcion - 1;
            break;
          } else {
            Serial.println("✗ Opción inválida. Intenta de nuevo:");
          }
        }
        delay(100);
      }

      if (opcion_horario == -1) {
        Serial.println("✗ Timeout");
        mostrarMenuPrincipal();
        return;
      }

      horario_id = horarios[opcion_horario]["horario_id"].as<String>();

      Serial.print("\n✓ Horario seleccionado: ");
      Serial.print(horarios[opcion_horario]["dia_semana"].as<String>());
      Serial.print(" ");
      Serial.print(horarios[opcion_horario]["hora_inicio"].as<String>());
      Serial.print(" - ");
      Serial.println(horarios[opcion_horario]["hora_fin"].as<String>());
    }
  }

  // SOLICITAR FECHA Y HORA SOLO PARA DOCENTES
  if (rol == "Docente") {
    Serial.println("\nIngresa la fecha (formato YYYY-MM-DD, ej: 2026-05-17):");
    String fecha_str = "";
    unsigned long timeout = millis();

    while (millis() - timeout < 30000) {
      if (Serial.available()) {
        fecha_str = Serial.readStringUntil('\n');
        fecha_str.trim();
        
        if (fecha_str.length() == 10 && fecha_str[4] == '-' && fecha_str[7] == '-') {
          break;
        } else {
          Serial.println("✗ Formato inválido. Intenta de nuevo:");
        }
      }
      delay(100);
    }

    if (fecha_str.length() != 10) {
      Serial.println("✗ Timeout o fecha inválida");
      mostrarMenuPrincipal();
      return;
    }

    Serial.print("✓ Fecha: ");
    Serial.println(fecha_str);

    Serial.println("\nIngresa la hora (formato HH:MM:SS, ej: 14:30:00):");
    String hora_str = "";
    timeout = millis();

    while (millis() - timeout < 30000) {
      if (Serial.available()) {
        hora_str = Serial.readStringUntil('\n');
        hora_str.trim();
        
        if (hora_str.length() == 8 && hora_str[2] == ':' && hora_str[5] == ':') {
          break;
        } else {
          Serial.println("✗ Formato inválido. Intenta de nuevo:");
        }
      }
      delay(100);
    }

    if (hora_str.length() != 8) {
      Serial.println("✗ Timeout o hora inválida");
      mostrarMenuPrincipal();
      return;
    }

    Serial.print("✓ Hora: ");
    Serial.println(hora_str);

    // Procesar docente
    procesarAsistenciaDocente(num_doc_usuario, horario_id, fecha_str, hora_str);
    
  } else if (rol == "Estudiante") {
    // Para estudiante, la fecha y hora se solicitan dentro de procesarAsistenciaEstudiante()
    procesarAsistenciaEstudiante(num_doc_usuario, horario_id, "", "", asignatura_id);
  } else {
    Serial.println("✗ Rol no reconocido");
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

void procesarAsistenciaDocente(String num_doc, String horario_id, String fecha, String hora) {
  Serial.println("\n[DOCENTE - ASISTENCIA]\n");

  // ★ PASO 1: VERIFICAR SI ES ENTRADA O SALIDA
  Serial.println("→ Determinando tipo de registro...");
  
  WiFiClient cliente;
  
  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    Serial.println("✗ Error conectando al servidor");
    return;
  }

  JsonDocument solicitud;
  solicitud["horario_id"] = horario_id;
  solicitud["fecha"] = fecha;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/sesiones/verificar HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  delay(1500);

  String respuesta_str = "";
  unsigned long timeout_resp = millis();
  
  while (millis() - timeout_resp < 5000) {
    while (cliente.available()) {
      respuesta_str += (char)cliente.read();
      delay(5);
    }
    delay(50);
  }
  cliente.stop();

  JsonDocument verificacion;
  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(verificacion, json_str);
  }

  bool hay_sesion = verificacion["hay_sesion_abierta"];
  
  String aula = "";

  if (!hay_sesion) {
    // ENTRADA: No hay sesión, pedir aula
    Serial.println("✓ No hay sesión abierta. Registrando ENTRADA.\n");
    
    Serial.println("Ingresa el aula (ej: A101):");
    unsigned long timeout = millis();
    
    while (millis() - timeout < 15000 && aula.length() == 0) {
      if (Serial.available()) {
        aula = Serial.readStringUntil('\n');
        aula.trim();
      }
      delay(100);
    }

    if (aula.length() == 0) {
      Serial.println("✗ Timeout");
      return;
    }

    Serial.print("✓ Aula: ");
    Serial.println(aula);
    
  } else {
    // SALIDA: Hay sesión abierta
    Serial.println("✓ Sesión abierta encontrada. Registrando SALIDA.\n");
  }

  // ★ PASO 2: VERIFICAR HUELLA (después de determinar tipo de registro)
  Serial.println("→ Verificando identidad biométrica...");
  
  JsonDocument datos_usuario = obtenerDatosUsuario(num_doc.c_str());

  if (!datos_usuario.containsKey("existe") || !datos_usuario["existe"]) {
    Serial.println("✗ Error obteniendo datos del usuario");
    return;
  }

  int sensor_id = datos_usuario["sensor_id"];

  if (sensor_id == -1) {
    Serial.println("✗ El usuario no tiene huella registrada");
    return;
  }

  if (!searchFingerprintInSensorWithID(sensor_id)) {
    Serial.println("✗ Verificación biométrica fallida");
    return;
  }

  Serial.println("✓ Identidad verificada\n");

  // ★ PASO 3: REGISTRAR ASISTENCIA
  Serial.println("→ Registrando asistencia...");
  
  JsonDocument respuesta = registrarAsistenciaDocente(
    num_doc.c_str(),
    horario_id.c_str(),
    fecha.c_str(),
    hora.c_str(),
    aula.c_str()
  );

  if (respuesta.containsKey("error")) {
    Serial.println("✗ Error conectando al servidor");
    return;
  }

  bool exito = respuesta["exito"];
  String mensaje = respuesta["mensaje"].as<String>();

  if (exito) {
    Serial.print("✓ ");
    Serial.println(mensaje);
  } else {
    String error_msg = respuesta["detail"].as<String>();
    Serial.print("✗ Error: ");
    Serial.println(error_msg);
  }
}

void procesarAsistenciaEstudiante(String num_doc, String horario_id, String fecha, String hora, String asignatura_id) {
  Serial.println("\n[ESTUDIANTE - ASISTENCIA]\n");

  // ★ PASO 1: MOSTRAR HORARIOS DE CLASE
  Serial.println("→ Horarios disponibles para esta asignatura:\n");
  
  JsonDocument horarios_resp = obtenerHorariosEstudianteAsignatura(num_doc.c_str(), asignatura_id.c_str());

  if (horarios_resp.containsKey("error") || !horarios_resp["existe"]) {
    Serial.println("✗ No hay horarios disponibles");
    return;
  }

  JsonArray horarios = horarios_resp["horarios"].as<JsonArray>();

  if (horarios.size() == 0) {
    Serial.println("✗ No hay horarios para tu grupo");
    return;
  }

  for (size_t i = 0; i < horarios.size(); i++) {
    Serial.print(i + 1);
    Serial.print(". ");
    Serial.print(horarios[i]["dia_semana"].as<String>());
    Serial.print(" ");
    Serial.print(horarios[i]["hora_inicio"].as<String>());
    Serial.print(" - ");
    Serial.println(horarios[i]["hora_fin"].as<String>());
  }

  Serial.print("\nSelecciona un horario (1-");
  Serial.print(horarios.size());
  Serial.println("):\n");

  // ★ PASO 2: SELECCIONAR HORARIO
  unsigned long timeout = millis();
  int opcion_horario = -1;

  while (millis() - timeout < 30000) {
    if (Serial.available()) {
      String entrada = Serial.readStringUntil('\n');
      entrada.trim();
      
      int opcion = entrada.toInt();
      
      if (opcion > 0 && opcion <= (int)horarios.size()) {
        opcion_horario = opcion - 1;
        break;
      } else {
        Serial.println("✗ Opción inválida. Intenta de nuevo:");
      }
    }
    delay(100);
  }

  if (opcion_horario == -1) {
    Serial.println("✗ Timeout");
    return;
  }

  horario_id = horarios[opcion_horario]["horario_id"].as<String>();

  Serial.print("\n✓ Horario seleccionado: ");
  Serial.print(horarios[opcion_horario]["dia_semana"].as<String>());
  Serial.print(" ");
  Serial.print(horarios[opcion_horario]["hora_inicio"].as<String>());
  Serial.print(" - ");
  Serial.println(horarios[opcion_horario]["hora_fin"].as<String>());

  // ★ PASO 3: INGRESAR FECHA Y HORA PARA VERIFICACIÓN
  Serial.println("\nIngresa la fecha (formato YYYY-MM-DD, ej: 2026-05-17):");
  String fecha_ingresada = "";
  timeout = millis();

  while (millis() - timeout < 30000) {
    if (Serial.available()) {
      fecha_ingresada = Serial.readStringUntil('\n');
      fecha_ingresada.trim();
      
      if (fecha_ingresada.length() == 10 && fecha_ingresada[4] == '-' && fecha_ingresada[7] == '-') {
        break;
      } else {
        Serial.println("✗ Formato inválido. Intenta de nuevo:");
      }
    }
    delay(100);
  }

  if (fecha_ingresada.length() != 10) {
    Serial.println("✗ Timeout o fecha inválida");
    return;
  }

  Serial.print("✓ Fecha: ");
  Serial.println(fecha_ingresada);

  Serial.println("\nIngresa la hora (formato HH:MM:SS, ej: 14:30:00):");
  String hora_ingresada = "";
  timeout = millis();

  while (millis() - timeout < 30000) {
    if (Serial.available()) {
      hora_ingresada = Serial.readStringUntil('\n');
      hora_ingresada.trim();
      
      if (hora_ingresada.length() == 8 && hora_ingresada[2] == ':' && hora_ingresada[5] == ':') {
        break;
      } else {
        Serial.println("✗ Formato inválido. Intenta de nuevo:");
      }
    }
    delay(100);
  }

  if (hora_ingresada.length() != 8) {
    Serial.println("✗ Timeout o hora inválida");
    return;
  }

  Serial.print("✓ Hora: ");
  Serial.println(hora_ingresada);

  // ★ PASO 4: VERIFICAR SESIÓN ABIERTA
  Serial.println("\n→ Verificando sesión...");
  
  WiFiClient cliente;
  
  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    Serial.println("✗ Error conectando al servidor");
    return;
  }

  JsonDocument solicitud;
  solicitud["horario_id"] = horario_id;
  solicitud["fecha"] = fecha_ingresada;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/sesiones/verificar HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  delay(1500);

  String respuesta_str = "";
  unsigned long timeout_resp = millis();
  
  while (millis() - timeout_resp < 5000) {
    while (cliente.available()) {
      respuesta_str += (char)cliente.read();
      delay(5);
    }
    delay(50);
  }
  cliente.stop();

  JsonDocument verificacion;
  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(verificacion, json_str);
  }

  bool hay_sesion = verificacion["hay_sesion_abierta"];

  if (!hay_sesion) {
    Serial.println("✗ No hay sesión abierta para este horario en esta fecha");
    return;
  }

  String sesion_id = verificacion["sesion_id"].as<String>();
  Serial.println("✓ Sesión disponible");

  // ★ PASO 5: VERIFICAR TIPO DE REGISTRO (entrada o salida)
  Serial.println("\n→ Determinando tipo de registro...");
  
  JsonDocument tipo_resp = verificarTipoRegistro(sesion_id.c_str(), num_doc.c_str());

  if (tipo_resp.containsKey("error")) {
    Serial.println("✗ Error verificando tipo de registro");
    return;
  }

  bool puede_entrada = tipo_resp["puede_entrada"];
  bool puede_salida = tipo_resp["puede_salida"];
  bool completado = tipo_resp.containsKey("completado") && tipo_resp["completado"];

  String tipo = "";

  if (completado) {
    Serial.println("✗ Ya completaste entrada y salida en esta sesión");
    return;
  }

  if (puede_entrada && !puede_salida) {
    tipo = "entrada";
    Serial.println("✓ Registrando ENTRADA");
  } else if (puede_salida && !puede_entrada) {
    tipo = "salida";
    Serial.println("✓ Registrando SALIDA");
  } else {
    Serial.println("✗ Error: No se pudo determinar el tipo de registro");
    return;
  }

  // ★ PASO 6: VERIFICAR HUELLA BIOMÉTRICA
  Serial.println("\n→ Verificando identidad biométrica...");
  
  JsonDocument datos_usuario = obtenerDatosUsuario(num_doc.c_str());

  if (!datos_usuario.containsKey("existe") || !datos_usuario["existe"]) {
    Serial.println("✗ Error obteniendo datos del usuario");
    return;
  }

  int sensor_id = datos_usuario["sensor_id"];

  if (sensor_id == -1) {
    Serial.println("✗ El usuario no tiene huella registrada");
    return;
  }

  if (!searchFingerprintInSensorWithID(sensor_id)) {
    Serial.println("✗ Verificación biométrica fallida");
    return;
  }

  Serial.println("✓ Identidad verificada");

  // ★ PASO 7: REGISTRAR ASISTENCIA
  Serial.println("\n→ Registrando asistencia...");
  
  JsonDocument respuesta = registrarAsistenciaEstudiante(
    num_doc.c_str(),
    horario_id.c_str(),
    fecha_ingresada.c_str(),
    hora_ingresada.c_str(),
    tipo.c_str()
  );

  if (respuesta.containsKey("error")) {
    Serial.println("✗ Error conectando al servidor");
    return;
  }

  bool exito = respuesta["exito"];
  String mensaje = respuesta["mensaje"].as<String>();

  if (exito) {
    Serial.print("✓ ");
    Serial.println(mensaje);
  } else {
    String error_msg = respuesta["detail"].as<String>();
    Serial.print("✗ ");
    Serial.println(error_msg);
  }
}