#include "StatusLed.h"

#include "Config.h"

#include <M5Cardputer.h>

LedState StatusLed::_state = LED_STATE_IDLE;
uint32_t StatusLed::_flashColor = 0;
uint32_t StatusLed::_flashUntil = 0;
uint32_t StatusLed::_lastShown = 0;
bool StatusLed::_available = false;
bool StatusLed::_hasShown = false;

namespace {

// How long an acknowledgement stays lit.
constexpr uint32_t kFlashMs = 250;

uint32_t colorForState(LedState state) {
  switch (state) {
    case LED_STATE_CLIPBOARD: return (uint32_t)LED_COLOR_CLIPBOARD;
    case LED_STATE_EDIT:      return (uint32_t)LED_COLOR_EDIT;
    case LED_STATE_USB:       return (uint32_t)LED_COLOR_USB;
    default:                  return (uint32_t)LED_COLOR_IDLE;
  }
}

} // namespace

void StatusLed::begin() {
  _available = M5.Led.begin();
  if (!_available) {
    Serial.println("StatusLed: no RGB LED on this board");
    return;
  }
  M5.Led.setAutoDisplay(true);
  applySettings();
}

void StatusLed::show(uint32_t rgb) {
  if (!_available) {
    return;
  }
  // Only touch the LED when the colour actually changes. setAllColor() drives
  // a bit-banged WS2812 waveform with interrupts disabled, so calling it every
  // frame would add jitter to the keyboard polling for no visible benefit.
  if (_hasShown && rgb == _lastShown) {
    return;
  }
  _lastShown = rgb;
  _hasShown = true;

  if (!LED_ENABLED) {
    M5.Led.setAllColor(0, 0, 0);
    return;
  }

  M5.Led.setAllColor((uint8_t)((rgb >> 16) & 0xFF), (uint8_t)((rgb >> 8) & 0xFF),
                     (uint8_t)(rgb & 0xFF));
}

void StatusLed::setState(LedState state) {
  _state = state;
  if (millis() < _flashUntil) {
    return; // A flash is still showing; it restores the steady colour itself.
  }
  show(colorForState(state));
}

void StatusLed::flashSuccess() {
  _flashColor = (uint32_t)LED_COLOR_OK;
  _flashUntil = millis() + kFlashMs;
  show(_flashColor);
}

void StatusLed::flashError() {
  _flashColor = (uint32_t)LED_COLOR_ERROR;
  _flashUntil = millis() + kFlashMs;
  show(_flashColor);
}

void StatusLed::update() {
  if (_flashUntil != 0 && millis() >= _flashUntil) {
    _flashUntil = 0;
    show(colorForState(_state));
  }
}

void StatusLed::applySettings() {
  if (!_available) {
    return;
  }
  M5.Led.setBrightness((uint8_t)LED_BRIGHTNESS);
  // Force the next show() through, since brightness and colours may both have
  // changed while the resulting RGB value did not.
  _hasShown = false;
  show(millis() < _flashUntil ? _flashColor : colorForState(_state));
}

void StatusLed::off() {
  if (!_available) {
    return;
  }
  M5.Led.setAllColor(0, 0, 0);
  _hasShown = false;
}
