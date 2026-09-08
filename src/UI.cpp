#include "UI.h"

#include "Feedback.h"
#include "StatusLed.h"

// Static variables
M5Canvas UI::canvas(&M5Cardputer.Display);
unsigned long UI::toastStartTime = 0;
String UI::toastMessage = "";
uint16_t UI::toastColor = TEXT_COLOR;
bool UI::toastActive = false;
bool UI::canvasReady = false;

// ==================== INIT ====================
void UI::init() {
  canvas.setColorDepth(16);
  // 240 * 135 * 2 = 64,800 bytes of an ~320KB heap. The result was never
  // checked, so a failed allocation left every draw call writing to nothing
  // and the screen simply stayed blank with no clue why.
  if (!canvas.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT)) {
    Serial.println("UI: canvas allocation failed - drawing direct to display");
    canvasReady = false;
  } else {
    canvasReady = true;
  }
  canvas.setTextSize(1);
  canvas.setTextColor(TEXT_COLOR, BG_COLOR);
  clearScreen();
}

// ==================== TARGET ====================
// Falls back to the display itself when the off-screen canvas could not be
// allocated. Slower and it flickers, but the app stays usable.
M5GFX &UI::displayTarget() { return M5Cardputer.Display; }
bool UI::hasCanvas() { return canvasReady; }

// ==================== PUSH CANVAS ====================
void UI::pushCanvas() {
  if (!canvasReady) {
    return; // Already drawn straight to the display.
  }
  drawToast();
  canvas.pushSprite(0, 0);
}

// ==================== DRAW HEADER ====================
void UI::drawHeader(const String &path, bool isSD, int battery) {
  canvas.fillRect(0, 0, SCREEN_WIDTH, HEADER_HEIGHT, MENU_BG);
  canvas.setTextColor(TEXT_COLOR, MENU_BG);

  // Path (truncated if needed)
  String displayPath = truncateString(path, 28);
  canvas.drawString(displayPath, 2, 2);

  // Storage indicator
  String storage = isSD ? "[SD]" : "[FL]";
  canvas.drawString(storage, 170, 2);

  // Battery. Stays a percentage - it is the most information for the fewest
  // pixels on a 240px header - but the colour carries the warning.
  uint16_t batColor = TEXT_COLOR;
  if (battery <= BAT_CRIT_LEVEL) {
    batColor = ERROR_COLOR;
  } else if (battery <= BAT_WARN_LEVEL) {
    batColor = WARNING_COLOR;
  }
  if (M5Cardputer.Power.isCharging()) {
    batColor = ACCENT_COLOR;
  }
  canvas.setTextColor(batColor, MENU_BG);
  String bat = String(battery) + "%";
  canvas.drawString(bat, 210, 2);
  canvas.setTextColor(TEXT_COLOR, MENU_BG);

  // Border
  canvas.drawLine(0, HEADER_HEIGHT - 1, SCREEN_WIDTH, HEADER_HEIGHT - 1,
                  BORDER_COLOR);
}

// ==================== DRAW FOOTER ====================
void UI::drawFooter(const String &hints) {
  int footerY = SCREEN_HEIGHT - FOOTER_HEIGHT;
  canvas.fillRect(0, footerY, SCREEN_WIDTH, FOOTER_HEIGHT, MENU_BG);

  // Border
  canvas.drawLine(0, footerY, SCREEN_WIDTH, footerY, BORDER_COLOR);

  // Hints only. The toast is painted by drawToast() from pushCanvas(), so it
  // appears over whatever is on screen - including the editor, which does not
  // draw a footer at all and therefore never showed a single toast. "File
  // saved" was invisible, and the flag stayed set until the user happened to
  // return to the file list.
  canvas.setTextColor(SECONDARY_COLOR, MENU_BG);
  String displayHints = truncateString(hints, 38);
  canvas.drawString(displayHints, 2, footerY + 2);
}

// ==================== DRAW TOAST ====================
void UI::drawToast() {
  if (!toastActive) {
    return;
  }
  if (millis() - toastStartTime >= (unsigned long)TOAST_DURATION) {
    toastActive = false;
    return;
  }

  const String text = truncateString(toastMessage, 38);
  const int width = min(SCREEN_WIDTH, (int)text.length() * CHAR_WIDTH + 8);
  const int x = (SCREEN_WIDTH - width) / 2;
  const int y = SCREEN_HEIGHT - FOOTER_HEIGHT - 14;

  canvas.fillRect(x, y, width, 12, MENU_BG);
  canvas.drawRect(x, y, width, 12, toastColor);
  canvas.setTextColor(toastColor, MENU_BG);
  canvas.drawString(text, x + 4, y + 2);
}

bool UI::toastVisible() {
  return toastActive &&
         (millis() - toastStartTime < (unsigned long)TOAST_DURATION);
}

// ==================== DRAW FOOTER PROGRESS ====================
void UI::drawFooterProgress(int percent, const String &status) {
   int footerY = SCREEN_HEIGHT - FOOTER_HEIGHT;
   // Don't fully clear, just overlay
   canvas.fillRect(0, footerY, SCREEN_WIDTH, FOOTER_HEIGHT, MENU_BG);
   canvas.drawLine(0, footerY, SCREEN_WIDTH, footerY, BORDER_COLOR);
   
   // Draw Bar
   int barWidth = 100;
   int barHeight = 6; // Reduced to 6px to fit 12px footer nicely (3px top/bottom space)
   int barX = SCREEN_WIDTH - barWidth - 5;
   int barY = footerY + 3; // Centered: (12-6)/2 = 3
   
   canvas.drawRect(barX, barY, barWidth, barHeight, BORDER_COLOR);
   int fillW = (barWidth - 4) * percent / 100;
   canvas.fillRect(barX + 2, barY + 2, fillW, barHeight - 4, ACCENT_COLOR);
   
   // Status text
   canvas.setTextColor(TEXT_COLOR, MENU_BG);
   canvas.drawString(status, 5, footerY + 2); // Centered text (y+2)
   
   pushCanvas();
}

// ==================== DRAW FILE LIST ====================
// Row layout, left to right. The star column is reserved whether or not the
// entry is bookmarked, so names stay aligned down the list:
//
//   0..2   selection bar
//   5..13  type icon
//   15..21 bookmark star
//   23..   name, with the size right-aligned
void UI::drawFileList(const std::vector<FileEntry> &files, int selected,
                      int scrollOffset) {
  int y = HEADER_HEIGHT + 2;
  // Use -2 to account for the top offset (y = HEADER + 2)
  // 111 - 2 = 109. 109 / 12 = 9.08 -> 9 lines.
  const int visibleLines = (CONTENT_HEIGHT - 2) / LINE_HEIGHT;

  for (int i = 0; i < visibleLines; i++) {
    const int fileIndex = i + scrollOffset;

    if (fileIndex >= (int)files.size()) {
      // Clear empty line
      canvas.fillRect(0, y, SCREEN_WIDTH, LINE_HEIGHT, BG_COLOR);
      y += LINE_HEIGHT;
      continue;
    }

    const FileEntry &entry = files[fileIndex];
    const bool isCursor = (fileIndex == selected);
    const uint16_t rowBg = isCursor ? SELECTED_BG : BG_COLOR;

    canvas.fillRect(0, y, SCREEN_WIDTH, LINE_HEIGHT, rowBg);

    // Multi-selection bar. Distinct from the cursor highlight so both can be
    // read at once - the cursor can sit on an unselected row and vice versa.
    if (entry.selected) {
      canvas.fillRect(0, y + 1, 3, LINE_HEIGHT - 2, ACCENT_COLOR);
    }

    // Type icon
    const int iconX = 5;
    const int iconY = y + 2;
    if (entry.isDirectory) {
      canvas.fillRect(iconX, iconY + 1, 8, 5, WARNING_COLOR);
      canvas.fillRect(iconX, iconY - 1, 4, 2, WARNING_COLOR);
    } else {
      canvas.drawRect(iconX + 1, iconY, 6, 8, SECONDARY_COLOR);
      canvas.drawLine(iconX + 2, iconY + 2, iconX + 5, iconY + 2, SECONDARY_COLOR);
      canvas.drawLine(iconX + 2, iconY + 4, iconX + 5, iconY + 4, SECONDARY_COLOR);
    }

    // Bookmark star
    if (entry.bookmarked) {
      canvas.setTextColor(ACCENT_COLOR, rowBg);
      canvas.drawString("*", 16, y + 2);
    }

    // Size first, so the name knows how much room is left.
    const String sizeStr = entry.getSizeStr();
    const int sizeX = SCREEN_WIDTH - ((int)sizeStr.length() * CHAR_WIDTH) - 2;
    canvas.setTextColor(SECONDARY_COLOR, rowBg);
    canvas.drawString(sizeStr, sizeX, y + 2);

    const int nameX = 23;
    const int nameChars = (sizeX - nameX - 4) / CHAR_WIDTH;
    canvas.setTextColor(entry.selected ? ACCENT_COLOR : TEXT_COLOR, rowBg);
    canvas.drawString(truncateString(entry.name, max(4, nameChars)), nameX,
                      y + 2);

    y += LINE_HEIGHT;
  }

  // Scroll indicator
  if ((int)files.size() > visibleLines) {
    const int scrollBarHeight = CONTENT_HEIGHT - 4;
    const int thumbHeight =
        max(10, (visibleLines * scrollBarHeight) / (int)files.size());
    const int thumbY = HEADER_HEIGHT + 2 +
                       (scrollOffset * (scrollBarHeight - thumbHeight)) /
                           max(1, (int)files.size() - visibleLines);

    canvas.fillRect(SCREEN_WIDTH - 3, thumbY, 2, thumbHeight, ACCENT_COLOR);
  }
}

// ==================== SHOW CONFIRM DIALOG ====================
bool UI::showConfirmDialog(const String &title, const String &message) {
  // The chord that opened this dialog is still held; drop it so it cannot be
  // read as an answer here.
  Input::flush();

  // Draw dialog box
  int boxW = 200;
  int boxH = 70;
  int boxX = (SCREEN_WIDTH - boxW) / 2;
  int boxY = (SCREEN_HEIGHT - boxH) / 2;

  canvas.fillRect(boxX, boxY, boxW, boxH, MENU_BG);
  canvas.drawRect(boxX, boxY, boxW, boxH, ACCENT_COLOR);

  // Title
  canvas.setTextColor(ACCENT_COLOR, MENU_BG);
  drawCenteredText(title, boxY + 5, ACCENT_COLOR);

  // Message
  canvas.setTextColor(TEXT_COLOR, MENU_BG);
  drawCenteredText(message, boxY + 20, TEXT_COLOR);

  // Buttons
  canvas.drawString("[Y]es", boxX + 30, boxY + 50);
  canvas.drawString("[N]o", boxX + 130, boxY + 50);

  pushCanvas(); // Show dialog

  // Wait for input. Input::flush() on the way out stops the key that opened
  // this dialog from being seen again - or from autorepeating - by the caller.
  bool result = false;
  bool decided = false;
  while (!decided) {
    M5Cardputer.update();
    Input::update();

    for (const KeyEvent &event : Input::events()) {
      if (event.repeat) {
        continue;
      }
      if (event.isEnter()) {
        result = true;
        decided = true;
        break;
      }
      // Esc and Backspace both cancel; Esc is the one the input dialog
      // documents, Backspace matches "go back" elsewhere in the app.
      if (event.isEsc() || event.isBackspace()) {
        result = false;
        decided = true;
        break;
      }
      const char letter = event.letter();
      if (letter == 'y') {
        result = true;
        decided = true;
        break;
      }
      if (letter == 'n') {
        result = false;
        decided = true;
        break;
      }
    }
    delay(10);
  }

  Input::flush();
  return result;
}

// ==================== SHOW INPUT DIALOG ====================
String UI::showInputDialog(const String &title, const String &prompt,
                           const String &defaultValue) {
  Input::flush();

  String input = defaultValue;

  while (true) {
    // Draw dialog
    int boxW = 220;
    int boxH = 60;
    int boxX = (SCREEN_WIDTH - boxW) / 2;
    int boxY = (SCREEN_HEIGHT - boxH) / 2;

    canvas.fillRect(boxX, boxY, boxW, boxH, MENU_BG);
    canvas.drawRect(boxX, boxY, boxW, boxH, ACCENT_COLOR);

    // Title
    canvas.setTextColor(ACCENT_COLOR, MENU_BG);
    drawCenteredText(title, boxY + 5, ACCENT_COLOR);

    // Prompt
    canvas.setTextColor(SECONDARY_COLOR, MENU_BG);
    canvas.drawString(prompt, boxX + 5, boxY + 20);

    // Input field
    canvas.fillRect(boxX + 5, boxY + 32, boxW - 10, 12, BG_COLOR);
    canvas.setTextColor(TEXT_COLOR, BG_COLOR);
    String displayInput = truncateString(input, 34);
    canvas.drawString(displayInput + "_", boxX + 7, boxY + 34);

    // Hints
    canvas.setTextColor(SECONDARY_COLOR, MENU_BG);
    canvas.drawString("Enter:OK  Esc:Cancel", boxX + 5, boxY + 48);

    pushCanvas(); // Show updates

    // Handle input
    M5Cardputer.update();
    Input::update();

    for (const KeyEvent &event : Input::events()) {
      if (event.isEnter()) {
        Input::flush();
        return input;
      }
      // Cancel. The old code looked for 0x1B inside KeysState::word, which the
      // fn layer never puts there, so these dialogs could not be cancelled at
      // all - the only way out was to clear the field and confirm an empty name.
      if (event.isEsc()) {
        Input::flush();
        return "";
      }
      if (event.isBackspace()) {
        if (input.length() > 0) {
          input = input.substring(0, input.length() - 1);
        }
        continue;
      }

      const char c = event.text();
      if (c != 0 && input.length() < MAX_FILENAME_LEN) {
        input += c;
      }
    }
    delay(10);
  }
}

// ==================== SHOW MESSAGE DIALOG ====================
void UI::showMessageDialog(const String &title, const String &message) {
  Input::flush();

  // Draw dialog
  int boxW = 200;
  int boxH = 60;
  int boxX = (SCREEN_WIDTH - boxW) / 2;
  int boxY = (SCREEN_HEIGHT - boxH) / 2;

  canvas.fillRect(boxX, boxY, boxW, boxH, MENU_BG);
  canvas.drawRect(boxX, boxY, boxW, boxH, ACCENT_COLOR);

  // Title
  canvas.setTextColor(ACCENT_COLOR, MENU_BG);
  drawCenteredText(title, boxY + 5, ACCENT_COLOR);

  // Message
  canvas.setTextColor(TEXT_COLOR, MENU_BG);
  drawCenteredText(message, boxY + 20, TEXT_COLOR);

  // Hint
  canvas.setTextColor(SECONDARY_COLOR, MENU_BG);
  drawCenteredText("Press any key...", boxY + 45, SECONDARY_COLOR);

  pushCanvas(); // Show dialog

  waitForAnyKey();
}

// ==================== WAIT FOR ANY KEY ====================
// Shared by the message and help screens. Ignores autorepeat so that a key
// still held from the action that opened the screen cannot dismiss it
// instantly, and flushes on exit so it is not seen again by the caller.
void UI::waitForAnyKey() {
  while (true) {
    M5Cardputer.update();
    Input::update();

    if (Input::optJustPressed()) {
      break;
    }
    bool dismissed = false;
    for (const KeyEvent &event : Input::events()) {
      if (!event.repeat) {
        dismissed = true;
        break;
      }
    }
    if (dismissed) {
      break;
    }
    delay(10);
  }

  Input::flush();
}

// ==================== SHOW HELP MENU ====================
void UI::showHelpMenu(bool isEditorMode) {
  Input::flush();
  clearScreen();

  canvas.setTextColor(ACCENT_COLOR, BG_COLOR);
  drawCenteredText("KEYBOARD SHORTCUTS", 5, ACCENT_COLOR);

  canvas.setTextColor(TEXT_COLOR, BG_COLOR);
  int y = 25;

  // Nine 10px rows fit between the title and the footer hint. Keep each line
  // under 40 characters: the font is 6px wide on a 240px screen.
  static const char *const editorHelp[] = {
      "; . , /    Move cursor",
      "Fn+;.,/    Type ; . , /",
      "Bksp       Delete back",
      "Fn+Bksp    Delete forward",
      "Tab        Indent",
      "Fn+S / Q   Save / Quit",
      "Fn+F / R   Find / Replace",
      "Fn+Z / Y   Undo / Redo",
  };
  static const char *const managerHelp[] = {
      "; . , /    Up/Down/Back/Open",
      "Space      Select  Fn+A all",
      "Fn+B       Bookmark (star)",
      "Fn+N       New file or folder",
      "Fn+D / R   Delete / Rename",
      "Fn+C/X/V   Copy / Cut / Paste",
      "Fn+P / F   Properties / Search",
      "Fn+O / M   Settings / USB",
  };

  const char *const *lines = isEditorMode ? editorHelp : managerHelp;
  const size_t lineCount =
      isEditorMode ? (sizeof(editorHelp) / sizeof(editorHelp[0]))
                   : (sizeof(managerHelp) / sizeof(managerHelp[0]));

  canvas.drawString(isEditorMode ? "TEXT EDITOR:" : "FILE MANAGER:", 5, y);
  y += 12;
  canvas.setTextColor(SECONDARY_COLOR, BG_COLOR);
  for (size_t i = 0; i < lineCount; i++) {
    canvas.drawString(lines[i], 5, y);
    y += 10;
  }

  canvas.setTextColor(ACCENT_COLOR, BG_COLOR);
  drawCenteredText("Press any key to close", SCREEN_HEIGHT - 15, ACCENT_COLOR);

  pushCanvas(); // Show help

  waitForAnyKey();

  // Clear screen after help closes
  clearScreen();
  pushCanvas(); // Update clear
}

// ==================== SHOW TOAST ====================
void UI::showToast(const String &message, uint16_t color) {
  toastMessage = message;
  toastColor = color;
  toastStartTime = millis();
  toastActive = true;

  // The colour already says whether this went well, so the tone follows it
  // rather than needing every call site to pick one.
  if (color == ERROR_COLOR) {
    Feedback::error();
    StatusLed::flashError();
  } else if (color == ACCENT_COLOR) {
    Feedback::confirm();
    StatusLed::flashSuccess();
  }
  // Does not push, will be shown on next render
}

// ==================== CLEAR SCREEN ====================
void UI::clearScreen() { canvas.fillScreen(BG_COLOR); }

// ==================== TRUNCATE STRING ====================
String UI::truncateString(const String &str, int maxLen) {
  if (str.length() <= maxLen) {
    return str;
  }
  return str.substring(0, maxLen - 2) + "..";
}

// ==================== DRAW CENTERED TEXT ====================
void UI::drawCenteredText(const String &text, int y, uint16_t color) {
  // Clamp before use: a string wider than the screen produced a negative x,
  // and readPixel() then sampled outside the sprite to pick a background.
  const int maxChars = SCREEN_WIDTH / CHAR_WIDTH;
  const String shown = truncateString(text, maxChars);
  const int x = max(0, (SCREEN_WIDTH - ((int)shown.length() * CHAR_WIDTH)) / 2);

  const int sampleY = constrain(y, 0, SCREEN_HEIGHT - 1);
  canvas.setTextColor(color, canvas.readPixel(x, sampleY));
  canvas.drawString(shown, x, y);
}
