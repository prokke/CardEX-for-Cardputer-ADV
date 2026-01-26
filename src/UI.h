#ifndef UI_H
#define UI_H

#include "Config.h"
#include "FileOps.h"
#include <M5Cardputer.h>
#include <vector>

// ==================== UI CLASS ====================
class UI {
public:
  // Global off-screen canvas for double buffering
  static M5Canvas canvas;
  static void pushCanvas();

  // Initialize UI
  static void init();

  // Header and Footer
  static void drawHeader(const String &path, bool isSD, int battery);
  static void drawFooter(const String &hints);
  static void drawFooterProgress(int percent, const String &status);

  // File list
  static void drawFileList(const std::vector<FileEntry> &files, int selected,
                           int scrollOffset);

  // Dialogs
  static bool showConfirmDialog(const String &title, const String &message);
  static String showInputDialog(const String &title, const String &prompt,
                                const String &defaultValue = "");
  static void showMessageDialog(const String &title, const String &message);
  static void showProgressBar(const String &operation, int percent,
                              const String &details = "");

  // Help menu
  static void showHelpMenu(bool isEditorMode);

  // Toast notification
  static void showToast(const String &message, uint16_t color = TEXT_COLOR);

  // Utilities
  static void clearScreen();
  static void clearContent();
  static String truncateString(const String &str, int maxLen);
  static void drawCenteredText(const String &text, int y,
                               uint16_t color = TEXT_COLOR);

private:
  static unsigned long toastStartTime;
  static String toastMessage;
  static uint16_t toastColor;
  static bool toastActive;
};

#endif // UI_H
