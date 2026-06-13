#include "display.h"

DisplayManager Display;

void DisplayManager::begin() {

  tft.init();

  SPI.begin(
    32,
    26,
    25
  );

  ts.begin();

  tft.setRotation(3);
  
  clear();

}

void DisplayManager::clear() {

  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);

  tft.setTextSize(3);

  tft.setCursor(10,10);

  tft.println("SARA");

  tft.drawLine(
    0,
    45,
    480,
    45,
    TFT_WHITE
  );

  tft.setTextColor(
    TFT_WHITE,
    TFT_BLACK
  );

  tft.setTextSize(2);
  tft.setTextWrap(true);

  totalLines = 0;
  currentLine = "";

}

void DisplayManager::print(String text) {

  currentLine += text;

}

void DisplayManager::println(String text) {

  currentLine += text;

  addLine(currentLine);

  currentLine = "";

}

void DisplayManager::redraw() {

  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.setTextSize(3);
  tft.setCursor(10,10);
  tft.println("SARA");

  tft.drawLine(0,45,480,45,TFT_WHITE);
  // Panel lateral

  tft.fillRect(0,46,40,274,TFT_DARKGREY);

  // Botón subir

  tft.drawRect(2,60,36,50,TFT_WHITE);
  tft.setCursor(13,75);
  tft.print("^");

  // Botón bajar

  tft.drawRect(2,130,36,50,TFT_WHITE);
  tft.setCursor(13,145);
  tft.print("v");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);

  tft.setTextWrap(false);

  int visibleLines = 7;

  int start;

  if(autoFollow) {

    start = totalLines - visibleLines;

    if(start < 0)
      start = 0;

  } else {

    start = firstVisibleLine;

  }

  int y = 60;
  int end = start + visibleLines;

  if(end > totalLines)
    end = totalLines;

  for(int i = start; i < end; i++) {

    tft.setCursor(40, y);
    tft.println(lines[i]);

    y += 35;

  }

}

void DisplayManager::addLine(String text) {

  String linea = "";

  while(text.length() > 0) {

    int espacio = text.indexOf(' ');

    String palabra;

    if(espacio == -1) {
      palabra = text;
      text = "";
    } else {
      palabra = text.substring(0, espacio + 1);
      text = text.substring(espacio + 1);
    }

    if(tft.textWidth(linea + palabra) > 430) {

      if(totalLines < MAX_LINES) {
        lines[totalLines++] = linea;
      } else {
        for(int i = 0; i < MAX_LINES - 1; i++)
          lines[i] = lines[i + 1];

        lines[MAX_LINES - 1] = linea;
      }

      linea = palabra;

    } else {

      linea += palabra;

    }
  }

  if(linea.length() > 0) {

    if(totalLines < MAX_LINES) {
      lines[totalLines++] = linea;
    } else {
      for(int i = 0; i < MAX_LINES - 1; i++)
        lines[i] = lines[i + 1];

      lines[MAX_LINES - 1] = linea;
    }

  }

  if(autoFollow) {

  firstVisibleLine = max(0, totalLines - 7);

  }

  redraw();
}

void DisplayManager::update() {

  handleTouch();

}

void DisplayManager::handleTouch() {

  if(!ts.touched())
    return;

  TS_Point p = ts.getPoint();

  int x = constrain(
    map(
      p.x,
      276,
      3787,
      0,
      480
    ),
    0,
    479
  );

  int y = constrain(
    map(
      p.y,
      320,
      3841,
      0,
      320
    ),
    0,
    319
  );

  delay(120);

  Serial.print("X=");
  Serial.print(x);
  Serial.print(" Y=");
  Serial.println(y);


  // Botón subir

  if(
    x >= 0 &&
    x <= 40 &&
    y >= 60 &&
    y <= 110
  ){

    autoFollow = false;

    firstVisibleLine -= 3;

    if(firstVisibleLine < 0)
      firstVisibleLine = 0;

    redraw();

    return;

  }

  // Botón bajar

  if(
    x >= 0 &&
    x <= 40 &&
    y >= 130 &&
    y <= 180
  ){

    firstVisibleLine += 3;

    int maxStart =
      max(
        0,
        totalLines - 7
      );

    if(firstVisibleLine > maxStart)
      firstVisibleLine = maxStart;

    redraw();

    return;

  }

}