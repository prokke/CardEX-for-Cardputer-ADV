#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include "Config.h"
#include "MassStorage.h"
#include "FileOps.h"
#include "TextEditor.h"
#include <Arduino.h>
#include <SD.h>
#include <vector>

// ==================== FILE MANAGER CLASS ====================
class FileManager {
public:
  FileManager();

  // Initialization
  bool init();

  // Main loop
  void update();
  void render();

  // Navigation
  void navigateUp();
  void navigateDown();
  void openSelected();
  void goBack();
  // void switchStorage(); // Removed

  // File operations
  void createNewFile();
  void createNewFolder();
  void deleteSelected();
  void renameSelected();
  void copySelected();
  void moveSelected();
  void pasteFromClipboard();
  void showFileProperties();

  // Search
  void searchFiles();

  // Help
  void showHelp();

  // State
  bool isInEditor() const { return inEditor; }

private:
  // Storage
private:
  // Storage
  bool useSD;
  bool sdAvailable;
  // bool flashAvailable; // Removed

  // Current path
  String currentPath;

  // File list
  std::vector<FileEntry> files;
  int selectedIndex;
  int scrollOffset;

  // Editor
  TextEditor editor;
  bool inEditor;

  // Clipboard
  std::vector<String> clipboard;
  FileOperation clipboardOperation;

  // Battery caching
  int lastBatteryLevel;
  unsigned long lastBatteryCheck;

  // Key repeat
  unsigned long lastKeyRepeatTime;

  // State
  AppMode appMode; // Current mode

  MassStorage massStorage;

  // Navigation history (stores directory names for back navigation)
  std::vector<String> dirHistory;

  // Helper methods
  fs::FS &getCurrentFS();
  void refreshFileList();
  void handleKeyboard();
  void handleEditorKeyboard();
  bool checkFnKey();
  void ensureSelectionVisible();
};

#endif // FILEMANAGER_H
