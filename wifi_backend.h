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
JsonDocument obtenerDatosUsuario(const char* num_doc);
bool notificarRegistroExitoso(const char* num_doc, uint16_t sensor_id);
JsonDocument obtenerAsignaturasUsuario(const char* num_doc);
JsonDocument obtenerAsignaturasPrograma(const char* num_doc, const char* programa_id);
JsonDocument obtenerHorariosAsignatura(const char* num_doc, const char* asignatura_id);
JsonDocument registrarAsistenciaDocente(const char* num_doc, const char* horario_id,

                                        const char* fecha, const char* hora, const char* aula);

JsonDocument obtenerHorariosEstudianteAsignatura(const char* num_doc, const char* asignatura_id);
JsonDocument obtenerSesionesDisponibles(const char* num_doc, const char* fecha);
JsonDocument verificarTipoRegistro(const char* sesion_id, const char* num_doc);
JsonDocument obtenerDocenteSesion(const char* sesion_id);
JsonDocument obtenerMetodoEntrada(const char* num_doc, const char* sesion_id);

JsonDocument registrarAsistenciaEstudianteConMetodo(const char* num_doc, const char* horario_id,
                                                     const char* fecha, const char* hora, const char* tipo,
                                                     const char* metodo_verificacion);


#endif // WIFI_BACKEND_H