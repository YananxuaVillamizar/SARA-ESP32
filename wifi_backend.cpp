#include "wifi_backend.h"
#include "display.h"
// ★ PARA CONEXIONES HTTPS
#include <WiFiClientSecure.h>

// ★ DEFINICIÓN DE CONSTANTES (no extern aquí)
const char* BACKEND_HOST = "sara-backend-0ml8.onrender.com";
const int BACKEND_PORT = 443;
const bool USE_HTTPS = true;

// Certificado SSL para Render (actualizado 2024)
const char* ca_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGfEwnt50uaSDANBgkqhkiG9w0BAQsFADBp
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaXRhbCBDZXJ0MScwJQYDVQQLEx5E
aWdpdGFsIENlcnRpZmljYXRlIFRydXN0IFRFQ0ExHDAaBgNVBAMTE1RFQ0EgR2xv
YmFsIFJvb3QgQ0EwHhcNMjEwNTA5MDAwMDAwWhcNMzgwMTE4MjM1OTU5WjBpMQsw
CQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaXRhbCBDZXJ0MScwJQYDVQQLEx5EaWdp
dGFsIENlcnRpZmljYXRlIFRydXN0IVRFQ0ExHDAaBgNVBAMTE1RFQ0EgR2xvYmFs
IFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDeLaQhtPcv
J4O6nnbyG2Wh8mVnMLtlAkkSMN3F1E9K7fKV5a0h3VNR7PkOmr2/F1CnGPXVJT9w
hDgfF/dL1wVvUABiAfMM5q0v9f+SPbHe9F1eFZfKz1VXWxJXOPwZVbDJQYkz7Gqv
P6pUvD7oqNDZvnJJV4N0M4xSiPZKZx6M1cWrVaXqHzKbFKEjqLvJSGpqCKEYMDbb
M/o7YQUNgZtVZO/Hqw3SlPxRLh8Kau7lZUJYIrFiTCPO+s3lrNQnTF+tpN8R2Qm0
8IqmxF0V+3l1J9VLk6EQQQx8sWWJuWGtlYK5ld4J0z1g7X5pZjLhHFcwCULU1jQY
CbGMzNXXSGhVAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8EBTAD
AQH/MB0GA1UdDgQWBBTK84OmtKBYvKcBQ/qREjbzn/2kLDANBgkqhkiG9w0BAQsF
AAOCAQEA5oLxLDVy0+FQ3pV5pYIVJYhV23TuWOIVYYvnYRHG7XD7qTtUyHxAm7ux
LlJvhVxQ6AxGHcbgfWnCBaM2U3s3oIyAyAqx6hPVXGHgzAfB4vd+zBoJ1vbsfDNO
dWu7t0YPHFHM3r3/fzvTjnYSPBpFLcPRQPqeK0K/CzCXrOkrZ6vVOCmTzEOJAZ+G
0VJ1YkCqZfTgVDJWKKkWzjzfqMkPuDkNc4kHWuqL6g3SVDT0k9NI1wqCQU9EW4zS
oeHLGS1iy0BHJAzPVJjqNDvTvBGFvTQHHFkqSqfU1p73dFVWPVLDwMoNdTa4MNfJ
gqQGFG7U3hfVJa5pfOXw2fYgv0ZEIA==
-----END CERTIFICATE-----
)EOF";


// ★ FUNCIÓN AUXILIAR PARA PETICIONES HTTPS
// ★ FUNCIÓN AUXILIAR PARA PETICIONES HTTPS
JsonDocument realizarPeticionHTTPS(const char* host, const char* endpoint, const String& payload) {
  JsonDocument respuesta;
  
  WiFiClientSecure client;
  client.setInsecure();
  
  if (!client.connect(host, 443)) {
    logPrintln("[ERROR] No se pudo conectar a " + String(host));
    respuesta["error"] = true;
    return respuesta;
  }

  String request = String("POST ") + endpoint + " HTTP/1.1\r\n";
  request += String("Host: ") + host + "\r\n";
  request += "Content-Type: application/json\r\n";
  request += "Content-Length: " + String(payload.length()) + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";
  request += payload;

  client.print(request);
  delay(300);  // ★ REDUCIDO de 1500 a 300

  String respuesta_str = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 5000) {  // ★ REDUCIDO de 10000 a 5000
    while (client.available()) {
      respuesta_str += (char)client.read();
    }
    if (respuesta_str.length() > 0 && !client.connected()) {
      break;  // ★ SALIR si recibimos datos y se cerró la conexión
    }
    delay(20);  // ★ REDUCIDO de 50 a 20
  }
  client.stop();

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    deserializeJson(respuesta, json_str);
  }

  return respuesta;
}

bool conectarWiFi() {
  logPrint("Conectando a WiFi: ");
  logPrintln(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    logPrint(".");
    intentos++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    logPrintln("\n✗ Error conectando a WiFi.");
    return false;
  }

  logPrintln("\n✓ WiFi conectado!");
  logPrint("IP: ");
  logPrintln(WiFi.localIP().toString());

  return true;
}

JsonDocument buscarUsuarioPorDocumento(const char* num_doc) {
  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/usuario/buscar", payload);
}

JsonDocument obtenerDatosUsuario(const char* num_doc) {
  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/usuario/datos", payload);
}

bool notificarRegistroExitoso(const char* num_doc, uint16_t sensor_id) {
  logPrintln("[DEBUG] Enviando notificación al servidor...");

  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;
  solicitud["sensor_id"] = sensor_id;

  String payload;
  serializeJson(solicitud, payload);

  JsonDocument respuesta = realizarPeticionHTTPS(BACKEND_HOST, "/hardware/registro/completado", payload);

  bool success = respuesta.containsKey("exito") && respuesta["exito"];

  if (success) {
    logPrintln("[DEBUG] ✓ Respuesta exitosa del servidor");
    return true;
  } else {
    logPrintln("[DEBUG] ✗ No se recibió confirmación");
    return !respuesta.containsKey("error");
  }
}

JsonDocument registrarAsistenciaDocente(const char* num_doc, const char* horario_id,
                                        const char* fecha, const char* hora, const char* aula, 
                                        const char* tipo_sesion, const char* metodo_verificacion) {
  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;
  solicitud["horario_id"] = horario_id;
  solicitud["fecha"] = fecha;
  solicitud["hora"] = hora;
  solicitud["aula"] = aula;
  solicitud["tipo_sesion"] = tipo_sesion;
  solicitud["metodo_verificacion"] = metodo_verificacion;  // ★ NUEVO

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/docente/asistencia/registrar", payload);
}

JsonDocument verificarTipoRegistro(const char* sesion_id, const char* num_doc) {
  JsonDocument solicitud;
  solicitud["sesion_id"] = sesion_id;
  solicitud["num_doc"] = num_doc;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/registro/tipo", payload);
}

JsonDocument obtenerDocenteSesion(const char* sesion_id) {
  JsonDocument solicitud;
  solicitud["sesion_id"] = sesion_id;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/docente/sesion", payload);
}

JsonDocument registrarAsistenciaEstudianteConMetodo(const char* num_doc, const char* horario_id,
                                                     const char* fecha, const char* hora, const char* tipo,
                                                     const char* metodo_verificacion) {
  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;
  solicitud["horario_id"] = horario_id;
  solicitud["fecha"] = fecha;
  solicitud["hora"] = hora;
  solicitud["tipo"] = tipo;
  solicitud["metodo_verificacion"] = metodo_verificacion;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/asistencia/estudiante/registrar-metodo", payload);
}

JsonDocument verificarSesionesDocente(const char* num_doc, const char* fecha) {
  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;
  solicitud["fecha"] = fecha;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/docente/sesiones-abierta-por-fecha", payload);
}

JsonDocument obtenerAsignaturasDocentePorDia(const char* num_doc, const char* dia_semana) {
  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;
  solicitud["dia_semana"] = dia_semana;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/docente/asignaturas-por-dia", payload);
}

JsonDocument obtenerHorariosAsignaturaDocente(const char* num_doc, const char* asignatura_id) {
  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;
  solicitud["asignatura_id"] = asignatura_id;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/docente/horarios-asignatura", payload);
}

JsonDocument obtenerDatosHorario(const char* horario_id) {
  JsonDocument solicitud;
  solicitud["horario_id"] = horario_id;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/horarios/datos", payload);
}

JsonDocument obtenerTodasAsignaturasDocente(const char* num_doc) {
  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/docente/todas-asignaturas", payload);
}

JsonDocument verificarPinAdmin(const char* pin) {
  JsonDocument solicitud;
  solicitud["pin"] = pin;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/admin/verificar-pin", payload);
}

JsonDocument obtenerSesionesDisponiblesParaEntrada(const char* num_doc, const char* fecha) {
  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;
  solicitud["fecha"] = fecha;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/estudiante/sesiones-disponibles-para-entrada", payload);
}

JsonDocument obtenerSesionParaSalida(const char* num_doc) {
  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/estudiante/sesion-para-salida", payload);
}

JsonDocument obtenerNombreDocentePorSesion(const char* sesion_id) {
  JsonDocument solicitud;
  solicitud["sesion_id"] = sesion_id;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/docente/nombre-por-sesion", payload);
}

JsonDocument obtenerMetodoEntradaParaSalida(const char* num_doc, const char* sesion_id) {
  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;
  solicitud["sesion_id"] = sesion_id;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/estudiante/metodo-entrada-para-salida", payload);
}

JsonDocument verificarPinDocente(const char* num_doc, const char* pin) {
  JsonDocument solicitud;
  solicitud["num_doc"] = num_doc;
  solicitud["pin"] = pin;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/docente/verificar-pin", payload);
}

JsonDocument obtenerUsuarioDocentePorSesion(const char* sesion_id) {
  JsonDocument solicitud;
  solicitud["sesion_id"] = sesion_id;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/docente/usuario-por-sesion", payload);
}

JsonDocument obtenerIdDisponible() {
  JsonDocument respuesta;
  
  WiFiClientSecure client;
  client.setInsecure();
  
  if (!client.connect(BACKEND_HOST, 443)) {
    logPrintln("[ERROR] No se pudo conectar al backend");
    respuesta["disponible"] = false;
    return respuesta;
  }

  String request = String("GET /hardware/sensor/id-disponible HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_HOST + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";

  client.print(request);
  delay(300);  // ★ REDUCIDO de 1500 a 300

  String respuesta_str = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 5000) {  // ★ REDUCIDO de 8000 a 5000
    while (client.available()) {
      respuesta_str += (char)client.read();
    }
    if (respuesta_str.length() > 0 && !client.connected()) {
      break;  // ★ SALIR si recibimos datos y se cerró la conexión
    }
    delay(20);  // ★ REDUCIDO de 50 a 20
  }
  client.stop();

  if (respuesta_str.length() == 0) {
    logPrintln("[ERROR] No se recibió respuesta");
    respuesta["disponible"] = false;
    return respuesta;
  }

  int json_inicio = respuesta_str.indexOf("{");
  if (json_inicio == -1) {
    respuesta["disponible"] = false;
    return respuesta;
  }

  String json_str = respuesta_str.substring(json_inicio);
  deserializeJson(respuesta, json_str);

  return respuesta;
}

JsonDocument obtenerComandosPendientes() {
  JsonDocument respuesta;
  
  WiFiClientSecure client;
  client.setInsecure();
  
  if (!client.connect(BACKEND_HOST, 443)) {
    respuesta["error"] = true;
    return respuesta;
  }

  String request = String("GET /hardware/sync HTTP/1.1\r\n");
  request += String("Host: ") + BACKEND_HOST + "\r\n";
  request += "Connection: close\r\n";
  request += "\r\n";

  client.print(request);
  delay(300);

  String respuesta_str = "";
  unsigned long timeout = millis();
  
  while (millis() - timeout < 5000) {
    while (client.available()) {
      respuesta_str += (char)client.read();
    }
    if (respuesta_str.length() > 0 && !client.connected()) {
      break;
    }
    delay(20);
  }
  client.stop();

  int json_inicio = respuesta_str.indexOf("[");
  if (json_inicio != -1) {
    String json_str = respuesta_str.substring(json_inicio);
    DeserializationError error = deserializeJson(respuesta, json_str);
    if (error) {
      logPrintln("[ERROR] Error al parsear comandos JSON");
      respuesta["error"] = true;
    }
  } else {
    respuesta["error"] = true;
  }

  return respuesta;
}

JsonDocument confirmarComandoEjecutado(int comando_id, bool success) {
  JsonDocument solicitud;
  solicitud["comando_id"] = comando_id;
  solicitud["success"] = success;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/sync/confirm", payload);
}

JsonDocument obtenerMetodoEntradaDocentePorSesion(const char* sesion_id, const char* num_doc) {
  JsonDocument solicitud;
  solicitud["sesion_id"] = sesion_id;
  solicitud["num_doc"] = num_doc;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/docente/metodo-entrada-por-sesion", payload);
}

JsonDocument validarSesionDisponible(const char* horario_id, const char* fecha, const char* num_doc) {
  JsonDocument solicitud;
  solicitud["horario_id"] = horario_id;
  solicitud["fecha"] = fecha;
  solicitud["num_doc"] = num_doc;

  String payload;
  serializeJson(solicitud, payload);

  return realizarPeticionHTTPS(BACKEND_HOST, "/hardware/docente/validar-sesion-disponible", payload);
}