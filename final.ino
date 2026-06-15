#include "biometric_sensor.h"
#include "wifi_backend.h"
#include "display.h"
#include "keyboard.h"

// ★ FUNCIÓN GLOBAL PARA LEER ENTRADA (Serial o Keyboard)
String leerEntrada(unsigned long timeoutMs = 30000) {
  unsigned long timeout = millis();
  
  while (millis() - timeout < timeoutMs) {
    Keyboard.update();
    Display.update();
    
    // ★ Verificar si Keyboard envió algo
    if (Keyboard.hasSentInput()) {
      return Keyboard.getSentInput();
    }
    
    // ★ O si Serial tiene algo
    if (Serial.available()) {
      return Serial.readStringUntil('\n');
    }
    
    delay(50);
  }
  
  return "";
}

// ★ ESPERAR CONFIRMACIÓN POR PANTALLA TÁCTIL, TECLADO O SERIAL
bool esperarConfirmacion(unsigned long timeoutMs = 10000) {
  unsigned long timeout = millis();
  
  while (millis() - timeout < timeoutMs) {
    Keyboard.update();
    Display.update();
    
    if (Keyboard.anyKeyPressed()) {
      Keyboard.clearInput();
      return true;
    }
    
    if (Display.isTouched()) {
      return true;
    }
    
    if (Serial.available()) {
      String temp = Serial.readStringUntil('\n');
      return true;
    }
    
    delay(50);
  }
  
  logPrintln("[DEBUG] Timeout en esperarConfirmacion");
  return false;
}

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
  Display.begin();
  Keyboard.begin(); 
  Display.println("Display iniciado");
  delay(1000);

  logPrintln("\n==== SARA - SISTEMA BIOMÉTRICO ====\n");

  if (!initFingerprintSensor()) {
    logPrintln("FATAL: Sensor no detectado");
    while (1);
  }

  if (!conectarWiFi()) {
    logPrintln("FATAL: WiFi no disponible");
    while (1);
  }

  // ★ SINCRONIZAR HORA CON NTP
  logPrintln("\n[BOOT] Sincronizando hora con servidor NTP...");
  
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
    logPrintln("[OK] Hora sincronizada: ");
    logPrintln(asctime(&timeinfo));
  } else {
    logPrintln("[WARN] No se pudo sincronizar la hora");
  }

  logPrintln("\n✓ Sistema listo\n");
  mostrarMenuPrincipal();
}

void loop() {

  Keyboard.update();
  Display.update();

  // ★ VERIFICAR SI EL KEYBOARD ENVIÓ ALGO
  if (Keyboard.hasSentInput()) {
    String entrada = Keyboard.getSentInput();
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
    }
  }

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
  verificarComandosPendientes();
  logPrintln("\n====================================");
  logPrintln("   SISTEMA SARA - MENÚ PRINCIPAL");
  logPrintln("====================================");
  logPrintln("| 1 -> Registrar huella             |");
  logPrintln("| 2 -> Verificar huella             |");
  logPrintln("| 3 -> Borrar todas las huellas     |");
  logPrintln("| 4 -> Registrar asistencia         |");
  logPrintln("====================================\n");
}

void iniciarFlujoPedirDocumento(String tipo_flujo) {
  logPrintln("\nIngresa el documento del usuario:");
  logPrintln("(Ej: 1234567890)\n");
 
  String num_doc = leerEntrada(30000);
  num_doc.trim();
 
  if (num_doc.length() == 0) {
    logPrintln("✗ Timeout");
    mostrarMenuPrincipal();
    return;
  }
 
  logPrint("→ Buscando usuario: ");
  logPrintln(num_doc);
 
  procesarDocumento(num_doc, tipo_flujo);
}

void procesarDocumento(String documento, String tipo_flujo) {
  logPrint("\n→ Buscando usuario: ");
  logPrintln(documento);
  
  JsonDocument respuesta = buscarUsuarioPorDocumento(documento.c_str());

  if (respuesta.containsKey("error")) {
    logPrintln("✗ Error conectando al backend\n");
    mostrarMenuPrincipal();
    return;
  }

  bool existe = respuesta["existe"];

  if (!existe) {
    // El backend retorna un mensaje específico
    String mensaje = respuesta["mensaje"].as<String>();
    
    logPrint("✗ ");
    logPrintln(mensaje);
    logPrintln();
    mostrarMenuPrincipal();
    return;
  }

  // Usuario encontrado
  num_doc_usuario = documento;
  nombre_usuario = respuesta["nombre_completo"].as<String>();
  usuario_id = respuesta["id"].as<String>();

  logPrint("✓ Bienvenido: ");
  logPrintln(nombre_usuario);

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
  logPrintln("\n[REGISTRO]\n");
  logPrint("Usuario: ");
  logPrintln(nombre_usuario);
  logPrint("Documento: ");
  logPrintln(num_doc_usuario);

  // ★ VERIFICAR PIN ADMIN PRIMERO
  if (!verificarPinAdmin()) {
    mostrarMenuPrincipal();
    return;
  }

  // Verificar si el usuario ya tiene huella registrada
  logPrintln("\n→ Verificando si usuario ya tiene huella...");
  
  JsonDocument datos_usuario = obtenerDatosUsuario(num_doc_usuario.c_str());
  
  uint16_t id_sensor = -1;
  
  if (datos_usuario["existe"] && datos_usuario["sensor_id"] != -1) {
    // Usuario ya tiene huella, usar el mismo ID
    id_sensor = datos_usuario["sensor_id"];
    logPrint("→ Usuario tiene huella anterior. Usando ID: ");
    logPrintln(id_sensor);
    logPrintln("→ Se actualizará la huella existente.\n");
  } else {
    // Usuario no tiene huella, obtener nuevo ID
    logPrintln("\n→ Solicitando ID disponible al servidor...");

    JsonDocument respuesta_json = obtenerIdDisponible();

    if (!respuesta_json["disponible"]) {
      logPrintln("✗ No hay IDs disponibles en el sensor");
      mostrarMenuPrincipal();
      return;
    }

    id_sensor = respuesta_json["sensor_id"];

    logPrint("✓ ID asignado: ");
    logPrintln(id_sensor);
  }

  logPrintln("Pulsa la pantalla o presiona cualquier tecla para continuar...");
  esperarConfirmacion(30000);

  if (enrollFingerprintWithID(id_sensor)) {
    logPrintln("\n→ Notificando al servidor...");
    
    if (notificarRegistroExitoso(num_doc_usuario.c_str(), id_sensor)) {
      logPrintln("✓ Registro completado exitosamente!");
    } else {
      logPrintln("⚠ Huella guardada en sensor (servidor no respondió)");
    }
  } else {
    logPrintln("✗ Fallo el registro");
  }

  num_doc_usuario = "";
  nombre_usuario = "";
  usuario_id = "";

  logPrintln("Pulsa la pantalla o presiona cualquier tecla para volver al menú...");
  esperarConfirmacion(30000);

  logPrintln();
  mostrarMenuPrincipal();
}

void flujoVerificacion() {
  logPrintln("\n[VERIFICACIÓN]\n");
  logPrint("Usuario: ");
  logPrintln(nombre_usuario);
  logPrint("Documento: ");
  logPrintln(num_doc_usuario);

  // Obtener sensor_id desde el backend
  logPrintln("\n→ Obteniendo sensor_id del usuario...");

  JsonDocument datos = obtenerDatosUsuario(num_doc_usuario.c_str());

  if (!datos["existe"]) {
    logPrintln("✗ Error obteniendo datos del usuario");
    mostrarMenuPrincipal();
    return;
  }

  if (datos["sensor_id"] == -1) {
    logPrintln("✗ El usuario no tiene huella registrada");
    mostrarMenuPrincipal();
    return;
  }

  uint16_t id_esperado = datos["sensor_id"];

  logPrint("✓ Sensor ID encontrado: ");
  logPrintln(id_esperado);
  logPrintln("Pulsa la pantalla o presiona cualquier tecla para continuar...");
  esperarConfirmacion(30000);

  if (searchFingerprintInSensorWithID(id_esperado)) {
    int confianza = getFingerprintConfidence();

    logPrintln("\n✓ ¡Verificación exitosa!");
    logPrint("Confianza: ");
    logPrintln(confianza);
    
    logPrintln("\n→ Registrando asistencia...");
    // Aquí irá registro de asistencia en el futuro
    logPrintln("✓ Asistencia registrada");
      
  } 

  num_doc_usuario = "";
  nombre_usuario = "";
  usuario_id = "";

  logPrintln("Pulsa la pantalla o presiona cualquier tecla para volver al menú...");
  esperarConfirmacion(30000);

  logPrintln();
  mostrarMenuPrincipal();
}

void borrarTodasLasHuellas() {
  // ★ VERIFICAR PIN ADMIN PRIMERO
  if (!verificarPinAdmin()) {
    mostrarMenuPrincipal();
    return;
  }
 
  logPrintln("\n═══════════════════════════════════");
  logPrintln("[BORRAR TODAS LAS HUELLAS]");
  logPrintln("═══════════════════════════════════");
  logPrintln("\n⚠ ¡ADVERTENCIA!");
  logPrintln("Esto eliminará TODAS las huellas del sensor.");
  logPrintln("\n¿Estás seguro?");
  logPrintln("1 -> Sí, borrar todo");
  logPrintln("2 -> No, cancelar\n");
 
  String entrada = leerEntrada(10000);
  entrada.trim();
 
  if (entrada == "1") {
    logPrintln("\n→ Borrando todas las huellas...\n");
    
    // Borrar base de datos del sensor
    if (clearDatabase()) {
      logPrintln("✓ Todas las huellas han sido eliminadas.");
    } else {
      logPrintln("✗ Error borrando las huellas.");
    }
    
    logPrintln("Pulsa la pantalla o presiona cualquier tecla para volver al menú...");
    esperarConfirmacion(30000);
    
    logPrintln();
    mostrarMenuPrincipal();
    return;
  } else if (entrada == "2") {
    logPrintln("Operación cancelada.\n");
    mostrarMenuPrincipal();
    return;
  } else {
    logPrintln("✗ Opción inválida\n");
    mostrarMenuPrincipal();
    return;
  }
}

void flujoAsistencia() {
  logPrintln("\n[ASISTENCIA]\n");
  // ★ OBTENER FECHA Y HORA AUTOMÁTICA
  String fecha_hora = obtenerFechaHoraActual();
  String fecha_str = fecha_hora.substring(0, 10); // YYYY-MM-DD
  String hora_str = fecha_hora.substring(11, 19);  // HH:MM:SS
  
  logPrintln("\n→ Fecha y hora actual obtenidas automáticamente");
  logPrint("   Fecha: ");
  logPrintln(fecha_str);
  logPrint("   Hora: ");
  logPrintln(hora_str);
  
  // ★ PASO 2: DETERMINAR SI ES DOCENTE O ESTUDIANTE
  String rol = "";
  JsonDocument datos_usuario = obtenerDatosUsuario(num_doc_usuario.c_str());

  if (!datos_usuario.containsKey("existe") || !datos_usuario["existe"]) {
    logPrintln("✗ Error obteniendo datos del usuario");
    mostrarMenuPrincipal();
    return;
  }

  rol = datos_usuario["rol"].as<String>();

  if (rol == "Docente") {
    procesarAsistenciaDocente(num_doc_usuario, fecha_str);
  } else if (rol == "Estudiante") {
    procesarAsistenciaEstudiante(num_doc_usuario, fecha_str, hora_str);
  } else {
    logPrintln("✗ Rol no reconocido");
    mostrarMenuPrincipal();
    return;
  }

  num_doc_usuario = "";
  nombre_usuario = "";
  usuario_id = "";

  logPrintln("Pulsa la pantalla o presiona cualquier tecla para volver al menú...");
  esperarConfirmacion(30000);

  logPrintln();
  mostrarMenuPrincipal();
}

void procesarAsistenciaDocente(String num_doc, String fecha) {
  logPrintln("\n[DOCENTE - ASISTENCIA]\n");
 
  String sesion_id = "";
  String horario_id = "";
  String aula = "";
  char metodo_char = '0';
 
  logPrintln("→ Verificando sesiones abiertas...");
  
  JsonDocument sesiones_resp = verificarSesionesDocente(num_doc.c_str(), fecha.c_str());
 
  if (sesiones_resp.containsKey("error")) {
    logPrintln("✗ Error conectando al servidor");
    return;
  }
 
  bool hay_sesiones = sesiones_resp["hay_sesiones"];
 
  if (hay_sesiones) {
    logPrintln("✓ Sesión abierta encontrada. Registrando SALIDA.\n");
 
    sesion_id = sesiones_resp["sesion_id"].as<String>();
    horario_id = sesiones_resp["horario_id"].as<String>();
    aula = sesiones_resp["aula"].as<String>();
 
    String nombre_asignatura = sesiones_resp["asignatura_nombre"].as<String>();
    String codigo_asignatura = sesiones_resp["asignatura_codigo"].as<String>();
    String grupo = sesiones_resp["grupo"].as<String>();
    String hora_inicio = sesiones_resp["hora_inicio"].as<String>();
    String hora_fin = sesiones_resp["hora_fin"].as<String>();
 
    logPrint("→ Sesión: ");
    logPrint(nombre_asignatura);
    logPrint(" (");
    logPrint(codigo_asignatura);
    logPrint(") - Grupo ");
    logPrint(grupo);
    logPrint(" - ");
    logPrint(hora_inicio);
    logPrint(" a ");
    logPrintln(hora_fin);
  
    String fecha_hora = obtenerFechaHoraActual();
    String fecha_str = fecha_hora.substring(0, 10);
    String hora_str = fecha_hora.substring(11, 19);
    
    logPrintln("\n→ Fecha y hora actual obtenidas automáticamente");
    logPrint("   Fecha: ");
    logPrintln(fecha_str);
    logPrint("   Hora: ");
    logPrintln(hora_str);
 
    logPrintln("\n→ Obteniendo método de entrada usado...");
    JsonDocument metodo_resp = obtenerMetodoEntradaDocentePorSesion(sesion_id.c_str(), num_doc.c_str());
 
    String metodo_entrada = "";
    if (metodo_resp["existe"]) {
      metodo_entrada = metodo_resp["metodo_verificacion"].as<String>();
      logPrint("✓ Método de entrada detectado: ");
      logPrintln(metodo_entrada);
    } else {
      logPrintln("✗ No se pudo obtener el método de entrada");
      return;
    }
 
    logPrintln("\n[REGISTRO DE SALIDA]\n");
 
    if (metodo_entrada == "Biometría") {
      logPrintln("¿Cómo deseas registrar tu salida?");
      logPrintln("1 -> Biometría (huella)");
      logPrintln("2 -> PIN docente\n");
 
      String entrada = leerEntrada(15000);
      entrada.trim();
      metodo_char = entrada.length() > 0 ? entrada[0] : '0';
 
      JsonDocument datos_usuario = obtenerDatosUsuario(num_doc.c_str());
 
      if (!datos_usuario.containsKey("existe") || !datos_usuario["existe"]) {
        logPrintln("✗ Error obteniendo datos del usuario");
        return;
      }
 
      int sensor_id = datos_usuario["sensor_id"];
 
      if (metodo_char == '1') {
        if (sensor_id == -1) {
          logPrintln("✗ El usuario no tiene huella registrada");
          return;
        }
 
        logPrintln();
        
        bool permitir_pin = false;
        int resultado = searchFingerprintWithRetries(sensor_id, permitir_pin);
 
        if (resultado == -1 && permitir_pin) {
          logPrintln("¿Desea hacer registro con PIN?");
          logPrintln("1 -> Sí");
          logPrintln("2 -> No\n");
 
          entrada = leerEntrada(15000);
          entrada.trim();
          char opcion_pin = entrada.length() > 0 ? entrada[0] : '0';
 
          if (opcion_pin == '1') {
            metodo_char = '2';
          } else {
            logPrintln("✗ Salida cancelada");
            return;
          }
        } else if (resultado == -1) {
          logPrintln("✗ Verificación biométrica fallida");
          return;
        } else {
          int confianza = getFingerprintConfidence();
          logPrint("✓ ¡Huella coincide! | Confianza: ");
          logPrintln(confianza);
          logPrintln("✓ Identidad verificada\n");
        }
      }
 
      if (metodo_char == '2') {
        logPrintln();
        
        int intentos_pin = 0;
        const int MAX_INTENTOS_PIN = 2;
        bool pin_valido_salida = false;
 
        while (intentos_pin < MAX_INTENTOS_PIN && !pin_valido_salida) {
          intentos_pin++;
          logPrint("Intento ");
          logPrint(intentos_pin);
          logPrint("/");
          logPrintln(MAX_INTENTOS_PIN);
          logPrintln("Ingresa tu PIN (4 dígitos):");
          
          Keyboard.setPinMode(true);  // ★ ACTIVAR MODO PIN
          String pin = leerEntrada(30000);
          Keyboard.setPinMode(false);  // ★ DESACTIVAR MODO PIN
          
          pin.trim();
 
          if (pin.length() != 4) {
            logPrintln("✗ PIN inválido (debe ser 4 dígitos)");
            if (intentos_pin < MAX_INTENTOS_PIN) {
              logPrintln();
            }
            continue;
          }
 
          logPrintln("→ Verificando PIN...");
          JsonDocument verificacion = verificarPinDocente(num_doc.c_str(), pin.c_str());
 
          if (!verificacion["valido"]) {
            logPrintln("✗ PIN incorrecto");
            if (intentos_pin < MAX_INTENTOS_PIN) {
              logPrintln();
            }
          } else {
            logPrintln("✓ PIN verificado correctamente\n");
            pin_valido_salida = true;
          }
        }
 
        if (!pin_valido_salida) {
          logPrintln("✗ Se agotaron los intentos de PIN");
          return;
        }
      } else if (metodo_char != '1') {
        logPrintln("✗ Opción inválida");
        return;
      }
 
    } else if (metodo_entrada == "PIN docente") {
      logPrintln("Entrada registrada con PIN. Registrando salida con PIN.\n");
 
      metodo_char = '2';
      
      int intentos_pin = 0;
      const int MAX_INTENTOS_PIN = 2;
      bool pin_valido_salida = false;
 
      while (intentos_pin < MAX_INTENTOS_PIN && !pin_valido_salida) {
        intentos_pin++;
        logPrint("Intento ");
        logPrint(intentos_pin);
        logPrint("/");
        logPrintln(MAX_INTENTOS_PIN);
        logPrintln("Ingresa tu PIN (4 dígitos):");
        
        Keyboard.setPinMode(true);  // ★ ACTIVAR MODO PIN
        String pin = leerEntrada(30000);
        Keyboard.setPinMode(false);  // ★ DESACTIVAR MODO PIN
        
        pin.trim();
 
        if (pin.length() != 4) {
          logPrintln("✗ PIN inválido (debe ser 4 dígitos)");
          if (intentos_pin < MAX_INTENTOS_PIN) {
            logPrintln();
          }
          continue;
        }
 
        logPrintln("→ Verificando PIN...");
        JsonDocument verificacion = verificarPinDocente(num_doc.c_str(), pin.c_str());
 
        if (!verificacion["valido"]) {
          logPrintln("✗ PIN incorrecto");
          if (intentos_pin < MAX_INTENTOS_PIN) {
            logPrintln();
          }
        } else {
          logPrintln("✓ PIN verificado correctamente\n");
          pin_valido_salida = true;
        }
      }
 
      if (!pin_valido_salida) {
        logPrintln("✗ Se agotaron los intentos de PIN");
        return;
      }
    }
 
    String metodo_salida = "";
    
    if (metodo_entrada == "Biometría") {
      metodo_salida = (metodo_char == '1') ? "Biometría" : "PIN docente";
    } else if (metodo_entrada == "PIN docente") {
      metodo_salida = "PIN docente";
    }
    
    logPrintln("→ Registrando asistencia...");
    
    JsonDocument respuesta = registrarAsistenciaDocente(
      num_doc.c_str(),
      horario_id.c_str(),
      fecha.c_str(),
      hora_str.c_str(),
      aula.c_str(),
      "",
      metodo_salida.c_str()
    );
 
    if (respuesta.containsKey("error")) {
      logPrintln("✗ Error conectando al servidor");
      return;
    }
 
    bool exito = respuesta["exito"];
    String mensaje = respuesta["mensaje"].as<String>();
 
    if (exito) {
      logPrint("✓ ");
      logPrintln(mensaje);
    } else {
      String error_msg = respuesta["detail"].as<String>();
      logPrint("✗ Error: ");
      logPrintln(error_msg);
    }
 
  } else {
    logPrintln("✓ No hay sesión abierta. Registrando ENTRADA.\n");
 
    logPrintln("¿Qué tipo de sesión?");
    logPrintln("1 -> Ordinaria (según horario)");
    logPrintln("2 -> Extraordinaria (fuera del horario)\n");
 
    String entrada = leerEntrada(15000);
    entrada.trim();
    char tipo_sesion_char = entrada.length() > 0 ? entrada[0] : '0';
 
    if (tipo_sesion_char == '1') {
      procesarSesionOrdinaria(num_doc, fecha);
    } else if (tipo_sesion_char == '2') {
      procesarSesionExtraordinaria(num_doc, fecha);
    } else {
      logPrintln("✗ Opción inválida");
      return;
    }
  }
}

void procesarSesionOrdinaria(String num_doc, String fecha) {
  logPrintln("\n[SESIÓN ORDINARIA]\n");
 
  String dia_semana = obtenerDiaSemana(fecha);
  logPrint("→ Día: ");
  logPrintln(dia_semana);
 
  logPrintln();
 
  String entrada = "";
 
  JsonDocument asignaturas_resp = obtenerAsignaturasDocentePorDia(num_doc.c_str(), dia_semana.c_str());
 
  if (asignaturas_resp.containsKey("error") || !asignaturas_resp["existe"]) {
    logPrintln("✗ No hay asignaturas para este día");
    return;
  }
 
  JsonArray asignaturas = asignaturas_resp["asignaturas"].as<JsonArray>();
 
  if (asignaturas.size() == 0) {
    logPrintln("✗ No hay asignaturas para este día");
    return;
  }
 
  int opcion_asignatura = -1;
 
  if (asignaturas.size() == 1) {
    opcion_asignatura = 0;
    logPrint("✓ Sesión encontrada para entrada: ");
    logPrint(asignaturas[0]["nombre"].as<String>());
    logPrint(" (");
    logPrint(asignaturas[0]["codigo"].as<String>());
    logPrint(") - Grupo ");
    logPrint(asignaturas[0]["grupo"].as<String>());
    logPrint(" - ");
    logPrint(asignaturas[0]["hora_inicio"].as<String>());
    logPrint(" a ");
    logPrintln(asignaturas[0]["hora_fin"].as<String>());
  } else {
    logPrintln("→ Asignaturas disponibles para este día:\n");
 
    for (size_t i = 0; i < asignaturas.size(); i++) {
      logPrint(i + 1);
      logPrint(". ");
      logPrint(asignaturas[i]["nombre"].as<String>());
      logPrint(" (");
      logPrint(asignaturas[i]["codigo"].as<String>());
      logPrint(") - Grupo ");
      logPrint(asignaturas[i]["grupo"].as<String>());
      logPrint(" - ");
      logPrint(asignaturas[i]["hora_inicio"].as<String>());
      logPrint(" a ");
      logPrintln(asignaturas[i]["hora_fin"].as<String>());
    }
 
    logPrint("\nSelecciona una asignatura (1-");
    logPrint(asignaturas.size());
    logPrintln("):\n");
 
    entrada = leerEntrada(30000);
    entrada.trim();
    int opcion = entrada.toInt();
    
    if (opcion > 0 && opcion <= (int)asignaturas.size()) {
      opcion_asignatura = opcion - 1;
    } else {
      logPrintln("✗ Opción inválida");
      return;
    }
 
    logPrint("\n✓ Seleccionado: ");
    logPrint(asignaturas[opcion_asignatura]["nombre"].as<String>());
    logPrint(" - ");
    logPrintln(asignaturas[opcion_asignatura]["hora_inicio"].as<String>());
  }
 
  String horario_id = asignaturas[opcion_asignatura]["horario_id"].as<String>();
  String fecha_hora_actual = obtenerFechaHoraActual();
  String fecha_str = fecha_hora_actual.substring(0, 10);
  String hora_fin = asignaturas[opcion_asignatura]["hora_fin"].as<String>();
  String hora_actual = fecha_hora_actual.substring(11, 19);
 
  logPrintln("\n→ Validando disponibilidad...");
  
  JsonDocument validacion = validarSesionDisponible(horario_id.c_str(), fecha_str.c_str(), num_doc.c_str());
  
  if (!validacion["disponible"]) {
    String razon = validacion["razon"].as<String>();
    logPrintln(razon);
    return;
  }
  
  logPrintln("✓ Sesión disponible\n");
 
  if (hora_actual > hora_fin) {
    logPrintln("\n✗ El horario ordinario para esta sesión ha finalizado.");
    logPrintln("✓ Pero puedes realizar esta clase como sesión extraordinaria.\n");
    
    logPrintln("¿Deseas continuar como sesión extraordinaria?");
    logPrintln("1 -> Sí");
    logPrintln("2 -> No\n");
 
    entrada = leerEntrada(15000);
    entrada.trim();
    char opcion_extraordinaria = entrada.length() > 0 ? entrada[0] : '0';
 
    if (opcion_extraordinaria == '1') {
      procesarSesionExtraordinaria(num_doc, fecha);
      return;
    } else {
      logPrintln("✗ Registro cancelado");
      return;
    }
  }
 
  String aula = asignaturas[opcion_asignatura]["aula"].as<String>();
 
  logPrintln("\nIngresa el aula donde se realizará la clase (ej: A101):");
  
  String aula_ingresada = leerEntrada(30000);
  aula_ingresada.trim();
 
  if (aula_ingresada.length() == 0) {
    logPrintln("✗ Timeout");
    return;
  }
 
  logPrint("✓ Aula: ");
  logPrintln(aula_ingresada);
 
  logPrintln("\n[REGISTRO DE ENTRADA]\n");
  logPrintln("¿Cómo deseas registrar tu entrada?");
  logPrintln("1 -> Biometría (huella)");
  logPrintln("2 -> PIN docente\n");
 
  entrada = leerEntrada(15000);
  entrada.trim();
  char metodo_char = entrada.length() > 0 ? entrada[0] : '0';
 
  JsonDocument datos_usuario = obtenerDatosUsuario(num_doc.c_str());
 
  if (!datos_usuario.containsKey("existe") || !datos_usuario["existe"]) {
    logPrintln("✗ Error obteniendo datos del usuario");
    return;
  }
 
  int sensor_id = datos_usuario["sensor_id"];
  String metodo_entrada = "";
 
  if (metodo_char == '1') {
    if (sensor_id == -1) {
      logPrintln("✗ El usuario no tiene huella registrada");
      return;
    }
 
    logPrintln();
    
    bool permitir_pin = false;
    int resultado = searchFingerprintWithRetries(sensor_id, permitir_pin);
 
    if (resultado == -1 && permitir_pin) {
      logPrintln("\n¿Desea hacer registro con PIN?");
      logPrintln("1 -> Sí");
      logPrintln("2 -> No\n");
 
      entrada = leerEntrada(15000);
      entrada.trim();
      char opcion_pin = entrada.length() > 0 ? entrada[0] : '0';
 
      if (opcion_pin == '1') {
        metodo_char = '2';
      } else {
        logPrintln("✗ Verificación cancelada");
        return;
      }
    } else if (resultado == -1) {
      logPrintln("✗ Verificación biométrica fallida");
      return;
    } else {
      int confianza = getFingerprintConfidence();
      logPrint("✓ ¡Huella coincide! | Confianza: ");
      logPrintln(confianza);
      logPrintln("✓ Identidad verificada\n");
      metodo_entrada = "Biometría";
    }
  }
 
  if (metodo_char == '2') {
    logPrintln();
    
    int intentos_pin = 0;
    const int MAX_INTENTOS_PIN = 2;
    bool pin_valido_entrada = false;
 
    while (intentos_pin < MAX_INTENTOS_PIN && !pin_valido_entrada) {
      intentos_pin++;
      logPrint("Intento ");
      logPrint(intentos_pin);
      logPrint("/");
      logPrintln(MAX_INTENTOS_PIN);
      logPrintln("Ingresa tu PIN (4 dígitos):");
      
      Keyboard.setPinMode(true);  // ★ ACTIVAR MODO PIN
      String pin = leerEntrada(30000);
      Keyboard.setPinMode(false);  // ★ DESACTIVAR MODO PIN
      
      pin.trim();
 
      if (pin.length() != 4) {
        logPrintln("✗ PIN inválido (debe ser 4 dígitos)");
        if (intentos_pin < MAX_INTENTOS_PIN) {
          logPrintln();
        }
        continue;
      }
 
      logPrintln("→ Verificando PIN...");
      JsonDocument verificacion = verificarPinDocente(num_doc.c_str(), pin.c_str());
 
      if (!verificacion["valido"]) {
        logPrintln("✗ PIN incorrecto");
        if (intentos_pin < MAX_INTENTOS_PIN) {
          logPrintln();
        }
      } else {
        logPrintln("✓ PIN verificado correctamente\n");
        pin_valido_entrada = true;
        metodo_entrada = "PIN docente";
      }
    }
 
    if (!pin_valido_entrada) {
      logPrintln("✗ Se agotaron los intentos de PIN");
      return;
    }
  } else if (metodo_char != '1') {
    logPrintln("✗ Opción inválida");
    return;
  }
 
  String hora_str = fecha_hora_actual.substring(11, 19);
  
  logPrintln("→ Registrando asistencia...");
  
  JsonDocument respuesta = registrarAsistenciaDocente(
    num_doc.c_str(),
    horario_id.c_str(),
    fecha_str.c_str(),
    hora_str.c_str(),
    aula_ingresada.c_str(),
    "ordinaria",
    metodo_entrada.c_str()
  );
 
  if (respuesta.containsKey("error")) {
    logPrintln("✗ Error conectando al servidor");
    return;
  }
 
  bool exito = respuesta["exito"];
  String mensaje = respuesta["mensaje"].as<String>();
 
  if (exito) {
    logPrint("✓ ");
    logPrintln(mensaje);
  } else {
    String error_msg = respuesta["detail"].as<String>();
    logPrint("✗ Error: ");
    logPrintln(error_msg);
  }
}

void procesarSesionExtraordinaria(String num_doc, String fecha) {
  logPrintln("\n[SESIÓN EXTRAORDINARIA]\n");
 
  logPrintln("→ Asignaturas disponibles:\n");
 
  JsonDocument asignaturas_resp = obtenerTodasAsignaturasDocente(num_doc.c_str());
 
  if (asignaturas_resp.containsKey("error") || !asignaturas_resp["existe"]) {
    logPrintln("✗ No hay asignaturas disponibles");
    return;
  }
 
  JsonArray asignaturas = asignaturas_resp["asignaturas"].as<JsonArray>();
 
  if (asignaturas.size() == 0) {
    logPrintln("✗ No hay asignaturas disponibles");
    return;
  }
 
  for (size_t i = 0; i < asignaturas.size(); i++) {
    logPrint(i + 1);
    logPrint(". ");
    logPrint(asignaturas[i]["nombre"].as<String>());
    logPrint(" (");
    logPrint(asignaturas[i]["codigo"].as<String>());
    logPrint(") - Grupo ");
    logPrintln(asignaturas[i]["grupo"].as<String>());
  }
 
  logPrint("\nSelecciona una asignatura (1-");
  logPrint(asignaturas.size());
  logPrintln("):\n");
 
  String entrada = leerEntrada(30000);
  entrada.trim();
  int opcion = entrada.toInt();
  
  int opcion_asignatura = -1;
  if (opcion > 0 && opcion <= (int)asignaturas.size()) {
    opcion_asignatura = opcion - 1;
  } else {
    logPrintln("✗ Opción inválida");
    return;
  }
 
  String asignatura_id = asignaturas[opcion_asignatura]["asignatura_id"].as<String>();
 
  logPrint("\n✓ Seleccionado: ");
  logPrintln(asignaturas[opcion_asignatura]["nombre"].as<String>());
 
  JsonDocument horarios_resp = obtenerHorariosAsignaturaDocente(num_doc.c_str(), asignatura_id.c_str());
 
  if (horarios_resp.containsKey("error") || !horarios_resp["existe"]) {
    logPrintln("✗ No hay horarios para esta asignatura");
    return;
  }
 
  JsonArray horarios = horarios_resp["horarios"].as<JsonArray>();
 
  if (horarios.size() == 0) {
    logPrintln("✗ No hay horarios para esta asignatura");
    return;
  }
 
  logPrintln("\n→ Horarios disponibles:\n");
 
  for (size_t i = 0; i < horarios.size(); i++) {
    logPrint(i + 1);
    logPrint(". ");
    
    String dia_semana_horario = horarios[i]["dia_semana"].as<String>();
    String hora_inicio = horarios[i]["hora_inicio"].as<String>();
    String hora_fin = horarios[i]["hora_fin"].as<String>();
    
    logPrint(dia_semana_horario);
    logPrint(" - ");
    logPrint(hora_inicio);
    logPrint(" a ");
    logPrintln(hora_fin);
  }
 
  logPrint("\nSelecciona un horario (1-");
  logPrint(horarios.size());
  logPrintln("):\n");
 
  entrada = leerEntrada(30000);
  entrada.trim();
  opcion = entrada.toInt();
  
  int opcion_horario = -1;
  if (opcion > 0 && opcion <= (int)horarios.size()) {
    opcion_horario = opcion - 1;
  } else {
    logPrintln("✗ Opción inválida");
    return;
  }
 
  String horario_id = horarios[opcion_horario]["horario_id"].as<String>();
 
  logPrint("\n✓ Horario seleccionado: ");
  logPrint(horarios[opcion_horario]["hora_inicio"].as<String>());
  logPrint(" a ");
  logPrintln(horarios[opcion_horario]["hora_fin"].as<String>());
 
  logPrintln("\nIngresa el aula donde se realizará la clase (ej: A101):");
  
  String aula_ingresada = leerEntrada(30000);
  aula_ingresada.trim();
 
  if (aula_ingresada.length() == 0) {
    logPrintln("✗ Timeout");
    return;
  }
 
  logPrint("✓ Aula: ");
  logPrintln(aula_ingresada);
 
  logPrintln("\n[REGISTRO DE ENTRADA]\n");
  logPrintln("¿Cómo deseas registrar tu entrada?");
  logPrintln("1 -> Biometría (huella)");
  logPrintln("2 -> PIN docente\n");
 
  entrada = leerEntrada(15000);
  entrada.trim();
  char metodo_char = entrada.length() > 0 ? entrada[0] : '0';
 
  JsonDocument datos_usuario = obtenerDatosUsuario(num_doc.c_str());
 
  if (!datos_usuario.containsKey("existe") || !datos_usuario["existe"]) {
    logPrintln("✗ Error obteniendo datos del usuario");
    return;
  }
 
  int sensor_id = datos_usuario["sensor_id"];
  String metodo_entrada = "";
 
  if (metodo_char == '1') {
    if (sensor_id == -1) {
      logPrintln("✗ El usuario no tiene huella registrada");
      return;
    }
 
    logPrintln();
    
    bool permitir_pin = false;
    int resultado = searchFingerprintWithRetries(sensor_id, permitir_pin);
 
    if (resultado == -1 && permitir_pin) {
      logPrintln("\n¿Desea hacer registro con PIN?");
      logPrintln("1 -> Sí");
      logPrintln("2 -> No\n");
 
      entrada = leerEntrada(15000);
      entrada.trim();
      char opcion_pin = entrada.length() > 0 ? entrada[0] : '0';
 
      if (opcion_pin == '1') {
        metodo_char = '2';
      } else {
        logPrintln("✗ Verificación cancelada");
        return;
      }
    } else if (resultado == -1) {
      logPrintln("✗ Verificación biométrica fallida");
      return;
    } else {
      int confianza = getFingerprintConfidence();
      logPrint("✓ ¡Huella coincide! | Confianza: ");
      logPrintln(confianza);
      logPrintln("✓ Identidad verificada\n");
      metodo_entrada = "Biometría";
    }
  }
 
  if (metodo_char == '2') {
    logPrintln();
    
    int intentos_pin = 0;
    const int MAX_INTENTOS_PIN = 2;
    bool pin_valido_entrada = false;
 
    while (intentos_pin < MAX_INTENTOS_PIN && !pin_valido_entrada) {
      intentos_pin++;
      logPrint("Intento ");
      logPrint(intentos_pin);
      logPrint("/");
      logPrintln(MAX_INTENTOS_PIN);
      logPrintln("Ingresa tu PIN (4 dígitos):");
      
      Keyboard.setPinMode(true);  // ★ ACTIVAR MODO PIN
      String pin = leerEntrada(30000);
      Keyboard.setPinMode(false);  // ★ DESACTIVAR MODO PIN
      
      pin.trim();
 
      if (pin.length() != 4) {
        logPrintln("✗ PIN inválido (debe ser 4 dígitos)");
        if (intentos_pin < MAX_INTENTOS_PIN) {
          logPrintln();
        }
        continue;
      }
 
      logPrintln("→ Verificando PIN...");
      JsonDocument verificacion = verificarPinDocente(num_doc.c_str(), pin.c_str());
 
      if (!verificacion["valido"]) {
        logPrintln("✗ PIN incorrecto");
        if (intentos_pin < MAX_INTENTOS_PIN) {
          logPrintln();
        }
      } else {
        logPrintln("✓ PIN verificado correctamente\n");
        pin_valido_entrada = true;
        metodo_entrada = "PIN docente";
      }
    }
 
    if (!pin_valido_entrada) {
      logPrintln("✗ Se agotaron los intentos de PIN");
      return;
    }
  } else if (metodo_char != '1') {
    logPrintln("✗ Opción inválida");
    return;
  }
 
  String fecha_hora_actual = obtenerFechaHoraActual();
  String fecha_str = fecha_hora_actual.substring(0, 10);
  String hora_str = fecha_hora_actual.substring(11, 19);
  
  logPrintln("→ Registrando asistencia...");
  
  JsonDocument respuesta = registrarAsistenciaDocente(
    num_doc.c_str(),
    horario_id.c_str(),
    fecha_str.c_str(),
    hora_str.c_str(),
    aula_ingresada.c_str(),
    "extraordinaria",
    metodo_entrada.c_str()
  );
 
  if (respuesta.containsKey("error")) {
    logPrintln("✗ Error conectando al servidor");
    return;
  }
 
  bool exito = respuesta["exito"];
  String mensaje = respuesta["mensaje"].as<String>();
 
  if (exito) {
    logPrint("✓ ");
    logPrintln(mensaje);
  } else {
    String error_msg = respuesta["detail"].as<String>();
    logPrint("✗ Error: ");
    logPrintln(error_msg);
  }
}

void procesarAsistenciaEstudiante(String num_doc, String fecha, String hora) {
  logPrintln("\n[ESTUDIANTE - ASISTENCIA]\n");
 
  logPrintln("→ Verificando si hay sesiones pendientes de salida...");
  JsonDocument sesion_salida = obtenerSesionParaSalida(num_doc.c_str());
 
  if (sesion_salida["encontrada"]) {
    logPrint("✓ Sesión encontrada: ");
    logPrint(sesion_salida["asignatura_nombre"].as<String>());
    logPrint(" - Grupo ");
    logPrintln(sesion_salida["grupo"].as<String>());
    logPrintln();
 
    procesarRegistroEstudianteSalida(
      num_doc, 
      sesion_salida["sesion_id"].as<String>(), 
      sesion_salida["horario_id"].as<String>(), 
      fecha, 
      hora
    );
    return;
  }
 
  logPrintln("→ Buscando sesiones disponibles para entrada...");
  JsonDocument sesiones_entrada = obtenerSesionesDisponiblesParaEntrada(num_doc.c_str(), fecha.c_str());
 
  if (sesiones_entrada["encontradas"]) {
    int cantidad = sesiones_entrada["cantidad"];
    JsonArray sesiones = sesiones_entrada["sesiones"];
 
    int opcion = -1;
 
    if (cantidad == 1) {
      opcion = 0;
      logPrint("✓ Sesión encontrada: ");
      logPrint(sesiones[0]["asignatura_nombre"].as<String>());
      logPrint(" (");
      logPrint(sesiones[0]["asignatura_codigo"].as<String>());
      logPrint(") - Grupo ");
      logPrintln(sesiones[0]["grupo"].as<String>());
    } else {
      logPrint("✓ Encontradas ");
      logPrint(cantidad);
      logPrintln(" sesión(es) para entrada\n");
 
      for (size_t i = 0; i < sesiones.size(); i++) {
        logPrint(i + 1);
        logPrint(". ");
        logPrint(sesiones[i]["asignatura_nombre"].as<String>());
        logPrint(" (");
        logPrint(sesiones[i]["asignatura_codigo"].as<String>());
        logPrint(") - Grupo ");
        logPrintln(sesiones[i]["grupo"].as<String>());
      }
 
      logPrint("\nSelecciona una sesión (1-");
      logPrint(cantidad);
      logPrintln("):\n");
 
      String entrada = leerEntrada(20000);
      entrada.trim();
      int opcion_temp = entrada.toInt();
      
      if (opcion_temp > 0 && opcion_temp <= cantidad) {
        opcion = opcion_temp - 1;
      } else {
        logPrintln("✗ Opción inválida");
        return;
      }
 
      logPrintln("\n✓ Sesión seleccionada");
    }
 
    JsonObject sesion_seleccionada = sesiones[opcion];
    String sesion_id = sesion_seleccionada["sesion_id"].as<String>();
    String horario_id = sesion_seleccionada["horario_id"].as<String>();
    String aula = sesion_seleccionada["aula"].as<String>();
 
    logPrintln();
 
    String metodo_entrada = procesarRegistroEstudianteEntrada(num_doc, sesion_id, horario_id, aula, fecha, hora);
    
    if (metodo_entrada.length() > 0) {
      logPrintln("\nPuedes registrar tu salida cuando termines.\n");
    }
    
    return;
  }
 
  logPrintln("✗ No hay sesiones disponibles");
}

String procesarRegistroEstudianteEntrada(String num_doc, String sesion_id, String horario_id, String aula, String fecha, String hora) {
  logPrintln("[REGISTRO DE ENTRADA]\n");
 
  logPrintln("¿Cómo deseas registrar tu entrada?");
  logPrintln("1 -> Biometría (huella)");
  logPrintln("2 -> Supervisado\n");
 
  String entrada = leerEntrada(15000);
  entrada.trim();
  char metodo_char = entrada.length() > 0 ? entrada[0] : '0';
 
  String metodo_verificacion = "";
 
  if (metodo_char == '1') {
    metodo_verificacion = "Biometría";
 
    JsonDocument datos_usuario = obtenerDatosUsuario(num_doc.c_str());
 
    if (!datos_usuario["existe"]) {
      logPrintln("✗ Error obteniendo datos del usuario");
      return "";
    }
 
    uint16_t sensor_id = datos_usuario["sensor_id"];
 
    if (sensor_id == -1) {
      logPrintln("✗ Usuario no tiene huella registrada");
      return "";
    }
 
    bool permitir_supervisado = false;
    int resultado = searchFingerprintWithRetries(sensor_id, permitir_supervisado);
 
    if (resultado == -1 && permitir_supervisado) {
      logPrintln("¿Deseas registrar supervisado?");
      logPrintln("1 -> Sí");
      logPrintln("2 -> No\n");
 
      entrada = leerEntrada(15000);
      entrada.trim();
      char opcion_sup = entrada.length() > 0 ? entrada[0] : '0';
 
      if (opcion_sup == '1') {
        metodo_verificacion = "Supervisado";
 
        JsonDocument docente_num_resp = obtenerUsuarioDocentePorSesion(sesion_id.c_str());
        if (!docente_num_resp["existe"]) {
          logPrintln("✗ Error obteniendo datos del docente");
          return "";
        }
 
        String num_doc_docente = docente_num_resp["num_doc"].as<String>();
        JsonDocument docente_resp = obtenerNombreDocentePorSesion(sesion_id.c_str());
        if (!docente_resp["existe"]) {
          logPrintln("✗ Error obteniendo datos del docente");
          return "";
        }
 
        String nombre_docente = docente_resp["nombre_docente"].as<String>();
 
        logPrint("→ ");
        logPrint(nombre_docente);
        logPrintln(", este registro quedará bajo tu responsabilidad.");
        logPrintln("Ingresa el PIN de confirmación (4 dígitos):");
        logPrintln("(O cualquier letra para cancelar)\n");
 
        Keyboard.setPinMode(true);  // ★ ACTIVAR MODO PIN
        String confirmacion_pin = leerEntrada(30000);
        Keyboard.setPinMode(false);  // ★ DESACTIVAR MODO PIN
        
        confirmacion_pin.trim();
 
        bool es_digitos = true;
        for (int i = 0; i < confirmacion_pin.length(); i++) {
          if (confirmacion_pin[i] < '0' || confirmacion_pin[i] > '9') {
            es_digitos = false;
            break;
          }
        }
 
        if (!es_digitos || confirmacion_pin.length() != 4) {
          logPrintln("✗ Registro cancelado");
          return "";
        }
 
        logPrintln("→ Verificando PIN...");
        JsonDocument verificacion_pin = verificarPinDocente(num_doc_docente.c_str(), confirmacion_pin.c_str());
 
        if (!verificacion_pin["valido"]) {
          logPrintln("✗ PIN incorrecto. Registro cancelado.");
          return "";
        }
 
        logPrintln("✓ PIN verificado. Registro confirmado.\n");
      } else {
        logPrintln("✗ Registro cancelado");
        return "";
      }
    } else if (resultado == -1) {
      logPrintln("✗ Verificación biométrica fallida");
      return "";
    } else {
      int confianza = getFingerprintConfidence();
      logPrint("✓ ¡Huella coincide! | Confianza: ");
      logPrintln(confianza);
      logPrintln("✓ Identidad verificada\n");
    }
 
  } else if (metodo_char == '2') {
    metodo_verificacion = "Supervisado";
 
    JsonDocument docente_num_resp = obtenerUsuarioDocentePorSesion(sesion_id.c_str());
    if (!docente_num_resp["existe"]) {
      logPrintln("✗ Error obteniendo datos del docente");
      return "";
    }
 
    String num_doc_docente = docente_num_resp["num_doc"].as<String>();
    JsonDocument docente_resp = obtenerNombreDocentePorSesion(sesion_id.c_str());
    if (!docente_resp["existe"]) {
      logPrintln("✗ Error obteniendo datos del docente");
      return "";
    }
 
    String nombre_docente = docente_resp["nombre_docente"].as<String>();
 
    logPrint("→ ");
    logPrint(nombre_docente);
    logPrintln(", este registro quedará bajo tu responsabilidad.");
    logPrintln("Ingresa el PIN de confirmación (4 dígitos):");
    logPrintln("(O cualquier letra para cancelar)\n");
 
    Keyboard.setPinMode(true);  // ★ ACTIVAR MODO PIN
    String confirmacion_pin = leerEntrada(30000);
    Keyboard.setPinMode(false);  // ★ DESACTIVAR MODO PIN
    
    confirmacion_pin.trim();
 
    bool es_digitos = true;
    for (int i = 0; i < confirmacion_pin.length(); i++) {
      if (confirmacion_pin[i] < '0' || confirmacion_pin[i] > '9') {
        es_digitos = false;
        break;
      }
    }
 
    if (!es_digitos || confirmacion_pin.length() != 4) {
      logPrintln("✗ Registro cancelado");
      return "";
    }
 
    logPrintln("→ Verificando PIN...");
    JsonDocument verificacion_pin = verificarPinDocente(num_doc_docente.c_str(), confirmacion_pin.c_str());
 
    if (!verificacion_pin["valido"]) {
      logPrintln("✗ PIN incorrecto. Registro cancelado.");
      return "";
    }
 
    logPrintln("✓ PIN verificado. Registro confirmado.\n");
  } else {
    logPrintln("✗ Opción inválida");
    return "";
  }
 
  logPrintln("→ Registrando entrada...");
  JsonDocument respuesta = registrarAsistenciaEstudianteConMetodo(
    num_doc.c_str(),
    horario_id.c_str(),
    fecha.c_str(),
    hora.c_str(),
    "entrada",
    metodo_verificacion.c_str()
  );
 
  if (respuesta["exito"]) {
    logPrintln("✓ Entrada registrada exitosamente\n");
    return metodo_verificacion;
  } else {
    logPrintln("✗ Error registrando entrada");
    return "";
  }
}

void procesarRegistroEstudianteSalida(String num_doc, String sesion_id, String horario_id, String fecha, String hora) {
  logPrintln("[REGISTRO DE SALIDA]\n");
 
  logPrintln("→ Verificando método de entrada...");
  JsonDocument metodo_resp = obtenerMetodoEntradaParaSalida(num_doc.c_str(), sesion_id.c_str());
 
  if (!metodo_resp["existe"]) {
    logPrintln("✗ Error obteniendo método de entrada");
    return;
  }
 
  String metodo_entrada = metodo_resp["metodo_verificacion"].as<String>();
  logPrint("✓ Método de entrada: ");
  logPrintln(metodo_entrada);
  logPrintln();
 
  String metodo_verificacion = "";
 
  if (metodo_entrada == "Supervisado") {
    metodo_verificacion = "Supervisado";
 
    JsonDocument docente_num_resp = obtenerUsuarioDocentePorSesion(sesion_id.c_str());
    if (!docente_num_resp["existe"]) {
      logPrintln("✗ Error obteniendo datos del docente");
      return;
    }
 
    String num_doc_docente = docente_num_resp["num_doc"].as<String>();
    JsonDocument docente_resp = obtenerNombreDocentePorSesion(sesion_id.c_str());
    if (!docente_resp["existe"]) {
      logPrintln("✗ Error obteniendo datos del docente");
      return;
    }
 
    String nombre_docente = docente_resp["nombre_docente"].as<String>();
 
    logPrint("→ ");
    logPrint(nombre_docente);
    logPrintln(", ingresa tu PIN de confirmación:");
    logPrintln("(O cualquier letra para cancelar)\n");
 
    Keyboard.setPinMode(true);  // ★ ACTIVAR MODO PIN
    String confirmacion_pin = leerEntrada(30000);
    Keyboard.setPinMode(false);  // ★ DESACTIVAR MODO PIN
    
    confirmacion_pin.trim();
 
    bool es_digitos = true;
    for (int i = 0; i < confirmacion_pin.length(); i++) {
      if (confirmacion_pin[i] < '0' || confirmacion_pin[i] > '9') {
        es_digitos = false;
        break;
      }
    }
 
    if (!es_digitos || confirmacion_pin.length() != 4) {
      logPrintln("✗ Registro cancelado");
      return;
    }
 
    logPrintln("→ Verificando PIN...");
    JsonDocument verificacion_pin = verificarPinDocente(num_doc_docente.c_str(), confirmacion_pin.c_str());
 
    if (!verificacion_pin["valido"]) {
      logPrintln("✗ PIN incorrecto. Registro cancelado.");
      return;
    }
 
    logPrintln("✓ PIN verificado. Registro confirmado.\n");
 
  } else if (metodo_entrada == "Biometría") {
    logPrintln("¿Cómo deseas registrar tu salida?");
    logPrintln("1 -> Biometría (huella)");
    logPrintln("2 -> Supervisado\n");
 
    String entrada = leerEntrada(15000);
    entrada.trim();
    char metodo_char = entrada.length() > 0 ? entrada[0] : '0';
 
    if (metodo_char == '1') {
      metodo_verificacion = "Biometría";
 
      JsonDocument datos_usuario = obtenerDatosUsuario(num_doc.c_str());
 
      if (!datos_usuario["existe"]) {
        logPrintln("✗ Error obteniendo datos del usuario");
        return;
      }
 
      uint16_t sensor_id = datos_usuario["sensor_id"];
 
      if (sensor_id == -1) {
        logPrintln("✗ Usuario no tiene huella registrada");
        return;
      }
 
      bool permitir_supervisado = false;
      int resultado = searchFingerprintWithRetries(sensor_id, permitir_supervisado);
 
      if (resultado == -1 && permitir_supervisado) {
        logPrintln("¿Deseas hacer un registro supervisado?");
        logPrintln("1 -> Sí");
        logPrintln("2 -> No\n");
 
        entrada = leerEntrada(15000);
        entrada.trim();
        char opcion_sup = entrada.length() > 0 ? entrada[0] : '0';
 
        if (opcion_sup == '1') {
          metodo_verificacion = "Supervisado";
 
          JsonDocument docente_num_resp = obtenerUsuarioDocentePorSesion(sesion_id.c_str());
          if (!docente_num_resp["existe"]) {
            logPrintln("✗ Error obteniendo datos del docente");
            return;
          }
 
          String num_doc_docente = docente_num_resp["num_doc"].as<String>();
          JsonDocument docente_resp = obtenerNombreDocentePorSesion(sesion_id.c_str());
          if (!docente_resp["existe"]) {
            logPrintln("✗ Error obteniendo datos del docente");
            return;
          }
 
          String nombre_docente = docente_resp["nombre_docente"].as<String>();
 
          logPrint("→ ");
          logPrint(nombre_docente);
          logPrintln(", ingresa tu PIN de confirmación:");
          logPrintln("(O cualquier letra para cancelar)\n");
 
          Keyboard.setPinMode(true);  // ★ ACTIVAR MODO PIN
          String confirmacion_pin = leerEntrada(30000);
          Keyboard.setPinMode(false);  // ★ DESACTIVAR MODO PIN
          
          confirmacion_pin.trim();
 
          bool es_digitos = true;
          for (int i = 0; i < confirmacion_pin.length(); i++) {
            if (confirmacion_pin[i] < '0' || confirmacion_pin[i] > '9') {
              es_digitos = false;
              break;
            }
          }
 
          if (!es_digitos || confirmacion_pin.length() != 4) {
            logPrintln("✗ Registro cancelado");
            return;
          }
 
          logPrintln("→ Verificando PIN...");
          JsonDocument verificacion_pin = verificarPinDocente(num_doc_docente.c_str(), confirmacion_pin.c_str());
 
          if (!verificacion_pin["valido"]) {
            logPrintln("✗ PIN incorrecto. Registro cancelado.");
            return;
          }
 
          logPrintln("✓ PIN verificado. Registro confirmado.\n");
        } else {
          logPrintln("✗ Registro cancelado");
          return;
        }
      } else if (resultado == -1) {
        logPrintln("✗ Verificación biométrica fallida");
        return;
      } else {
        int confianza = getFingerprintConfidence();
        logPrint("✓ ¡Huella coincide! | Confianza: ");
        logPrintln(confianza);
        logPrintln("✓ Identidad verificada\n");
      }
 
    } else if (metodo_char == '2') {
      metodo_verificacion = "Supervisado";
 
      JsonDocument docente_num_resp = obtenerUsuarioDocentePorSesion(sesion_id.c_str());
      if (!docente_num_resp["existe"]) {
        logPrintln("✗ Error obteniendo datos del docente");
        return;
      }
 
      String num_doc_docente = docente_num_resp["num_doc"].as<String>();
      JsonDocument docente_resp = obtenerNombreDocentePorSesion(sesion_id.c_str());
      if (!docente_resp["existe"]) {
        logPrintln("✗ Error obteniendo datos del docente");
        return;
      }
 
      String nombre_docente = docente_resp["nombre_docente"].as<String>();
 
      logPrint("→ ");
      logPrint(nombre_docente);
      logPrintln(", ingresa tu PIN de confirmación:");
      logPrintln("(O cualquier letra para cancelar)\n");
 
      Keyboard.setPinMode(true);  // ★ ACTIVAR MODO PIN
      String confirmacion_pin = leerEntrada(30000);
      Keyboard.setPinMode(false);  // ★ DESACTIVAR MODO PIN
      
      confirmacion_pin.trim();
 
      bool es_digitos = true;
      for (int i = 0; i < confirmacion_pin.length(); i++) {
        if (confirmacion_pin[i] < '0' || confirmacion_pin[i] > '9') {
          es_digitos = false;
          break;
        }
      }
 
      if (!es_digitos || confirmacion_pin.length() != 4) {
        logPrintln("✗ Registro cancelado");
        return;
      }
 
      logPrintln("→ Verificando PIN...");
      JsonDocument verificacion_pin = verificarPinDocente(num_doc_docente.c_str(), confirmacion_pin.c_str());
 
      if (!verificacion_pin["valido"]) {
        logPrintln("✗ PIN incorrecto. Registro cancelado.");
        return;
      }
 
      logPrintln("✓ PIN verificado. Registro confirmado.\n");
    } else {
      logPrintln("✗ Opción inválida");
      return;
    }
  }
 
  logPrintln("→ Registrando salida...");
  JsonDocument respuesta = registrarAsistenciaEstudianteConMetodo(
    num_doc.c_str(),
    horario_id.c_str(),
    fecha.c_str(),
    hora.c_str(),
    "salida",
    metodo_verificacion.c_str()
  );
 
  if (respuesta["exito"]) {
    logPrintln("✓ Salida registrada exitosamente\n");
  } else {
    logPrintln("✗ Error registrando salida");
  }
}

bool verificarPinAdmin() {
  logPrintln("\n[VERIFICACIÓN DE ADMINISTRADOR]\n");
  logPrintln("Ingresa PIN de administrativo:\n");
  
  Keyboard.setPinMode(true);  // ★ ACTIVAR MODO PIN
  String pin = leerEntrada(30000);
  Keyboard.setPinMode(false);  // ★ DESACTIVAR MODO PIN
  
  pin.trim();
  
  if (pin.length() == 0) {
    logPrintln("✗ Timeout");
    return false;
  }
  
  logPrintln("→ Verificando PIN...");
  JsonDocument verificacion = verificarPinAdmin(pin.c_str());
  
  if (!verificacion["valido"]) {
    logPrintln("✗ PIN incorrecto");
    return false;
  }
  
  logPrintln("✓ Autenticado como administrador\n");
  return true;
}

bool verificarDocenteConPinOBiometria(String num_doc, uint16_t sensor_id, String& metodo_usado) {
  logPrintln("→ Verificando identidad...\n");
  
  bool permitir_pin = false;
  int resultado = searchFingerprintWithRetries(sensor_id, permitir_pin);
 
  if (resultado != -1) {
    int confianza = getFingerprintConfidence();
    logPrint("✓ ¡Huella coincide! | Confianza: ");
    logPrintln(confianza);
    logPrintln("✓ Identidad verificada\n");
    metodo_usado = "Biometría";
    return true;
  }
 
  if (!permitir_pin) {
    logPrintln("✗ Verificación biométrica fallida");
    return false;
  }
 
  logPrintln("✗ Huella no verificada después de 3 intentos");
  logPrintln("¿Deseas usar PIN?\n");
  logPrintln("1 -> Sí");
  logPrintln("2 -> No\n");
 
  String entrada = leerEntrada(15000);
  entrada.trim();
  char opcion_pin = entrada.length() > 0 ? entrada[0] : '0';
 
  if (opcion_pin != '1') {
    logPrintln("✗ Verificación cancelada");
    return false;
  }
 
  logPrintln("\nIngresa tu PIN (4 dígitos):\n");
  
  Keyboard.setPinMode(true);  // ★ ACTIVAR MODO PIN
  String pin = leerEntrada(30000);
  Keyboard.setPinMode(false);  // ★ DESACTIVAR MODO PIN
  
  pin.trim();
  
  if (pin.length() != 4) {
    logPrintln("✗ PIN inválido (debe ser 4 dígitos)");
    return false;
  }
 
  logPrintln("→ Verificando PIN...");
  JsonDocument verificacion = verificarPinDocente(num_doc.c_str(), pin.c_str());
 
  if (!verificacion["valido"]) {
    logPrintln("✗ PIN incorrecto");
    return false;
  }
 
  logPrintln("✓ PIN verificado correctamente\n");
  metodo_usado = "PIN";
  return true;
}

void verificarComandosPendientes() {
  // ★ SOLO EJECUTAR CADA 30 SEGUNDOS
  if (millis() - ultimo_sync < SYNC_INTERVAL) {
    return;
  }

  ultimo_sync = millis();

  logPrintln("\n[SYNC] Verificando comandos pendientes...");

  JsonDocument comandos_resp = obtenerComandosPendientes();

  if (comandos_resp.containsKey("error") && comandos_resp["error"]) {
    logPrintln("[SYNC] Error obteniendo comandos (sin conexión)");
    return;
  }

  JsonArray comandos = comandos_resp.as<JsonArray>();

  if (comandos.size() == 0) {
    logPrintln("[SYNC] No hay comandos pendientes");
    return;
  }

  logPrintln("[SYNC] Encontrados ");
  logPrint(comandos.size());
  logPrintln(" comando(s)");

  // ★ PROCESAR CADA COMANDO
  for (size_t i = 0; i < comandos.size(); i++) {
    JsonObject comando = comandos[i].as<JsonObject>();

    int comando_id = comando["id"];
    int huella_id = comando["huella_id"];
    String cmd = comando["comando"].as<String>();

    logPrintln("[SYNC] Procesando comando ");
    logPrint(comando_id);
    logPrint(": ");
    logPrintln(cmd);

    bool success = false;

    if (cmd == "DELETE") {
      // ★ EJECUTAR LA ELIMINACIÓN
      success = deleteFingerprintById(huella_id);
    } else {
      logPrintln("[SYNC] Comando desconocido");
    }

    // ★ CONFIRMAR AL SERVIDOR
    logPrintln("[SYNC] Confirmando ejecución al servidor...");
    JsonDocument confirm_resp = confirmarComandoEjecutado(comando_id, success);

    if (confirm_resp.containsKey("error")) {
      logPrintln("[SYNC] Error confirmando comando");
    } else {
      logPrintln("[SYNC] Comando confirmado");
    }
  }

  logPrintln("[SYNC] Sincronización completada\n");
}