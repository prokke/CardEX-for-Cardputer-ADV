#include "Config.h"
#include <FS.h>
#include <SD.h>

// ==================== GLOBAL VARIABLES ====================
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
int SCROLL_DELAY = DEFAULT_SCROLL_DELAY;
int KEY_REPEAT_DELAY = DEFAULT_KEY_REPEAT_DELAY;
int KEY_REPEAT_RATE = DEFAULT_KEY_REPEAT_RATE;
int SCREEN_BRIGHTNESS = DEFAULT_SCREEN_BRIGHTNESS;

// Helper to trim check for comment
String cleanLine(String line) {
  int commentIdx = line.indexOf("//");
  if (commentIdx >= 0) line = line.substring(0, commentIdx);
  line.trim();
  return line;
}

// Helper to parse HEX or DEC
long parseValue(String val) {
  val.trim();
  if (val.startsWith("0x") || val.startsWith("0X")) {
    return strtol(val.c_str(), NULL, 16);
  }
  return val.toInt();
}

void loadConfig() {
  if (!SD.exists("/CardEX.ini")) {
    saveDefaultConfig();
    return;
  }

  File file = SD.open("/CardEX.ini", FILE_READ);
  if (!file) return;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line = cleanLine(line);
    if (line.length() == 0) continue;

    int sep = line.indexOf('=');
    if (sep < 0) continue;

    String key = line.substring(0, sep);
    key.trim();
    String valStr = line.substring(sep + 1);
    long val = parseValue(valStr);

    // Color Palette
    if (key == "BG_COLOR") BG_COLOR = (uint16_t)val;
    else if (key == "TEXT_COLOR") TEXT_COLOR = (uint16_t)val;
    else if (key == "ACCENT_COLOR") ACCENT_COLOR = (uint16_t)val;
    else if (key == "SECONDARY_COLOR") SECONDARY_COLOR = (uint16_t)val;
    else if (key == "ERROR_COLOR") ERROR_COLOR = (uint16_t)val;
    else if (key == "WARNING_COLOR") WARNING_COLOR = (uint16_t)val;
    else if (key == "MENU_BG") MENU_BG = (uint16_t)val;
    else if (key == "SELECTED_BG") SELECTED_BG = (uint16_t)val;
    else if (key == "BORDER_COLOR") BORDER_COLOR = (uint16_t)val;
    else if (key == "CURSOR_COLOR") CURSOR_COLOR = (uint16_t)val;
    
    // Editor Settings
    else if (key == "TAB_SIZE") TAB_SIZE = (int)val;
    else if (key == "CURSOR_BLINK_MS") CURSOR_BLINK_MS = (int)val;
    else if (key == "AUTO_SAVE_INTERVAL") AUTO_SAVE_INTERVAL = (int)val;
    else if (key == "MAX_UNDO_LEVELS") MAX_UNDO_LEVELS = (int)val;

    // UI Settings
    else if (key == "TOAST_DURATION") TOAST_DURATION = (int)val;
    else if (key == "SCROLL_DELAY") SCROLL_DELAY = (int)val;
    else if (key == "KEY_REPEAT_DELAY") KEY_REPEAT_DELAY = (int)val;
    else if (key == "KEY_REPEAT_RATE") KEY_REPEAT_RATE = (int)val;
    else if (key == "SCREEN_BRIGHTNESS") SCREEN_BRIGHTNESS = (int)val;
  }
  file.close();
}

void saveDefaultConfig() {
  File file = SD.open("/CardEX.ini", FILE_WRITE);
  if (!file) return;

  file.println("// ==================== COLOR PALETTE (RGB565) ====================");
  file.println("// Main colors");
  file.printf("BG_COLOR = 0x%04X\n", DEFAULT_BG_COLOR);
  file.printf("TEXT_COLOR = 0x%04X\n", DEFAULT_TEXT_COLOR);
  file.printf("ACCENT_COLOR = 0x%04X\n", DEFAULT_ACCENT_COLOR);
  file.printf("SECONDARY_COLOR = 0x%04X\n", DEFAULT_SECONDARY_COLOR);
  file.printf("ERROR_COLOR = 0x%04X\n", DEFAULT_ERROR_COLOR);
  file.printf("WARNING_COLOR = 0x%04X\n", DEFAULT_WARNING_COLOR);
  file.println();
  
  file.println("// UI elements");
  file.printf("MENU_BG = 0x%04X\n", DEFAULT_MENU_BG);
  file.printf("SELECTED_BG = 0x%04X\n", DEFAULT_SELECTED_BG);
  file.printf("BORDER_COLOR = 0x%04X\n", DEFAULT_BORDER_COLOR);
  file.printf("CURSOR_COLOR = 0x%04X\n", DEFAULT_CURSOR_COLOR);
  file.println();

  file.println("// ==================== EDITOR SETTINGS ====================");
  file.printf("TAB_SIZE = %d\n", DEFAULT_TAB_SIZE);
  file.printf("CURSOR_BLINK_MS = %d\n", DEFAULT_CURSOR_BLINK_MS);
  file.printf("AUTO_SAVE_INTERVAL = %d\n", DEFAULT_AUTO_SAVE_INTERVAL);
  file.printf("MAX_UNDO_LEVELS = %d\n", DEFAULT_MAX_UNDO_LEVELS);
  file.println();
  
  file.println("// ==================== UI SETTINGS ====================");
  file.printf("TOAST_DURATION = %d\n", DEFAULT_TOAST_DURATION);
  file.printf("SCROLL_DELAY = %d\n", DEFAULT_SCROLL_DELAY);
  file.printf("KEY_REPEAT_DELAY = %d\n", DEFAULT_KEY_REPEAT_DELAY);
  file.printf("KEY_REPEAT_RATE = %d\n", DEFAULT_KEY_REPEAT_RATE);
  file.printf("SCREEN_BRIGHTNESS = %d // 0-255\n", DEFAULT_SCREEN_BRIGHTNESS);
  
  file.close();
}
