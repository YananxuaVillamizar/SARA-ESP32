#ifndef DISPLAY_H
#define DISPLAY_H

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// ★ PALETA DE COLORES
#define COLOR_FONDO      0x2945  // Gris oscuro (Dracula)
#define COLOR_EXITO      0x07E0  // Verde Lima
#define COLOR_ERROR      0xF800  // Rojo Claro
#define COLOR_INFO       0x07FF  // Cyan
#define COLOR_INPUT      0xFFE0  // Amarillo
#define COLOR_WARNING    0xFD20  // Naranja
#define COLOR_TITULO     0xFFFF  // Blanco
#define COLOR_NORMAL     0xFFFF  // Blanco (por defecto)

// ★ ESTRUCTURA PARA ALMACENAR LÍNEA CON COLOR
struct LineaConColor {
  String texto;
  uint16_t color;
};

class DisplayManager {

  public:

    void begin();

    void clear();

    void print(String text);

    void println(String text);

    void addLine(String text);

    void update();

  private:

    TFT_eSPI tft = TFT_eSPI();

    static const int MAX_LINES = 200;

    LineaConColor lines[MAX_LINES];  // ★ CAMBIO: ahora almacena color también

    int totalLines = 0;

    String currentLine = "";

    void redraw();

    int firstVisibleLine = 0;

    bool autoFollow = true;

    void handleTouch();

    XPT2046_Touchscreen ts = XPT2046_Touchscreen(14, 13);

    bool touching = false;

    int lastTouchY = 0;

    // ★ NUEVAS FUNCIONES AUXILIARES
    uint16_t detectarColor(String texto);
    String limpiarCaracteresEspeciales(String texto);

};

extern DisplayManager Display;

// Funciones globales de consola

template<typename T>
void logPrint(T msg) {

  Serial.print(msg);
  Display.print(String(msg));

}

template<typename T>
void logPrintln(T msg) {

  Serial.println(msg);
  Display.println(String(msg));

}

inline void logPrintln() {

  Serial.println();
  Display.println("");

}


#endif