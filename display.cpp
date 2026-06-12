#include "display.h"

DisplayManager Display;

void DisplayManager::begin() {

  tft.init();
  tft.setRotation(3);

  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  clear();
}

void DisplayManager::clear() {

  for(int i = 0; i < DISPLAY_MAX_LINES; i++) {
    lines[i] = "";
  }

  redraw();
}

void DisplayManager::print(String text) {

  currentLine += text;

  redraw();

}

void DisplayManager::println(String text) {

  currentLine += text;

  Serial.print("DEBUG GUARDANDO: ");
  Serial.println(currentLine);

  for(int i=0;i<DISPLAY_MAX_LINES-1;i++){
    lines[i] = lines[i+1];
  }

  lines[DISPLAY_MAX_LINES-1] = currentLine;

  currentLine = "";

  redraw();

}

void DisplayManager::redraw() {

  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(3);

  tft.setCursor(10,10);
  tft.println("SARA");

  tft.drawLine(0,45,480,45,TFT_WHITE);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  int y = 60;

  // Historial de líneas terminadas

  for(int i = 0; i < DISPLAY_MAX_LINES; i++) {

    if(lines[i].length() > 0) {

      tft.setCursor(10,y);
      tft.println(lines[i]);

    }

    y += 20;

  }

  // Línea actual que todavía no ha terminado

  if(currentLine.length() > 0) {

    tft.setCursor(10,y);
    tft.print(currentLine);

  }

}