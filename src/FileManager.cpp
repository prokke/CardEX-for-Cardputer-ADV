#include "FileManager.h"
#include "Bookmarks.h"
#include "Feedback.h"
#include "StatusLed.h"
#include "PathUtils.h"
#include "Settings.h"
#include "SettingsScreen.h"
#include "UI.h"
#include "viewers/HexViewer.h"
#include "viewers/ImageViewer.h"
#include "viewers/TextViewer.h"
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
  searchMode = false;
  listTruncated = false;
  lastBatteryLevel = 0;
  lastBatteryCheck = 0;
  cachedFreeBytes = 0;
  lastFreeSpaceCheck = 0;
  renderRequested = true;
}

// ==================== INIT STORAGE ====================
bool FileManager::initStorage() {
  static bool spiStarted = false;
  if (!spiStarted) {
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
    spiStarted = true;
  }
  return SD.begin(SD_SPI_CS_PIN, SPI);
}

// ==================== INIT ====================
bool FileManager::init() {
  // The card is mounted and the config loaded by setup() before the splash.
  sdAvailable = true;
  useSD = true;
  Settings::apply();

  // Initial battery check
  lastBatteryLevel = M5Cardputer.Power.getBatteryLevel();
  lastBatteryCheck = millis();

  // Load initial file list
  refreshFileList();

  return true;
}

// ==================== UPDATE ====================
void FileManager::update() {
  renderRequested = false;

  for (const KeyEvent &event : Input::events()) {
    if (!event.repeat) {
      Feedback::key();
      break;
    }
  }

  // Update battery every 30 seconds
  if (millis() - lastBatteryCheck > 30000) {
    const int level = M5Cardputer.Power.getBatteryLevel();
    if (level != lastBatteryLevel) {
      renderRequested = true;
    }
    lastBatteryLevel = level;
    lastBatteryCheck = millis();
  }

  updateFreeSpace(false);

  // Mass storage runs its own blocking loop from handleKeyboard(), so this
  // state is never observed here.
  if (appMode == MODE_MASS_STORAGE) {
    return;
  }

  // Any key event may have changed something, so that alone earns a repaint.
  if (!Input::events().empty() || Input::optJustPressed()) {
    renderRequested = true;
  }
  // The toast has to be drawn while it lives and once more when it expires.
  if (UI::toastVisible()) {
    renderRequested = true;
  }

  if (inEditor) {
    if (editor.update()) {
      renderRequested = true; // Cursor blinked or an autosave ran.
    }
    handleEditorKeyboard();
  } else {
    handleKeyboard();
  }

  // Unsaved work outranks a pending paste - it is the one that loses data.
  if (inEditor && editor.isModified()) {
    StatusLed::setState(LED_STATE_EDIT);
  } else if (!clipboard.empty()) {
    StatusLed::setState(LED_STATE_CLIPBOARD);
  } else {
    StatusLed::setState(LED_STATE_IDLE);
  }
}

// ==================== UPDATE FREE SPACE ====================
void FileManager::updateFreeSpace(bool force) {
  if (!sdAvailable) {
    cachedFreeBytes = 0;
    return;
  }
  // usedBytes() is a full FAT walk. Calling it every frame was the single
  // biggest cost in the render path, and on the ADV a slow loop also overflows
  // the keyboard controller's 10-event FIFO and drops keystrokes.
  if (!force && lastFreeSpaceCheck != 0 &&
      millis() - lastFreeSpaceCheck < 30000) {
    return;
  }
  cachedFreeBytes = SD.totalBytes() - SD.usedBytes();
  lastFreeSpaceCheck = millis();
  renderRequested = true;
}

// ==================== RENDER ====================
void FileManager::render() {
  if (inEditor) {
    editor.render();
  } else {
    // Draw to canvas. In search mode the list no longer reflects currentPath,
    // so say so rather than showing a path the entries do not belong to.
    UI::drawHeader(searchMode ? ("Found: " + searchQuery) : currentPath, useSD,
                   lastBatteryLevel);
    UI::drawFileList(files, selectedIndex, scrollOffset);

    // Footer info. The selection count replaces the item count while a
    // selection exists - that is the number that matters then.
    const int selectedCount = selectionCount();
    String countInfo = selectedCount > 0
                           ? (String(selectedCount) + "/" +
                              String(files.size()) + " sel")
                           : (String(files.size()) + " items");
    if (listTruncated) {
      // MAX_FILES_IN_LIST entries were listed and there were more; the old
      // build just stopped, with nothing on screen to say so.
      countInfo += "+";
    }

    // Free space, from the cache rather than a FAT walk per frame.
    String status = countInfo;
    if (sdAvailable) {
      status += " | " + FileOps::formatBytes(cachedFreeBytes) + " free";
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
  // (int) matters: files.size() is unsigned, so on an empty list size() - 1 is
  // SIZE_MAX and the comparison was always true.
  if (selectedIndex < (int)files.size() - 1) {
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
  const String fullPath = entry.fullPath;

  if (entry.isDirectory) {
    // Navigate into directory. Search results live outside currentPath, so the
    // entry's own absolute path is the only correct source here.
    currentPath = fullPath;
    selectedIndex = 0;
    scrollOffset = 0;
    searchMode = false;
    refreshFileList();
  } else {
    openFileByType(fullPath, entry.size);
  }
}

// ==================== OPEN FILE BY TYPE ====================
// Everything used to go to the editor, including binaries - which loaded as
// garbage and were written back as garbage on save - and anything over
// MAX_FILE_SIZE simply refused to open at all.
void FileManager::openFileByType(const String &path, size_t size) {
  if (ImageViewer::handles(path)) {
    ImageViewer::run(getCurrentFS(), path);
    return;
  }

  if (!looksLikeText(path)) {
    HexViewer::run(getCurrentFS(), path);
    return;
  }

  // Too large to hold in RAM, but still perfectly readable.
  if (size > MAX_FILE_SIZE) {
    TextViewer::run(getCurrentFS(), path);
    return;
  }

  if (editor.openFile(getCurrentFS(), path)) {
    inEditor = true;
    Input::flush();
  } else {
    // openFile() reports its own reason (too large, binary, unreadable).
    Input::flush();
    UI::clearScreen();
    UI::pushCanvas();
  }
}

// ==================== LOOKS LIKE TEXT ====================
// Extension first, then a peek at the content. Sniffing matters because plain
// text on a Cardputer often has no extension at all.
bool FileManager::looksLikeText(const String &path) {
  static const char *const kTextExtensions[] = {
      "txt", "md",   "ini", "cfg", "conf", "json", "xml",  "csv", "log",
      "c",   "h",    "cpp", "hpp", "py",   "js",   "ts",   "css", "html",
      "sh",  "yaml", "yml", "toml", "ino", "rs",   "go",   "lua"};

  const std::string ext = PathUtils::extension(std::string(path.c_str()));
  for (const char *candidate : kTextExtensions) {
    if (ext == candidate) {
      return true;
    }
  }

  File file = getCurrentFS().open(path, FILE_READ);
  if (!file) {
    return false;
  }

  // A NUL in the first block means binary. Counting control characters as well
  // catches files that are technically NUL-free but unreadable as text.
  uint8_t sample[256];
  const size_t count = file.read(sample, sizeof(sample));
  file.close();

  if (count == 0) {
    return true; // Empty file: let the editor have it.
  }

  size_t suspicious = 0;
  for (size_t i = 0; i < count; i++) {
    const uint8_t c = sample[i];
    if (c == 0) {
      return false;
    }
    const bool printable = (c >= 0x20 && c < 0x7F) || c == '\t' || c == '\n' ||
                           c == '\r' || c >= 0x80; // >=0x80 may be UTF-8.
    if (!printable) {
      suspicious++;
    }
  }
  return suspicious * 10 < count; // Under 10% control bytes.
}

// ==================== GO BACK ====================
void FileManager::goBack() {
  // Leaving search results returns to the directory the search started in.
  if (searchMode) {
    searchMode = false;
    refreshFileList();
    return;
  }

  if (currentPath == "/") {
    return;
  }

  // The directory we are leaving is the last component of the current path.
  // Deriving it beats the old parallel dirHistory stack, which drifted out of
  // sync whenever the list was replaced by a search.
  const String lastDir = FileOps::getFileName(currentPath);

  currentPath = FileOps::getParentPath(currentPath);
  selectedIndex = 0;
  scrollOffset = 0;
  refreshFileList();
  
  // Find and select the directory we came from
  if (lastDir.length() > 0) {
    for (int i = 0; i < (int)files.size(); i++) {
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

  // The dialog accepts any character, so "../CardEX.ini" used to be a valid
  // answer: joinPath would resolve it and the file would be created outside
  // the current directory.
  if (!FileOps::isValidFileName(fileName)) {
    UI::showToast("Invalid name", ERROR_COLOR);
    return;
  }

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

  if (!FileOps::isValidFileName(folderName)) {
    UI::showToast("Invalid name", ERROR_COLOR);
    return;
  }

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
  const std::vector<String> targets = targetPaths();
  if (targets.empty())
    return;

  // One confirmation for the batch, naming what is about to go.
  String message;
  if (targets.size() == 1) {
    message = "Delete \"" +
              UI::truncateString(FileOps::getFileName(targets[0]), 18) + "\"?";
    if (FileOps::isDirectory(getCurrentFS(), targets[0])) {
      const int childCount = FileOps::countEntries(getCurrentFS(), targets[0]);
      if (childCount > 0) {
        message = "Folder + " + String(childCount) + " items?";
      }
    }
  } else {
    message = "Delete " + String(targets.size()) + " items?";
  }

  if (CONFIRM_DELETE && !UI::showConfirmDialog("Confirm Delete", message)) {
    return;
  }

  int deleted = 0;
  for (const String &path : targets) {
    const bool isDir = FileOps::isDirectory(getCurrentFS(), path);
    const bool ok = isDir ? FileOps::deleteDirectory(getCurrentFS(), path)
                          : FileOps::deleteFile(getCurrentFS(), path);
    if (ok) {
      deleted++;
      // A bookmark pointing at something that no longer exists would show up
      // as a star on an unrelated file created later at the same path.
      Bookmarks::remove(path);
    }
  }

  if (deleted == (int)targets.size()) {
    UI::showToast(String(deleted) + " deleted", ACCENT_COLOR);
  } else if (deleted > 0) {
    UI::showToast(String(deleted) + "/" + String(targets.size()) + " deleted",
                  WARNING_COLOR);
  } else {
    UI::showToast("Delete failed", ERROR_COLOR);
  }

  // Keep the cursor inside what will be left. refreshFileList() clamps the
  // upper bound but not the lower one, so clamp here.
  selectedIndex = max(0, selectedIndex - deleted);
  refreshFileList();
}

// ==================== RENAME SELECTED ====================
void FileManager::renameSelected() {
  if (files.empty() || selectedIndex >= files.size())
    return;

  FileEntry &entry = files[selectedIndex];
  String newName = UI::showInputDialog("Rename", "New name:", entry.name);

  if (newName.length() == 0 || newName == entry.name)
    return;

  if (!FileOps::isValidFileName(newName)) {
    UI::showToast("Invalid name", ERROR_COLOR);
    return;
  }

  const String oldPath = entry.fullPath;
  const String newPath =
      FileOps::joinPath(FileOps::getParentPath(oldPath), newName);

  // rename() over an existing entry is not something the user asked for.
  if (FileOps::fileExists(getCurrentFS(), newPath)) {
    UI::showToast("Name already exists", ERROR_COLOR);
    return;
  }

  if (FileOps::renameFile(getCurrentFS(), oldPath, newPath)) {
    // Otherwise the star would stay behind on a path that no longer exists.
    if (entry.bookmarked) {
      Bookmarks::remove(oldPath);
      Bookmarks::toggle(newPath);
    }
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

  clipboard = targetPaths();
  if (clipboard.empty()) {
    return;
  }
  clipboardOperation = OP_COPY;

  UI::showToast(String(clipboard.size()) + " copied", ACCENT_COLOR);
}

// ==================== MOVE SELECTED ====================
void FileManager::moveSelected() {
  if (files.empty() || selectedIndex >= files.size())
    return;

  clipboard = targetPaths();
  if (clipboard.empty()) {
    return;
  }
  clipboardOperation = OP_MOVE;

  UI::showToast(String(clipboard.size()) + " cut", ACCENT_COLOR);
}

// ==================== PASTE FROM CLIPBOARD ====================
void FileManager::pasteFromClipboard() {
  if (clipboard.empty() || clipboardOperation == OP_NONE) {
    UI::showToast("Clipboard empty", WARNING_COLOR);
    return;
  }

  int successCount = 0;
  for (const String &srcPath : clipboard) {
    fs::FS &fs = getCurrentFS();

    if (!FileOps::fileExists(fs, srcPath)) {
      UI::showToast("Source is gone", ERROR_COLOR);
      continue;
    }

    const bool srcIsDir = FileOps::isDirectory(fs, srcPath);
    const String fileName = FileOps::getFileName(srcPath);
    String dstPath = FileOps::joinPath(currentPath, fileName);

    // Copying or moving a folder into its own subtree would recurse until the
    // card filled up. isPathInside() normalizes both sides first.
    if (srcIsDir && FileOps::isPathInside(srcPath, currentPath)) {
      UI::showToast("Cannot paste into itself", ERROR_COLOR);
      continue;
    }
    if (dstPath == srcPath) {
      UI::showToast("Already here", WARNING_COLOR);
      continue;
    }

    // Handle name collision: name.ext -> name-copy.ext, then -copy-1 and so on.
    if (FileOps::fileExists(fs, dstPath)) {
      const int extIndex = srcIsDir ? -1 : fileName.lastIndexOf('.');
      const String namePart =
          (extIndex > 0) ? fileName.substring(0, extIndex) : fileName;
      const String extPart = (extIndex > 0) ? fileName.substring(extIndex) : "";

      String newName = namePart + "-copy" + extPart;
      dstPath = FileOps::joinPath(currentPath, newName);

      int attempt = 1;
      while (FileOps::fileExists(fs, dstPath) && attempt <= 99) {
        newName = namePart + "-copy-" + String(attempt) + extPart;
        dstPath = FileOps::joinPath(currentPath, newName);
        attempt++;
      }
      if (FileOps::fileExists(fs, dstPath)) {
        UI::showToast("Too many copies", ERROR_COLOR);
        continue;
      }
    }

    // Callback for progress
    auto progress = [](int p) {
        String msg = "Processing... " + String(p) + "%";
        UI::drawFooterProgress(p, msg);
    };

    bool success = false;
    if (clipboardOperation == OP_COPY) {
      // Folders used to fall straight through copyFile(), which refuses
      // directories, so "cut/copy a folder then paste" always failed.
      success = srcIsDir ? FileOps::copyDirectory(fs, srcPath, fs, dstPath)
                         : FileOps::copyFile(fs, srcPath, fs, dstPath, progress);
    } else if (clipboardOperation == OP_MOVE) {
      if (srcIsDir) {
        success = fs.rename(srcPath, dstPath);
        if (!success && FileOps::copyDirectory(fs, srcPath, fs, dstPath)) {
          success = FileOps::deleteDirectory(fs, srcPath);
        }
      } else {
        success = FileOps::moveFile(fs, srcPath, fs, dstPath, progress);
      }
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
  UI::canvas.drawString(
      "Path: " + UI::truncateString(FileOps::getParentPath(entry.fullPath), 30),
      5, y);

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

// ==================== SELECTION ====================
int FileManager::selectionCount() const {
  int count = 0;
  for (const FileEntry &entry : files) {
    if (entry.selected) {
      count++;
    }
  }
  return count;
}

void FileManager::toggleSelection() {
  if (files.empty() || selectedIndex >= (int)files.size()) {
    return;
  }
  files[selectedIndex].selected = !files[selectedIndex].selected;
  // Move on, so tapping Space repeatedly picks out a run of files.
  navigateDown();
}

void FileManager::selectAll(bool select) {
  for (FileEntry &entry : files) {
    entry.selected = select;
  }
  UI::showToast(select ? (String(files.size()) + " selected") : String("Selection cleared"),
                select ? ACCENT_COLOR : SECONDARY_COLOR);
}

std::vector<String> FileManager::targetPaths() const {
  std::vector<String> paths;
  for (const FileEntry &entry : files) {
    if (entry.selected) {
      paths.push_back(entry.fullPath);
    }
  }
  // Nothing ticked means "the thing I am pointing at", which is how the app
  // behaved before multi-selection existed.
  if (paths.empty() && !files.empty() && selectedIndex < (int)files.size()) {
    paths.push_back(files[selectedIndex].fullPath);
  }
  return paths;
}

// ==================== BOOKMARKS ====================
void FileManager::toggleBookmark() {
  if (files.empty() || selectedIndex >= (int)files.size()) {
    return;
  }

  FileEntry &entry = files[selectedIndex];
  if (!entry.bookmarked && Bookmarks::isFull()) {
    UI::showToast("Bookmark list is full", ERROR_COLOR);
    return;
  }

  const String path = entry.fullPath;
  const bool nowBookmarked = Bookmarks::toggle(path);
  UI::showToast(nowBookmarked ? "Bookmarked" : "Bookmark removed",
                nowBookmarked ? ACCENT_COLOR : SECONDARY_COLOR);

  // Re-listing re-reads the bookmark flags and re-sorts, which moves the entry
  // to or from the top of the list. Keep the cursor on it.
  refreshFileList();
  for (int i = 0; i < (int)files.size(); i++) {
    if (files[i].fullPath == path) {
      selectedIndex = i;
      ensureSelectionVisible();
      break;
    }
  }
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
    // Without this the replaced list was a dead end: currentPath still pointed
    // at the search root, so Back at the top level did nothing at all.
    searchMode = true;
    searchQuery = query;
    UI::showToast(String(results.size()) + " found - Esc to exit", ACCENT_COLOR);
  }
}

// ==================== SHOW HELP ====================
void FileManager::showHelp() { UI::showHelpMenu(inEditor); }

// ==================== GET CURRENT FS ====================
fs::FS &FileManager::getCurrentFS() { return (fs::FS &)SD; }

// ==================== REFRESH FILE LIST ====================
void FileManager::refreshFileList() {
  searchMode = false;
  updateFreeSpace(true); // The listing changed, so the number probably did too.
  FileOps::listDirectory(getCurrentFS(), currentPath, files, &listTruncated);

  // Ensure selection is valid
  if (selectedIndex >= (int)files.size()) {
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
        case 'b': toggleBookmark(); return;
        case 'a':
          // Toggle: a second Fn+A clears, which is what you want after a
          // batch operation.
          selectAll(selectionCount() != (int)files.size());
          return;
        case 'm': enterMassStorage(); return;
        case 'o':
          // Settings. Re-sort afterwards: sort order and hidden-file
          // visibility are both settings.
          if (SettingsScreen::run()) {
            refreshFileList();
          }
          return;
        default: break;
      }
      continue;
    }

    // Space ticks the entry under the cursor. Operations then act on every
    // ticked entry instead of just the one being pointed at.
    if (event.text() == ' ') {
      toggleSelection();
      continue;
    }

    if (event.isEnter()) {
      openSelected();
      return; // The list may have been replaced; do not process stale events.
    }
    if (event.isBackspace() || event.isEsc()) {
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
  // While the host owns the card it writes raw sectors underneath our
  // filesystem. Anything we have open would be working from a stale view, and
  // unsaved edits would be lost by the remount below.
  if (inEditor) {
    UI::showToast("Close the editor first", WARNING_COLOR);
    return;
  }

  appMode = MODE_MASS_STORAGE;
  StatusLed::setState(LED_STATE_USB);
  UI::clearScreen();

  massStorage.begin();
  massStorage.loop(); // Blocks until the user exits or the host ejects.
  massStorage.end();

  // Remount. The host has very likely rewritten the FAT and directory sectors
  // under us, so the mounted filesystem's cached view is stale; re-listing
  // alone would show entries that no longer exist.
  SD.end();
  sdAvailable = initStorage();
  if (!sdAvailable) {
    UI::showMessageDialog("Error", "SD remount failed");
  }

  appMode = MODE_FILE_MANAGER;
  currentPath = "/"; // The path we were in may be gone.
  selectedIndex = 0;
  scrollOffset = 0;
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
      if (TAB_USES_SPACES) {
        for (int i = 0; i < TAB_SIZE; i++) {
          editor.insertChar(' ');
        }
      } else {
        editor.insertChar('\t');
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
