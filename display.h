#ifndef DISPLAY_H
#define DISPLAY_H

#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

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

    String lines[MAX_LINES];

    int totalLines = 0;

    String currentLine = "";

    void redraw();

    int firstVisibleLine = 0;

    bool autoFollow = true;

    void handleTouch();

    XPT2046_Touchscreen ts = XPT2046_Touchscreen(14, 13);

    bool touching = false;

    int lastTouchY = 0;

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