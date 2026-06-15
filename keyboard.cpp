#include "keyboard.h"
#include "display.h"

KeyboardManager Keyboard;

void KeyboardManager::begin() {
  keypad = new Keypad(
    makeKeymap(keys),
    rowPins,
    colPins,
    KEYBOARD_ROWS,
    KEYBOARD_COLS
  );
}

void KeyboardManager::update() {
  if (keypad == nullptr) return;

  char key = keypad->getKey();

  if (key) {
    handleKeyPress(key);
    displayInput();
  } else {
    // ★ Si NO hay tecla nueva, resetear el flag
    confirmationKeyPressed = false;
  }
}

String KeyboardManager::getInput() {
  return input;
}

void KeyboardManager::clearInput() {
  input = "";
  displayInput();
}

bool KeyboardManager::hasSentInput() {
  return enviado;
}

String KeyboardManager::getSentInput() {
  enviado = false;
  return inputEnviado;
}

void KeyboardManager::handleKeyPress(char key) {

  // ★ CUALQUIER TECLA cuenta para confirmación
  confirmationKeyPressed = true;
  
  // ★ AGREGAR DÍGITO (0-9)
  if (key >= '0' && key <= '9') {
    if (input.length() < 20) {
      input += key;
    }
  }

  // ★ BORRAR UN CARÁCTER (#)
  else if (key == '#') {
    if (input.length() > 0) {
      input.remove(input.length() - 1);
    }
  }

  // ★ BORRAR TODO (*)
  else if (key == '*') {
    input = "";
  }

  // ★ ENVIAR (A, B, C, D)
  else if (key == 'A' || key == 'B' || key == 'C' || key == 'D') {
    if (input.length() > 0) {
      inputEnviado = input;
      enviado = true;
      input = "";
      delay(100);
      while (keypad->getKey());
    }
  }
}

void KeyboardManager::displayInput() {
  if (pinMode) {
    // Mostrar asteriscos en lugar de números
    String asteriscos = "";
    for (int i = 0; i < input.length(); i++) {
      asteriscos += "*";
    }
    Display.mostrarInputKeyboard(asteriscos);
  } else {
    Display.mostrarInputKeyboard(input);
  }
}

bool KeyboardManager::anyKeyPressed() {
  if (confirmationKeyPressed) {
    confirmationKeyPressed = false;  // Consumir
    return true;
  }
  return false;
}

void KeyboardManager::setPinMode(bool mode) {
  pinMode = mode;
}