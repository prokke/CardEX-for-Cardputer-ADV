#include "SettingsScreen.h"

#include "Config.h"
#include "Input.h"
#include "Settings.h"
#include "UI.h"

#include <M5Cardputer.h>

namespace {

// Rows are drawn on the same 12px grid as the file list.
int visibleRows() { return (CONTENT_HEIGHT - 2) / LINE_HEIGHT; }

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
    if (def.type == SETTING_COLOR) {
      valueX -= 12;
      UI::canvas.fillRect(SCREEN_WIDTH - 12, y + 2, 9, 8,
                          (uint16_t)Settings::get(def));
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
          const int32_t step = (key == '/') ? def.step : -def.step;
          // Booleans and enums wrap so a single key reaches every value.
          if (def.type == SETTING_BOOL) {
            Settings::set(def, Settings::get(def) ? 0 : 1);
          } else if (def.type == SETTING_ENUM) {
            const int32_t count = def.enumCount > 0 ? def.enumCount : 1;
            Settings::set(def, (Settings::get(def) + (step > 0 ? 1 : count - 1)) %
                                   count);
          } else {
            Settings::set(def, Settings::get(def) + step);
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
