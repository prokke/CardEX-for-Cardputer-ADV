#include "SettingsScreen.h"

#include "Config.h"
#include "Input.h"
#include "Settings.h"
#include "UI.h"

#include <M5Cardputer.h>

namespace {

// Rows are drawn on the same 12px grid as the file list.
int visibleRows() { return (CONTENT_HEIGHT - 2) / LINE_HEIGHT; }

// Colours step through presets rather than incrementing the packed value.
// Adding a constant to an RGB565 or RGB888 integer walks through essentially
// random colours, which is unusable on a device with no colour picker.
const uint32_t kPalette[] = {
    0x000000, 0x303030, 0x606060, 0x9E9E9E, 0xFFFFFF, 0xFF0000,
    0xFF6000, 0xFFC000, 0xFFFF00, 0x00FF00, 0x00FF80, 0x00FFFF,
    0x00C0FF, 0x0040FF, 0x0000FF, 0x8000FF, 0xFF00FF, 0xFF0080,
};
constexpr int kPaletteCount = sizeof(kPalette) / sizeof(kPalette[0]);

uint16_t toRgb565(uint32_t rgb) {
  return (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) |
                    ((rgb >> 3) & 0x001F));
}

// The value a palette entry takes for this setting: RGB565 for screen
// colours, 24-bit for the LED.
int32_t paletteValue(const SettingDef &def, int index) {
  const uint32_t rgb = kPalette[(index % kPaletteCount + kPaletteCount) % kPaletteCount];
  return def.type == SETTING_COLOR ? (int32_t)toRgb565(rgb) : (int32_t)rgb;
}

// Where the current value sits in the palette, or -1 when it was set by hand
// in the .ini to something not listed.
int paletteIndexOf(const SettingDef &def) {
  const int32_t current = Settings::get(def);
  for (int i = 0; i < kPaletteCount; i++) {
    if (paletteValue(def, i) == current) {
      return i;
    }
  }
  return -1;
}

// Preview swatch colour for a row.
uint16_t swatchColor(const SettingDef &def) {
  const int32_t value = Settings::get(def);
  return def.type == SETTING_COLOR ? (uint16_t)value : toRgb565((uint32_t)value);
}

void drawScreen(int selected, int scrollOffset, bool dirty) {
  const SettingDef *settings = Settings::all();
  const int total = (int)Settings::count();

  UI::drawHeader(dirty ? "Settings *" : "Settings", true,
                 M5Cardputer.Power.getBatteryLevel());

  int y = HEADER_HEIGHT + 2;
  const int rows = visibleRows();

  for (int i = 0; i < rows; i++) {
    const int index = i + scrollOffset;

    if (index >= total) {
      UI::canvas.fillRect(0, y, SCREEN_WIDTH, LINE_HEIGHT, BG_COLOR);
      y += LINE_HEIGHT;
      continue;
    }

    const SettingDef &def = settings[index];
    const bool isSelected = (index == selected);

    UI::canvas.fillRect(0, y, SCREEN_WIDTH, LINE_HEIGHT,
                        isSelected ? SELECTED_BG : BG_COLOR);

    // A category marker on the first row of each group, so the flat list still
    // reads as sections without spending a whole row on a header.
    const bool startsCategory =
        (index == 0) || (settings[index - 1].category != def.category);
    if (startsCategory) {
      UI::canvas.fillRect(0, y, 2, LINE_HEIGHT, ACCENT_COLOR);
    }

    UI::canvas.setTextColor(TEXT_COLOR, isSelected ? SELECTED_BG : BG_COLOR);
    UI::canvas.drawString(UI::truncateString(def.label, 24), 6, y + 2);

    const String value = Settings::valueText(def);
    // Colours are shown as a swatch as well as a number - a hex value alone is
    // unreadable when you are trying to pick one.
    int valueX = SCREEN_WIDTH - ((int)value.length() * CHAR_WIDTH) - 4;
    if (def.type == SETTING_COLOR || def.type == SETTING_RGB) {
      valueX -= 12;
      UI::canvas.fillRect(SCREEN_WIDTH - 12, y + 2, 9, 8, swatchColor(def));
      UI::canvas.drawRect(SCREEN_WIDTH - 12, y + 2, 9, 8, BORDER_COLOR);
    }
    UI::canvas.setTextColor(isSelected ? ACCENT_COLOR : SECONDARY_COLOR,
                            isSelected ? SELECTED_BG : BG_COLOR);
    UI::canvas.drawString(value, valueX, y + 2);

    y += LINE_HEIGHT;
  }

  // Scroll indicator
  if (total > rows) {
    const int barHeight = CONTENT_HEIGHT - 4;
    const int thumbHeight = max(10, (rows * barHeight) / total);
    const int thumbY =
        HEADER_HEIGHT + 2 + (scrollOffset * (barHeight - thumbHeight)) /
                                max(1, total - rows);
    UI::canvas.fillRect(SCREEN_WIDTH - 3, thumbY, 2, thumbHeight, ACCENT_COLOR);
  }

  UI::drawFooter(", / change  R reset  Esc save");
  UI::pushCanvas();
}

} // namespace

bool SettingsScreen::run() {
  Input::flush();

  const SettingDef *settings = Settings::all();
  const int total = (int)Settings::count();
  const int rows = visibleRows();

  int selected = 0;
  int scrollOffset = 0;
  bool dirty = false;
  bool running = true;

  while (running) {
    drawScreen(selected, scrollOffset, dirty);

    M5Cardputer.update();
    Input::update();

    if (Input::optJustPressed()) {
      break;
    }

    for (const KeyEvent &event : Input::events()) {
      // Same navigation as everywhere else: bare ';' and '.' move, ',' and '/'
      // adjust. Holding a key repeats, which matters for the 0..255 ranges.
      const char key = event.key();
      const bool isNav = !event.fn && (key == ';' || key == '.' || key == ',' ||
                                       key == '/');
      if (event.repeat && !isNav) {
        continue;
      }

      if (event.isEsc() || event.isBackspace()) {
        running = false;
        break;
      }

      const SettingDef &def = settings[selected];

      switch (key) {
        case ';':
          if (selected > 0) selected--;
          continue;
        case '.':
          if (selected < total - 1) selected++;
          continue;
        case ',':
        case '/': {
          if (event.fn) {
            continue; // Fn+',' and Fn+'/' type characters elsewhere; ignore here.
          }
          const bool forward = (key == '/');
          // Booleans, enums and colours all wrap, so a single key reaches
          // every value without having to hold it.
          if (def.type == SETTING_BOOL) {
            Settings::set(def, Settings::get(def) ? 0 : 1);
          } else if (def.type == SETTING_ENUM) {
            const int32_t count = def.enumCount > 0 ? def.enumCount : 1;
            Settings::set(def,
                          (Settings::get(def) + (forward ? 1 : count - 1)) % count);
          } else if (def.type == SETTING_COLOR || def.type == SETTING_RGB) {
            const int current = paletteIndexOf(def);
            // A hand-edited colour that is not in the palette starts from the
            // beginning rather than jumping somewhere arbitrary.
            const int next = (current < 0) ? 0 : current + (forward ? 1 : -1);
            Settings::set(def, paletteValue(def, next));
          } else {
            Settings::set(def, Settings::get(def) + (forward ? def.step : -def.step));
          }
          dirty = true;
          Settings::apply();
          continue;
        }
        default:
          break;
      }

      if (event.letter() == 'r') {
        Settings::resetCategory(def.category);
        dirty = true;
        continue;
      }
      if (event.isEnter() && def.type == SETTING_BOOL) {
        Settings::set(def, Settings::get(def) ? 0 : 1);
        dirty = true;
        continue;
      }
    }

    // Keep the selection on screen.
    if (selected < scrollOffset) {
      scrollOffset = selected;
    } else if (selected >= scrollOffset + rows) {
      scrollOffset = selected - rows + 1;
    }
    scrollOffset = constrain(scrollOffset, 0, max(0, total - rows));

    delay(10);
  }

  if (dirty && !Settings::save()) {
    UI::showToast("Could not write CardEX.ini", ERROR_COLOR);
  }

  Input::flush();
  UI::clearScreen();
  UI::pushCanvas();
  return dirty;
}
