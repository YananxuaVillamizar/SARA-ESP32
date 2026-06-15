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
      
      // ★ Mostrar en Serial como el usuario escribió
      Serial.println(inputEnviado);
      
      input = "";
    }
  }

}

void KeyboardManager::displayInput() {
  Display.mostrarInputKeyboard(input);
}