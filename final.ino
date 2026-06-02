#include "biometric_sensor.h"
#include "wifi_backend.h"

String obtenerDiaSemana(String fecha_str) {
  // fecha_str formato: YYYY-MM-DD
  // Usar librería para calcular día de la semana
  // Por ahora usaremos una aproximación simple
  
  int year = fecha_str.substring(0, 4).toInt();
  int month = fecha_str.substring(5, 7).toInt();
  int day = fecha_str.substring(8, 10).toInt();
  
  // Algoritmo de Zeller para obtener día de la semana
  if (month < 3) {
    month += 12;
    year--;
  }
  
  int q = day;
  int m = month;
  int k = year % 100;
  int j = year / 100;
  
  int h = (q + (13 * (m + 1)) / 5 + k + (k / 4) + (j / 4) - (2 * j)) % 7;
  
  // h: 0=Sat, 1=Sun, 2=Mon, 3=Tue, 4=Wed, 5=Thu, 6=Fri
  String dias[] = {"sabado", "domingo", "lunes", "martes", "miercoles", "jueves", "viernes"};
  
  return dias[h];
}

String obtenerFechaHoraActual() {
  time_t ahora = time(nullptr);
  struct tm* timeinfo = localtime(&ahora);
  
  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
  
  return String(buffer);
}

String num_doc_usuario = "";
String nombre_usuario = "";
String usuario_id = "";

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== SARA - SISTEMA BIOMÉTRICO ===\n");

  if (!initFingerprintSensor()) {
    Serial.println("FATAL: Sensor no detectado");
    while (1);
  }

  if (!conectarWiFi()) {
    Serial.println("FATAL: WiFi no disponible");
    while (1);
  }

  // ★ SINCRONIZAR HORA CON NTP
  Serial.println("\n[BOOT] Sincronizando hora con servidor NTP...");
  
  // Configurar zona horaria (Colombia es UTC-5)
  configTime(-5 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  
  // Esperar a que se sincronice
  time_t ahora = time(nullptr);
  int intentos = 0;
  while (ahora < 24 * 3600 && intentos < 20) {
    delay(500);
    ahora = time(nullptr);
    intentos++;
  }
  
  if (ahora > 24 * 3600) {
    struct tm timeinfo = *localtime(&ahora);
    Serial.print("[OK] Hora sincronizada: ");
    Serial.println(asctime(&timeinfo));
  } else {
    Serial.println("[WARN] No se pudo sincronizar la hora");
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

  // ★ VERIFICAR PIN ADMIN PRIMERO
  if (!verificarPinAdmin()) {
    mostrarMenuPrincipal();
    return;
  }

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
  // ★ VERIFICAR PIN ADMIN PRIMERO
  if (!verificarPinAdmin()) {
    mostrarMenuPrincipal();
    return;
  }

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
  Serial.println("\n[ASISTENCIA]\n");
  Serial.print("Usuario: ");
  Serial.println(nombre_usuario);
  Serial.print("Documento: ");
  Serial.println(num_doc_usuario);

  // ★ OBTENER FECHA Y HORA AUTOMÁTICA
  String fecha_hora = obtenerFechaHoraActual();
  String fecha_str = fecha_hora.substring(0, 10); // YYYY-MM-DD
  String hora_str = fecha_hora.substring(11, 19);  // HH:MM:SS
  
  Serial.println("\n→ Fecha y hora actual obtenidas automáticamente");
  Serial.print("   Fecha: ");
  Serial.println(fecha_str);
  Serial.print("   Hora: ");
  Serial.println(hora_str);
  
  // ★ PASO 2: DETERMINAR SI ES DOCENTE O ESTUDIANTE
  String rol = "";
  JsonDocument datos_usuario = obtenerDatosUsuario(num_doc_usuario.c_str());

  if (!datos_usuario.containsKey("existe") || !datos_usuario["existe"]) {
    Serial.println("✗ Error obteniendo datos del usuario");
    mostrarMenuPrincipal();
    return;
  }

  rol = datos_usuario["rol"].as<String>();

  if (rol == "Docente") {
    procesarAsistenciaDocente(num_doc_usuario, fecha_str);
  } else if (rol == "Estudiante") {
    procesarAsistenciaEstudiante(num_doc_usuario, fecha_str);
  } else {
    Serial.println("✗ Rol no reconocido");
    mostrarMenuPrincipal();
    return;
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


void procesarAsistenciaDocente(String num_doc, String fecha) {
  Serial.println("\n[DOCENTE - ASISTENCIA]\n");

  // Variables globales para toda la función
  String sesion_id = "";
  String horario_id = "";
  String aula = "";

  // ★ PASO 1: VERIFICAR SI HAY SESIONES ABIERTAS
  Serial.println("→ Verificando sesiones abiertas...");
  
  JsonDocument sesiones_resp = verificarSesionesDocente(num_doc.c_str(), fecha.c_str());

  if (sesiones_resp.containsKey("error")) {
    Serial.println("✗ Error conectando al servidor");
    return;
  }

  bool hay_sesiones = sesiones_resp["hay_sesiones"];

  if (hay_sesiones) {
    // ★ HAY SESIÓN ABIERTA: REGISTRAR SALIDA
    Serial.println("✓ Sesión abierta encontrada. Registrando SALIDA.\n");

    String sesion_id = sesiones_resp["sesion_id"].as<String>();
    horario_id = sesiones_resp["horario_id"].as<String>();

    // Obtener datos completos del horario para mostrar información
    JsonDocument horario_completo = obtenerHorarioCompleto(horario_id.c_str());
    
    if (horario_completo["existe"]) {
      String asignatura_id = horario_completo["asignatura_id"].as<String>();
      String grupo = horario_completo["grupo"].as<String>();
      String hora_inicio = horario_completo["hora_inicio"].as<String>();
      String hora_fin = horario_completo["hora_fin"].as<String>();
      
      // Obtener nombre de asignatura
      JsonDocument asignatura_datos = obtenerDatosAsignatura(asignatura_id.c_str());
      
      if (asignatura_datos["existe"]) {
        String nombre_asignatura = asignatura_datos["nombre"].as<String>();
        String codigo_asignatura = asignatura_datos["codigo"].as<String>();
        
        Serial.print("→ Sesión: ");
        Serial.print(nombre_asignatura);
        Serial.print(" (");
        Serial.print(codigo_asignatura);
        Serial.print(") - Grupo ");
        Serial.print(grupo);
        Serial.print(" - ");
        Serial.print(hora_inicio);
        Serial.print(" a ");
        Serial.println(hora_fin);
      }
    }

    // ★ OBTENER FECHA Y HORA AUTOMÁTICA
  String fecha_hora = obtenerFechaHoraActual();
  String fecha_str = fecha_hora.substring(0, 10); // YYYY-MM-DD
  String hora_str = fecha_hora.substring(11, 19);  // HH:MM:SS
  
  Serial.println("\n→ Fecha y hora actual obtenidas automáticamente");
  Serial.print("   Fecha: ");
  Serial.println(fecha_str);
  Serial.print("   Hora: ");
  Serial.println(hora_str);


    // Verificar huella
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

    Serial.println("✓ Identidad verificada\n");

    // Registrar salida
    Serial.println("→ Registrando asistencia...");
    
    horario_id = sesiones_resp["horario_id"].as<String>();
    String aula = sesiones_resp["aula"].as<String>();

    JsonDocument respuesta = registrarAsistenciaDocente(
      num_doc.c_str(),
      horario_id.c_str(),
      fecha.c_str(),
      hora_str.c_str(),
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

  } else {
    // ★ NO HAY SESIÓN: REGISTRAR ENTRADA
    Serial.println("✓ No hay sesión abierta. Registrando ENTRADA.\n");

    // Limpiar buffer
    while (Serial.available()) {
      Serial.read();
    }
    delay(200);

    Serial.println("¿Qué tipo de sesión?");
    Serial.println("1 -> Ordinaria (según horario)");
    Serial.println("2 -> Extraordinaria (fuera del horario)\n");

    unsigned long timeout = millis();
    char tipo_sesion_char = '0';

    while (millis() - timeout < 15000 && tipo_sesion_char == '0') {
      if (Serial.available()) {
        tipo_sesion_char = Serial.read();
      }
      delay(100);
    }

    if (tipo_sesion_char == '1') {
      // SESIÓN ORDINARIA
      procesarSesionOrdinaria(num_doc, fecha);
    } else if (tipo_sesion_char == '2') {
      // SESIÓN EXTRAORDINARIA
      procesarSesionExtraordinaria(num_doc, fecha);
    } else {
      Serial.println("✗ Opción inválida");
      return;
    }
  }
}


void procesarSesionOrdinaria(String num_doc, String fecha) {
  Serial.println("\n[SESIÓN ORDINARIA]\n");

  // Obtener día de la semana
  String dia_semana = obtenerDiaSemana(fecha);
  Serial.print("→ Día: ");
  Serial.println(dia_semana);

  // Obtener asignaturas para este día
  Serial.println("\n→ Asignaturas disponibles para este día:\n");

  JsonDocument asignaturas_resp = obtenerAsignaturasDocentePorDia(num_doc.c_str(), dia_semana.c_str());

  if (asignaturas_resp.containsKey("error") || !asignaturas_resp["existe"]) {
    Serial.println("✗ No hay asignaturas para este día");
    return;
  }

  JsonArray asignaturas = asignaturas_resp["asignaturas"].as<JsonArray>();

  if (asignaturas.size() == 0) {
    Serial.println("✗ No hay asignaturas para este día");
    return;
  }

  for (size_t i = 0; i < asignaturas.size(); i++) {
    Serial.print(i + 1);
    Serial.print(". ");
    Serial.print(asignaturas[i]["nombre"].as<String>());
    Serial.print(" (");
    Serial.print(asignaturas[i]["codigo"].as<String>());
    Serial.print(") - Grupo ");
    Serial.print(asignaturas[i]["grupo"].as<String>());
    Serial.print(" - ");
    Serial.print(asignaturas[i]["hora_inicio"].as<String>());
    Serial.print(" a ");
    Serial.println(asignaturas[i]["hora_fin"].as<String>());
  }

  // Limpiar buffer antes del menú
  while (Serial.available()) {
    Serial.read();
  }
  delay(200);

  Serial.print("\nSelecciona una asignatura (1-");
  Serial.print(asignaturas.size());
  Serial.println("):\n");

  unsigned long timeout = millis();
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
    return;
  }

  String horario_id = asignaturas[opcion_asignatura]["horario_id"].as<String>();
  String aula = asignaturas[opcion_asignatura]["aula"].as<String>();

  Serial.print("\n✓ Seleccionado: ");
  Serial.print(asignaturas[opcion_asignatura]["nombre"].as<String>());
  Serial.print(" - ");
  Serial.println(asignaturas[opcion_asignatura]["hora_inicio"].as<String>());

  // Solicitar aula
  Serial.println("\nIngresa el aula donde se realizará la clase (ej: A101):");
  String aula_ingresada = "";
  timeout = millis();

  while (millis() - timeout < 30000) {
    if (Serial.available()) {
      aula_ingresada = Serial.readStringUntil('\n');
      aula_ingresada.trim();
      
      if (aula_ingresada.length() > 0) {
        break;
      } else {
        Serial.println("✗ Aula inválida. Intenta de nuevo:");
      }
    }
    delay(100);
  }

  if (aula_ingresada.length() == 0) {
    Serial.println("✗ Timeout");
    return;
  }

  Serial.print("✓ Aula: ");
  Serial.println(aula_ingresada);

  // Verificar huella
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

  Serial.println("✓ Identidad verificada\n");

  // Registrar entrada
  Serial.println("→ Registrando asistencia...");
  
  // ★ OBTENER FECHA Y HORA AUTOMÁTICAMENTE
  String fecha_hora_actual = obtenerFechaHoraActual();
  String fecha_str = fecha_hora_actual.substring(0, 10);  // YYYY-MM-DD
  String hora_str = fecha_hora_actual.substring(11, 19);  // HH:MM:SS
  
  JsonDocument respuesta = registrarAsistenciaDocente(
    num_doc_usuario.c_str(),
    horario_id.c_str(),
    fecha_str.c_str(),
    hora_str.c_str(),
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

void procesarSesionExtraordinaria(String num_doc, String fecha) {
  Serial.println("\n[SESIÓN EXTRAORDINARIA]\n");

  // Obtener todas las asignaturas del docente
  Serial.println("→ Asignaturas disponibles:\n");

  JsonDocument asignaturas_resp = obtenerTodasAsignaturasDocente(num_doc.c_str());

  if (asignaturas_resp.containsKey("error") || !asignaturas_resp["existe"]) {
    Serial.println("✗ No hay asignaturas disponibles");
    return;
  }

  JsonArray asignaturas = asignaturas_resp["asignaturas"].as<JsonArray>();

  if (asignaturas.size() == 0) {
    Serial.println("✗ No hay asignaturas disponibles");
    return;
  }

  for (size_t i = 0; i < asignaturas.size(); i++) {
    Serial.print(i + 1);
    Serial.print(". ");
    Serial.print(asignaturas[i]["nombre"].as<String>());
    Serial.print(" (");
    Serial.print(asignaturas[i]["codigo"].as<String>());
    Serial.print(") - Grupo ");
    Serial.println(asignaturas[i]["grupo"].as<String>());
  }

  // Limpiar buffer antes del menú
  while (Serial.available()) {
    Serial.read();
  }
  delay(200);

  Serial.print("\nSelecciona una asignatura (1-");
  Serial.print(asignaturas.size());
  Serial.println("):\n");

  unsigned long timeout = millis();
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
    return;
  }

  String asignatura_id = asignaturas[opcion_asignatura]["asignatura_id"].as<String>();
  String asignatura_nombre = asignaturas[opcion_asignatura]["nombre"].as<String>();

  Serial.print("\n✓ Seleccionado: ");
  Serial.println(asignatura_nombre);

  // Obtener horarios para esta asignatura
  Serial.println("\n→ Horarios disponibles para esta asignatura:\n");

  JsonDocument horarios_resp = obtenerHorariosAsignaturaDocente(num_doc.c_str(), asignatura_id.c_str());

  if (horarios_resp.containsKey("error") || !horarios_resp["existe"]) {
    Serial.println("✗ No hay horarios disponibles");
    return;
  }

  JsonArray horarios = horarios_resp["horarios"].as<JsonArray>();

  if (horarios.size() == 0) {
    Serial.println("✗ No hay horarios disponibles");
    return;
  }

  for (size_t i = 0; i < horarios.size(); i++) {
    Serial.print(i + 1);
    Serial.print(". ");
    Serial.print(horarios[i]["dia_semana"].as<String>());
    Serial.print(" - ");
    Serial.print(horarios[i]["hora_inicio"].as<String>());
    Serial.print(" a ");
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
    return;
  }

  String horario_id = horarios[opcion_horario]["horario_id"].as<String>();
  String aula = horarios[opcion_horario]["aula"].as<String>();

  Serial.print("\n✓ Horario seleccionado: ");
  Serial.print(horarios[opcion_horario]["dia_semana"].as<String>());
  Serial.print(" - ");
  Serial.println(horarios[opcion_horario]["hora_inicio"].as<String>());

  // Solicitar aula
  Serial.println("\nIngresa el aula donde se realizará la clase (ej: A101):");
  String aula_ingresada = "";
  timeout = millis();

  while (millis() - timeout < 30000) {
    if (Serial.available()) {
      aula_ingresada = Serial.readStringUntil('\n');
      aula_ingresada.trim();
      
      if (aula_ingresada.length() > 0) {
        break;
      } else {
        Serial.println("✗ Aula inválida. Intenta de nuevo:");
      }
    }
    delay(100);
  }

  if (aula_ingresada.length() == 0) {
    Serial.println("✗ Timeout");
    return;
  }

  Serial.print("✓ Aula: ");
  Serial.println(aula_ingresada);


  // Verificar huella
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

  Serial.println("✓ Identidad verificada\n");

  // Registrar entrada
  Serial.println("→ Registrando asistencia...");
  
  // ★ OBTENER FECHA Y HORA AUTOMÁTICAMENTE
  String fecha_hora_actual = obtenerFechaHoraActual();
  String fecha_str = fecha_hora_actual.substring(0, 10);  // YYYY-MM-DD
  String hora_str = fecha_hora_actual.substring(11, 19);  // HH:MM:SS
  
  JsonDocument respuesta = registrarAsistenciaDocente(
    num_doc_usuario.c_str(),
    horario_id.c_str(),
    fecha_str.c_str(),
    hora_str.c_str(),
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

void procesarAsistenciaEstudiante(String num_doc, String fecha) {
  Serial.println("\n[ESTUDIANTE - ASISTENCIA]\n");

  // ★ PASO 1: OBTENER SESIONES ABIERTAS EN ESTA FECHA
  Serial.println("→ Buscando sesiones abiertas...");
  
  JsonDocument sesiones_resp = obtenerSesionesAbiertasPorFecha(fecha.c_str());

  if (sesiones_resp.containsKey("error")) {
    Serial.println("✗ Error conectando al servidor");
    return;
  }

  bool hay_sesiones = sesiones_resp["hay_sesiones"];

  if (!hay_sesiones) {
    Serial.println("✗ No hay sesiones abiertas disponibles");
    return;
  }

  JsonArray sesiones = sesiones_resp["sesiones"].as<JsonArray>();

  Serial.print("✓ Encontradas ");
  Serial.print(sesiones.size());
  Serial.println(" sesión(es) abierta(s)\n");

  // ★ PASO 2: FILTRAR SESIONES PARA LAS QUE EL ESTUDIANTE ESTÁ MATRICULADO
  Serial.println("→ Verificando matrículas...");

  // Crear documento para almacenar sesiones válidas
  JsonDocument doc_sesiones_validas;
  JsonArray sesiones_validas = doc_sesiones_validas.to<JsonArray>();

  for (size_t i = 0; i < sesiones.size(); i++) {
    String horario_id = sesiones[i]["horario_id"].as<String>();
    
    Serial.print("[DEBUG] Procesando sesion ");
    Serial.print(i + 1);
    Serial.print(" con horario_id: ");
    Serial.println(horario_id);
    
    // Obtener asignatura_id y grupo del horario
    JsonDocument horario_datos = obtenerDatosHorario(horario_id.c_str());

    if (!horario_datos["existe"]) {
      Serial.println("[DEBUG] Horario no encontrado");
      continue;
    }

    String asignatura_id = horario_datos["asignatura_id"].as<String>();
    String grupo = horario_datos["grupo"].as<String>();

    Serial.print("[DEBUG] Asignatura: ");
    Serial.print(asignatura_id);
    Serial.print(", Grupo: ");
    Serial.println(grupo);

    // Verificar si estudiante está matriculado
    JsonDocument matricula_resp = verificarMatriculaEstudiante(num_doc.c_str(), asignatura_id.c_str(), grupo.c_str());

    Serial.print("[DEBUG] Matriculado: ");
    Serial.println(matricula_resp["matriculado"] ? "true" : "false");

    if (matricula_resp["matriculado"]) {
      // Crear nuevo objeto para agregar a array
      JsonObject sesion_valida = sesiones_validas.add<JsonObject>();
      sesion_valida["sesion_id"] = sesiones[i]["sesion_id"].as<String>();
      sesion_valida["horario_id"] = horario_id;
      sesion_valida["asignatura_id"] = asignatura_id;
      sesion_valida["grupo"] = grupo;
      sesion_valida["aula"] = sesiones[i]["aula"].as<String>();
      sesion_valida["created_at"] = sesiones[i]["created_at"].as<String>();
      
      Serial.println("[DEBUG] Sesion agregada a sesiones_validas");
    }
  }

  if (sesiones_validas.size() == 0) {
    Serial.println("✗ No hay sesiones abiertas disponibles");
    return;
  }

  Serial.print("✓ Tienes acceso a ");
  Serial.print(sesiones_validas.size());
  Serial.println(" sesión(es)\n");

  // Obtener datos de la(s) sesión(es) válida(s) para mostrar
  // NOTA: Por ahora solo mostramos los datos que ya tenemos sin hacer llamadas HTTP adicionales
  // para optimizar tiempo. Si necesitas más detalles, descomentar las llamadas.
  
  for (size_t i = 0; i < sesiones_validas.size(); i++) {
    String asignatura_id = sesiones_validas[i]["asignatura_id"].as<String>();
    String grupo = sesiones_validas[i]["grupo"].as<String>();
    
    // Obtener nombre de asignatura SOLO UNA VEZ
    JsonDocument asignatura_datos = obtenerDatosAsignatura(asignatura_id.c_str());
    
    if (asignatura_datos["existe"]) {
      String nombre_asignatura = asignatura_datos["nombre"].as<String>();
      String codigo_asignatura = asignatura_datos["codigo"].as<String>();
      
      Serial.print("→ Sesión ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(nombre_asignatura);
      Serial.print(" (");
      Serial.print(codigo_asignatura);
      Serial.print(") - Grupo ");
      Serial.println(grupo);
    }
  }

  // ★ OBTENER FECHA Y HORA AUTOMÁTICA
  String fecha_hora = obtenerFechaHoraActual();
  String fecha_str = fecha_hora.substring(0, 10); // YYYY-MM-DD
  String hora_str = fecha_hora.substring(11, 19);  // HH:MM:SS
  
  Serial.println("\n→ Fecha y hora actual obtenidas automáticamente");
  Serial.print("   Fecha: ");
  Serial.println(fecha_str);
  Serial.print("   Hora: ");
  Serial.println(hora_str);

  // ★ PASO 4: PROCESAR CADA SESIÓN VÁLIDA
  // Si hay una única sesión, procesarla directamente
  // Si hay múltiples, procesar la más antigua para salida, o crear entrada

  if (sesiones_validas.size() == 1) {
    String sesion_id = sesiones_validas[0]["sesion_id"].as<String>();
    String horario_id = sesiones_validas[0]["horario_id"].as<String>();
    String aula = sesiones_validas[0]["aula"].as<String>();
    String asignatura_id = sesiones_validas[0]["asignatura_id"].as<String>();
    String grupo = sesiones_validas[0]["grupo"].as<String>();

    procesarRegistroEstudiante(num_doc, sesion_id, horario_id, aula, fecha, hora_str, asignatura_id, grupo);
  } else {
    // Múltiples sesiones: procesar la más antigua para salida
    // o crear entrada si ninguna tiene registro previo

    String sesion_mas_antigua = sesiones_validas[0]["sesion_id"].as<String>();
    String created_at_mas_antigua = sesiones_validas[0]["created_at"].as<String>();
    int indice_mas_antigua = 0;

    for (size_t i = 1; i < sesiones_validas.size(); i++) {
      String created_at = sesiones_validas[i]["created_at"].as<String>();
      if (created_at < created_at_mas_antigua) {
        sesion_mas_antigua = sesiones_validas[i]["sesion_id"].as<String>();
        created_at_mas_antigua = created_at;
        indice_mas_antigua = i;
      }
    }

    String horario_id = sesiones_validas[indice_mas_antigua]["horario_id"].as<String>();
    String aula = sesiones_validas[indice_mas_antigua]["aula"].as<String>();
    String asignatura_id = sesiones_validas[indice_mas_antigua]["asignatura_id"].as<String>();
    String grupo = sesiones_validas[indice_mas_antigua]["grupo"].as<String>();

    // Imprimir información de la sesión más antigua (para salida)
    JsonDocument horario_completo = obtenerHorarioCompleto(horario_id.c_str());
    
    if (horario_completo["existe"]) {
      String hora_inicio = horario_completo["hora_inicio"].as<String>();
      String hora_fin = horario_completo["hora_fin"].as<String>();
      
      // Obtener nombre de asignatura
      JsonDocument asignatura_datos = obtenerDatosAsignatura(asignatura_id.c_str());
      
      if (asignatura_datos["existe"]) {
        String nombre_asignatura = asignatura_datos["nombre"].as<String>();
        String codigo_asignatura = asignatura_datos["codigo"].as<String>();
        
        Serial.print("→ Sesión: ");
        Serial.print(nombre_asignatura);
        Serial.print(" (");
        Serial.print(codigo_asignatura);
        Serial.print(") - Grupo ");
        Serial.print(grupo);
        Serial.print(" - ");
        Serial.print(hora_inicio);
        Serial.print(" a ");
        Serial.println(hora_fin);
      }
    }

    Serial.println();
    procesarRegistroEstudiante(num_doc, sesion_mas_antigua, horario_id, aula, fecha, hora_str, asignatura_id, grupo);
  }
}


void procesarRegistroEstudiante(String num_doc, String sesion_id, String horario_id, String aula, String fecha, String hora, String asignatura_id, String grupo) {
  // ★ PASO 1: VERIFICAR SI TIENE REGISTRO PREVIO
  JsonDocument asistencia_resp = obtenerAsistenciaEstudiantePorSesion(num_doc.c_str(), sesion_id.c_str());

  String tipo_registro = "";
  String metodo_verificacion = "";

  if (asistencia_resp["existe"]) {
    // Tiene registro previo en estado "inasistencia" → SALIDA
    tipo_registro = "salida";
    metodo_verificacion = asistencia_resp["metodo_verificacion"].as<String>();

    Serial.println("\n→ Registro previo encontrado. Registrando SALIDA.");
    
    // Obtener datos del horario para mostrar información de la sesión
    JsonDocument horario_completo = obtenerHorarioCompleto(horario_id.c_str());
    
    if (horario_completo["existe"]) {
      String hora_inicio = horario_completo["hora_inicio"].as<String>();
      String hora_fin = horario_completo["hora_fin"].as<String>();
      
      // Obtener nombre de asignatura
      JsonDocument asignatura_datos = obtenerDatosAsignatura(asignatura_id.c_str());
      
      if (asignatura_datos["existe"]) {
        String nombre_asignatura = asignatura_datos["nombre"].as<String>();
        String codigo_asignatura = asignatura_datos["codigo"].as<String>();
        
        Serial.print("   Sesión: ");
        Serial.print(nombre_asignatura);
        Serial.print(" (");
        Serial.print(codigo_asignatura);
        Serial.print(") - Grupo ");
        Serial.print(grupo);
        Serial.print(" - ");
        Serial.print(hora_inicio);
        Serial.print(" a ");
        Serial.println(hora_fin);
      }
    }
    
    Serial.print("   Método de entrada: ");
    Serial.println(metodo_verificacion);

    // Si entrada fue supervisada, salida es automáticamente supervisada
    if (metodo_verificacion == "Supervisado") {
      // Obtener docente y pedir confirmación
      JsonDocument docente_resp = obtenerDocenteSesion(sesion_id.c_str());

      if (!docente_resp["existe"]) {
        Serial.println("✗ Error obteniendo datos del docente");
        return;
      }

      String nombre_docente = docente_resp["nombre_docente"].as<String>();
      
      Serial.print("\n→ ");
      Serial.print(nombre_docente);
      Serial.println(", ¿confirmas que el estudiante es quien dice ser?");
      Serial.println("(El registro de asistencia quedará bajo tu responsabilidad)\n");
      Serial.println("1 -> Confirmar");
      Serial.println("2 -> Cancelar\n");

      // Limpiar buffer
      while (Serial.available()) {
        Serial.read();
      }
      delay(200);

      unsigned long timeout_conf = millis();
      char confirmacion_char = '0';

      while (millis() - timeout_conf < 15000 && confirmacion_char == '0') {
        if (Serial.available()) {
          confirmacion_char = Serial.read();
        }
        delay(100);
      }

      if (confirmacion_char != '1') {
        Serial.println("✗ Registro cancelado por el docente");
        return;
      }

      Serial.println("✓ Confirmado por docente\n");
    } else {
      // Entrada fue biométrica, permitir elegir método para salida
      Serial.println("\n→ Selecciona método de registro:\n");
      Serial.println("1 -> Biométrico (Huella dactilar)");
      Serial.println("2 -> Supervisado (Por docente)\n");

      // Limpiar buffer
      while (Serial.available()) {
        Serial.read();
      }
      delay(200);

      unsigned long timeout = millis();
      char metodo_char = '0';

      while (millis() - timeout < 15000 && metodo_char == '0') {
        if (Serial.available()) {
          metodo_char = Serial.read();
        }
        delay(100);
      }

      if (metodo_char == '1') {
        metodo_verificacion = "Biometría";
        Serial.println("→ Método: Biométrico\n");
        
        // Verificar huella
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

      bool permitir_supervisado_entrada = false;
      int resultado_entrada = searchFingerprintWithRetries(sensor_id, permitir_supervisado_entrada);

      if (resultado_entrada == -1 && permitir_supervisado_entrada) {
        // Falló biometría 3 veces - ofrecer supervisado
        Serial.println("\n✗ La huella no corresponde al usuario o hay un problema en su verificación.");
        Serial.println("¿Desea hacer un registro supervisado?\n");
        Serial.println("1 -> Sí");
        Serial.println("2 -> No\n");

        // Limpiar buffer
        while (Serial.available()) {
          Serial.read();
        }
        delay(200);

        unsigned long timeout_sup = millis();
        char opcion_sup = '0';

        while (millis() - timeout_sup < 15000 && opcion_sup == '0') {
          if (Serial.available()) {
            opcion_sup = Serial.read();
          }
          delay(100);
        }

        if (opcion_sup == '1') {
          metodo_verificacion = "Supervisado";
          
          // Obtener docente y pedir confirmación
          JsonDocument docente_resp = obtenerDocenteSesion(sesion_id.c_str());

          if (!docente_resp["existe"]) {
            Serial.println("✗ Error obteniendo datos del docente");
            return;
          }

          String nombre_docente = docente_resp["nombre_docente"].as<String>();
          
          Serial.print("→ ");
          Serial.print(nombre_docente);
          Serial.println(", ¿confirmas que el estudiante es quien dice ser?");
          Serial.println("(El registro de asistencia quedará bajo tu responsabilidad)\n");
          Serial.println("1 -> Confirmar");
          Serial.println("2 -> Cancelar\n");

          // Limpiar buffer
          while (Serial.available()) {
            Serial.read();
          }
          delay(200);

          unsigned long timeout_conf = millis();
          char confirmacion_char = '0';

          while (millis() - timeout_conf < 15000 && confirmacion_char == '0') {
            if (Serial.available()) {
              confirmacion_char = Serial.read();
            }
            delay(100);
          }

          if (confirmacion_char != '1') {
            Serial.println("✗ Registro cancelado por el docente");
            return;
          }

          Serial.println("✓ Confirmado por docente\n");
        } else {
          Serial.println("✗ Registro cancelado");
          return;
        }
      } else if (resultado_entrada == -1) {
        Serial.println("✗ Verificación biométrica fallida");
        return;
      } else {
        Serial.println("✓ Identidad verificada\n");
      }

      } else if (metodo_char == '2') {
        metodo_verificacion = "Supervisado";
        Serial.println("→ Método: Supervisado\n");
        
        // Obtener docente y pedir confirmación
        JsonDocument docente_resp = obtenerDocenteSesion(sesion_id.c_str());

        if (!docente_resp["existe"]) {
          Serial.println("✗ Error obteniendo datos del docente");
          return;
        }

        String nombre_docente = docente_resp["nombre_docente"].as<String>();
        
        Serial.print("→ ");
        Serial.print(nombre_docente);
        Serial.println(", ¿confirmas que el estudiante es quien dice ser?");
        Serial.println("(El registro de asistencia quedará bajo tu responsabilidad)\n");
        Serial.println("1 -> Confirmar");
        Serial.println("2 -> Cancelar\n");

        // Limpiar buffer
        while (Serial.available()) {
          Serial.read();
        }
        delay(200);

        unsigned long timeout_conf = millis();
        char confirmacion_char = '0';

        while (millis() - timeout_conf < 15000 && confirmacion_char == '0') {
          if (Serial.available()) {
            confirmacion_char = Serial.read();
          }
          delay(100);
        }

        if (confirmacion_char != '1') {
          Serial.println("✗ Registro cancelado por el docente");
          return;
        }

        Serial.println("✓ Confirmado por docente\n");

      } else {
        Serial.println("✗ Opción inválida");
        return;
      }
    }
  } else {
    // No tiene registro → ENTRADA
    tipo_registro = "entrada";

    Serial.println("\n→ Registrando ENTRADA.");

    // Limpiar buffer
    while (Serial.available()) {
      Serial.read();
    }
    delay(200);

    Serial.println("\n→ Selecciona método de registro:\n");
    Serial.println("1 -> Biométrico (Huella dactilar)");
    Serial.println("2 -> Supervisado (Por docente)\n");

    unsigned long timeout = millis();
    char metodo_char = '0';

    while (millis() - timeout < 15000 && metodo_char == '0') {
      if (Serial.available()) {
        metodo_char = Serial.read();
      }
      delay(100);
    }

    if (metodo_char == '1') {
      metodo_verificacion = "Biometría";
      Serial.println("→ Método: Biométrico\n");
      
      // Verificar huella
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

    } else if (metodo_char == '2') {
      metodo_verificacion = "Supervisado";
      Serial.println("→ Método: Supervisado\n");
      
      // Obtener docente y pedir confirmación
      JsonDocument docente_resp = obtenerDocenteSesion(sesion_id.c_str());

      if (!docente_resp["existe"]) {
        Serial.println("✗ Error obteniendo datos del docente");
        return;
      }

      String nombre_docente = docente_resp["nombre_docente"].as<String>();
      
      Serial.print("→ ");
      Serial.print(nombre_docente);
      Serial.println(", ¿confirmas que el estudiante es quien dice ser?");
      Serial.println("(El registro de asistencia quedará bajo tu responsabilidad)\n");
      Serial.println("1 -> Confirmar");
      Serial.println("2 -> Cancelar\n");

      // Limpiar buffer
      while (Serial.available()) {
        Serial.read();
      }
      delay(200);

      unsigned long timeout_conf = millis();
      char confirmacion_char = '0';

      while (millis() - timeout_conf < 15000 && confirmacion_char == '0') {
        if (Serial.available()) {
          confirmacion_char = Serial.read();
        }
        delay(100);
      }

      if (confirmacion_char != '1') {
        Serial.println("✗ Registro cancelado por el docente");
        return;
      }

      Serial.println("✓ Confirmado por docente\n");

    } else {
      Serial.println("✗ Opción inválida");
      return;
    }
  }

  // ★ PASO FINAL: REGISTRAR ASISTENCIA
  Serial.println("→ Registrando asistencia...");
  
  JsonDocument respuesta = registrarAsistenciaEstudianteConMetodo(
    num_doc.c_str(),
    horario_id.c_str(),
    fecha.c_str(),
    hora.c_str(),
    tipo_registro.c_str(),
    metodo_verificacion.c_str()
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


bool verificarPinAdmin() {
  Serial.println("\n[VERIFICACIÓN DE ADMINISTRADOR]\n");
  Serial.println("Ingresa PIN de administrativo:");
  
  String pin = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 30000) {
    if (Serial.available()) {
      pin = Serial.readStringUntil('\n');
      pin.trim();
      
      if (pin.length() > 0) {
        break;
      }
    }
    delay(100);
  }
  
  if (pin.length() == 0) {
    Serial.println("✗ Timeout");
    return false;
  }
  
  Serial.println("→ Verificando PIN...");
  JsonDocument verificacion = verificarPinAdmin(pin.c_str());
  
  if (!verificacion["valido"]) {
    Serial.println("✗ PIN incorrecto");
    return false;
  }
  
  Serial.println("✓ Autenticado como administrador\n");
  return true;
}
