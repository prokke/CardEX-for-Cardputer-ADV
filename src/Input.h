#ifndef INPUT_H
#define INPUT_H

#include <M5Cardputer.h>

#include <cstdint>
#include <vector>

// ==================== KEYBOARD INPUT LAYER ====================
//
// Why this exists instead of M5Cardputer.Keyboard.keysState():
//
//   1. In M5Cardputer 1.2.0 updateKeysState() handles the fn layer in its own
//      pass and returns early, so KeysState::word is ALWAYS EMPTY while Fn is
//      held. Letters have KEY_NONE in the fn layer and reach neither `word` nor
//      `hid_keys`. Every Fn+letter shortcut is therefore undetectable through
//      keysState() - which is what broke saving, quitting, copy/paste and the
//      USB mode exit on this board.
//   2. isChange() only compares how MANY keys are down, so swapping one key for
//      another of the same count reports "no change", and it mutates state on
//      every call.
//   3. keysState().word carries every key currently held, so the old code
//      re-inserted characters that were already typed whenever a finger roll
//      overlapped two keys.
//   4. getKey() treats Ctrl as Shift, so Ctrl could never be a real modifier.
//
// keyList() and getKeyValue() sit below all of that: they expose the raw 4x14
// key matrix and its three-layer value map, and have been stable since 1.0.x.
// This layer turns them into discrete, edge-triggered events with autorepeat.
//
// Call Input::update() once per frame, right after M5Cardputer.update().

// The Cardputer keyboard matrix, both on the original (74HC138 scan) and the
// ADV (TCA8418 remapped to the same geometry).
constexpr int KB_ROWS = 4;
constexpr int KB_COLS = 14;
constexpr int KB_KEY_COUNT = KB_ROWS * KB_COLS;

struct KeyEvent {
  // Layer values of the physical key, taken straight from the library's map.
  uint8_t code = KEY_NONE;    // unmodified: 'a', ';', or KEY_ENTER/BACKSPACE/TAB
  uint8_t shifted = KEY_NONE; // shift layer: 'A', ':'
  uint8_t fnCode = KEY_NONE;  // fn layer: KEY_UP, KEY_ESCAPE, KEY_F1, KEY_DELETE

  // Modifier state at the moment this event was produced.
  bool fn = false;
  bool ctrl = false;
  bool shift = false;
  bool alt = false;
  bool opt = false;
  bool caps = false;

  // True for autorepeat, false for the initial press. Callers that must not
  // repeat (destructive shortcuts, toggles) simply ignore repeat events.
  bool repeat = false;

  // The unmodified character of the physical key. This is what identifies a
  // key regardless of modifiers, so `key() == ';'` matches both the bare ';'
  // (navigation) and Fn+';' (typing a semicolon).
  char key() const { return static_cast<char>(code); }

  // Lower-cased a-z for shortcut dispatch, 0 for anything else.
  char letter() const;

  // The character this event should insert, or 0 when it is not text.
  // Enter, Backspace and Tab are excluded: their codes (0x28/0x2a/0x2b) collide
  // with the printable '(', '*' and '+', which only ever appear as SHIFTED
  // values, so they must be filtered by the unmodified code.
  char text() const;

  bool isEnter() const { return !fn && code == KEY_ENTER; }
  bool isBackspace() const { return !fn && code == KEY_BACKSPACE; }
  bool isTab() const { return !fn && code == KEY_TAB; }

  // Fn layer specials. Note that Fn+Backspace is forward-delete, which is why
  // isBackspace() requires !fn.
  bool isEsc() const { return fn && fnCode == KEY_ESCAPE; }
  bool isDeleteForward() const { return fn && fnCode == KEY_DELETE; }

  // Fn-layer arrows. CardEX keeps navigation on the bare ';' '.' ',' '/' keys,
  // so these are free for typing those characters; exposed for completeness.
  bool isFnUp() const { return fn && fnCode == KEY_UP; }
  bool isFnDown() const { return fn && fnCode == KEY_DOWN; }
  bool isFnLeft() const { return fn && fnCode == KEY_LEFT; }
  bool isFnRight() const { return fn && fnCode == KEY_RIGHT; }

  // Function keys F1..F12, or 0 when this is not one.
  int functionKey() const;
};

class Input {
public:
  // Resets all key tracking. Safe to call before the keyboard is initialised.
  static void begin();

  // Samples the matrix and produces this frame's events. Must be called after
  // M5Cardputer.update(), which is what refreshes keyList().
  static void update();

  // Events produced by the most recent update(), in key-index order.
  static const std::vector<KeyEvent> &events() { return _events; }

  // Live modifier state, for callers that need it outside of an event.
  static bool fn() { return _fn; }
  static bool ctrl() { return _ctrl; }
  static bool shift() { return _shift; }
  static bool alt() { return _alt; }
  static bool opt() { return _opt; }

  // True only on the frame Opt goes down. Opt is a modifier and never produces
  // a KeyEvent, but CardEX opens the help screen on Opt alone.
  static bool optJustPressed() { return _optJustPressed; }

  static bool anyKeyDown();

  // Adopts the currently held keys as "already handled": no press events are
  // emitted for them and their autorepeat timers restart. Call this after any
  // modal screen that ran its own input loop, otherwise a key still held on
  // exit either re-triggers or starts repeating at full speed immediately -
  // the bug that made dialogs feel like they "ran away" with the selection.
  static void flush();

private:
  struct KeyTrack {
    bool down = false;
    uint32_t pressedAt = 0;
    uint32_t lastRepeat = 0;
  };

  static void sample(bool (&pressed)[KB_KEY_COUNT]);

  static KeyTrack _keys[KB_KEY_COUNT];
  static std::vector<KeyEvent> _events;
  static bool _fn, _ctrl, _shift, _alt, _opt;
  static bool _optJustPressed;
};

#endif // INPUT_H
