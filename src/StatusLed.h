#ifndef STATUSLED_H
#define STATUSLED_H

#include <cstdint>

// ==================== STATUS LED ====================
// The RGB LED on G21, driven through M5Unified's M5.Led. The board has had it
// all along and the app never lit it once.
//
// A steady colour reports what the app is doing - idle, something on the
// clipboard, unsaved edits, USB mass storage - and a brief flash acknowledges
// an operation. Every colour, the brightness and the on/off switch are
// settings.

enum LedState {
  LED_STATE_IDLE,       // Browsing, nothing pending
  LED_STATE_CLIPBOARD,  // Something is waiting to be pasted
  LED_STATE_EDIT,       // Editor open with unsaved changes
  LED_STATE_USB,        // USB mass storage active
};

class StatusLed {
public:
  static void begin();

  // The steady state. Cheap to call every frame: it only touches the hardware
  // when the resulting colour actually changes.
  static void setState(LedState state);

  // Brief acknowledgements. They take priority over the steady colour for
  // LED_FLASH_MS and then fall back to it.
  static void flashSuccess();
  static void flashError();

  // Drives flash expiry. Call once per loop.
  static void update();

  // Re-reads brightness and colours after the settings screen.
  static void applySettings();

  // Turns the LED off without changing the stored state, for shutdown paths.
  static void off();

private:
  static void show(uint32_t rgb);

  static LedState _state;
  static uint32_t _flashColor;
  static uint32_t _flashUntil;
  static uint32_t _lastShown;
  static bool _available;
  static bool _hasShown;
};

#endif // STATUSLED_H
