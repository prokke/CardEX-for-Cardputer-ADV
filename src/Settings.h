#ifndef SETTINGS_H
#define SETTINGS_H

#include "Config.h"
#include <Arduino.h>

// ==================== SETTINGS REGISTRY ====================
//
// One table describes every user-visible setting: its name in /CardEX.ini, its
// label, its type and its valid range. That single description drives parsing,
// clamping, the settings screen and writing the file back, so adding a setting
// is one line and cannot go half-done.
//
// This replaces a hand-written if/else chain that validated nothing:
// SCREEN_BRIGHTNESS = 9999 went straight to setBrightness(), a negative
// MAX_UNDO_LEVELS disabled undo trimming through a signed/unsigned comparison,
// and a bad colour could make the interface unreadable with no way back.

enum SettingType {
  SETTING_BOOL,
  SETTING_INT,
  SETTING_COLOR, // RGB565, stored in a uint16_t
  SETTING_ENUM,
};

enum SettingCategory {
  CAT_APPEARANCE,
  CAT_BATTERY,
  CAT_MANAGER,
  CAT_EDITOR,
  CAT_KEYBOARD,
  CAT_COUNT,
};

struct SettingDef {
  const char *key;   // Name in CardEX.ini
  const char *label; // Shown on the settings screen
  SettingCategory category;
  SettingType type;
  void *value;    // int*, bool* or uint16_t* depending on `type`
  int32_t defaultValue;
  int32_t minValue;
  int32_t maxValue;
  int32_t step;                 // Increment on the settings screen
  const char *const *enumNames; // SETTING_ENUM only
  uint8_t enumCount;
};

// Sort orders for the file list.
enum SortMode { SORT_NAME, SORT_SIZE, SORT_DATE };

class Settings {
public:
  // The registry.
  static const SettingDef *all();
  static size_t count();
  static const char *categoryName(SettingCategory category);

  // Typed access, dispatching on SettingDef::type.
  static int32_t get(const SettingDef &def);
  static void set(const SettingDef &def, int32_t value);

  // Clamps to [minValue, maxValue]; enums and bools clamp to their own range.
  static int32_t clamp(const SettingDef &def, int32_t value);

  // Formats the current value for display ("On", "Off", "0x07E0", "4", "Name").
  static String valueText(const SettingDef &def);

  // Applies everything that has an immediate side effect (screen brightness).
  static void apply();

  // Resets every setting, or one category, to its default.
  static void resetAll();
  static void resetCategory(SettingCategory category);

  // Reads /CardEX.ini, clamping every value. Missing file writes the defaults.
  static void load();

  // Writes /CardEX.ini, preserving comments and any keys it does not know
  // about. Returns false if the file could not be written.
  static bool save();
};

// Kept as a free function so existing call sites do not change.
void loadConfig();

#endif // SETTINGS_H
