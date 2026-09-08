#include "FileManager.h"
#include "UI.h"
#include <M5Cardputer.h>

// ==================== CONSTRUCTOR ====================
FileManager::FileManager() {
  appMode = MODE_FILE_MANAGER;
  useSD = true;
  sdAvailable = false;
  currentPath = "/";
  selectedIndex = 0;
  scrollOffset = 0;
  inEditor = false;
  clipboardOperation = OP_NONE;
  lastBatteryLevel = 0;
  lastBatteryCheck = 0;
}

// ==================== INIT ====================
bool FileManager::init() {
  // Initialize SD card
  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
  sdAvailable = SD.begin(SD_SPI_CS_PIN, SPI);


  if (!sdAvailable) {
    Serial.println("SD Card initialization failed!");
    UI::showMessageDialog("Error", "No SD Card found!");
    // return false; // Allow running without SD?
    // For now, let's return false as per original logic if SD is critical
    return false;
  } else {
    Serial.println("SD Card initialized");
    useSD = true;
    
    // Load config from SD
    loadConfig();
    
    // Apply brightness setting
    M5Cardputer.Display.setBrightness(SCREEN_BRIGHTNESS);
  }

  // Initial battery check
  lastBatteryLevel = M5Cardputer.Power.getBatteryLevel();
  lastBatteryCheck = millis();

  // Load initial file list
  refreshFileList();

  return true;
}

// ==================== UPDATE ====================
void FileManager::update() {
  // Update battery every 30 seconds
  if (millis() - lastBatteryCheck > 30000) {
    lastBatteryLevel = M5Cardputer.Power.getBatteryLevel();
    lastBatteryCheck = millis();
  }

  if (appMode == MODE_MASS_STORAGE) {
      // Should not be in this state in update() if loop is blocking, 
      // but if we are here, we handle entry/exit or just run loop.
      // Actually, we'll enter the loop from the keyboard handler, so we shouldn't be here in update() 
      // unless we want to use update() as the blocking container (state machine).
      // Let's stick to the state machine but make massStorage.loop() blocking.
      
      // Wait, if we block in handleKeyboard, update() won't run.
      // Let's change how we enter the mode.
      return;
  }

  if (inEditor) {
    editor.update();
    handleEditorKeyboard();
  } else {
    handleKeyboard();
  }
}

// ==================== RENDER ====================
void FileManager::render() {
  if (inEditor) {
    editor.render();
  } else {
    // Draw to canvas
    UI::drawHeader(currentPath, useSD, lastBatteryLevel);
    UI::drawFileList(files, selectedIndex, scrollOffset);

    // Footer info
    String countInfo = String(files.size()) + " items";

    // Add free space info
    String freeSpaceStr = "";
    if (sdAvailable) {
      uint64_t freeBytes = SD.totalBytes() - SD.usedBytes();
      freeSpaceStr = FileOps::formatBytes(freeBytes) + " free";
    }

    // Construct footer status string
    String status = countInfo;
    if (freeSpaceStr.length() > 0) {
        // Pad with spaces to push free space to the right (approximate, since font is mono)
        // Or just separate with pipe
        status += " | " + freeSpaceStr;
    }

    // Use centralized UI method
    UI::drawFooter(status);

    // Push complete frame
    UI::pushCanvas();
  }
}

// ==================== NAVIGATE UP ====================
void FileManager::navigateUp() {
  if (selectedIndex > 0) {
    selectedIndex--;
    ensureSelectionVisible();
  }
}

// ==================== NAVIGATE DOWN ====================
void FileManager::navigateDown() {
  if (selectedIndex < files.size() - 1) {
    selectedIndex++;
    ensureSelectionVisible();
  }
}

// ==================== OPEN SELECTED ====================
void FileManager::openSelected() {
  if (files.empty() || selectedIndex >= files.size()) {
    return;
  }

  FileEntry &entry = files[selectedIndex];
  String fullPath = FileOps::joinPath(currentPath, entry.name);

  if (entry.isDirectory) {
    // Save current directory name to history before entering
    dirHistory.push_back(entry.name);
    
    // Navigate into directory
    currentPath = fullPath;
    selectedIndex = 0;
    scrollOffset = 0;
    refreshFileList();
  } else {
    // Open file in editor
    if (editor.openFile(getCurrentFS(), fullPath)) {
      inEditor = true;
      Input::flush();
    } else {
      UI::showToast("Failed to open file", ERROR_COLOR);
    }
  }
}

// ==================== GO BACK ====================
void FileManager::goBack() {
  if (currentPath == "/") {
    return;
  }

  // Get the directory name we're returning to
  String lastDir = "";
  if (!dirHistory.empty()) {
    lastDir = dirHistory.back();
    dirHistory.pop_back();
  }

  currentPath = FileOps::getParentPath(currentPath);
  selectedIndex = 0;
  scrollOffset = 0;
  refreshFileList();
  
  // Find and select the directory we came from
  if (lastDir.length() > 0) {
    for (int i = 0; i < files.size(); i++) {
      if (files[i].name == lastDir) {
        selectedIndex = i;
        ensureSelectionVisible();
        break;
      }
    }
  }
}

// ==================== CREATE NEW FILE ====================
void FileManager::createNewFile() {
  String fileName = UI::showInputDialog("New File", "Name:");
  if (fileName.length() == 0)
    return;

  String fullPath = FileOps::joinPath(currentPath, fileName);

  if (FileOps::fileExists(getCurrentFS(), fullPath)) {
    UI::showToast("File exists!", ERROR_COLOR);
    return;
  }

  File file = getCurrentFS().open(fullPath, FILE_WRITE);
  if (file) {
    file.close();
    UI::showToast("File created", ACCENT_COLOR);
    refreshFileList();
  } else {
    UI::showToast("Failed to create file", ERROR_COLOR);
  }
}

// ==================== CREATE NEW FOLDER ====================
void FileManager::createNewFolder() {
  String folderName = UI::showInputDialog("New Folder", "Name:");
  if (folderName.length() == 0)
    return;

  String fullPath = FileOps::joinPath(currentPath, folderName);

  if (FileOps::createDirectory(getCurrentFS(), fullPath)) {
    UI::showToast("Folder created", ACCENT_COLOR);
    refreshFileList();
  } else {
    UI::showToast("Failed to create folder", ERROR_COLOR);
  }
}

// ==================== DELETE SELECTED ====================
void FileManager::deleteSelected() {
  if (files.empty() || selectedIndex >= files.size())
    return;

  FileEntry &entry = files[selectedIndex];
  String message = "Delete \"" + entry.name + "\"?";

  if (!UI::showConfirmDialog("Confirm Delete", message)) {
    return;
  }

  String fullPath = FileOps::joinPath(currentPath, entry.name);
  bool success = false;

  if (entry.isDirectory) {
    success = FileOps::deleteDirectory(getCurrentFS(), fullPath);
  } else {
    success = FileOps::deleteFile(getCurrentFS(), fullPath);
  }

  if (success) {
    UI::showToast("Deleted", ACCENT_COLOR);
    if (selectedIndex >= files.size() - 1 && selectedIndex > 0) {
      selectedIndex--;
    }
    refreshFileList();
  } else {
    UI::showToast("Delete failed", ERROR_COLOR);
  }
}

// ==================== RENAME SELECTED ====================
void FileManager::renameSelected() {
  if (files.empty() || selectedIndex >= files.size())
    return;

  FileEntry &entry = files[selectedIndex];
  String newName = UI::showInputDialog("Rename", "New name:", entry.name);

  if (newName.length() == 0 || newName == entry.name)
    return;

  String oldPath = FileOps::joinPath(currentPath, entry.name);
  String newPath = FileOps::joinPath(currentPath, newName);

  if (FileOps::renameFile(getCurrentFS(), oldPath, newPath)) {
    UI::showToast("Renamed", ACCENT_COLOR);
    refreshFileList();
  } else {
    UI::showToast("Rename failed", ERROR_COLOR);
  }
}

// ==================== COPY SELECTED ====================
void FileManager::copySelected() {
  if (files.empty() || selectedIndex >= files.size())
    return;

  clipboard.clear();
  clipboard.push_back(
      FileOps::joinPath(currentPath, files[selectedIndex].name));
  clipboardOperation = OP_COPY;

  UI::showToast("Copied to clipboard", ACCENT_COLOR);
}

// ==================== MOVE SELECTED ====================
void FileManager::moveSelected() {
  if (files.empty() || selectedIndex >= files.size())
    return;

  clipboard.clear();
  clipboard.push_back(
      FileOps::joinPath(currentPath, files[selectedIndex].name));
  clipboardOperation = OP_MOVE;

  UI::showToast("Cut to clipboard", ACCENT_COLOR);
}

// ==================== PASTE FROM CLIPBOARD ====================
void FileManager::pasteFromClipboard() {
  if (clipboard.empty() || clipboardOperation == OP_NONE) {
    UI::showToast("Clipboard empty", WARNING_COLOR);
    return;
  }

  int successCount = 0;
  for (const String &srcPath : clipboard) {
    String fileName = FileOps::getFileName(srcPath);
    String dstPath = FileOps::joinPath(currentPath, fileName);

    // Check availability
    fs::FS &fs = getCurrentFS(); // Only SD supported now

    // Handle name collision for COPY or if needed
    if (FileOps::fileExists(fs, dstPath)) {
        // Naming strategy: name.ext -> name-copy.ext
        // If name-copy.ext exists -> name-copy-copy.ext (simple)
        // or name-copy-1.ext (better, but simpler for now)
        
        int extIndex = fileName.lastIndexOf('.');
        String namePart = (extIndex > 0) ? fileName.substring(0, extIndex) : fileName;
        String extPart = (extIndex > 0) ? fileName.substring(extIndex) : "";
        
        String newName = namePart + "-copy" + extPart;
        dstPath = FileOps::joinPath(currentPath, newName);
        
        // Very basic simple check loop for multiple copies
        int safety = 0;
        while (FileOps::fileExists(fs, dstPath) && safety < 10) {
            newName = namePart + "-copy-" + String(safety + 1) + extPart;
            dstPath = FileOps::joinPath(currentPath, newName);
            safety++;
        }
    }

    // Callback for progress
    auto progress = [](int p) {
        String msg = "Processing... " + String(p) + "%";
        UI::drawFooterProgress(p, msg);
    };

    bool success = false;
    if (clipboardOperation == OP_COPY) {
      success = FileOps::copyFile(fs, srcPath, fs, dstPath, progress);
    } else if (clipboardOperation == OP_MOVE) {
      success = FileOps::moveFile(fs, srcPath, fs, dstPath, progress);
    }

    if (success) {
      successCount++;
    }
  }

  if (successCount > 0) {
    String msg = (clipboardOperation == OP_MOVE ? "Moved " : "Copied ") +
                 String(successCount) + " items";
    UI::showToast(msg, ACCENT_COLOR);

    // Clear clipboard if move
    if (clipboardOperation == OP_MOVE) {
      clipboard.clear();
      clipboardOperation = OP_NONE;
    }
    refreshFileList();
  } else {
    UI::showToast("Operation failed", ERROR_COLOR);
  }
}

// ==================== SHOW FILE PROPERTIES ====================
void FileManager::showFileProperties() {
  if (files.empty() || selectedIndex >= files.size())
    return;

  Input::flush();

  FileEntry &entry = files[selectedIndex];

  UI::canvas.fillScreen(BG_COLOR);
  UI::canvas.setTextColor(ACCENT_COLOR, BG_COLOR);
  UI::drawCenteredText("PROPERTIES", 5, ACCENT_COLOR);

  UI::canvas.setTextColor(TEXT_COLOR, BG_COLOR);
  int y = 25;
  UI::canvas.drawString("Name: " + UI::truncateString(entry.name, 30), 5, y);
  y += 12;
  UI::canvas.drawString("Size: " + entry.getSizeStr(), 5, y);
  y += 12;
  UI::canvas.drawString(
      "Type: " + (entry.isDirectory ? String("Folder") : String("File")), 5, y);
  y += 12;
  UI::canvas.drawString("Path: " + UI::truncateString(currentPath, 30), 5, y);

  y += 20;
  UI::canvas.setTextColor(SECONDARY_COLOR, BG_COLOR);
  UI::canvas.drawString("Actions:", 5, y);
  y += 12;

  // Action shortcuts
  UI::canvas.setTextColor(TEXT_COLOR, BG_COLOR);
  UI::canvas.drawString("[O]pen  [D]elete  [R]ename", 5, y);
  y += 12;
  UI::canvas.drawString("[C]opy  [X]Cut    [V]Paste", 5, y);

  UI::pushCanvas();

  // Pick an action, or dismiss on any other key.
  bool done = false;
  while (!done) {
    M5Cardputer.update();
    Input::update();

    if (Input::optJustPressed()) {
      break;
    }

    for (const KeyEvent &event : Input::events()) {
      if (event.repeat) {
        continue;
      }
      done = true;

      // Flush before acting: these actions open dialogs of their own, and the
      // key that chose them must not leak into those.
      switch (event.letter()) {
        case 'o': Input::flush(); openSelected(); break;
        case 'd': Input::flush(); deleteSelected(); break;
        case 'r': Input::flush(); renameSelected(); break;
        case 'c': Input::flush(); copySelected(); break;
        case 'x': Input::flush(); moveSelected(); break;
        case 'v': Input::flush(); pasteFromClipboard(); break;
        default: break;
      }
      break;
    }
    delay(10);
  }

  Input::flush();
  UI::clearScreen();
  UI::pushCanvas();
}

// ==================== SEARCH FILES ====================
void FileManager::searchFiles() {
  String query = UI::showInputDialog("Search", "Find:");
  if (query.length() == 0)
    return;

  std::vector<FileEntry> results;
  FileOps::searchFiles(getCurrentFS(), currentPath, query, results);

  if (results.empty()) {
    UI::showToast("No files found", WARNING_COLOR);
  } else {
    files = results;
    selectedIndex = 0;
    scrollOffset = 0;
    UI::showToast(String(results.size()) + " files found", ACCENT_COLOR);
  }
}

// ==================== SHOW HELP ====================
void FileManager::showHelp() { UI::showHelpMenu(inEditor); }

// ==================== GET CURRENT FS ====================
fs::FS &FileManager::getCurrentFS() { return (fs::FS &)SD; }

// ==================== REFRESH FILE LIST ====================
void FileManager::refreshFileList() {
  FileOps::listDirectory(getCurrentFS(), currentPath, files);

  // Ensure selection is valid
  if (selectedIndex >= files.size()) {
    selectedIndex = max(0, (int)files.size() - 1);
  }

  ensureSelectionVisible();
}

// ==================== HANDLE KEYBOARD ====================
void FileManager::handleKeyboard() {
  // Opt is a modifier and never reaches the event queue, so it is polled.
  if (Input::optJustPressed()) {
    showHelp();
    return;
  }

  for (const KeyEvent &event : Input::events()) {
    // Navigation is the only thing that autorepeats here; repeating a delete or
    // a paste because a key was held would be actively dangerous.
    const bool navigation = !event.fn && (event.key() == ';' || event.key() == '.' ||
                                          event.key() == ',' || event.key() == '/');
    if (event.repeat && !navigation) {
      continue;
    }

    if (event.fn) {
      // Fn+letter shortcuts. These are the ones that keysState().word could not
      // report at all on library 1.2.0 - see the comment block in Input.h.
      switch (event.letter()) {
        case 'h': showHelp(); return;
        case 'f': searchFiles(); return;
        case 'n': {
          String choice = UI::showInputDialog("Create New", "F:File D:Dir", "f");
          if (choice.length() > 0) {
            if (tolower(choice[0]) == 'd') {
              createNewFolder();
            } else {
              createNewFile();
            }
          }
          return;
        }
        case 'd': deleteSelected(); return;
        case 'r': renameSelected(); return;
        case 'c': copySelected(); return;
        case 'x': moveSelected(); return;
        case 'v': pasteFromClipboard(); return;
        case 'p': showFileProperties(); return;
        case 'm': enterMassStorage(); return;
        default: break;
      }
      continue;
    }

    if (event.isEnter()) {
      openSelected();
      return; // The list may have been replaced; do not process stale events.
    }
    if (event.isBackspace()) {
      goBack();
      return;
    }

    switch (event.key()) {
      case ';': navigateUp(); break;
      case '.': navigateDown(); break;
      // ',' and '/' were dead keys marked "// Optional". Wiring them to the
      // parent/enter pair keeps the four navigation keys behaving as one cross.
      case ',': goBack(); return;
      case '/': openSelected(); return;
      default: break;
    }
  }
}

// ==================== ENTER MASS STORAGE ====================
void FileManager::enterMassStorage() {
  appMode = MODE_MASS_STORAGE;
  UI::clearScreen();

  massStorage.begin();
  massStorage.loop(); // Blocks until the user exits or the host ejects.
  massStorage.end();

  appMode = MODE_FILE_MANAGER;
  // The host may have rewritten the FAT underneath us.
  refreshFileList();
  Input::flush();
}

// ==================== HANDLE EDITOR KEYBOARD ====================
void FileManager::handleEditorKeyboard() {
  if (Input::optJustPressed()) {
    showHelp();
    UI::clearScreen();
    UI::pushCanvas();
    return;
  }

  for (const KeyEvent &event : Input::events()) {
    // Cursor movement, backspace and plain typing may repeat; commands must not.
    const bool navigation = !event.fn && (event.key() == ';' || event.key() == '.' ||
                                          event.key() == ',' || event.key() == '/');
    const bool repeatable = navigation || event.isBackspace() || event.text() != 0;
    if (event.repeat && !repeatable) {
      continue;
    }

    if (event.fn) {
      // Fn + the four navigation keys types the character the key is printed
      // with. Navigation stays on the bare keys, so this is the inverse of the
      // library's fn layer - and it frees Ctrl, which the old code abused for
      // this and which getKey() treats as Shift anyway.
      switch (event.key()) {
        case ';':
        case '.':
        case ',':
        case '/':
          editor.insertChar(event.key());
          continue;
        default:
          break;
      }

      if (event.isDeleteForward()) {
        editor.deleteCharForward();
        continue;
      }

      switch (event.letter()) {
        case 's': editor.saveFile(); return;
        case 'q': quitEditor(); return;
        case 'h':
          showHelp();
          UI::clearScreen();
          UI::pushCanvas();
          return;
        case 'f': {
          String query = UI::showInputDialog("Find", "Search:");
          if (query.length() > 0 && !editor.findText(query, true)) {
            UI::showToast("Not found", WARNING_COLOR);
          }
          UI::clearScreen();
          UI::pushCanvas();
          return;
        }
        case 'r': {
          String find = UI::showInputDialog("Replace", "Find:");
          if (find.length() > 0) {
            String replace = UI::showInputDialog("Replace", "Replace with:");
            int count = editor.replaceText(find, replace, true);
            UI::showToast(String(count) + " replaced", ACCENT_COLOR);
          }
          UI::clearScreen();
          UI::pushCanvas();
          return;
        }
        case 'z': editor.undo(); return;
        case 'y': editor.redo(); return;
        default: break;
      }
      continue;
    }

    if (event.isEnter()) {
      editor.insertNewLine();
      continue;
    }
    if (event.isBackspace()) {
      editor.deleteChar();
      continue;
    }
    if (event.isTab()) {
      for (int i = 0; i < TAB_SIZE; i++) {
        editor.insertChar(' ');
      }
      continue;
    }

    switch (event.key()) {
      case ';': editor.moveCursorUp(); continue;
      case '.': editor.moveCursorDown(); continue;
      case ',': editor.moveCursorLeft(); continue;
      case '/': editor.moveCursorRight(); continue;
      default: break;
    }

    // Exactly one character per physical key press. The old code inserted the
    // whole `word` vector, so overlapping two keys re-inserted the first one.
    const char c = event.text();
    if (c != 0) {
      editor.insertChar(c);
    }
  }
}

// ==================== QUIT EDITOR ====================
void FileManager::quitEditor() {
  if (editor.isModified() &&
      UI::showConfirmDialog("Unsaved Changes", "Save before quit?")) {
    editor.saveFile();
  }
  editor.closeFile();
  inEditor = false;
  UI::clearScreen();
  UI::pushCanvas();
}

// ==================== ENSURE SELECTION VISIBLE ====================
void FileManager::ensureSelectionVisible() {
  int visibleLines = (CONTENT_HEIGHT - 4) / LINE_HEIGHT;

  if (selectedIndex < scrollOffset) {
    scrollOffset = selectedIndex;
  } else if (selectedIndex >= scrollOffset + visibleLines) {
    scrollOffset = selectedIndex - visibleLines + 1;
  }

  scrollOffset =
      constrain(scrollOffset, 0, max(0, (int)files.size() - visibleLines));
}
