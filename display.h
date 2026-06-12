#ifndef DISPLAY_H
#define DISPLAY_H

#include <TFT_eSPI.h>

#define DISPLAY_MAX_LINES 10


class DisplayManager {

  public:

    void begin();
    void clear();

    void print(String text);
    void println(String text);

  private:

    TFT_eSPI tft = TFT_eSPI();

    String lines[DISPLAY_MAX_LINES];
    String currentLine = "";

    void redraw();
  

};

extern DisplayManager Display;

#endif