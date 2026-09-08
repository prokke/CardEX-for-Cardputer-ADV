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

  // Mounts the SD card. Separate from init() so setup() can load the config
  // before anything is drawn, and can retry without a reboot.
  static bool initStorage();

  // Initialization
  bool init();

  // Main loop
  void update();
  void render();

  // True when something changed since the last frame. The loop used to repaint
  // and push the whole 64KB sprite every 10ms regardless.
  bool needsRender() const { return renderRequested; }

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

  // Multi-selection and bookmarks
  void toggleSelection();
  void selectAll(bool select);
  void toggleBookmark();
  int selectionCount() const;

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

  // Free-space caching. SD.usedBytes() walks the whole FAT, and render() called
  // it on every frame.
  uint64_t cachedFreeBytes;
  unsigned long lastFreeSpaceCheck;

  // Set by update() when the frame produced anything worth drawing.
  bool renderRequested;

  // State
  AppMode appMode; // Current mode

  MassStorage massStorage;

  // Helper methods
  fs::FS &getCurrentFS();
  void refreshFileList();
  void handleKeyboard();
  void handleEditorKeyboard();
  void enterMassStorage();
  void openFileByType(const String &path, size_t size);
  bool looksLikeText(const String &path);
  void updateFreeSpace(bool force);

  // Paths the current action applies to: every selected entry, or the entry
  // under the cursor when nothing is selected.
  std::vector<String> targetPaths() const;
  void quitEditor();
  void ensureSelectionVisible();
};

#endif // FILEMANAGER_H
