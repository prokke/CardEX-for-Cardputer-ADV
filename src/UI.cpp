#include "UI.h"

// Static variables
M5Canvas UI::canvas(&M5Cardputer.Display);
unsigned long UI::toastStartTime = 0;
String UI::toastMessage = "";
uint16_t UI::toastColor = TEXT_COLOR;
bool UI::toastActive = false;

// ==================== INIT ====================
void UI::init() {
  canvas.createSprite(SCREEN_WIDTH, SCREEN_HEIGHT);
  canvas.setColorDepth(16);
  canvas.setTextSize(1);
  canvas.setTextColor(TEXT_COLOR, BG_COLOR);
  clearScreen();
}

// ==================== PUSH CANVAS ====================
void UI::pushCanvas() { canvas.pushSprite(0, 0); }

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

  // Battery
  String bat = String(battery) + "%";
  canvas.drawString(bat, 210, 2);

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

   // Show toast if active (OVERLAYS footer text)
  if (toastActive && (millis() - toastStartTime < TOAST_DURATION)) {
    // Fill footer again to clear hints (already cleared above but fine to keep suitable for overlay logic)
    // Actually we just cleared it above at start of function, so we can just draw.
    
    // Draw Toast
    canvas.setTextColor(toastColor, MENU_BG);
    String displayToast = truncateString(toastMessage, 38);
    canvas.drawString(displayToast, 2, footerY + 2); // Centered in footer
  } else {
    // Toast expired or inactive
    if (toastActive) toastActive = false;

    // Draw Hints (Normal Status)
    canvas.setTextColor(SECONDARY_COLOR, MENU_BG);
    String displayHints = truncateString(hints, 38);
    canvas.drawString(displayHints, 2, footerY + 2);
  }
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
void UI::drawFileList(const std::vector<FileEntry> &files, int selected,
                      int scrollOffset) {
  int y = HEADER_HEIGHT + 2;
  // Use -2 to account for the top offset (y = HEADER + 2)
  // 111 - 2 = 109. 109 / 12 = 9.08 -> 9 lines.
  int visibleLines = (CONTENT_HEIGHT - 2) / LINE_HEIGHT;

  for (int i = 0; i < visibleLines; i++) {
    int fileIndex = i + scrollOffset;

    // Check if we have a file to draw
    if (fileIndex < files.size()) {
      const FileEntry &entry = files[fileIndex];

      // Highlight selected or clear background
      if (fileIndex == selected) {
        canvas.fillRect(0, y, SCREEN_WIDTH, LINE_HEIGHT, SELECTED_BG);
        canvas.setTextColor(TEXT_COLOR, SELECTED_BG);
      } else {
        canvas.fillRect(0, y, SCREEN_WIDTH, LINE_HEIGHT, BG_COLOR);
        canvas.setTextColor(TEXT_COLOR, BG_COLOR);
      }

      // Draw Icon Primitives
      int iconX = 4;
      int iconY = y + 2;
      
      if (entry.isDirectory) {
        // Folder Icon (Yellow-ish/Orange)
        uint16_t folderColor = 0xFD20; // WARNING_COLOR
        // Simple folder shape
        canvas.fillRect(iconX, iconY + 1, 8, 5, folderColor);
        canvas.fillRect(iconX, iconY - 1, 4, 2, folderColor);
      } else {
         // File Icon (White/Grey)
         uint16_t fileColor = SECONDARY_COLOR;
         // Page shape
         canvas.drawRect(iconX + 1, iconY, 6, 8, fileColor);
         canvas.drawLine(iconX + 2, iconY + 2, iconX + 5, iconY + 2, fileColor);
         canvas.drawLine(iconX + 2, iconY + 4, iconX + 5, iconY + 4, fileColor);
      }

      // Filename (truncated)
      String displayName = truncateString(entry.name, 30);
      canvas.drawString(displayName, 16, y + 2); // Shifted text x position

      // Size (right-aligned)
      String sizeStr = entry.getSizeStr();
      int sizeX = SCREEN_WIDTH - (sizeStr.length() * CHAR_WIDTH) - 2;
      canvas.drawString(sizeStr, sizeX, y + 2);
    } else {
      // Clear empty line
      canvas.fillRect(0, y, SCREEN_WIDTH, LINE_HEIGHT, BG_COLOR);
    }

    y += LINE_HEIGHT;
  }

  // Scroll indicator
  if (files.size() > visibleLines) {
    int scrollBarHeight = CONTENT_HEIGHT - 4;
    int thumbHeight =
        max(10, (int)((visibleLines * scrollBarHeight) / files.size()));
    int thumbY = HEADER_HEIGHT + 2 +
                 (scrollOffset * (scrollBarHeight - thumbHeight)) /
                     (int)(files.size() - visibleLines);

    canvas.fillRect(SCREEN_WIDTH - 3, thumbY, 2, thumbHeight, ACCENT_COLOR);
  }
}

// ==================== SHOW CONFIRM DIALOG ====================
bool UI::showConfirmDialog(const String &title, const String &message) {
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

  // Wait for input
  while (true) {
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange()) {
      auto status = M5Cardputer.Keyboard.keysState();
      if (status.word.size() > 0) {
        char key = status.word[0];
        if (key == 'y' || key == 'Y')
          return true;
        if (key == 'n' || key == 'N')
          return false;
      }
      if (status.enter)
        return true;
      if (status.del)
        return false;
    }
    delay(10);
  }
}

// ==================== SHOW INPUT DIALOG ====================
String UI::showInputDialog(const String &title, const String &prompt,
                           const String &defaultValue) {
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
    if (M5Cardputer.Keyboard.isChange()) {
      auto status = M5Cardputer.Keyboard.keysState();

      if (status.enter) {
        return input;
      }
      if (status.del && input.length() > 0) {
        input = input.substring(0, input.length() - 1);
      }
      if (status.word.size() > 0) {
        for (char c : status.word) {
          if (c == 0x1B) { // ESC
            return "";
          }
          if (input.length() < MAX_FILENAME_LEN) {
            input += c;
          }
        }
      }
    }
    delay(10);
  }
}

// ==================== SHOW MESSAGE DIALOG ====================
void UI::showMessageDialog(const String &title, const String &message) {
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

  // Wait for key
  while (true) {
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange()) {
      auto status = M5Cardputer.Keyboard.keysState();
      if (status.word.size() > 0 || status.enter || status.del) {
        break;
      }
    }
    delay(10);
  }
}

// ==================== SHOW PROGRESS BAR ====================
void UI::showProgressBar(const String &operation, int percent,
                         const String &details) {
  int boxW = 200;
  int boxH = 60;
  int boxX = (SCREEN_WIDTH - boxW) / 2;
  int boxY = (SCREEN_HEIGHT - boxH) / 2;

  canvas.fillRect(boxX, boxY, boxW, boxH, MENU_BG);
  canvas.drawRect(boxX, boxY, boxW, boxH, ACCENT_COLOR);

  // Operation
  canvas.setTextColor(ACCENT_COLOR, MENU_BG);
  drawCenteredText(operation, boxY + 5, ACCENT_COLOR);

  // Progress bar
  int barW = boxW - 20;
  int barH = 12;
  int barX = boxX + 10;
  int barY = boxY + 25;

  canvas.drawRect(barX, barY, barW, barH, BORDER_COLOR);
  int fillW = (barW - 4) * percent / 100;
  canvas.fillRect(barX + 2, barY + 2, fillW, barH - 4, ACCENT_COLOR);

  // Percentage
  canvas.setTextColor(TEXT_COLOR, MENU_BG);
  String percentStr = String(percent) + "%";
  drawCenteredText(percentStr, boxY + 40, TEXT_COLOR);

  // Details
  if (details.length() > 0) {
    canvas.setTextColor(SECONDARY_COLOR, MENU_BG);
    drawCenteredText(truncateString(details, 30), boxY + 50, SECONDARY_COLOR);
  }

  pushCanvas(); // Show progress
}

// ==================== SHOW HELP MENU ====================
void UI::showHelpMenu(bool isEditorMode) {
  clearScreen();

  canvas.setTextColor(ACCENT_COLOR, BG_COLOR);
  drawCenteredText("KEYBOARD SHORTCUTS", 5, ACCENT_COLOR);

  canvas.setTextColor(TEXT_COLOR, BG_COLOR);
  int y = 25;

  if (isEditorMode) {
    canvas.drawString("TEXT EDITOR:", 5, y);
    y += 12;
    canvas.setTextColor(SECONDARY_COLOR, BG_COLOR);
    canvas.drawString("Arrows  Move cursor", 5, y);
    y += 10;
    canvas.drawString("Fn+S    Save file", 5, y);
    y += 10;
    canvas.drawString("Fn+Q    Quit editor", 5, y);
    y += 10;
    canvas.drawString("Fn+F    Find text", 5, y);
    y += 10;
    canvas.drawString("Fn+R    Replace text", 5, y);
    y += 10;
    canvas.drawString("Fn+Z/Y  Undo/Redo", 5, y);
    y += 10;
  } else {
    canvas.drawString("FILE MANAGER:", 5, y);
    y += 12;
    canvas.setTextColor(SECONDARY_COLOR, BG_COLOR);
    canvas.drawString("Fn+N    New File/Dir", 5, y);
    y += 10;
    canvas.drawString("Fn+D/R  Delete/Rename", 5, y);
    y += 10;
    canvas.drawString("Fn+C/V  Copy/Paste", 5, y);
    y += 10;
    canvas.drawString("Fn+X    Cut (Move)", 5, y);
    y += 10;
    canvas.drawString("Fn+P    Properties", 5, y);
    y += 10;
    canvas.drawString("Fn+F    Search", 5, y);
    y += 10;
    canvas.drawString("Fn+M    USB Mode", 5, y);
    y += 10;
  }

  canvas.setTextColor(ACCENT_COLOR, BG_COLOR);
  drawCenteredText("Press any key to close", SCREEN_HEIGHT - 15, ACCENT_COLOR);

  pushCanvas(); // Show help

  // Wait for key
  while (true) {
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
      auto status = M5Cardputer.Keyboard.keysState();
      if (status.word.size() > 0 || status.enter || status.del || status.opt) {
        break;
      }
    }
    delay(10);
  }

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
  // Does not push, will be shown on next render
}

// ==================== CLEAR SCREEN ====================
void UI::clearScreen() { canvas.fillScreen(BG_COLOR); }

// ==================== CLEAR CONTENT ====================
void UI::clearContent() {
  canvas.fillRect(0, HEADER_HEIGHT, SCREEN_WIDTH, CONTENT_HEIGHT, BG_COLOR);
}

// ==================== TRUNCATE STRING ====================
String UI::truncateString(const String &str, int maxLen) {
  if (str.length() <= maxLen) {
    return str;
  }
  return str.substring(0, maxLen - 2) + "..";
}

// ==================== DRAW CENTERED TEXT ====================
void UI::drawCenteredText(const String &text, int y, uint16_t color) {
  int x = (SCREEN_WIDTH - (text.length() * CHAR_WIDTH)) / 2;
  canvas.setTextColor(color, canvas.readPixel(x, y));
  canvas.drawString(text, x, y);
}
