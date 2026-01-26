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
  lastKeyRepeatTime = 0;
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

  while (true) {
    M5Cardputer.update();
    if (M5Cardputer.Keyboard.isChange()) {
      auto status = M5Cardputer.Keyboard.keysState();

      if (status.word.size() > 0 || status.enter || status.del || status.opt) {
        if (status.word.size() > 0) {
          char key = tolower(status.word[0]);
          if (key == 'o') {
            openSelected();
            break;
          } else if (key == 'd') {
            deleteSelected();
            break;
          } else if (key == 'r') {
            renameSelected();
            break;
          } else if (key == 'c') {
            copySelected();
            break;
          } else if (key == 'x') {
            moveSelected();
            break;
          } else if (key == 'v') {
            pasteFromClipboard();
            break;
          }
        }
        break;
      }
    }
    delay(10);
  }

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
  auto status = M5Cardputer.Keyboard.keysState();
  bool isChange = M5Cardputer.Keyboard.isChange();

  // Check if any relevant key is active
  bool anyPressed = (status.word.size() > 0 || status.enter || status.del ||
                     status.opt || status.fn);

  if (!anyPressed) {
    lastKeyRepeatTime = 0; // Reset repeat timer
    return;
  }

  // Convert word vector to string for easier handling
  String keyWord = "";
  for (char c : status.word) {
    keyWord += c;
  }

  // Check for Opt key for fullscreen help
  if (status.opt && isChange) {
    showHelp();
    return;
  }

  // Check for Fn key combinations (No repeat)
  if (status.fn && isChange) {
    if (keyWord.length() > 0) {
      char key = tolower(keyWord[0]);

      if (key == 'h') {
        showHelp();
      } else if (key == 'f') {
        searchFiles();
      } else if (key == 'n') {
        String choice = UI::showInputDialog("Create New", "F:File D:Dir", "f");
        if (choice.length() > 0) {
          char c = tolower(choice[0]);
          if (c == 'd')
            createNewFolder();
          else
            createNewFile();
        }
      } else if (key == 'd') {
        deleteSelected();
      } else if (key == 'r') {
        renameSelected();
      } else if (key == 'c') {
        copySelected();
      } else if (key == 'x') {
        moveSelected();
      } else if (key == 'v') {
        pasteFromClipboard();
      } else if (key == 'p') {
        showFileProperties();
      } else if (key == 'm') {
        // Enter Mass Storage (Blocking)
        appMode = MODE_MASS_STORAGE;
        UI::clearScreen();
        massStorage.begin();
        massStorage.loop(); // Blocks here until exit
        
        // After exit
        massStorage.end();
        appMode = MODE_FILE_MANAGER;
        refreshFileList();
      }
    }
  }

  // Navigation (if not Fn)
  if (!status.fn) {
    bool isRepeatable = (keyWord == ";" || keyWord == "."); // Up/Down only
    bool shouldProcess = false;

    // Handle repeat
    if (isRepeatable) {
      if (isChange) {
        shouldProcess = true;
        lastKeyRepeatTime = millis() + 500; // Initial delay
      } else if (millis() > lastKeyRepeatTime) {
        shouldProcess = true;
        lastKeyRepeatTime = millis() + 250; // Repeat rate
      }
    } else {
      // Non-repeatable keys: trigger only on change
      shouldProcess = isChange;
    }

    if (shouldProcess) {
      if (keyWord == ";") { // Up arrow (Fn + ;) -> ;
        navigateUp();
      } else if (keyWord == ".") { // Down arrow
        navigateDown();
      } else if (keyWord == ",") { // Left
        // Optional
      } else if (keyWord == "/") { // Right
        // Optional
      } else if (status.enter) {
        openSelected();
      } else if (status.del) {
        goBack();
      }
    }
  }
}

// ==================== HANDLE EDITOR KEYBOARD ====================
void FileManager::handleEditorKeyboard() {
  auto status = M5Cardputer.Keyboard.keysState();
  bool isChange = M5Cardputer.Keyboard.isChange();

  bool anyPressed = (status.word.size() > 0 || status.enter || status.del ||
                     status.opt || status.fn);

  if (!anyPressed) {
    lastKeyRepeatTime = 0;
    return;
  }

  String keyWord = "";
  for (char c : status.word) {
    keyWord += c;
  }

  // Check for Opt key for fullscreen help
  if (status.opt && isChange) {
    showHelp();
    UI::clearScreen();
    UI::pushCanvas();
    return;
  }

  // Check for Fn key combinations (No repeat)
  if (status.fn && isChange) {
    if (keyWord.length() > 0) {
      char key = tolower(keyWord[0]);

      if (key == 's') {
        editor.saveFile();
      } else if (key == 'q') {
        if (editor.isModified()) {
          if (UI::showConfirmDialog("Unsaved Changes", "Save before quit?")) {
            editor.saveFile();
          }
        }
        editor.closeFile();
        inEditor = false;
        UI::clearScreen();
        UI::pushCanvas();
      } else if (key == 'h') {
        showHelp();
        UI::clearScreen();
        UI::pushCanvas();
      } else if (key == 'f') {
        String query = UI::showInputDialog("Find", "Search:");
        if (query.length() > 0) {
          if (!editor.findText(query, true)) {
            UI::showToast("Not found", WARNING_COLOR);
          }
        }
        UI::clearScreen();
        UI::pushCanvas();
      } else if (key == 'r') {
        String find = UI::showInputDialog("Replace", "Find:");
        if (find.length() > 0) {
          String replace = UI::showInputDialog("Replace", "Replace with:");
          int count = editor.replaceText(find, replace, true);
          UI::showToast(String(count) + " replaced", ACCENT_COLOR);
        }
        UI::clearScreen();
        UI::pushCanvas();
      } else if (key == 'z') {
        editor.undo();
      } else if (key == 'y') {
        editor.redo();
      }
    }
    return;
  }

  // Navigation and typing
  if (!status.fn) {
    // Check CTRL state for special char typing vs navigation
    bool isCtrl = status.ctrl;

    String keyWord = "";
    for (char c : status.word) {
        keyWord += c;
    }

    bool isRepeatable = (keyWord == ";" || keyWord == "." || keyWord == "," ||
                         keyWord == "/" || status.del);
    bool shouldProcess = false;

    if (isRepeatable && !isCtrl) { // Repeat only for navigation/del
      if (isChange) {
        shouldProcess = true;
        lastKeyRepeatTime = millis() + 500;
      } else if (millis() > lastKeyRepeatTime) {
        shouldProcess = true;
        lastKeyRepeatTime = millis() + 150;
      }
    } else {
      shouldProcess = isChange;
    }

    if (shouldProcess) {
      if (keyWord == ";") { // Up or ;
        if (isCtrl) editor.insertChar(';'); else editor.moveCursorUp();
      } else if (keyWord == ".") { // Down or .
        if (isCtrl) editor.insertChar('.'); else editor.moveCursorDown();
      } else if (keyWord == ",") { // Left or ,
        if (isCtrl) editor.insertChar(','); else editor.moveCursorLeft();
      } else if (keyWord == "/") { // Right or /
        if (isCtrl) editor.insertChar('/'); else editor.moveCursorRight();
      } else if (isCtrl && keyWord == ":") { // Handle Shifted ;
         editor.insertChar(';');
      } else if (isCtrl && keyWord == ">") { // Handle Shifted .
         editor.insertChar('.');
      } else if (isCtrl && keyWord == "<") { // Handle Shifted ,
         editor.insertChar(',');
      } else if (isCtrl && keyWord == "?") { // Handle Shifted /
         editor.insertChar('/');
      } else if (status.enter) { 
        editor.insertNewLine();
      } else if (status.del && !isCtrl) { // Ctrl+Backspace might be needed?
        editor.deleteChar();
      } else if (keyWord.length() > 0 &&
                 isChange) { // Typing (strictly on change)
        for (char c : keyWord) {
          // Filter out navigation keys if they were already handled above
          if (c == ';' || c == '.' || c == ',' || c == '/') continue;
          // Filter out shifted versions if handled
          if (isCtrl && (c == ':' || c == '>' || c == '<' || c == '?')) continue;
          
          if (c >= 32 && c < 127) {
            editor.insertChar(c);
          }
        }
      }
    }
  }
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
