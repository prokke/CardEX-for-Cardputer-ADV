#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==================== DISPLAY CONFIGURATION ====================
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 135

// Layout dimensionsge
#define HEADER_HEIGHT 12
#define FOOTER_HEIGHT 12
#define CONTENT_HEIGHT (SCREEN_HEIGHT - HEADER_HEIGHT - FOOTER_HEIGHT) // 111px

// Text dimensions (for Font0 - 8px monospace)
#define CHAR_WIDTH 6
#define CHAR_HEIGHT 8
#define LINE_HEIGHT 12
#define LINES_VISIBLE (CONTENT_HEIGHT / LINE_HEIGHT) // ~8 lines

// ==================== COLOR PALETTE (RGB565) ====================
// Default values
#define DEFAULT_BG_COLOR 0x3186
#define DEFAULT_TEXT_COLOR 0xFFFF
#define DEFAULT_ACCENT_COLOR 0x07E0
#define DEFAULT_SECONDARY_COLOR 0x7BEF
#define DEFAULT_ERROR_COLOR 0xF800
#define DEFAULT_WARNING_COLOR 0xFD20
#define DEFAULT_MENU_BG 0x3186
#define DEFAULT_SELECTED_BG 0x0410
#define DEFAULT_BORDER_COLOR 0x4208
#define DEFAULT_CURSOR_COLOR 0xfd20

// Runtime variables
extern uint16_t BG_COLOR;
extern uint16_t TEXT_COLOR;
extern uint16_t ACCENT_COLOR;
extern uint16_t SECONDARY_COLOR;
extern uint16_t ERROR_COLOR;
extern uint16_t WARNING_COLOR;
extern uint16_t MENU_BG;
extern uint16_t SELECTED_BG;
extern uint16_t BORDER_COLOR;
extern uint16_t CURSOR_COLOR;

// ==================== SD CARD PINS (M5Cardputer) ====================
#define SD_SPI_SCK_PIN 40
#define SD_SPI_MISO_PIN 39
#define SD_SPI_MOSI_PIN 14
#define SD_SPI_CS_PIN 12

// ==================== MEMORY LIMITS ====================
// ==================== MEMORY LIMITS ====================
#define MAX_FILES_IN_LIST 100    // Maximum files to load at once
#define MAX_FILE_SIZE 32768      // 32KB max for editing (Safe for SRAM)
#define EDITOR_BUFFER_SIZE 32768 // Match file size
#define TEXT_BLOCK_SIZE 4096     // 4KB blocks
#define MAX_FILENAME_LEN 64
#define MAX_PATH_LEN 256

// ==================== EDITOR SETTINGS ====================
#define DEFAULT_TAB_SIZE 4
#define DEFAULT_CURSOR_BLINK_MS 500
#define DEFAULT_AUTO_SAVE_INTERVAL 30000
#define DEFAULT_MAX_UNDO_LEVELS 10

extern int TAB_SIZE;
extern int CURSOR_BLINK_MS;
extern int AUTO_SAVE_INTERVAL;
extern int MAX_UNDO_LEVELS;

// ==================== UI SETTINGS ====================
#define DEFAULT_TOAST_DURATION 2000
#define DEFAULT_SCROLL_DELAY 100
#define DEFAULT_KEY_REPEAT_DELAY 500
#define DEFAULT_KEY_REPEAT_RATE 100
#define DEFAULT_SCREEN_BRIGHTNESS 128 // 0-255

extern int TOAST_DURATION;
extern int SCROLL_DELAY;
extern int KEY_REPEAT_DELAY;
extern int KEY_REPEAT_RATE;
extern int SCREEN_BRIGHTNESS;

// ==================== CONFIG LOADER ====================
void loadConfig();
void saveDefaultConfig();


// ==================== FILE ICONS (Handled by UI::drawFileList) ====================
// Defined as empty strings as we will draw them using primitives
#define ICON_FOLDER ""
#define ICON_TEXT ""
#define ICON_CODE ""
#define ICON_CONFIG ""
#define ICON_BINARY ""
#define ICON_LOG ""
#define ICON_UNKNOWN ""

// ==================== KEYBOARD KEYS ====================
// Special keys (from M5Cardputer Keyboard)
#define KEY_FN 0x00
#define KEY_TAB '\t'
#define KEY_ENTER '\n'
#define KEY_BACKSPACE '\b'
#define KEY_ESC 0x1B
#define KEY_DELETE 0x7F

// ==================== APPLICATION MODES ====================
enum AppMode { MODE_FILE_MANAGER, MODE_TEXT_EDITOR, MODE_DIALOG, MODE_HELP, MODE_MASS_STORAGE };

// ==================== DIALOG TYPES ====================
enum DialogType {
  DIALOG_CONFIRM,
  DIALOG_INPUT,
  DIALOG_PROGRESS,
  DIALOG_MESSAGE
};

// ==================== FILE OPERATION TYPES ====================
enum FileOperation {
  OP_NONE,
  OP_COPY,
  OP_MOVE,
  OP_DELETE,
  OP_RENAME,
  OP_NEW_FILE,
  OP_NEW_FOLDER
};

#endif // CONFIG_H
