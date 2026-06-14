#ifndef KEYBOARD_H
#define KEYBOARD_H
bool hasInput();  // ★ Verifica si hay input disponible
#include <Keypad.h>

class DisplayManager;
#define KEYBOARD_ROWS 4
#define KEYBOARD_COLS 4

class KeyboardManager {

  public:

    void begin();

    void setDisplay(DisplayManager* disp);  // ★ NUEVA: referencia a Display

    void update();  // ★ Llamar en el loop principal

    String getInput();

    void clearInput();

    // Callback para cuando se envía
    void setOnSendCallback(void (*callback)(String));

    // ★ Funciones para leer entrada con timeout
    String readDocument(unsigned long timeoutMs = 30000);
    String readPIN(int digits = 4, unsigned long timeoutMs = 30000);
    String readOption(int maxOptions, unsigned long timeoutMs = 15000);

  private:

    Keypad* keypad = nullptr;  // ★ Puntero, no objeto directo

    String input = "";

    char keys[KEYBOARD_ROWS][KEYBOARD_COLS] = {
      {'1','2','3','A'},
      {'4','5','6','B'},
      {'7','8','9','C'},
      {'*','0','#','D'}
    };

    byte rowPins[KEYBOARD_ROWS] = {15, 4, 5, 18};
    byte colPins[KEYBOARD_COLS] = {19, 21, 22, 23};

    void (*onSendCallback)(String) = nullptr;

    void handleKeyPress(char key);

    void displayInput();

};

extern KeyboardManager Keyboard;

#endif