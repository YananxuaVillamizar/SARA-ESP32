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

  // ★ ERROR - Rojo (debe estar ANTES)
  if (textoBajo.indexOf("[error]") != -1 ||
      textoBajo.indexOf("error") != -1 || 
      textoBajo.indexOf("fallo") != -1 ||
      textoBajo.indexOf("no se pudo") != -1) {
    return COLOR_ERROR;
  }

  // ★ ÉXITO - Verde
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

  // ★ INPUT/ENTRADA - Amarillo (solo si pregunta explícita)
  if ((textoBajo.indexOf("ingresa") != -1 && textoBajo.length() > 20) || 
      (textoBajo.indexOf("selecciona") != -1 && textoBajo.length() > 20) ||
      (textoBajo.indexOf("escribe") != -1 && textoBajo.length() > 20)) {
    return COLOR_INPUT;
  }

  // ★ TÍTULOS Y SECCIONES - Cyan (SOLO si comienza con [ y termina con ])
  if (texto[0] == '[' && texto[texto.length() - 1] == ']' &&
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

  // ★ Si es línea vacía, agregar un espacio
  if (text.length() == 0) {
    text = " ";
  }

  // ★ Limpiar caracteres especiales
  text = limpiarCaracteresEspeciales(text);

  // ★ Detectar color según contenido
  uint16_t color = detectarColor(text);

  // ★ NUEVO: Wrapping carácter por carácter para mayor precisión
  String linea = "";

  for (int i = 0; i < text.length(); i++) {
    char c = text[i];
    String temp = linea + c;

    // Si el carácter no cabe, guardar línea actual y comenzar nueva
    if (tft.textWidth(temp) > 455) {

      if (linea.length() > 0) {
        if (totalLines < MAX_LINES) {
          lines[totalLines].texto = linea;
          lines[totalLines].color = color;
          totalLines++;
        } else {
          for (int j = 0; j < MAX_LINES - 1; j++) {
            lines[j] = lines[j + 1];
          }
          lines[MAX_LINES - 1].texto = linea;
          lines[MAX_LINES - 1].color = color;
        }
      }

      linea = "";
      linea += c;  // Comenzar nueva línea con este carácter

    } else {

      linea += c;

    }
  }

  // Guardar última línea
  if (linea.length() > 0) {

    if (totalLines < MAX_LINES) {
      lines[totalLines].texto = linea;
      lines[totalLines].color = color;
      totalLines++;
    } else {
      for (int j = 0; j < MAX_LINES - 1; j++) {
        lines[j] = lines[j + 1];
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

void DisplayManager::mostrarInputKeyboard(String texto) {

  // Limpiar área del input (al lado de SARA)
  tft.fillRect(120, 10, 350, 30, COLOR_FONDO);

  // Dibujar el input
  tft.setTextColor(COLOR_INPUT, COLOR_FONDO);
  tft.setTextSize(2);
  tft.setCursor(130, 15);

  tft.print("Input: ");
  tft.print(texto);
  tft.print("_");  // Cursor

}