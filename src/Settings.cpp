#include "Settings.h"

#include "StatusLed.h"

#include <M5Cardputer.h>
#include <SD.h>
#include <vector>

// ==================== RUNTIME VALUES ====================
uint16_t BG_COLOR = DEFAULT_BG_COLOR;
uint16_t TEXT_COLOR = DEFAULT_TEXT_COLOR;
uint16_t ACCENT_COLOR = DEFAULT_ACCENT_COLOR;
uint16_t SECONDARY_COLOR = DEFAULT_SECONDARY_COLOR;
uint16_t ERROR_COLOR = DEFAULT_ERROR_COLOR;
uint16_t WARNING_COLOR = DEFAULT_WARNING_COLOR;
uint16_t MENU_BG = DEFAULT_MENU_BG;
uint16_t SELECTED_BG = DEFAULT_SELECTED_BG;
uint16_t BORDER_COLOR = DEFAULT_BORDER_COLOR;
uint16_t CURSOR_COLOR = DEFAULT_CURSOR_COLOR;

int TAB_SIZE = DEFAULT_TAB_SIZE;
int CURSOR_BLINK_MS = DEFAULT_CURSOR_BLINK_MS;
int AUTO_SAVE_INTERVAL = DEFAULT_AUTO_SAVE_INTERVAL;
int MAX_UNDO_LEVELS = DEFAULT_MAX_UNDO_LEVELS;

int TOAST_DURATION = DEFAULT_TOAST_DURATION;
int KEY_REPEAT_DELAY = DEFAULT_KEY_REPEAT_DELAY;
int KEY_REPEAT_RATE = DEFAULT_KEY_REPEAT_RATE;
int SCREEN_BRIGHTNESS = DEFAULT_SCREEN_BRIGHTNESS;

int BAT_WARN_LEVEL = DEFAULT_BAT_WARN_LEVEL;
int BAT_CRIT_LEVEL = DEFAULT_BAT_CRIT_LEVEL;

bool SHOW_HIDDEN_FILES = false;
bool CONFIRM_DELETE = true;
int SORT_MODE = SORT_NAME;
bool SORT_DESCENDING = false;

bool SOUND_ENABLED = true;
bool SOUND_KEY_CLICK = false;
int SOUND_VOLUME = DEFAULT_SOUND_VOLUME;

bool LED_ENABLED = true;
int LED_BRIGHTNESS = DEFAULT_LED_BRIGHTNESS;
int LED_COLOR_IDLE = DEFAULT_LED_COLOR_IDLE;
int LED_COLOR_CLIPBOARD = DEFAULT_LED_COLOR_CLIPBOARD;
int LED_COLOR_EDIT = DEFAULT_LED_COLOR_EDIT;
int LED_COLOR_USB = DEFAULT_LED_COLOR_USB;
int LED_COLOR_OK = DEFAULT_LED_COLOR_OK;
int LED_COLOR_ERROR = DEFAULT_LED_COLOR_ERROR;

bool SHOW_LINE_NUMBERS = true;
bool AUTO_INDENT = true;
bool TAB_USES_SPACES = true;

// ==================== REGISTRY ====================
namespace {

const char *const kSortNames[] = {"Name", "Size", "Date"};

const char *const kCategoryNames[CAT_COUNT] = {
    "Appearance", "Battery", "File manager", "Editor",
    "Keyboard",   "Sound",   "Status LED",
};

// Fields in order: key, label, category, type, value, default, min, max, step,
// enumNames, enumCount.
const SettingDef kSettings[] = {
    // ---- Appearance ----
    {"BG_COLOR", "Background", CAT_APPEARANCE, SETTING_COLOR, &BG_COLOR,
     DEFAULT_BG_COLOR, 0, 0xFFFF, 0x0821, nullptr, 0},
    {"TEXT_COLOR", "Text", CAT_APPEARANCE, SETTING_COLOR, &TEXT_COLOR,
     DEFAULT_TEXT_COLOR, 0, 0xFFFF, 0x0821, nullptr, 0},
    {"ACCENT_COLOR", "Accent", CAT_APPEARANCE, SETTING_COLOR, &ACCENT_COLOR,
     DEFAULT_ACCENT_COLOR, 0, 0xFFFF, 0x0821, nullptr, 0},
    {"SECONDARY_COLOR", "Secondary", CAT_APPEARANCE, SETTING_COLOR,
     &SECONDARY_COLOR, DEFAULT_SECONDARY_COLOR, 0, 0xFFFF, 0x0821, nullptr, 0},
    {"ERROR_COLOR", "Error", CAT_APPEARANCE, SETTING_COLOR, &ERROR_COLOR,
     DEFAULT_ERROR_COLOR, 0, 0xFFFF, 0x0821, nullptr, 0},
    {"WARNING_COLOR", "Warning", CAT_APPEARANCE, SETTING_COLOR, &WARNING_COLOR,
     DEFAULT_WARNING_COLOR, 0, 0xFFFF, 0x0821, nullptr, 0},
    {"MENU_BG", "Bars", CAT_APPEARANCE, SETTING_COLOR, &MENU_BG,
     DEFAULT_MENU_BG, 0, 0xFFFF, 0x0821, nullptr, 0},
    {"SELECTED_BG", "Selection", CAT_APPEARANCE, SETTING_COLOR, &SELECTED_BG,
     DEFAULT_SELECTED_BG, 0, 0xFFFF, 0x0821, nullptr, 0},
    {"BORDER_COLOR", "Borders", CAT_APPEARANCE, SETTING_COLOR, &BORDER_COLOR,
     DEFAULT_BORDER_COLOR, 0, 0xFFFF, 0x0821, nullptr, 0},
    {"CURSOR_COLOR", "Cursor", CAT_APPEARANCE, SETTING_COLOR, &CURSOR_COLOR,
     DEFAULT_CURSOR_COLOR, 0, 0xFFFF, 0x0821, nullptr, 0},
    {"SCREEN_BRIGHTNESS", "Brightness", CAT_APPEARANCE, SETTING_INT,
     &SCREEN_BRIGHTNESS, DEFAULT_SCREEN_BRIGHTNESS, 10, 255, 15, nullptr, 0},
    {"TOAST_DURATION", "Message time", CAT_APPEARANCE, SETTING_INT,
     &TOAST_DURATION, DEFAULT_TOAST_DURATION, 500, 10000, 250, nullptr, 0},

    // ---- Battery ----
    {"BAT_WARN_LEVEL", "Warn below %", CAT_BATTERY, SETTING_INT,
     &BAT_WARN_LEVEL, DEFAULT_BAT_WARN_LEVEL, 0, 100, 5, nullptr, 0},
    {"BAT_CRIT_LEVEL", "Critical below %", CAT_BATTERY, SETTING_INT,
     &BAT_CRIT_LEVEL, DEFAULT_BAT_CRIT_LEVEL, 0, 100, 5, nullptr, 0},

    // ---- File manager ----
    {"SHOW_HIDDEN_FILES", "Show hidden", CAT_MANAGER, SETTING_BOOL,
     &SHOW_HIDDEN_FILES, 0, 0, 1, 1, nullptr, 0},
    {"CONFIRM_DELETE", "Confirm delete", CAT_MANAGER, SETTING_BOOL,
     &CONFIRM_DELETE, 1, 0, 1, 1, nullptr, 0},
    {"SORT_MODE", "Sort by", CAT_MANAGER, SETTING_ENUM, &SORT_MODE, SORT_NAME,
     0, 2, 1, kSortNames, 3},
    {"SORT_DESCENDING", "Reverse order", CAT_MANAGER, SETTING_BOOL,
     &SORT_DESCENDING, 0, 0, 1, 1, nullptr, 0},

    // ---- Editor ----
    {"TAB_SIZE", "Tab width", CAT_EDITOR, SETTING_INT, &TAB_SIZE,
     DEFAULT_TAB_SIZE, 1, 8, 1, nullptr, 0},
    {"TAB_USES_SPACES", "Tab inserts spaces", CAT_EDITOR, SETTING_BOOL,
     &TAB_USES_SPACES, 1, 0, 1, 1, nullptr, 0},
    {"AUTO_INDENT", "Auto indent", CAT_EDITOR, SETTING_BOOL, &AUTO_INDENT, 1, 0,
     1, 1, nullptr, 0},
    {"SHOW_LINE_NUMBERS", "Line numbers", CAT_EDITOR, SETTING_BOOL,
     &SHOW_LINE_NUMBERS, 1, 0, 1, 1, nullptr, 0},
    {"MAX_UNDO_LEVELS", "Undo steps", CAT_EDITOR, SETTING_INT, &MAX_UNDO_LEVELS,
     DEFAULT_MAX_UNDO_LEVELS, 1, 200, 5, nullptr, 0},
    {"CURSOR_BLINK_MS", "Cursor blink ms", CAT_EDITOR, SETTING_INT,
     &CURSOR_BLINK_MS, DEFAULT_CURSOR_BLINK_MS, 100, 2000, 50, nullptr, 0},
    {"AUTO_SAVE_INTERVAL", "Autosave ms (0 off)", CAT_EDITOR, SETTING_INT,
     &AUTO_SAVE_INTERVAL, DEFAULT_AUTO_SAVE_INTERVAL, 0, 600000, 15000, nullptr,
     0},

    // ---- Keyboard ----
    {"KEY_REPEAT_DELAY", "Repeat delay ms", CAT_KEYBOARD, SETTING_INT,
     &KEY_REPEAT_DELAY, DEFAULT_KEY_REPEAT_DELAY, 100, 2000, 50, nullptr, 0},
    {"KEY_REPEAT_RATE", "Repeat rate ms", CAT_KEYBOARD, SETTING_INT,
     &KEY_REPEAT_RATE, DEFAULT_KEY_REPEAT_RATE, 20, 1000, 10, nullptr, 0},

    // ---- Sound ----
    {"SOUND_ENABLED", "Sounds", CAT_SOUND, SETTING_BOOL, &SOUND_ENABLED, 1, 0,
     1, 1, nullptr, 0},
    {"SOUND_KEY_CLICK", "Key click", CAT_SOUND, SETTING_BOOL, &SOUND_KEY_CLICK,
     0, 0, 1, 1, nullptr, 0},
    {"SOUND_VOLUME", "Volume", CAT_SOUND, SETTING_INT, &SOUND_VOLUME,
     DEFAULT_SOUND_VOLUME, 0, 255, 10, nullptr, 0},

    // ---- Status LED ----
    {"LED_ENABLED", "LED", CAT_LED, SETTING_BOOL, &LED_ENABLED, 1, 0, 1, 1,
     nullptr, 0},
    {"LED_BRIGHTNESS", "Brightness", CAT_LED, SETTING_INT, &LED_BRIGHTNESS,
     DEFAULT_LED_BRIGHTNESS, 0, 255, 5, nullptr, 0},
    {"LED_COLOR_IDLE", "Idle", CAT_LED, SETTING_RGB, &LED_COLOR_IDLE,
     DEFAULT_LED_COLOR_IDLE, 0, 0xFFFFFF, 0x100010, nullptr, 0},
    {"LED_COLOR_CLIPBOARD", "Clipboard", CAT_LED, SETTING_RGB,
     &LED_COLOR_CLIPBOARD, DEFAULT_LED_COLOR_CLIPBOARD, 0, 0xFFFFFF, 0x100010,
     nullptr, 0},
    {"LED_COLOR_EDIT", "Unsaved edits", CAT_LED, SETTING_RGB, &LED_COLOR_EDIT,
     DEFAULT_LED_COLOR_EDIT, 0, 0xFFFFFF, 0x100010, nullptr, 0},
    {"LED_COLOR_USB", "USB mode", CAT_LED, SETTING_RGB, &LED_COLOR_USB,
     DEFAULT_LED_COLOR_USB, 0, 0xFFFFFF, 0x100010, nullptr, 0},
    {"LED_COLOR_OK", "Success flash", CAT_LED, SETTING_RGB, &LED_COLOR_OK,
     DEFAULT_LED_COLOR_OK, 0, 0xFFFFFF, 0x100010, nullptr, 0},
    {"LED_COLOR_ERROR", "Error flash", CAT_LED, SETTING_RGB, &LED_COLOR_ERROR,
     DEFAULT_LED_COLOR_ERROR, 0, 0xFFFFFF, 0x100010, nullptr, 0},
};

constexpr size_t kSettingCount = sizeof(kSettings) / sizeof(kSettings[0]);

String trimmed(const String &in) {
  String s = in;
  s.trim();
  return s;
}

// Strips a trailing "//" comment. Values are numeric, so nothing legitimate
// contains "//".
String stripComment(const String &in) {
  const int idx = in.indexOf("//");
  return idx >= 0 ? in.substring(0, idx) : in;
}

bool parseValue(const String &raw, int32_t &out) {
  String value = trimmed(stripComment(raw));
  if (value.length() == 0) {
    return false;
  }

  char *end = nullptr;
  const bool hex = value.startsWith("0x") || value.startsWith("0X");
  const long parsed = strtol(value.c_str(), &end, hex ? 16 : 10);
  if (end == value.c_str()) {
    return false; // Not a number at all.
  }
  out = (int32_t)parsed;
  return true;
}

const SettingDef *findByKey(const String &key) {
  for (size_t i = 0; i < kSettingCount; i++) {
    if (key.equals(kSettings[i].key)) {
      return &kSettings[i];
    }
  }
  return nullptr;
}

String formatForFile(const SettingDef &def) {
  const int32_t value = Settings::get(def);
  char buffer[16];
  if (def.type == SETTING_COLOR) {
    snprintf(buffer, sizeof(buffer), "0x%04X", (unsigned)(value & 0xFFFF));
    return String(buffer);
  }
  if (def.type == SETTING_RGB) {
    snprintf(buffer, sizeof(buffer), "0x%06X", (unsigned)(value & 0xFFFFFF));
    return String(buffer);
  }
  return String((int)value);
}

} // namespace

// ==================== ACCESS ====================
const SettingDef *Settings::all() { return kSettings; }
size_t Settings::count() { return kSettingCount; }

const char *Settings::categoryName(SettingCategory category) {
  return (category >= 0 && category < CAT_COUNT) ? kCategoryNames[category]
                                                 : "?";
}

int32_t Settings::get(const SettingDef &def) {
  switch (def.type) {
    case SETTING_BOOL:
      return *static_cast<bool *>(def.value) ? 1 : 0;
    case SETTING_COLOR:
      return *static_cast<uint16_t *>(def.value);
    default: // SETTING_INT, SETTING_ENUM, SETTING_RGB
      return *static_cast<int *>(def.value);
  }
}

void Settings::set(const SettingDef &def, int32_t value) {
  const int32_t clamped = clamp(def, value);
  switch (def.type) {
    case SETTING_BOOL:
      *static_cast<bool *>(def.value) = (clamped != 0);
      break;
    case SETTING_COLOR:
      *static_cast<uint16_t *>(def.value) = (uint16_t)clamped;
      break;
    default:
      *static_cast<int *>(def.value) = (int)clamped;
      break;
  }
}

int32_t Settings::clamp(const SettingDef &def, int32_t value) {
  int32_t low = def.minValue;
  int32_t high = def.maxValue;
  if (def.type == SETTING_ENUM && def.enumCount > 0) {
    high = def.enumCount - 1;
    low = 0;
  }
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

String Settings::valueText(const SettingDef &def) {
  const int32_t value = get(def);
  switch (def.type) {
    case SETTING_BOOL:
      return value ? "On" : "Off";
    case SETTING_COLOR: {
      char buffer[16];
      snprintf(buffer, sizeof(buffer), "0x%04X", (unsigned)(value & 0xFFFF));
      return String(buffer);
    }
    case SETTING_RGB: {
      char buffer[16];
      snprintf(buffer, sizeof(buffer), "0x%06X", (unsigned)(value & 0xFFFFFF));
      return String(buffer);
    }
    case SETTING_ENUM:
      if (def.enumNames && value >= 0 && value < (int32_t)def.enumCount) {
        return String(def.enumNames[value]);
      }
      return String((int)value);
    default:
      return String((int)value);
  }
}

void Settings::apply() {
  M5Cardputer.Display.setBrightness((uint8_t)SCREEN_BRIGHTNESS);
  M5Cardputer.Speaker.setVolume((uint8_t)SOUND_VOLUME);
  StatusLed::applySettings();

  // A critical threshold above the warning one would make the warning colour
  // unreachable; keep them ordered rather than rejecting the user's input.
  if (BAT_CRIT_LEVEL > BAT_WARN_LEVEL) {
    BAT_CRIT_LEVEL = BAT_WARN_LEVEL;
  }
}

void Settings::resetAll() {
  for (size_t i = 0; i < kSettingCount; i++) {
    set(kSettings[i], kSettings[i].defaultValue);
  }
  apply();
}

void Settings::resetCategory(SettingCategory category) {
  for (size_t i = 0; i < kSettingCount; i++) {
    if (kSettings[i].category == category) {
      set(kSettings[i], kSettings[i].defaultValue);
    }
  }
  apply();
}

// ==================== LOAD ====================
void Settings::load() {
  if (!SD.exists(CONFIG_FILE_PATH)) {
    save(); // Write the defaults so the file is there to edit.
    return;
  }

  File file = SD.open(CONFIG_FILE_PATH, FILE_READ);
  if (!file) {
    return;
  }

  while (file.available()) {
    const String line = trimmed(stripComment(file.readStringUntil('\n')));
    if (line.length() == 0) {
      continue;
    }

    const int separator = line.indexOf('=');
    if (separator < 0) {
      continue;
    }

    const SettingDef *def = findByKey(trimmed(line.substring(0, separator)));
    if (!def) {
      continue; // Unknown key: left alone here and preserved on save.
    }

    int32_t value = 0;
    if (parseValue(line.substring(separator + 1), value)) {
      // set() clamps, so a corrupt or hand-edited file cannot put the app into
      // an unusable state.
      set(*def, value);
    }
  }
  file.close();

  apply();
}

// ==================== SAVE ====================
bool Settings::save() {
  // Read the existing file so comments, blank lines and keys this build does
  // not know about survive. The old writer only ever emitted defaults, so
  // saving from the app would have thrown away everything else.
  std::vector<String> existing;
  bool seen[kSettingCount] = {false};

  if (SD.exists(CONFIG_FILE_PATH)) {
    File file = SD.open(CONFIG_FILE_PATH, FILE_READ);
    if (file) {
      while (file.available() && existing.size() < 300) {
        String line = file.readStringUntil('\n');
        line.replace("\r", "");
        existing.push_back(line);
      }
      file.close();
    }
  }

  const String tempPath = String(CONFIG_FILE_PATH) + ".tmp";
  SD.remove(tempPath);
  File out = SD.open(tempPath, FILE_WRITE);
  if (!out) {
    return false;
  }

  bool ok = true;
  for (String &line : existing) {
    const String cleaned = trimmed(stripComment(line));
    const int separator = cleaned.indexOf('=');

    const SettingDef *def =
        separator >= 0 ? findByKey(trimmed(cleaned.substring(0, separator)))
                       : nullptr;

    if (def) {
      seen[def - kSettings] = true;
      ok = out.println(String(def->key) + " = " + formatForFile(*def)) > 0;
    } else {
      // println() returns size_t, so a blank line writing just the terminator
      // still returns non-zero; nothing to check beyond that.
      ok = out.println(line) > 0;
    }
    if (!ok) {
      break;
    }
  }

  // Append anything the file did not already carry, grouped by category.
  if (ok) {
    for (int category = 0; category < CAT_COUNT && ok; category++) {
      bool headerWritten = false;
      for (size_t i = 0; i < kSettingCount && ok; i++) {
        if (seen[i] || kSettings[i].category != category) {
          continue;
        }
        if (!headerWritten) {
          out.println();
          out.println(String("// ") +
                      categoryName((SettingCategory)category));
          headerWritten = true;
        }
        ok = out.println(String(kSettings[i].key) + " = " +
                         formatForFile(kSettings[i])) > 0;
      }
    }
  }

  out.flush();
  out.close();

  if (!ok) {
    SD.remove(tempPath);
    return false;
  }

  SD.remove(CONFIG_FILE_PATH);
  return SD.rename(tempPath, CONFIG_FILE_PATH);
}

// ==================== COMPATIBILITY ====================
void loadConfig() { Settings::load(); }
