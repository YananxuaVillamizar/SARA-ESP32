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

  tft.fillScreen(COLOR_FONDO);  // ★ Fondo gris oscuro

  tft.setTextColor(COLOR_TITULO, COLOR_FONDO);
  tft.setTextSize(3);
  tft.setCursor(10, 10);
  tft.println("SARA");

  tft.drawLine(0, 45, 480, 45, COLOR_TITULO);

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

// ★ NUEVA FUNCIÓN: Detectar tipo de mensaje y retornar color
uint16_t DisplayManager::detectarColor(String texto) {

  String textoBajo = texto;
  textoBajo.toLowerCase();

  // ★ EXCEPCIÓN: Líneas solo de "=" siempre blancas
  bool esLinea = true;
  for (int i = 0; i < texto.length(); i++) {
    char c = texto[i];
    if (c != '=' && c != ' ' && c != '\t') {
      esLinea = false;
      break;
    }
  }
  if (esLinea && texto.indexOf("=") != -1) {
    return COLOR_NORMAL;
  }

  // ★ ERROR - Rojo (ANTES de TÍTULOS)
  if (textoBajo.indexOf("[error]") != -1 ||
      textoBajo.indexOf("error") != -1 || 
      textoBajo.indexOf("fallo") != -1 ||
      textoBajo.indexOf("no se pudo") != -1) {
    return COLOR_ERROR;
  }

  // ★ ÉXITO - Verde (ANTES de TÍTULOS)
  if (textoBajo.indexOf("[ok]") != -1 ||
      textoBajo.indexOf("correctamente") != -1 ||
      textoBajo.indexOf("conectado") != -1 ||
      textoBajo.indexOf("sincronizada") != -1 ||
      textoBajo.indexOf("listo") != -1 ||
      (textoBajo.indexOf("verificado") != -1 && textoBajo.length() > 25) ||
      (textoBajo.indexOf("encontrado") != -1 && textoBajo.length() > 25)) {
    return COLOR_EXITO;
  }

  // ★ WARNING - Naranja
  if (textoBajo.indexOf("timeout") != -1 ||
      textoBajo.indexOf("atención") != -1) {
    return COLOR_WARNING;
  }

  // ★ INPUT/ENTRADA - Amarillo (muy restrictivo)
  if ((textoBajo.indexOf("ingresa") != -1 && textoBajo.length() > 20) || 
      (textoBajo.indexOf("selecciona") != -1 && textoBajo.length() > 20) ||
      (textoBajo.indexOf("escribe") != -1 && textoBajo.length() > 20)) {
    return COLOR_INPUT;
  }

  // ★ TÍTULOS Y SECCIONES - Cyan (DESPUÉS de todo lo demás)
  if (texto.indexOf("[") != -1 && texto.indexOf("]") != -1 &&
      textoBajo.indexOf("[ok]") == -1 &&
      textoBajo.indexOf("[error]") == -1) {
    return COLOR_INFO;
  }

  // ★ Por defecto - Blanco
  return COLOR_NORMAL;

}

// ★ NUEVA FUNCIÓN: Limpiar y reemplazar caracteres especiales
String DisplayManager::limpiarCaracteresEspeciales(String texto) {

  // ★ NUEVO: Reemplazar símbolo de flecha
  texto.replace("→", "->");
  
  // Reemplazar caracteres especiales por ASCII equivalentes
  texto.replace("✓", "[OK]");
  texto.replace("✗", "[ERROR]");
  texto.replace("á", "a");
  texto.replace("é", "e");
  texto.replace("í", "i");
  texto.replace("ó", "o");
  texto.replace("ú", "u");
  texto.replace("ñ", "n");
  texto.replace("Á", "A");
  texto.replace("É", "E");
  texto.replace("Í", "I");
  texto.replace("Ó", "O");
  texto.replace("Ú", "U");
  texto.replace("Ñ", "N");

  return texto;

}

void DisplayManager::redraw() {

  tft.fillScreen(COLOR_FONDO);  // ★ Fondo gris oscuro

  // ★ HEADER mejorado
  tft.setTextColor(COLOR_TITULO, COLOR_FONDO);
  tft.setTextSize(3);
  tft.setCursor(10, 10);
  tft.println("SARA");

  tft.drawLine(0, 45, 480, 45, COLOR_TITULO);
  tft.drawLine(0, 46, 480, 46, COLOR_FONDO);  // Línea de separación

  tft.setTextSize(2);
  tft.setTextWrap(false);

  int visibleLines = 8;
  int start;

  if (autoFollow) {
    start = totalLines - visibleLines;
    if (start < 0)
      start = 0;
  } else {
    start = firstVisibleLine;
  }

  int y = 60;
  int end = start + visibleLines;

  if (end > totalLines)
    end = totalLines;

  for (int i = start; i < end; i++) {
    tft.setCursor(10, y);
    tft.setTextColor(lines[i].color, COLOR_FONDO);  // ★ Color según tipo
    tft.println(lines[i].texto);
    y += 35;
  }

}

void DisplayManager::addLine(String text) {

  // ★ Si es línea vacía, agregar un espacio NO vacío para mantener espaciado
  if (text.length() == 0) {
    text = " ";
  }

  // ★ Limpiar caracteres especiales
  text = limpiarCaracteresEspeciales(text);

  // ★ Detectar color según contenido
  uint16_t color = detectarColor(text);

  String linea = "";

  while (text.length() > 0) {

    int espacio = text.indexOf(' ');
    String palabra;

    if (espacio == -1) {
      palabra = text;
      text = "";
    } else {
      palabra = text.substring(0, espacio + 1);
      text = text.substring(espacio + 1);
    }

    // ★ AUMENTADO a 480 para menos wrapping
    if (tft.textWidth(linea + palabra) > 480) {

      if (totalLines < MAX_LINES) {
        lines[totalLines].texto = linea;
        lines[totalLines].color = color;
        totalLines++;
      } else {
        for (int i = 0; i < MAX_LINES - 1; i++) {
          lines[i] = lines[i + 1];
        }
        lines[MAX_LINES - 1].texto = linea;
        lines[MAX_LINES - 1].color = color;
      }

      linea = palabra;

    } else {

      linea += palabra;

    }
  }

  if (linea.length() > 0) {

    if (totalLines < MAX_LINES) {
      lines[totalLines].texto = linea;
      lines[totalLines].color = color;
      totalLines++;
    } else {
      for (int i = 0; i < MAX_LINES - 1; i++) {
        lines[i] = lines[i + 1];
      }
      lines[MAX_LINES - 1].texto = linea;
      lines[MAX_LINES - 1].color = color;
    }

  }

  if (autoFollow) {
    firstVisibleLine = max(0, totalLines - 7);
  }

  redraw();

}

void DisplayManager::update() {

  handleTouch();

}

void DisplayManager::handleTouch() {

  if (!ts.touched()) {
    touching = false;
    return;
  }

  TS_Point p = ts.getPoint();

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

  if (!touching) {
    touching = true;
    lastTouchY = y;
    return;
  }

  int deltaY = y - lastTouchY;

  if (abs(deltaY) > 20) {

    autoFollow = false;
    firstVisibleLine -= deltaY / 15;

    int maxStart = max(0, totalLines - 7);

    if (firstVisibleLine < 0)
      firstVisibleLine = 0;

    if (firstVisibleLine > maxStart)
      firstVisibleLine = maxStart;

    redraw();
    lastTouchY = y;

  }

}