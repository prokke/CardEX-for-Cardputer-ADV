#ifndef FILEMANAGER_H
#define FILEMANAGER_H

#include "Config.h"
#include "Input.h"
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
  bool useSD;
  bool sdAvailable;

  // Current path
  String currentPath;

  // File list
  std::vector<FileEntry> files;
  int selectedIndex;
  int scrollOffset;

  // True while `files` holds search results rather than the contents of
  // currentPath. Entries then carry paths from anywhere under the search root.
  bool searchMode;
  String searchQuery;

  // Set when a listing hit MAX_FILES_IN_LIST and is therefore incomplete.
  bool listTruncated;

  // Editor
  TextEditor editor;
  bool inEditor;

  // Clipboard
  std::vector<String> clipboard;
  FileOperation clipboardOperation;

  // Battery caching
  int lastBatteryLevel;
  unsigned long lastBatteryCheck;

  // State
  AppMode appMode; // Current mode

  MassStorage massStorage;

  // Helper methods
  fs::FS &getCurrentFS();
  void refreshFileList();
  void handleKeyboard();
  void handleEditorKeyboard();
  void enterMassStorage();
  void quitEditor();
  void ensureSelectionVisible();
};

#endif // FILEMANAGER_H
