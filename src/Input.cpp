#include "Input.h"

#include "Config.h"

Input::KeyTrack Input::_keys[KB_KEY_COUNT];
std::vector<KeyEvent> Input::_events;
bool Input::_fn = false;
bool Input::_ctrl = false;
bool Input::_shift = false;
bool Input::_alt = false;
bool Input::_opt = false;
bool Input::_optJustPressed = false;

namespace {

// The library stores layer values as `char`, but the modifier constants go up
// to 0xff. Comparing a signed char against 0xff never matches, so every read
// goes through this - the same thing Keyboard.cpp does internally.
inline uint8_t layerValue(char v) { return static_cast<uint8_t>(v); }

// Point2D_t carries default member initialisers, which stops it being an
// aggregate under the gnu++11 the ESP32 core builds with, so it cannot be
// brace-initialised.
inline Point2D_t keyPos(int index) {
  Point2D_t pos;
  pos.x = index % KB_COLS;
  pos.y = index / KB_COLS;
  return pos;
}

bool isModifierCode(uint8_t code) {
  switch (code) {
    case KEY_FN:
    case KEY_OPT:
    case KEY_LEFT_CTRL:
    case KEY_LEFT_SHIFT:
    case KEY_LEFT_ALT:
      return true;
    default:
      return false;
  }
}

} // namespace

// ==================== KEY EVENT HELPERS ====================

char KeyEvent::letter() const {
  if (code >= 'a' && code <= 'z') {
    return static_cast<char>(code);
  }
  if (code >= 'A' && code <= 'Z') {
    return static_cast<char>(code - 'A' + 'a');
  }
  return 0;
}

char KeyEvent::text() const {
  // The fn layer is navigation and commands, never text. Callers that want
  // Fn+';' to type a semicolon look at key() instead.
  if (fn) {
    return 0;
  }
  if (code == KEY_ENTER || code == KEY_BACKSPACE || code == KEY_TAB) {
    return 0;
  }

  const uint8_t value = (shift || caps) ? shifted : code;
  if (value < 0x20 || value >= 0x7F) {
    return 0;
  }
  return static_cast<char>(value);
}

int KeyEvent::functionKey() const {
  if (!fn || fnCode < KEY_F1 || fnCode > KEY_F12) {
    return 0;
  }
  return fnCode - KEY_F1 + 1;
}

// ==================== INPUT ====================

void Input::begin() {
  for (int i = 0; i < KB_KEY_COUNT; i++) {
    _keys[i] = KeyTrack();
  }
  _events.clear();
  _fn = _ctrl = _shift = _alt = _opt = false;
  _optJustPressed = false;
}

void Input::sample(bool (&pressed)[KB_KEY_COUNT]) {
  for (int i = 0; i < KB_KEY_COUNT; i++) {
    pressed[i] = false;
  }

  for (const Point2D_t &pos : M5Cardputer.Keyboard.keyList()) {
    if (pos.x < 0 || pos.x >= KB_COLS || pos.y < 0 || pos.y >= KB_ROWS) {
      continue; // Defensive: a corrupt FIFO event must not index out of bounds.
    }
    pressed[pos.y * KB_COLS + pos.x] = true;
  }
}

void Input::update() {
  _events.clear();

  bool pressed[KB_KEY_COUNT];
  sample(pressed);

  // Pass 1: resolve modifiers before emitting anything, so every event in this
  // frame carries a consistent modifier state regardless of matrix order.
  const bool wasOpt = _opt;
  _fn = _ctrl = _shift = _alt = _opt = false;

  for (int i = 0; i < KB_KEY_COUNT; i++) {
    if (!pressed[i]) {
      continue;
    }
    const Point2D_t pos = keyPos(i);
    switch (layerValue(M5Cardputer.Keyboard.getKeyValue(pos).value_first)) {
      case KEY_FN:         _fn = true; break;
      case KEY_OPT:        _opt = true; break;
      case KEY_LEFT_CTRL:  _ctrl = true; break;
      case KEY_LEFT_SHIFT: _shift = true; break;
      case KEY_LEFT_ALT:   _alt = true; break;
      default: break;
    }
  }
  _optJustPressed = _opt && !wasOpt;

  const uint32_t now = millis();
  // A misconfigured .ini must not turn autorepeat into a busy loop.
  const uint32_t repeatDelay = KEY_REPEAT_DELAY > 0 ? (uint32_t)KEY_REPEAT_DELAY : 500;
  const uint32_t repeatRate = KEY_REPEAT_RATE > 0 ? (uint32_t)KEY_REPEAT_RATE : 100;
  const bool caps = M5Cardputer.Keyboard.capslocked();

  // Pass 2: emit press and repeat events for non-modifier keys.
  for (int i = 0; i < KB_KEY_COUNT; i++) {
    KeyTrack &track = _keys[i];

    if (!pressed[i]) {
      track.down = false;
      continue;
    }

    const Point2D_t pos = keyPos(i);
    const KeyValue_t value = M5Cardputer.Keyboard.getKeyValue(pos);
    const uint8_t code = layerValue(value.value_first);

    if (isModifierCode(code)) {
      track.down = true;
      continue;
    }

    bool emit = false;
    bool isRepeat = false;

    if (!track.down) {
      // Edge: this is a genuinely new press. Comparing against the previous
      // frame is what stops a held key from being re-inserted when a second
      // key joins it mid-roll.
      track.down = true;
      track.pressedAt = now;
      track.lastRepeat = now;
      emit = true;
    } else if (now - track.pressedAt >= repeatDelay &&
               now - track.lastRepeat >= repeatRate) {
      track.lastRepeat = now;
      emit = true;
      isRepeat = true;
    }

    if (!emit) {
      continue;
    }

    KeyEvent event;
    event.code = code;
    event.shifted = layerValue(value.value_second);
    event.fnCode = layerValue(value.value_third);
    event.fn = _fn;
    event.ctrl = _ctrl;
    event.shift = _shift;
    event.alt = _alt;
    event.opt = _opt;
    event.caps = caps;
    event.repeat = isRepeat;
    _events.push_back(event);
  }
}

bool Input::anyKeyDown() {
  return !M5Cardputer.Keyboard.keyList().empty();
}

void Input::flush() {
  bool pressed[KB_KEY_COUNT];
  sample(pressed);

  const uint32_t now = millis();
  for (int i = 0; i < KB_KEY_COUNT; i++) {
    // Marking held keys as already-down (rather than clearing them) is the
    // point: on the next update() they produce neither a press nor an
    // immediate repeat, because their timers start from now.
    _keys[i].down = pressed[i];
    _keys[i].pressedAt = now;
    _keys[i].lastRepeat = now;
  }

  _events.clear();
  _optJustPressed = false;
}
