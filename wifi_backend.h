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
JsonDocument registrarAsistenciaDocente(const char* num_doc, const char* horario_id,
                                        const char* fecha, const char* hora, const char* aula);

JsonDocument verificarTipoRegistro(const char* sesion_id, const char* num_doc);
JsonDocument obtenerDocenteSesion(const char* sesion_id);
JsonDocument obtenerMetodoEntrada(const char* num_doc, const char* sesion_id);

JsonDocument registrarAsistenciaEstudianteConMetodo(const char* num_doc, const char* horario_id,
                                                     const char* fecha, const char* hora, const char* tipo,
                                                     const char* metodo_verificacion);
                                                  
JsonDocument verificarSesionesDocente(const char* num_doc, const char* fecha);
JsonDocument obtenerAsignaturasDocentePorDia(const char* num_doc, const char* dia_semana);
JsonDocument obtenerHorariosAsignaturaDocente(const char* num_doc, const char* asignatura_id);
JsonDocument obtenerSesionesAbiertasPorFecha(const char* fecha);
JsonDocument obtenerDatosHorario(const char* horario_id);
JsonDocument verificarMatriculaEstudiante(const char* num_doc, const char* asignatura_id, const char* grupo);
JsonDocument obtenerAsistenciaEstudiantePorSesion(const char* num_doc, const char* sesion_id);
JsonDocument obtenerTodasAsignaturasDocente(const char* num_doc);
JsonDocument obtenerDatosAsignatura(const char* asignatura_id);
JsonDocument obtenerHorarioCompleto(const char* horario_id);
JsonDocument verificarPinAdmin(const char* pin);

#endif // WIFI_BACKEND_H