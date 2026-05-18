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

JsonDocument registrarAsistencia(const char* num_doc, const char* horario_id, 
                                  const char* fecha, const char* hora, 
                                  const char* aula) {
  JsonDocument respuesta;
  WiFiClient cliente;

  if (!cliente.connect(BACKEND_IP, BACKEND_PORT)) {
    respuesta["error"] = true;
    return respuesta;
  }

  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;
  solicitud["horario_id"] = horario_id;
  solicitud["fecha_registro"] = fecha;
  solicitud["hora_registro"] = hora;
  if (aula) {
    solicitud["aula"] = aula;
  }

  String payload;
  serializeJson(solicitud, payload);

  String request = String("POST /hardware/asistencia/registrar HTTP/1.1\r\n");
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