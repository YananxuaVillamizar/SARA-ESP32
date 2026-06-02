#include "wifi_backend.h"

bool conectarWiFi() {
  Serial.print("Conectando a WiFi: ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    Serial.print(".");
    intentos++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n✗ Error conectando a WiFi.");
    return false;
  }

  Serial.println("\n✓ WiFi conectado!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  return true;
}

JsonDocument buscarUsuarioPorDocumento(const char* num_doc) {
  JsonDocument respuesta;
  WiFiClient cliente;

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    Serial.println("✗ Error: No se puede conectar al backend.");
    respuesta["error"] = true;
    return respuesta;
  }

  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/usuario/buscar HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  delay(1000);

  String respuesta_str = "";
  while (cliente.available()) {
    respuesta_str += (char)cliente.read();
  }
  cliente.stop();

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(respuesta, json_str);
  }

  return respuesta;
}

JsonDocument obtenerDatosUsuario(const char* num_doc) {
  JsonDocument respuesta;
  WiFiClient cliente;

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    Serial.println("✗ Error: No se puede conectar al backend.");
    respuesta["error"] = true;
    return respuesta;
  }

  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/usuario/datos HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  delay(1000);

  String respuesta_str = "";
  while (cliente.available()) {
    respuesta_str += (char)cliente.read();
  }
  cliente.stop();

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(respuesta, json_str);
  }

  return respuesta;
}

bool notificarRegistroExitoso(const char* num_doc, uint16_t sensor_id) {
  WiFiClient cliente;

  Serial.println("[DEBUG] Enviando notificación al servidor...");

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    Serial.println("[DEBUG] Error conectando");
    return false;
  }

  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;
  solicitud["sensor_id"] = sensor_id;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/registro/completado HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  
  // Esperar más tiempo para recibir respuesta
  delay(2000);

  String respuesta_str = "";
  unsigned long timeout = millis();
  while (cliente.available() && millis() - timeout < 3000) {
    respuesta_str += (char)cliente.read();
    delay(10);
  }
  cliente.stop();

  Serial.println("[DEBUG] Bytes recibidos: " + String(respuesta_str.length()));

  // Verificar si contiene "exito": true
  bool success = (respuesta_str.indexOf("\"exito\": true") != -1) || 
                 (respuesta_str.indexOf("\"exito\":true") != -1);

  if (success) {
    Serial.println("[DEBUG] ✓ Respuesta exitosa del servidor");
    return true;
  } else {
    Serial.println("[DEBUG] ✗ No se recibió confirmación");
    // Aún así retornar true si el backend respondió algo
    if (respuesta_str.length() > 0) {
      Serial.println("[DEBUG] Pero se recibió respuesta, asumiendo éxito");
      return true;
    }
    return false;
  }
}

JsonDocument registrarAsistenciaDocente(const char* num_doc, const char* horario_id,
                                        const char* fecha, const char* hora, const char* aula) {
  JsonDocument respuesta;
  WiFiClient cliente;

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    respuesta["error"] = true;
    return respuesta;
  }

  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;
  solicitud["horario_id"] = horario_id;
  solicitud["fecha"] = fecha;
  solicitud["hora"] = hora;
  solicitud["aula"] = aula;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/asistencia/docente/registrar HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  delay(2000);

  String respuesta_str = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 8000) {
    while (cliente.available()) {
      respuesta_str += (char)cliente.read();
      delay(5);
    }
    delay(50);
  }
  cliente.stop();

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(respuesta, json_str);
  }

  return respuesta;
}

JsonDocument verificarTipoRegistro(const char* sesion_id, const char* num_doc) {
  JsonDocument respuesta;
  WiFiClient cliente;

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    respuesta["error"] = true;
    return respuesta;
  }

  JsonDocument solicitud;
  solicitud["sesion_id"] = sesion_id;
  solicitud["num_doc"] = num_doc;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/asistencia/tipo-registro HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  delay(2000);

  String respuesta_str = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 8000) {
    while (cliente.available()) {
      respuesta_str += (char)cliente.read();
      delay(5);
    }
    delay(50);
  }
  cliente.stop();

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(respuesta, json_str);
  }

  return respuesta;
}

JsonDocument obtenerDocenteSesion(const char* sesion_id) {
  JsonDocument respuesta;
  WiFiClient cliente;

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    respuesta["error"] = true;
    return respuesta;
  }

  JsonDocument solicitud;
  solicitud["sesion_id"] = sesion_id;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/sesiones/docente HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  delay(1500);

  String respuesta_str = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 8000) {
    while (cliente.available()) {
      respuesta_str += (char)cliente.read();
      delay(5);
    }
    delay(50);
  }
  cliente.stop();

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(respuesta, json_str);
  }

  return respuesta;
}

JsonDocument obtenerMetodoEntrada(const char* num_doc, const char* sesion_id) {
  JsonDocument respuesta;
  WiFiClient cliente;

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    respuesta["error"] = true;
    return respuesta;
  }

  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;
  solicitud["sesion_id"] = sesion_id;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/asistencia/obtener-metodo-entrada HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  delay(1500);

  String respuesta_str = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 8000) {
    while (cliente.available()) {
      respuesta_str += (char)cliente.read();
      delay(5);
    }
    delay(50);
  }
  cliente.stop();

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(respuesta, json_str);
  }

  return respuesta;
}

JsonDocument registrarAsistenciaEstudianteConMetodo(const char* num_doc, const char* horario_id,
                                                     const char* fecha, const char* hora, const char* tipo,
                                                     const char* metodo_verificacion) {
  JsonDocument respuesta;
  WiFiClient cliente;

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    respuesta["error"] = true;
    return respuesta;
  }

  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;
  solicitud["horario_id"] = horario_id;
  solicitud["fecha"] = fecha;
  solicitud["hora"] = hora;
  solicitud["tipo"] = tipo;
  solicitud["metodo_verificacion"] = metodo_verificacion;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/asistencia/estudiante/registrar-metodo HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  delay(2000);

  String respuesta_str = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 8000) {
    while (cliente.available()) {
      respuesta_str += (char)cliente.read();
      delay(5);
    }
    delay(50);
  }
  cliente.stop();

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(respuesta, json_str);
  }

  return respuesta;
}

JsonDocument verificarSesionesDocente(const char* num_doc, const char* fecha) {
  JsonDocument respuesta;
  WiFiClient cliente;

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    respuesta["error"] = true;
    return respuesta;
  }

  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;
  solicitud["fecha"] = fecha;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/docente/sesiones-abierta-por-fecha HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  delay(2000);

  String respuesta_str = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 8000) {
    while (cliente.available()) {
      respuesta_str += (char)cliente.read();
      delay(5);
    }
    delay(50);
  }
  cliente.stop();

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(respuesta, json_str);
  }

  return respuesta;
}


JsonDocument obtenerAsignaturasDocentePorDia(const char* num_doc, const char* dia_semana) {
  JsonDocument respuesta;
  WiFiClient cliente;

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    respuesta["error"] = true;
    return respuesta;
  }

  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;
  solicitud["dia_semana"] = dia_semana;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/docente/asignaturas-por-dia HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  delay(2000);

  String respuesta_str = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 8000) {
    while (cliente.available()) {
      respuesta_str += (char)cliente.read();
      delay(5);
    }
    delay(50);
  }
  cliente.stop();

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(respuesta, json_str);
  }

  return respuesta;
}

JsonDocument obtenerHorariosAsignaturaDocente(const char* num_doc, const char* asignatura_id) {
  JsonDocument respuesta;
  WiFiClient cliente;

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    respuesta["error"] = true;
    Serial.println("[ERROR] No se pudo conectar al servidor");
    return respuesta;
  }

  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;
  solicitud["asignatura_id"] = asignatura_id;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/docente/horarios-asignatura HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  Serial.println("[DEBUG] Solicitando horarios...");
  cliente.print(request);
  delay(2000);

  String respuesta_str = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 8000) {
    while (cliente.available()) {
      respuesta_str += (char)cliente.read();
      delay(5);
    }
    delay(50);
  }
  cliente.stop();

  Serial.print("[DEBUG] Respuesta del servidor: ");
  Serial.println(respuesta_str.substring(0, 200)); // Imprimir primeros 200 caracteres

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(respuesta, json_str);
    
    Serial.print("[DEBUG] Horarios encontrados: ");
    Serial.println(respuesta["horarios"].size());
  }

  return respuesta;
}

JsonDocument obtenerSesionesAbiertasPorFecha(const char* fecha) {
  JsonDocument respuesta;
  WiFiClient cliente;

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    respuesta["error"] = true;
    return respuesta;
  }

  JsonDocument solicitud;
  solicitud["fecha"] = fecha;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/estudiante/sesiones-abiertas-por-fecha HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  delay(2000);

  String respuesta_str = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 8000) {
    while (cliente.available()) {
      respuesta_str += (char)cliente.read();
      delay(5);
    }
    delay(50);
  }
  cliente.stop();

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(respuesta, json_str);
  }

  return respuesta;
}


JsonDocument obtenerDatosHorario(const char* horario_id) {
  JsonDocument respuesta;
  WiFiClient cliente;

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    respuesta["error"] = true;
    return respuesta;
  }

  JsonDocument solicitud;
  solicitud["horario_id"] = horario_id;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/horarios/datos HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  delay(1500);

  String respuesta_str = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 8000) {
    while (cliente.available()) {
      respuesta_str += (char)cliente.read();
      delay(5);
    }
    delay(50);
  }
  cliente.stop();

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(respuesta, json_str);
  }

  return respuesta;
}


JsonDocument verificarMatriculaEstudiante(const char* num_doc, const char* asignatura_id, const char* grupo) {
  JsonDocument respuesta;
  WiFiClient cliente;

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    respuesta["error"] = true;
    return respuesta;
  }

  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;
  solicitud["asignatura_id"] = asignatura_id;
  solicitud["grupo"] = grupo;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/estudiante/verificar-matricula HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  delay(1500);

  String respuesta_str = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 8000) {
    while (cliente.available()) {
      respuesta_str += (char)cliente.read();
      delay(5);
    }
    delay(50);
  }
  cliente.stop();

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(respuesta, json_str);
  }

  return respuesta;
}


JsonDocument obtenerAsistenciaEstudiantePorSesion(const char* num_doc, const char* sesion_id) {
  JsonDocument respuesta;
  WiFiClient cliente;

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    respuesta["error"] = true;
    return respuesta;
  }

  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;
  solicitud["sesion_id"] = sesion_id;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/estudiante/asistencia-por-sesion HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  delay(1500);

  String respuesta_str = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 8000) {
    while (cliente.available()) {
      respuesta_str += (char)cliente.read();
      delay(5);
    }
    delay(50);
  }
  cliente.stop();

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(respuesta, json_str);
  }

  return respuesta;
}

JsonDocument obtenerTodasAsignaturasDocente(const char* num_doc) {
  JsonDocument respuesta;
  WiFiClient cliente;

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    respuesta["error"] = true;
    return respuesta;
  }

  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/docente/todas-asignaturas HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  delay(2000);

  String respuesta_str = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 8000) {
    while (cliente.available()) {
      respuesta_str += (char)cliente.read();
      delay(5);
    }
    delay(50);
  }
  cliente.stop();

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(respuesta, json_str);
  }

  return respuesta;
}

JsonDocument obtenerDatosAsignatura(const char* asignatura_id) {
  JsonDocument respuesta;
  WiFiClient cliente;

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    respuesta["error"] = true;
    return respuesta;
  }

  JsonDocument solicitud;
  solicitud["asignatura_id"] = asignatura_id;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/asignaturas/datos HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  delay(1500);

  String respuesta_str = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 8000) {
    while (cliente.available()) {
      respuesta_str += (char)cliente.read();
      delay(5);
    }
    delay(50);
  }
  cliente.stop();

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(respuesta, json_str);
  }

  return respuesta;
}

JsonDocument obtenerHorarioCompleto(const char* horario_id) {
  JsonDocument respuesta;
  WiFiClient cliente;

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    respuesta["error"] = true;
    return respuesta;
  }

  JsonDocument solicitud;
  solicitud["horario_id"] = horario_id;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/horarios/completo HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  delay(1500);

  String respuesta_str = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 8000) {
    while (cliente.available()) {
      respuesta_str += (char)cliente.read();
      delay(5);
    }
    delay(50);
  }
  cliente.stop();

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(respuesta, json_str);
  }

  return respuesta;
}

JsonDocument verificarPinAdmin(const char* pin) {
  JsonDocument respuesta;
  WiFiClient cliente;

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    respuesta["error"] = true;
    return respuesta;
  }

  JsonDocument solicitud;
  solicitud["pin"] = pin;

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/admin/verificar-pin HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_IP + ":" + BACKEND_PORT + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  cliente.print(request);
  delay(1500);

  String respuesta_str = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 8000) {
    while (cliente.available()) {
      respuesta_str += (char)cliente.read();
      delay(5);
    }
    delay(50);
  }
  cliente.stop();

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(respuesta, json_str);
  }

  return respuesta;
}
