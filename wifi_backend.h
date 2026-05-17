#ifndef WIFI_BACKEND_H
#define WIFI_BACKEND_H

#include <WiFi.h>
#include <ArduinoJson.h>

#define WIFI_SSID "FamiliaVillamizar"
#define WIFI_PASSWORD "13347175-2024"
#define BACKEND_IP "192.168.1.42"
#define BACKEND_PORT 8000

bool conectarWiFi();
JsonDocument buscarUsuarioPorDocumento(const char* num_doc);
bool notificarRegistroExitoso(const char* num_doc);
JsonDocument registrarAsistencia(const char* num_doc, const char* horario_id, 
                                  const char* fecha, const char* hora, 
                                  const char* aula = nullptr);

#endif // WIFI_BACKEND_H