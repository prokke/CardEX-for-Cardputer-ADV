#include "Feedback.h"

#include "Config.h"

#include <M5Cardputer.h>

namespace {

// Deliberately short. Anything longer than a click is irritating on a device
// you type on.
void tone(int frequency, int durationMs) {
  if (!SOUND_ENABLED) {
    return;
  }
  M5Cardputer.Speaker.setVolume((uint8_t)SOUND_VOLUME);
  M5Cardputer.Speaker.tone(frequency, durationMs);
}

} // namespace

void Feedback::begin() {
  M5Cardputer.Speaker.begin();
  M5Cardputer.Speaker.setVolume((uint8_t)SOUND_VOLUME);
}

void Feedback::key() {
  if (!SOUND_KEY_CLICK) {
    return;
  }
  tone(2200, 6);
}

void Feedback::confirm() { tone(1500, 40); }

void Feedback::error() {
  // Low and a touch longer, so it is distinguishable without looking.
  tone(320, 120);
}

void Feedback::done() { tone(1800, 60); }
