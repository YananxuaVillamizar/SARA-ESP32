#include "keyboard.h"
#include "display.h"

KeyboardManager Keyboard;

DisplayManager* keyboardDisplay = nullptr;  // ★ Puntero a DisplayManager

void KeyboardManager::begin() {

  // ★ Inicializar keypad con new
  keypad = new Keypad(
    makeKeymap(keys),
    rowPins,
    colPins,
    KEYBOARD_ROWS,
    KEYBOARD_COLS
  );

}

void KeyboardManager::setDisplay(DisplayManager* disp) {

  keyboardDisplay = disp;

}

void KeyboardManager::update() {

  if (keypad == nullptr) return;

  char key = keypad->getKey();

  if (key) {
    handleKeyPress(key);
    displayInput();
  }

}

String KeyboardManager::getInput() {

  return input;

}

void KeyboardManager::clearInput() {

  input = "";
  displayInput();

}

void KeyboardManager::setOnSendCallback(void (*callback)(String)) {

  onSendCallback = callback;

}

void KeyboardManager::handleKeyPress(char key) {

  // ★ AGREGAR DÍGITO (0-9)
  if (key >= '0' && key <= '9') {

    if (input.length() < 20) {
      input += key;
      Serial.print(key);
    }

  }

  // ★ BORRAR UN CARÁCTER (#)
  else if (key == '#') {

    if (input.length() > 0) {
      input.remove(input.length() - 1);
      Serial.println("[BACKSPACE]");
    }

  }

  // ★ BORRAR TODO (*)
  else if (key == '*') {

    input = "";
    Serial.println("[CLEAR ALL]");

  }

  // ★ ENVIAR (A, B, C, D)
  else if (key == 'A' || key == 'B' || key == 'C' || key == 'D') {

    Serial.println("[SEND] " + input);

    if (onSendCallback != nullptr) {
      onSendCallback(input);
    }

    input = "";

  }

}

void KeyboardManager::displayInput() {

  // ★ Mostrar en el header de la pantalla TFT
  if (keyboardDisplay == nullptr) return;

  keyboardDisplay->mostrarInputKeyboard(input);

}

bool KeyboardManager::hasInput() {
  return input.length() > 0;
}

String KeyboardManager::readDocument(unsigned long timeoutMs) {

  String result = "";
  unsigned long timeout = millis();

  logPrintln("\nIngresa el documento del usuario:");
  logPrintln("(Ej: 1234567890)\n");

  while (millis() - timeout < timeoutMs) {
    
    Keyboard.update();  // ★ Procesar input del teclado
    
    // ★ Si presionó enviar (A, B, C, D)
    if (input.length() > 0 && keypad->getKey() == 0) {
      // Esperar a que se procese
      continue;
    }
    
    // ★ Si envió algo
    if (onSendCallback != nullptr) {
      result = input;
      input = "";
      displayInput();
      return result;
    }
    
    Display.update();
    delay(50);
  }

  logPrintln("✗ Timeout");
  return "";

}

String KeyboardManager::readPIN(int digits, unsigned long timeoutMs) {

  String result = "";
  unsigned long timeout = millis();
  int intentos = 0;
  const int MAX_INTENTOS = 2;

  while (intentos < MAX_INTENTOS && millis() - timeout < timeoutMs) {
    
    intentos++;
    logPrint("Intento ");
    logPrint(intentos);
    logPrint("/");
    logPrintln(MAX_INTENTOS);
    logPrintln("Ingresa tu PIN (4 dígitos):");
    
    input = "";
    displayInput();
    timeout = millis();

    while (millis() - timeout < 15000) {
      
      Keyboard.update();
      Display.update();
      
      if (input.length() == digits) {
        logPrintln("\n→ Verificando PIN...");
        return input;
      }
      
      delay(50);
    }

    if (input.length() != digits) {
      logPrintln("✗ Timeout o PIN incorrecto");
      input = "";
      displayInput();
    }
  }

  return "";

}

String KeyboardManager::readOption(int maxOptions, unsigned long timeoutMs) {

  input = "";
  unsigned long timeout = millis();

  logPrint("Selecciona una opción (1-");
  logPrintln(maxOptions);

  while (millis() - timeout < timeoutMs) {
    
    Keyboard.update();
    Display.update();
    
    if (input.length() > 0) {
      int opcion = input.toInt();
      if (opcion >= 1 && opcion <= maxOptions) {
        return input;
      } else {
        logPrintln("✗ Opción inválida. Intenta de nuevo:");
        input = "";
        displayInput();
      }
    }
    
    delay(50);
  }

  logPrintln("✗ Timeout");
  return "";

}