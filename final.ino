#include "biometric_sensor.h"
#include "wifi_backend.h"

unsigned long ultimo_sync = 0;
const unsigned long SYNC_INTERVAL = 30000;  // 30 segundos

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
  // ★ VERIFICAR COMANDOS PENDIENTES CADA 30 SEGUNDOS
  verificarComandosPendientes();
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

    JsonDocument respuesta_json = obtenerIdDisponible();

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
    procesarAsistenciaEstudiante(num_doc_usuario, fecha_str, hora_str);
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

    sesion_id = sesiones_resp["sesion_id"].as<String>();
    horario_id = sesiones_resp["horario_id"].as<String>();
    aula = sesiones_resp["aula"].as<String>();

    // ★ Toda la información ya viene en la respuesta (optimizada)
    String nombre_asignatura = sesiones_resp["asignatura_nombre"].as<String>();
    String codigo_asignatura = sesiones_resp["asignatura_codigo"].as<String>();
    String grupo = sesiones_resp["grupo"].as<String>();
    String hora_inicio = sesiones_resp["hora_inicio"].as<String>();
    String hora_fin = sesiones_resp["hora_fin"].as<String>();

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
      aula.c_str(),
      ""  // Para salida, el tipo ya está registrado en la sesión
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

  String dia_semana = obtenerDiaSemana(fecha);
  Serial.print("→ Día: ");
  Serial.println(dia_semana);

  Serial.println();

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

  int opcion_asignatura = -1;

  if (asignaturas.size() == 1) {
    opcion_asignatura = 0;
    Serial.print("✓ Sesión encontrada para entrada: ");
    Serial.print(asignaturas[0]["nombre"].as<String>());
    Serial.print(" (");
    Serial.print(asignaturas[0]["codigo"].as<String>());
    Serial.print(") - Grupo ");
    Serial.print(asignaturas[0]["grupo"].as<String>());
    Serial.print(" - ");
    Serial.print(asignaturas[0]["hora_inicio"].as<String>());
    Serial.print(" a ");
    Serial.println(asignaturas[0]["hora_fin"].as<String>());
  } else {
    Serial.println("→ Asignaturas disponibles para este día:\n");

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

    while (Serial.available()) {
      Serial.read();
    }
    delay(200);

    Serial.print("\nSelecciona una asignatura (1-");
    Serial.print(asignaturas.size());
    Serial.println("):\n");

    unsigned long timeout = millis();

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

    Serial.print("\n✓ Seleccionado: ");
    Serial.print(asignaturas[opcion_asignatura]["nombre"].as<String>());
    Serial.print(" - ");
    Serial.println(asignaturas[opcion_asignatura]["hora_inicio"].as<String>());
  }

  // ★ OBTENER HORA ACTUAL Y VALIDAR ANTES DE PEDIR AULA
  String fecha_hora_actual = obtenerFechaHoraActual();
  String hora_fin = asignaturas[opcion_asignatura]["hora_fin"].as<String>();
  String hora_actual = fecha_hora_actual.substring(11, 19);  // HH:MM:SS
  
  // Comparar horas (formato HH:MM:SS)
  if (hora_actual > hora_fin) {
    Serial.println("\n✗ El horario ordinario para esta sesión ha finalizado.");
    Serial.println("✓ Pero puedes realizar esta clase como sesión extraordinaria.\n");
    
    Serial.println("¿Deseas continuar como sesión extraordinaria?");
    Serial.println("1 -> Sí");
    Serial.println("2 -> No\n");

    while (Serial.available()) {
      Serial.read();
    }
    delay(200);

    unsigned long timeout = millis();
    char opcion_extraordinaria = '0';

    while (millis() - timeout < 15000 && opcion_extraordinaria == '0') {
      if (Serial.available()) {
        opcion_extraordinaria = Serial.read();
      }
      delay(100);
    }

    if (opcion_extraordinaria == '1') {
      // ★ CAMBIAR A SESIÓN EXTRAORDINARIA
      procesarSesionExtraordinaria(num_doc, fecha);
      return;
    } else {
      Serial.println("✗ Registro cancelado");
      return;
    }
  }

  String horario_id = asignaturas[opcion_asignatura]["horario_id"].as<String>();
  String aula = asignaturas[opcion_asignatura]["aula"].as<String>();

  Serial.println("\nIngresa el aula donde se realizará la clase (ej: A101):");
  
  while (Serial.available()) {
    Serial.read();
  }
  delay(200);
  
  String aula_ingresada = "";
  unsigned long timeout = millis();

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

  Serial.println("\n[REGISTRO DE ENTRADA]\n");
  Serial.println("¿Cómo deseas registrar tu entrada?");
  Serial.println("1 -> Biometría (huella)");
  Serial.println("2 -> PIN docente\n");

  while (Serial.available()) {
    Serial.read();
  }
  delay(200);

  timeout = millis();
  char metodo_char = '0';

  while (millis() - timeout < 15000 && metodo_char == '0') {
    if (Serial.available()) {
      metodo_char = Serial.read();
    }
    delay(100);
  }

  JsonDocument datos_usuario = obtenerDatosUsuario(num_doc.c_str());

  if (!datos_usuario.containsKey("existe") || !datos_usuario["existe"]) {
    Serial.println("✗ Error obteniendo datos del usuario");
    return;
  }

  int sensor_id = datos_usuario["sensor_id"];
  String metodo_entrada = ""; // ★ GUARDAR MÉTODO DE ENTRADA

  if (metodo_char == '1') {
    if (sensor_id == -1) {
      Serial.println("✗ El usuario no tiene huella registrada");
      return;
    }

    Serial.println();
    
    bool permitir_pin = false;
    int resultado = searchFingerprintWithRetries(sensor_id, permitir_pin);

    if (resultado == -1 && permitir_pin) {
      Serial.println("¿Deseas usar PIN en su lugar?\n");
      Serial.println("1 -> Sí");
      Serial.println("2 -> No\n");

      while (Serial.available()) {
        Serial.read();
      }
      delay(200);

      timeout = millis();
      char opcion_pin = '0';

      while (millis() - timeout < 15000 && opcion_pin == '0') {
        if (Serial.available()) {
          opcion_pin = Serial.read();
        }
        delay(100);
      }

      if (opcion_pin == '1') {
        metodo_char = '2';
      } else {
        Serial.println("✗ Verificación cancelada");
        return;
      }
    } else if (resultado == -1) {
      Serial.println("✗ Verificación biométrica fallida");
      return;
    } else {
      int confianza = getFingerprintConfidence();
      Serial.print("✓ ¡Huella coincide! | Confianza: ");
      Serial.println(confianza);
      Serial.println("✓ Identidad verificada\n");
      metodo_entrada = "Biometría";
    }
  }

  if (metodo_char == '2') {
    Serial.println();
    
    while (Serial.available()) {
      Serial.read();
    }
    delay(300);
    
    // ★ 2 INTENTOS DE PIN
    int intentos_pin = 0;
    const int MAX_INTENTOS_PIN = 2;
    bool pin_valido_entrada = false;

    while (intentos_pin < MAX_INTENTOS_PIN && !pin_valido_entrada) {
      intentos_pin++;
      Serial.print("Intento ");
      Serial.print(intentos_pin);
      Serial.print("/");
      Serial.println(MAX_INTENTOS_PIN);
      Serial.println("Ingresa tu PIN (4 dígitos):");
      
      String pin = "";
      timeout = millis();
      bool pin_completo = false;

      while (millis() - timeout < 30000 && !pin_completo) {
        if (Serial.available()) {
          char c = Serial.read();
          
          if (c == '\n') {
            if (pin.length() == 4) {
              pin_completo = true;
              break;
            } else if (pin.length() > 0) {
              Serial.print("✗ PIN inválido (debe ser 4 dígitos, ingresaste ");
              Serial.print(pin.length());
              Serial.println("):");
              pin = "";
            }
          } else if (c >= '0' && c <= '9') {
            pin += c;
            Serial.print("*");
          }
        }
        delay(50);
      }

      if (pin.length() != 4) {
        Serial.println("\n✗ Timeout");
        return;
      }

      Serial.println();
      Serial.println("→ Verificando PIN...");
      JsonDocument verificacion = verificarPinDocente(num_doc.c_str(), pin.c_str());

      if (!verificacion["valido"]) {
        Serial.println("✗ PIN incorrecto");
        if (intentos_pin < MAX_INTENTOS_PIN) {
          Serial.println();
        }
      } else {
        Serial.println("✓ PIN verificado correctamente\n");
        pin_valido_entrada = true;
        metodo_entrada = "PIN";
      }
    }

    if (!pin_valido_entrada) {
      Serial.println("✗ Se agotaron los intentos de PIN");
      return;
    }
  } else if (metodo_char != '1') {
    Serial.println("✗ Opción inválida");
    return;
  }

  // ★ fecha_hora_actual ya fue obtenida antes
  String fecha_str = fecha_hora_actual.substring(0, 10);
  String hora_str = fecha_hora_actual.substring(11, 19);
  
  Serial.println("→ Registrando asistencia...");
  
  JsonDocument respuesta = registrarAsistenciaDocente(
    num_doc_usuario.c_str(),
    horario_id.c_str(),
    fecha_str.c_str(),
    hora_str.c_str(),
    aula_ingresada.c_str(),
    "ordinaria"
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
    
    // ★ GUARDAR SESIÓN Y MÉTODO PARA SALIDA POSTERIOR
    // (Estos serían variables globales que se usan en procesarAsistenciaDocente cuando hay sesión abierta)
  } else {
    String error_msg = respuesta["detail"].as<String>();
    Serial.print("✗ Error: ");
    Serial.println(error_msg);
  }
}

void procesarSesionExtraordinaria(String num_doc, String fecha) {
  Serial.println("\n[SESIÓN EXTRAORDINARIA]\n");

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

  Serial.print("\n✓ Seleccionado: ");
  Serial.println(asignaturas[opcion_asignatura]["nombre"].as<String>());

  JsonDocument horarios_resp = obtenerHorariosAsignaturaDocente(num_doc.c_str(), asignatura_id.c_str());

  if (horarios_resp.containsKey("error") || !horarios_resp["existe"]) {
    Serial.println("✗ No hay horarios para esta asignatura");
    return;
  }

  JsonArray horarios = horarios_resp["horarios"].as<JsonArray>();

  if (horarios.size() == 0) {
    Serial.println("✗ No hay horarios para esta asignatura");
    return;
  }

  Serial.println("\n→ Horarios disponibles:\n");

  for (size_t i = 0; i < horarios.size(); i++) {
    Serial.print(i + 1);
    Serial.print(". ");
    
    // ★ MOSTRAR DÍA Y HORARIO (sin grupo, ya se seleccionó)
    String dia_semana_horario = horarios[i]["dia_semana"].as<String>();
    String hora_inicio = horarios[i]["hora_inicio"].as<String>();
    String hora_fin = horarios[i]["hora_fin"].as<String>();
    
    Serial.print(dia_semana_horario);
    Serial.print(" - ");
    Serial.print(hora_inicio);
    Serial.print(" a ");
    Serial.println(hora_fin);
  }

  while (Serial.available()) {
    Serial.read();
  }
  delay(200);

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

  Serial.print("\n✓ Horario seleccionado: ");
  Serial.print(horarios[opcion_horario]["hora_inicio"].as<String>());
  Serial.print(" a ");
  Serial.println(horarios[opcion_horario]["hora_fin"].as<String>());

  Serial.println("\nIngresa el aula donde se realizará la clase (ej: A101):");
  
  while (Serial.available()) {
    Serial.read();
  }
  delay(200);
  
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

  Serial.println("\n[REGISTRO DE ENTRADA]\n");
  Serial.println("¿Cómo deseas registrar tu entrada?");
  Serial.println("1 -> Biometría (huella)");
  Serial.println("2 -> PIN docente\n");

  while (Serial.available()) {
    Serial.read();
  }
  delay(200);

  timeout = millis();
  char metodo_char = '0';

  while (millis() - timeout < 15000 && metodo_char == '0') {
    if (Serial.available()) {
      metodo_char = Serial.read();
    }
    delay(100);
  }

  JsonDocument datos_usuario = obtenerDatosUsuario(num_doc.c_str());

  if (!datos_usuario.containsKey("existe") || !datos_usuario["existe"]) {
    Serial.println("✗ Error obteniendo datos del usuario");
    return;
  }

  int sensor_id = datos_usuario["sensor_id"];
  String metodo_entrada = ""; // ★ GUARDAR MÉTODO DE ENTRADA

  if (metodo_char == '1') {
    if (sensor_id == -1) {
      Serial.println("✗ El usuario no tiene huella registrada");
      return;
    }

    Serial.println();
    
    bool permitir_pin = false;
    int resultado = searchFingerprintWithRetries(sensor_id, permitir_pin);

    if (resultado == -1 && permitir_pin) {
      Serial.println("¿Deseas usar PIN en su lugar?\n");
      Serial.println("1 -> Sí");
      Serial.println("2 -> No\n");

      while (Serial.available()) {
        Serial.read();
      }
      delay(200);

      timeout = millis();
      char opcion_pin = '0';

      while (millis() - timeout < 15000 && opcion_pin == '0') {
        if (Serial.available()) {
          opcion_pin = Serial.read();
        }
        delay(100);
      }

      if (opcion_pin == '1') {
        metodo_char = '2';
      } else {
        Serial.println("✗ Verificación cancelada");
        return;
      }
    } else if (resultado == -1) {
      Serial.println("✗ Verificación biométrica fallida");
      return;
    } else {
      int confianza = getFingerprintConfidence();
      Serial.print("✓ ¡Huella coincide! | Confianza: ");
      Serial.println(confianza);
      Serial.println("✓ Identidad verificada\n");
      metodo_entrada = "Biometría";
    }
  }

  if (metodo_char == '2') {
    Serial.println();
    
    while (Serial.available()) {
      Serial.read();
    }
    delay(300);
    
    // ★ 2 INTENTOS DE PIN
    int intentos_pin = 0;
    const int MAX_INTENTOS_PIN = 2;
    bool pin_valido_entrada = false;

    while (intentos_pin < MAX_INTENTOS_PIN && !pin_valido_entrada) {
      intentos_pin++;
      Serial.print("Intento ");
      Serial.print(intentos_pin);
      Serial.print("/");
      Serial.println(MAX_INTENTOS_PIN);
      Serial.println("Ingresa tu PIN (4 dígitos):");
      
      String pin = "";
      timeout = millis();
      bool pin_completo = false;

      while (millis() - timeout < 30000 && !pin_completo) {
        if (Serial.available()) {
          char c = Serial.read();
          
          if (c == '\n') {
            if (pin.length() == 4) {
              pin_completo = true;
              break;
            } else if (pin.length() > 0) {
              Serial.print("✗ PIN inválido (debe ser 4 dígitos, ingresaste ");
              Serial.print(pin.length());
              Serial.println("):");
              pin = "";
            }
          } else if (c >= '0' && c <= '9') {
            pin += c;
            Serial.print("*");
          }
        }
        delay(50);
      }

      if (pin.length() != 4) {
        Serial.println("\n✗ Timeout");
        return;
      }

      Serial.println();
      Serial.println("→ Verificando PIN...");
      JsonDocument verificacion = verificarPinDocente(num_doc.c_str(), pin.c_str());

      if (!verificacion["valido"]) {
        Serial.println("✗ PIN incorrecto");
        if (intentos_pin < MAX_INTENTOS_PIN) {
          Serial.println();
        }
      } else {
        Serial.println("✓ PIN verificado correctamente\n");
        pin_valido_entrada = true;
        metodo_entrada = "PIN";
      }
    }

    if (!pin_valido_entrada) {
      Serial.println("✗ Se agotaron los intentos de PIN");
      return;
    }
  } else if (metodo_char != '1') {
    Serial.println("✗ Opción inválida");
    return;
  }

  String fecha_hora_actual = obtenerFechaHoraActual();
  String fecha_str = fecha_hora_actual.substring(0, 10);
  String hora_str = fecha_hora_actual.substring(11, 19);
  
  Serial.println("→ Registrando asistencia...");
  
  JsonDocument respuesta = registrarAsistenciaDocente(
    num_doc_usuario.c_str(),
    horario_id.c_str(),
    fecha_str.c_str(),
    hora_str.c_str(),
    aula_ingresada.c_str(),
    "extraordinaria"
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

void procesarAsistenciaEstudiante(String num_doc, String fecha, String hora) {
  Serial.println("\n[ESTUDIANTE - ASISTENCIA]\n");

  // ★ PASO 1: BUSCAR SESIONES DISPONIBLES PARA ENTRADA
  Serial.println("→ Buscando sesiones disponibles para entrada...");
  JsonDocument sesiones_entrada = obtenerSesionesDisponiblesParaEntrada(num_doc.c_str(), fecha.c_str());

  if (sesiones_entrada["encontradas"]) {
    int cantidad = sesiones_entrada["cantidad"];
    JsonArray sesiones = sesiones_entrada["sesiones"];

    int opcion = -1;

    // ★ SI HAY SOLO 1 SESIÓN, AUTO-SELECCIONAR
    if (cantidad == 1) {
      opcion = 0;
      Serial.print("✓ Sesión encontrada para entrada: ");
      Serial.print(sesiones[0]["asignatura_nombre"].as<String>());
      Serial.print(" (");
      Serial.print(sesiones[0]["asignatura_codigo"].as<String>());
      Serial.print(") - Grupo ");
      Serial.println(sesiones[0]["grupo"].as<String>());
    } else {
      // ★ SI HAY MÚLTIPLES SESIONES, MOSTRAR MENÚ
      Serial.print("✓ Encontradas ");
      Serial.print(cantidad);
      Serial.println(" sesión(es) para entrada\n");

      for (size_t i = 0; i < sesiones.size(); i++) {
        Serial.print("→ Sesión ");
        Serial.print(i + 1);
        Serial.print(": ");
        Serial.print(sesiones[i]["asignatura_nombre"].as<String>());
        Serial.print(" (");
        Serial.print(sesiones[i]["asignatura_codigo"].as<String>());
        Serial.print(") - Grupo ");
        Serial.println(sesiones[i]["grupo"].as<String>());
      }

      Serial.println("\n¿En cuál deseas registrar entrada? (escribe el número):");

      while (Serial.available()) {
        Serial.read();
      }
      delay(200);

      unsigned long timeout = millis();

      while (millis() - timeout < 20000 && opcion == -1) {
        if (Serial.available()) {
          String input = Serial.readStringUntil('\n');
          input.trim();
          int opcion_temp = input.toInt();
          
          if (opcion_temp > 0 && opcion_temp <= cantidad) {
            opcion = opcion_temp - 1;
          } else {
            Serial.println("✗ Opción inválida");
          }
        }
        delay(100);
      }

      if (opcion == -1) {
        Serial.println("✗ Timeout");
        return;
      }

      Serial.println("\n✓ Sesión seleccionada");
    }

    JsonObject sesion_seleccionada = sesiones[opcion];
    String sesion_id = sesion_seleccionada["sesion_id"].as<String>();
    String horario_id = sesion_seleccionada["horario_id"].as<String>();
    String aula = sesion_seleccionada["aula"].as<String>();
    String grupo = sesion_seleccionada["grupo"].as<String>();

    Serial.println();

    // ★ PROCESAR ENTRADA Y CAPTURAR EL MÉTODO USADO
    String metodo_entrada = procesarRegistroEstudianteEntrada(num_doc, sesion_id, horario_id, aula, fecha, hora);
    
    // Si entrada fue exitosa, metodo_entrada no estará vacío
    if (metodo_entrada.length() > 0) {
      Serial.println("\nPuedes registrar tu salida cuando termines la clase.\n");
    }
    
    return;
  }

  // ★ PASO 2: SI NO HAY PARA ENTRADA, BUSCAR PARA SALIDA
  Serial.println("✗ No hay sesiones disponibles para entrada");
  Serial.println("\n→ Buscando sesiones abiertas para salida...");

  JsonDocument sesion_salida = obtenerSesionParaSalida(num_doc.c_str());

  if (sesion_salida["encontrada"]) {
    Serial.print("✓ Sesión encontrada para salida: ");
    Serial.print(sesion_salida["asignatura_nombre"].as<String>());
    Serial.print(" - Grupo ");
    Serial.println(sesion_salida["grupo"].as<String>());
    Serial.println();

    String sesion_id = sesion_salida["sesion_id"].as<String>();
    String horario_id = sesion_salida["horario_id"].as<String>();

    // ★ EN SALIDA SIN MÉTODO DE ENTRADA PREVIO, PASAR STRING VACÍA
    procesarRegistroEstudianteSalida(num_doc, sesion_id, horario_id, fecha, hora);
    return;
  }

  Serial.println("✗ No hay sesiones disponibles");
}

String procesarRegistroEstudianteEntrada(String num_doc, String sesion_id, String horario_id, String aula, String fecha, String hora) {
  Serial.println("[REGISTRO DE ENTRADA]\n");

  Serial.println("¿Cómo deseas registrar tu entrada?");
  Serial.println("1 -> Biometría (huella)");
  Serial.println("2 -> Supervisado\n");

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

  String metodo_verificacion = "";

  if (metodo_char == '1') {
    metodo_verificacion = "Biometría";

    JsonDocument datos_usuario = obtenerDatosUsuario(num_doc.c_str());

    if (!datos_usuario["existe"]) {
      Serial.println("✗ Error obteniendo datos del usuario");
      return "";
    }

    uint16_t sensor_id = datos_usuario["sensor_id"];

    if (sensor_id == -1) {
      Serial.println("✗ Usuario no tiene huella registrada");
      return "";
    }

    bool permitir_supervisado = false;
    int resultado = searchFingerprintWithRetries(sensor_id, permitir_supervisado);

    if (resultado == -1 && permitir_supervisado) {
      Serial.println("¿Deseas registrar supervisado?\n");
      Serial.println("1 -> Sí");
      Serial.println("2 -> No\n");

      while (Serial.available()) {
        Serial.read();
      }
      delay(200);

      timeout = millis();
      char opcion_sup = '0';

      while (millis() - timeout < 15000 && opcion_sup == '0') {
        if (Serial.available()) {
          opcion_sup = Serial.read();
        }
        delay(100);
      }

      if (opcion_sup == '1') {
        metodo_verificacion = "Supervisado";

        // ★ OBTENER NUM_DOC DEL DOCENTE DESDE LA SESIÓN
        JsonDocument docente_num_resp = obtenerUsuarioDocentePorSesion(sesion_id.c_str());

        if (!docente_num_resp["existe"]) {
          Serial.println("✗ Error obteniendo datos del docente");
          return "";
        }

        String num_doc_docente = docente_num_resp["num_doc"].as<String>();

        JsonDocument docente_resp = obtenerNombreDocentePorSesion(sesion_id.c_str());

        if (!docente_resp["existe"]) {
          Serial.println("✗ Error obteniendo datos del docente");
          return "";
        }

        String nombre_docente = docente_resp["nombre_docente"].as<String>();

        Serial.print("→ ");
        Serial.print(nombre_docente);
        Serial.println(", este registro quedará bajo tu responsabilidad.");
        Serial.println("Para confirmarlo por favor ingresa tu PIN de acceso.");
        Serial.println("Para cancelarlo ingresa cualquier letra.\n");

        while (Serial.available()) {
          Serial.read();
        }
        delay(300);

        timeout = millis();
        String confirmacion_pin = "";
        bool entrada_valida = false;

        while (millis() - timeout < 30000 && !entrada_valida) {
          if (Serial.available()) {
            char c = Serial.read();
            
            if (c == '\n') {
              if (confirmacion_pin.length() > 0) {
                entrada_valida = true;
              }
            } else if (c != '\r') {
              confirmacion_pin += c;
              if (confirmacion_pin.length() <= 4) {
                Serial.print("*");
              }
            }
          }
          delay(50);
        }

        Serial.println();

        bool es_digitos = true;
        for (int i = 0; i < confirmacion_pin.length(); i++) {
          if (confirmacion_pin[i] < '0' || confirmacion_pin[i] > '9') {
            es_digitos = false;
            break;
          }
        }

        if (!es_digitos || confirmacion_pin.length() != 4) {
          Serial.println("✗ Registro cancelado");
          return "";
        }

        Serial.println("→ Verificando PIN...");
        JsonDocument verificacion_pin = verificarPinDocente(num_doc_docente.c_str(), confirmacion_pin.c_str());

        if (!verificacion_pin["valido"]) {
          Serial.println("✗ PIN incorrecto. Registro cancelado.");
          return "";
        }

        Serial.println("✓ PIN verificado. Registro confirmado.\n");
      } else {
        Serial.println("✗ Registro cancelado");
        return "";
      }
    } else if (resultado == -1) {
      Serial.println("✗ Verificación biométrica fallida");
      return "";
    } else {
      int confianza = getFingerprintConfidence();
      Serial.print("✓ ¡Huella coincide! | Confianza: ");
      Serial.println(confianza);
      Serial.println("✓ Identidad verificada\n");
    }

  } else if (metodo_char == '2') {
    metodo_verificacion = "Supervisado";

    // ★ OBTENER NUM_DOC DEL DOCENTE DESDE LA SESIÓN
    JsonDocument docente_num_resp = obtenerUsuarioDocentePorSesion(sesion_id.c_str());

    if (!docente_num_resp["existe"]) {
      Serial.println("✗ Error obteniendo datos del docente");
      return "";
    }

    String num_doc_docente = docente_num_resp["num_doc"].as<String>();

    JsonDocument docente_resp = obtenerNombreDocentePorSesion(sesion_id.c_str());

    if (!docente_resp["existe"]) {
      Serial.println("✗ Error obteniendo datos del docente");
      return "";
    }

    String nombre_docente = docente_resp["nombre_docente"].as<String>();

    Serial.print("→ ");
    Serial.print(nombre_docente);
    Serial.println(", este registro quedará bajo tu responsabilidad.");
    Serial.println("Para confirmarlo por favor ingresa tu PIN de acceso.");
    Serial.println("Para cancelarlo ingresa cualquier letra.\n");

    while (Serial.available()) {
      Serial.read();
    }
    delay(300);

    timeout = millis();
    String confirmacion_pin = "";
    bool entrada_valida = false;

    while (millis() - timeout < 30000 && !entrada_valida) {
      if (Serial.available()) {
        char c = Serial.read();
        
        if (c == '\n') {
          if (confirmacion_pin.length() > 0) {
            entrada_valida = true;
          }
        } else if (c != '\r') {
          confirmacion_pin += c;
          if (confirmacion_pin.length() <= 4) {
            Serial.print("*");
          }
        }
      }
      delay(50);
    }

    Serial.println();

    bool es_digitos = true;
    for (int i = 0; i < confirmacion_pin.length(); i++) {
      if (confirmacion_pin[i] < '0' || confirmacion_pin[i] > '9') {
        es_digitos = false;
        break;
      }
    }

    if (!es_digitos || confirmacion_pin.length() != 4) {
      Serial.println("✗ Registro cancelado");
      return "";
    }

    Serial.println("→ Verificando PIN...");
    JsonDocument verificacion_pin = verificarPinDocente(num_doc_docente.c_str(), confirmacion_pin.c_str());

    if (!verificacion_pin["valido"]) {
      Serial.println("✗ PIN incorrecto. Registro cancelado.");
      return "";
    }

    Serial.println("✓ PIN verificado. Registro confirmado.\n");
  } else {
    Serial.println("✗ Opción inválida");
    return "";
  }

  Serial.println("→ Registrando entrada...");
  JsonDocument respuesta = registrarAsistenciaEstudianteConMetodo(
    num_doc.c_str(),
    horario_id.c_str(),
    fecha.c_str(),
    hora.c_str(),
    "entrada",
    metodo_verificacion.c_str()
  );

  if (respuesta["exito"]) {
    Serial.println("✓ Entrada registrada exitosamente\n");
    return metodo_verificacion;
  } else {
    Serial.println("✗ Error registrando entrada");
    return "";
  }
}

void procesarRegistroEstudianteSalida(String num_doc, String sesion_id, String horario_id, String fecha, String hora) {
  Serial.println("[REGISTRO DE SALIDA]\n");

  Serial.println("→ Verificando método de entrada...");
  JsonDocument metodo_resp = obtenerMetodoEntradaParaSalida(num_doc.c_str(), sesion_id.c_str());

  if (!metodo_resp["existe"]) {
    Serial.println("✗ Error obteniendo método de entrada");
    return;
  }

  String metodo_entrada = metodo_resp["metodo_verificacion"].as<String>();
  Serial.print("✓ Método de entrada: ");
  Serial.println(metodo_entrada);
  Serial.println();

  String metodo_verificacion = "";

  if (metodo_entrada == "Supervisado") {
    // ★ SI ENTRADA FUE SUPERVISADA, SALIDA TAMBIÉN DEBE SERLO
    metodo_verificacion = "Supervisado";

    JsonDocument docente_num_resp = obtenerUsuarioDocentePorSesion(sesion_id.c_str());

    if (!docente_num_resp["existe"]) {
      Serial.println("✗ Error obteniendo datos del docente");
      return;
    }

    String num_doc_docente = docente_num_resp["num_doc"].as<String>();

    JsonDocument docente_resp = obtenerNombreDocentePorSesion(sesion_id.c_str());

    if (!docente_resp["existe"]) {
      Serial.println("✗ Error obteniendo datos del docente");
      return;
    }

    String nombre_docente = docente_resp["nombre_docente"].as<String>();

    Serial.print("→ ");
    Serial.print(nombre_docente);
    Serial.println(", este registro quedará bajo tu responsabilidad.");
    Serial.println("Para confirmarlo por favor ingresa tu PIN de acceso.");
    Serial.println("Para cancelarlo ingresa cualquier letra.\n");

    while (Serial.available()) {
      Serial.read();
    }
    delay(300);

    unsigned long timeout = millis();
    String confirmacion_pin = "";
    bool entrada_valida = false;

    while (millis() - timeout < 30000 && !entrada_valida) {
      if (Serial.available()) {
        char c = Serial.read();
        
        if (c == '\n') {
          if (confirmacion_pin.length() > 0) {
            entrada_valida = true;
          }
        } else if (c != '\r') {
          confirmacion_pin += c;
          if (confirmacion_pin.length() <= 4) {
            Serial.print("*");
          }
        }
      }
      delay(50);
    }

    Serial.println();

    bool es_digitos = true;
    for (int i = 0; i < confirmacion_pin.length(); i++) {
      if (confirmacion_pin[i] < '0' || confirmacion_pin[i] > '9') {
        es_digitos = false;
        break;
      }
    }

    if (!es_digitos || confirmacion_pin.length() != 4) {
      Serial.println("✗ Registro cancelado");
      return;
    }

    Serial.println("→ Verificando PIN...");
    JsonDocument verificacion_pin = verificarPinDocente(num_doc_docente.c_str(), confirmacion_pin.c_str());

    if (!verificacion_pin["valido"]) {
      Serial.println("✗ PIN incorrecto. Registro cancelado.");
      return;
    }

    Serial.println("✓ PIN verificado. Registro confirmado.\n");

  } else if (metodo_entrada == "Biometría") {
    // ★ SI ENTRADA FUE BIOMÉTRICA, OFRECER OPCIONES EN SALIDA
    Serial.println("¿Cómo deseas registrar tu salida?");
    Serial.println("1 -> Biometría (huella)");
    Serial.println("2 -> Supervisado\n");

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

      JsonDocument datos_usuario = obtenerDatosUsuario(num_doc.c_str());

      if (!datos_usuario["existe"]) {
        Serial.println("✗ Error obteniendo datos del usuario");
        return;
      }

      uint16_t sensor_id = datos_usuario["sensor_id"];

      if (sensor_id == -1) {
        Serial.println("✗ Usuario no tiene huella registrada");
        return;
      }

      bool permitir_supervisado = false;
      int resultado = searchFingerprintWithRetries(sensor_id, permitir_supervisado);

      if (resultado == -1 && permitir_supervisado) {
        Serial.println("¿Deseas hacer un registro supervisado?\n");
        Serial.println("1 -> Sí");
        Serial.println("2 -> No\n");

        while (Serial.available()) {
          Serial.read();
        }
        delay(200);

        timeout = millis();
        char opcion_sup = '0';

        while (millis() - timeout < 15000 && opcion_sup == '0') {
          if (Serial.available()) {
            opcion_sup = Serial.read();
          }
          delay(100);
        }

        if (opcion_sup == '1') {
          metodo_verificacion = "Supervisado";

          JsonDocument docente_num_resp = obtenerUsuarioDocentePorSesion(sesion_id.c_str());

          if (!docente_num_resp["existe"]) {
            Serial.println("✗ Error obteniendo datos del docente");
            return;
          }

          String num_doc_docente = docente_num_resp["num_doc"].as<String>();

          JsonDocument docente_resp = obtenerNombreDocentePorSesion(sesion_id.c_str());

          if (!docente_resp["existe"]) {
            Serial.println("✗ Error obteniendo datos del docente");
            return;
          }

          String nombre_docente = docente_resp["nombre_docente"].as<String>();

          Serial.print("→ ");
          Serial.print(nombre_docente);
          Serial.println(", este registro quedará bajo tu responsabilidad.");
          Serial.println("Para confirmarlo por favor ingresa tu PIN de acceso.");
          Serial.println("Para cancelarlo ingresa cualquier letra.\n");

          while (Serial.available()) {
            Serial.read();
          }
          delay(300);

          timeout = millis();
          String confirmacion_pin = "";
          bool entrada_valida = false;

          while (millis() - timeout < 30000 && !entrada_valida) {
            if (Serial.available()) {
              char c = Serial.read();
              
              if (c == '\n') {
                if (confirmacion_pin.length() > 0) {
                  entrada_valida = true;
                }
              } else if (c != '\r') {
                confirmacion_pin += c;
                if (confirmacion_pin.length() <= 4) {
                  Serial.print("*");
                }
              }
            }
            delay(50);
          }

          Serial.println();

          bool es_digitos = true;
          for (int i = 0; i < confirmacion_pin.length(); i++) {
            if (confirmacion_pin[i] < '0' || confirmacion_pin[i] > '9') {
              es_digitos = false;
              break;
            }
          }

          if (!es_digitos || confirmacion_pin.length() != 4) {
            Serial.println("✗ Registro cancelado");
            return;
          }

          Serial.println("→ Verificando PIN...");
          JsonDocument verificacion_pin = verificarPinDocente(num_doc_docente.c_str(), confirmacion_pin.c_str());

          if (!verificacion_pin["valido"]) {
            Serial.println("✗ PIN incorrecto. Registro cancelado.");
            return;
          }

          Serial.println("✓ PIN verificado. Registro confirmado.\n");
        } else {
          Serial.println("✗ Registro cancelado");
          return;
        }
      } else if (resultado == -1) {
        Serial.println("✗ Verificación biométrica fallida");
        return;
      } else {
        int confianza = getFingerprintConfidence();
        Serial.print("✓ ¡Huella coincide! | Confianza: ");
        Serial.println(confianza);
        Serial.println("✓ Identidad verificada\n");
      }

    } else if (metodo_char == '2') {
      metodo_verificacion = "Supervisado";

      JsonDocument docente_num_resp = obtenerUsuarioDocentePorSesion(sesion_id.c_str());

      if (!docente_num_resp["existe"]) {
        Serial.println("✗ Error obteniendo datos del docente");
        return;
      }

      String num_doc_docente = docente_num_resp["num_doc"].as<String>();

      JsonDocument docente_resp = obtenerNombreDocentePorSesion(sesion_id.c_str());

      if (!docente_resp["existe"]) {
        Serial.println("✗ Error obteniendo datos del docente");
        return;
      }

      String nombre_docente = docente_resp["nombre_docente"].as<String>();

      Serial.print("→ ");
      Serial.print(nombre_docente);
      Serial.println(", este registro quedará bajo tu responsabilidad.");
      Serial.println("Para confirmarlo por favor ingresa tu PIN de acceso.");
      Serial.println("Para cancelarlo ingresa cualquier letra.\n");

      while (Serial.available()) {
        Serial.read();
      }
      delay(300);

      unsigned long timeout = millis();
      String confirmacion_pin = "";
      bool entrada_valida = false;

      while (millis() - timeout < 30000 && !entrada_valida) {
        if (Serial.available()) {
          char c = Serial.read();
          
          if (c == '\n') {
            if (confirmacion_pin.length() > 0) {
              entrada_valida = true;
            }
          } else if (c != '\r') {
            confirmacion_pin += c;
            if (confirmacion_pin.length() <= 4) {
              Serial.print("*");
            }
          }
        }
        delay(50);
      }

      Serial.println();

      bool es_digitos = true;
      for (int i = 0; i < confirmacion_pin.length(); i++) {
        if (confirmacion_pin[i] < '0' || confirmacion_pin[i] > '9') {
          es_digitos = false;
          break;
        }
      }

      if (!es_digitos || confirmacion_pin.length() != 4) {
        Serial.println("✗ Registro cancelado");
        return;
      }

      Serial.println("→ Verificando PIN...");
      JsonDocument verificacion_pin = verificarPinDocente(num_doc_docente.c_str(), confirmacion_pin.c_str());

      if (!verificacion_pin["valido"]) {
        Serial.println("✗ PIN incorrecto. Registro cancelado.");
        return;
      }

      Serial.println("✓ PIN verificado. Registro confirmado.\n");
    } else {
      Serial.println("✗ Opción inválida");
      return;
    }
  }

  Serial.println("→ Registrando salida...");
  JsonDocument respuesta = registrarAsistenciaEstudianteConMetodo(
    num_doc.c_str(),
    horario_id.c_str(),
    fecha.c_str(),
    hora.c_str(),
    "salida",
    metodo_verificacion.c_str()
  );

  if (respuesta["exito"]) {
    Serial.println("✓ Salida registrada exitosamente\n");
  } else {
    Serial.println("✗ Error registrando salida");
    if (respuesta.containsKey("detail")) {
      Serial.println(respuesta["detail"].as<String>());
    }
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

bool verificarDocenteConPinOBiometria(String num_doc, uint16_t sensor_id, String& metodo_usado) {
  // ★ INTENTA BIOMETRÍA PRIMERO
  Serial.println("→ Verificando identidad biométrica...");
  
  bool permitir_pin = false;
  int resultado = searchFingerprintWithRetries(sensor_id, permitir_pin);

  if (resultado != -1) {
    int confianza = getFingerprintConfidence();
    Serial.print("✓ ¡Huella coincide! | Confianza: ");
    Serial.println(confianza);
    Serial.println("✓ Identidad verificada\n");
    metodo_usado = "Biometría";
    return true;
  }

  if (!permitir_pin) {
    Serial.println("✗ Verificación biométrica fallida");
    return false;
  }

  // ★ BIOMETRÍA FALLÓ → OFRECER PIN
  Serial.println("\n✗ La huella no se verificó después de 3 intentos");
  Serial.println("¿Deseas usar PIN de verificación?\n");
  Serial.println("1 -> Sí");
  Serial.println("2 -> No\n");

  while (Serial.available()) {
    Serial.read();
  }
  delay(200);

  unsigned long timeout = millis();
  char opcion_pin = '0';

  while (millis() - timeout < 15000 && opcion_pin == '0') {
    if (Serial.available()) {
      opcion_pin = Serial.read();
    }
    delay(100);
  }

  if (opcion_pin != '1') {
    Serial.println("✗ Verificación cancelada");
    return false;
  }

  // ★ INGRESAR PIN
  Serial.println("\nIngresa tu PIN (4 dígitos):");
  String pin = "";
  timeout = millis();

  while (millis() - timeout < 30000) {
    if (Serial.available()) {
      pin = Serial.readStringUntil('\n');
      pin.trim();
      
      if (pin.length() == 4) {
        break;
      } else {
        Serial.println("✗ PIN inválido (debe ser 4 dígitos)");
      }
    }
    delay(100);
  }

  if (pin.length() != 4) {
    Serial.println("✗ Timeout");
    return false;
  }

  // ★ VERIFICAR PIN CON BACKEND
  Serial.println("→ Verificando PIN...");
  JsonDocument verificacion = verificarPinDocente(num_doc.c_str(), pin.c_str());

  if (!verificacion["valido"]) {
    Serial.println("✗ PIN incorrecto");
    return false;
  }

  Serial.println("✓ PIN verificado correctamente\n");
  metodo_usado = "PIN";
  return true;
}

void verificarComandosPendientes() {
  // ★ SOLO EJECUTAR CADA 30 SEGUNDOS
  if (millis() - ultimo_sync < SYNC_INTERVAL) {
    return;
  }

  ultimo_sync = millis();

  Serial.println("\n[SYNC] Verificando comandos pendientes...");

  JsonDocument comandos_resp = obtenerComandosPendientes();

  if (comandos_resp.containsKey("error") && comandos_resp["error"]) {
    Serial.println("[SYNC] Error obteniendo comandos (sin conexión)");
    return;
  }

  JsonArray comandos = comandos_resp.as<JsonArray>();

  if (comandos.size() == 0) {
    Serial.println("[SYNC] No hay comandos pendientes");
    return;
  }

  Serial.print("[SYNC] Encontrados ");
  Serial.print(comandos.size());
  Serial.println(" comando(s)");

  // ★ PROCESAR CADA COMANDO
  for (size_t i = 0; i < comandos.size(); i++) {
    JsonObject comando = comandos[i].as<JsonObject>();

    int comando_id = comando["id"];
    int huella_id = comando["huella_id"];
    String cmd = comando["comando"].as<String>();

    Serial.print("[SYNC] Procesando comando ");
    Serial.print(comando_id);
    Serial.print(": ");
    Serial.println(cmd);

    bool success = false;

    if (cmd == "DELETE") {
      // ★ EJECUTAR LA ELIMINACIÓN
      success = deleteFingerprintById(huella_id);
    } else {
      Serial.println("[SYNC] Comando desconocido");
    }

    // ★ CONFIRMAR AL SERVIDOR
    Serial.println("[SYNC] Confirmando ejecución al servidor...");
    JsonDocument confirm_resp = confirmarComandoEjecutado(comando_id, success);

    if (confirm_resp.containsKey("error")) {
      Serial.println("[SYNC] Error confirmando comando");
    } else {
      Serial.println("[SYNC] Comando confirmado");
    }
  }

  Serial.println("[SYNC] Sincronización completada\n");
}