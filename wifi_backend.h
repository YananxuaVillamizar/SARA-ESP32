#ifndef WIFI_BACKEND_H
#define WIFI_BACKEND_H

#include <WiFi.h>
#include <ArduinoJson.h>

// ★ CREDENCIALES WIFI
#define WIFI_SSID "FamiliaVillamizar"
#define WIFI_PASSWORD "13347175-2024"

// ★ CONFIGURACIÓN DEL BACKEND EN RENDER (HTTPS)
// ★ USAR extern PARA EVITAR MÚLTIPLES DEFINICIONES
extern const char* BACKEND_HOST;
extern const int BACKEND_PORT;
extern const bool USE_HTTPS;

// ★ PROTOTIPOS DE FUNCIONES
bool conectarWiFi();
JsonDocument buscarUsuarioPorDocumento(const char* num_doc);
JsonDocument obtenerDatosUsuario(const char* num_doc);
bool notificarRegistroExitoso(const char* num_doc, uint16_t sensor_id);

JsonDocument registrarAsistenciaDocente(const char* num_doc, const char* horario_id,
                                        const char* fecha, const char* hora, const char* aula, 
                                        const char* tipo_sesion, const char* metodo_verificacion);
                                                                               
JsonDocument verificarTipoRegistro(const char* sesion_id, const char* num_doc);
JsonDocument obtenerDocenteSesion(const char* sesion_id);
JsonDocument registrarAsistenciaEstudianteConMetodo(const char* num_doc, const char* horario_id,
                                                     const char* fecha, const char* hora, const char* tipo,
                                                     const char* metodo_verificacion);
                                                  
JsonDocument verificarSesionesDocente(const char* num_doc, const char* fecha);
JsonDocument obtenerAsignaturasDocentePorDia(const char* num_doc, const char* dia_semana);
JsonDocument obtenerHorariosAsignaturaDocente(const char* num_doc, const char* asignatura_id);
JsonDocument obtenerDatosHorario(const char* horario_id);
JsonDocument obtenerTodasAsignaturasDocente(const char* num_doc);
JsonDocument verificarPinAdmin(const char* pin);
JsonDocument obtenerSesionesDisponiblesParaEntrada(const char* num_doc, const char* fecha);
JsonDocument obtenerSesionParaSalida(const char* num_doc);
JsonDocument obtenerNombreDocentePorSesion(const char* sesion_id);
JsonDocument obtenerMetodoEntradaParaSalida(const char* num_doc, const char* sesion_id);
JsonDocument verificarPinDocente(const char* num_doc, const char* pin);
JsonDocument obtenerUsuarioDocentePorSesion(const char* sesion_id);
JsonDocument obtenerIdDisponible();
JsonDocument obtenerMetodoEntradaDocentePorSesion(const char* sesion_id, const char* num_doc);
JsonDocument validarSesionDisponible(const char* horario_id, const char* fecha, const char* num_doc);

// ★ FUNCIÓN AUXILIAR HTTPS (declarada en .cpp)
JsonDocument realizarPeticionHTTPS(const char* host, const char* endpoint, const String& payload);


JsonDocument obtenerComandosPendientes();
JsonDocument confirmarComandoEjecutado(int comando_id, bool success);

#endif // WIFI_BACKEND_H