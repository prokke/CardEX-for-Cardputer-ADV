#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==================== DISPLAY CONFIGURATION ====================
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 135

// Layout dimensions
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
#define MAX_FILES_IN_LIST 100    // Maximum files to load at once
#define MAX_FILE_SIZE 32768      // 32KB max for editing (Safe for SRAM)
#define EDITOR_BUFFER_SIZE 32768 // Match file size
#define TEXT_BLOCK_SIZE 4096     // 4KB blocks
#define MAX_FILENAME_LEN 64
#define MAX_PATH_LEN 256

// ==================== EDITOR SETTINGS ====================
#define DEFAULT_TAB_SIZE 4
#define DEFAULT_CURSOR_BLINK_MS 500
#define DEFAULT_AUTO_SAVE_INTERVAL 0 // 0 disables autosave
#define DEFAULT_MAX_UNDO_LEVELS 50

extern int TAB_SIZE;
extern int CURSOR_BLINK_MS;
extern int AUTO_SAVE_INTERVAL;
extern int MAX_UNDO_LEVELS;
extern bool SHOW_LINE_NUMBERS;
extern bool AUTO_INDENT;
extern bool TAB_USES_SPACES;

// ==================== UI SETTINGS ====================
#define DEFAULT_TOAST_DURATION 2000
#define DEFAULT_KEY_REPEAT_DELAY 500
#define DEFAULT_KEY_REPEAT_RATE 100
#define DEFAULT_SCREEN_BRIGHTNESS 128 // 0-255

extern int TOAST_DURATION;
extern int KEY_REPEAT_DELAY;
extern int KEY_REPEAT_RATE;
extern int SCREEN_BRIGHTNESS;

// ==================== BATTERY THRESHOLDS ====================
// The percentage is drawn in WARNING_COLOR below the first and ERROR_COLOR
// below the second.
#define DEFAULT_BAT_WARN_LEVEL 50
#define DEFAULT_BAT_CRIT_LEVEL 25

extern int BAT_WARN_LEVEL;
extern int BAT_CRIT_LEVEL;

// ==================== FILE MANAGER BEHAVIOUR ====================
extern bool SHOW_HIDDEN_FILES;
extern bool CONFIRM_DELETE;
extern int SORT_MODE;
extern bool SORT_DESCENDING;

// ==================== SOUND (ADV: ES8311 codec) ====================
#define DEFAULT_SOUND_VOLUME 80

extern bool SOUND_ENABLED;
extern bool SOUND_KEY_CLICK;
extern int SOUND_VOLUME;

// ==================== STATUS LED (ADV/Cardputer: WS2812 on G21) ====================
// 24-bit RGB, unlike the RGB565 used for the screen palette.
#define DEFAULT_LED_BRIGHTNESS 40
#define DEFAULT_LED_COLOR_IDLE 0x000000      // off while simply browsing
#define DEFAULT_LED_COLOR_CLIPBOARD 0xFF6000 // amber: something to paste
#define DEFAULT_LED_COLOR_EDIT 0x0040FF      // blue: unsaved changes
#define DEFAULT_LED_COLOR_USB 0x00C0FF       // cyan: card owned by the host
#define DEFAULT_LED_COLOR_OK 0x00FF00
#define DEFAULT_LED_COLOR_ERROR 0xFF0000

extern bool LED_ENABLED;
extern int LED_BRIGHTNESS;
extern int LED_COLOR_IDLE;
extern int LED_COLOR_CLIPBOARD;
extern int LED_COLOR_EDIT;
extern int LED_COLOR_USB;
extern int LED_COLOR_OK;
extern int LED_COLOR_ERROR;

// ==================== CONFIG ====================
#define CONFIG_FILE_PATH "/CardEX.ini"
#define BOOKMARKS_FILE_PATH "/CardEX.bookmarks"


// ==================== KEYBOARD KEYS ====================
// Deliberately not defined here. M5Cardputer's utility/Keyboard/Keyboard_def.h
// owns KEY_FN, KEY_TAB, KEY_ENTER, KEY_BACKSPACE and KEY_DELETE, with different
// values (KEY_TAB is 0x2b there, not '\t'). Defining them again produced four
// "macro redefined" warnings and left the effective value dependent on include
// order. Include <M5Cardputer.h> - or src/Input.h - for the real constants.

// ==================== APPLICATION MODES ====================
enum AppMode { MODE_FILE_MANAGER, MODE_TEXT_EDITOR, MODE_DIALOG, MODE_HELP, MODE_MASS_STORAGE };

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
