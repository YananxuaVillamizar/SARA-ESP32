#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <Keypad.h>

class DisplayManager;

#define KEYBOARD_ROWS 4
#define KEYBOARD_COLS 4

class KeyboardManager {

  public:

    void begin();
    void update();
    
    String getInput();
    void clearInput();
    
    bool hasSentInput();  // ★ Verifica si se envió algo
    String getSentInput();  // ★ Obtiene lo que se envió
    bool anyKeyPressed();  // ★ Devuelve true si se presionó cualquier tecla (0-9, *, #, A-D)
    void setPinMode(bool mode);  // ★ Activar/desactivar modo PIN

  private:

    Keypad* keypad = nullptr;
    String input = "";
    String inputEnviado = "";  // ★ Lo que se envió
    bool enviado = false;  // ★ Flag de envío

    char keys[KEYBOARD_ROWS][KEYBOARD_COLS] = {
      {'1','2','3','A'},
      {'4','5','6','B'},
      {'7','8','9','C'},
      {'*','0','#','D'}
    };

    byte rowPins[KEYBOARD_ROWS] = {15, 4, 5, 18};
    byte colPins[KEYBOARD_COLS] = {19, 21, 22, 23};

    void handleKeyPress(char key);
    void displayInput();
    bool lastKeyPressed = false;  // ★ Flag para cualquier tecla presionada
    bool confirmationKeyPressed = false;  // ★ NUEVO: solo para confirmación
    bool pinMode = false;  // ★ Flag para mostrar asteriscos

};

extern KeyboardManager Keyboard;

#endif