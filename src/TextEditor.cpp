#include "TextEditor.h"
#include "Settings.h"
#include "UI.h"
#include <M5Cardputer.h>

// ==================== CONSTRUCTOR ====================
TextEditor::TextEditor() {
  fileSystem = nullptr;
  filePath = "";
  fileOpen = false;
  modified = false;
  cursorRow = 0;
  cursorCol = 0;
  scrollRow = 0;
  lastBlinkTime = 0;
  cursorVisible = true;
  lastSearchRow = lastSearchCol = 0;
  eolStyle = EOL_LF;
  trailingNewline = true;
  totalChars = 0;
  nextUndoGroup = 1;

  lastBatteryLevel = 0;
  lastBatteryCheck = 0;
}

// ==================== OPEN FILE ====================
bool TextEditor::openFile(fs::FS &fs, const String &path) {
  File file = fs.open(path, FILE_READ);
  if (!file) {
    return false;
  }
  if (file.isDirectory()) {
    file.close();
    return false;
  }

  // Check file size
  size_t fileSize = file.size();
  if (fileSize > MAX_FILE_SIZE) {
    file.close();
    UI::showMessageDialog("Error", "File too large (>" + String(MAX_FILE_SIZE / 1024) + "KB)");
    return false;
  }

  // Clear existing content
  lines.clear();
  undoStack.clear();
  redoStack.clear();

  // Read in blocks. The old loop called file.read() once per byte and grew a
  // String one character at a time, which is a reallocation per character - a
  // 32KB file took seconds to open.
  static uint8_t buffer[1024];
  String currentLine;
  currentLine.reserve(96);

  bool pendingCR = false;      // Last byte was '\r'; a following '\n' pairs with it.
  bool endedWithTerminator = false;
  bool eolDetected = false;
  bool binary = false;

  while (file.available() && !binary) {
    const size_t count = file.read(buffer, sizeof(buffer));
    if (count == 0) {
      break;
    }

    for (size_t i = 0; i < count; i++) {
      const char c = (char)buffer[i];

      // A NUL byte means this is not text. Opening a binary file used to fill
      // the editor with garbage, and saving it wrote that garbage back.
      if (c == '\0') {
        binary = true;
        break;
      }

      if (c == '\r') {
        if (!eolDetected) {
          eolStyle = EOL_CR; // Upgraded to CRLF below if '\n' follows.
        }
        lines.push_back(currentLine);
        currentLine = "";
        pendingCR = true;
        endedWithTerminator = true;
        continue;
      }

      if (c == '\n') {
        if (pendingCR) {
          // The line was already pushed when the '\r' arrived.
          if (!eolDetected) {
            eolStyle = EOL_CRLF;
          }
          pendingCR = false;
        } else {
          if (!eolDetected) {
            eolStyle = EOL_LF;
          }
          lines.push_back(currentLine);
          currentLine = "";
        }
        eolDetected = true;
        endedWithTerminator = true;
        continue;
      }

      if (pendingCR) {
        pendingCR = false;
        eolDetected = true; // A lone '\r' really was the terminator.
      }
      currentLine += c;
      endedWithTerminator = false;
    }
  }

  file.close();

  if (binary) {
    lines.clear();
    UI::showMessageDialog("Error", "Binary file");
    return false;
  }

  trailingNewline = endedWithTerminator && fileSize > 0;
  if (!endedWithTerminator) {
    lines.push_back(currentLine);
  }
  if (lines.empty()) {
    lines.push_back("");
  }

  // Initialize state
  fileSystem = &fs;
  filePath = path;
  fileOpen = true;
  modified = false;
  recomputeTotalChars();

  // Initial battery check
  lastBatteryLevel = M5Cardputer.Power.getBatteryLevel();
  lastBatteryCheck = millis();

  cursorRow = 0;
  cursorCol = 0;
  scrollRow = 0;
  lastAutoSave = millis();

  return true;
}

// ==================== SAVE FILE ====================
bool TextEditor::saveFile() {
  if (!fileOpen || !fileSystem) {
    return false;
  }

  // Write to a sibling temp file and swap it in. Opening the real file with
  // FILE_WRITE truncates it first, so a failure part-way through - a full
  // card, a yanked card, a flat battery - destroyed the original and still
  // reported "File saved", because no write result was ever checked.
  const String tempPath = filePath + ".tmp";
  fileSystem->remove(tempPath);

  File file = fileSystem->open(tempPath, FILE_WRITE);
  if (!file) {
    UI::showToast("Save failed!", ERROR_COLOR);
    return false;
  }

  const char *eol =
      (eolStyle == EOL_CRLF) ? "\r\n" : (eolStyle == EOL_CR ? "\r" : "\n");
  const size_t eolLen = strlen(eol);

  size_t expected = 0;
  bool ok = true;

  for (size_t i = 0; i < lines.size() && ok; i++) {
    const String &line = lines[i];
    if (line.length() > 0 && file.print(line) != line.length()) {
      ok = false;
      break;
    }
    expected += line.length();

    // Reproduce the file's original trailing-newline behaviour instead of
    // always dropping it.
    const bool needsEol = (i + 1 < lines.size()) || trailingNewline;
    if (needsEol) {
      if (file.print(eol) != eolLen) {
        ok = false;
        break;
      }
      expected += eolLen;
    }
  }

  file.flush();
  const size_t written = file.size();
  file.close();

  if (!ok || written != expected) {
    fileSystem->remove(tempPath);
    UI::showToast("Save failed - file intact", ERROR_COLOR);
    return false;
  }

  // Swap. If power is lost between these two calls the data survives in the
  // .tmp file, which is strictly better than a truncated original.
  fileSystem->remove(filePath);
  if (!fileSystem->rename(tempPath, filePath)) {
    UI::showToast("Save failed - see .tmp", ERROR_COLOR);
    return false;
  }

  modified = false;
  UI::showToast("File saved", ACCENT_COLOR);
  return true;
}

// ==================== CLOSE FILE ====================
void TextEditor::closeFile() {
  fileOpen = false;
  fileSystem = nullptr;
  filePath = "";
  lines.clear();
  undoStack.clear();
  redoStack.clear();
}

// ==================== INSERT CHAR ====================
void TextEditor::insertChar(char c) {
  if (!fileOpen)
    return;

  ensureLineExists(cursorRow);

  String &line = lines[cursorRow];
  String oldLine = line;

  // Insert character
  if (cursorCol >= (int)line.length()) {
    line += c;
  } else {
    line = line.substring(0, cursorCol) + String(c) + line.substring(cursorCol);
  }

  addUndoAction(UndoAction::INSERT, cursorRow, cursorCol, String(c), "");

  cursorCol++;
  totalChars++;
  modified = true;
  ensureCursorInBounds();
}

// ==================== DELETE CHAR (BACKSPACE) ====================
void TextEditor::deleteChar() {
  if (!fileOpen)
    return;

  if (cursorCol > 0) {
    // Delete character before cursor
    ensureLineExists(cursorRow);
    String &line = lines[cursorRow];

    if (cursorCol <= (int)line.length()) {
      char deletedChar = line[cursorCol - 1];
      line = line.substring(0, cursorCol - 1) + line.substring(cursorCol);
      addUndoAction(UndoAction::DELETE, cursorRow, cursorCol - 1,
                    String(deletedChar), "");
      cursorCol--;
      if (totalChars > 0) totalChars--;
      modified = true;
    }
  } else if (cursorRow > 0) {
    // Merge with previous line
    String currentLine = lines[cursorRow];
    int prevLineLen = lines[cursorRow - 1].length();

    lines[cursorRow - 1] += currentLine;
    lines.erase(lines.begin() + cursorRow);

    addUndoAction(UndoAction::DELETE, cursorRow - 1, prevLineLen,
                  "\n" + currentLine, "");

    cursorRow--;
    cursorCol = prevLineLen;
    modified = true;
  }

  ensureCursorInBounds();
  adjustScroll();
}

// ==================== DELETE CHAR FORWARD ====================
void TextEditor::deleteCharForward() {
  if (!fileOpen)
    return;

  ensureLineExists(cursorRow);
  String &line = lines[cursorRow];

  if (cursorCol < (int)line.length()) {
    // Delete character at cursor
    char deletedChar = line[cursorCol];
    line = line.substring(0, cursorCol) + line.substring(cursorCol + 1);
    addUndoAction(UndoAction::DELETE, cursorRow, cursorCol, String(deletedChar),
                  "");
    if (totalChars > 0) totalChars--;
    modified = true;
  } else if (cursorRow < (int)lines.size() - 1) {
    // Merge with next line
    String nextLine = lines[cursorRow + 1];
    line += nextLine;
    lines.erase(lines.begin() + cursorRow + 1);
    addUndoAction(UndoAction::DELETE, cursorRow, cursorCol, "\n" + nextLine,
                  "");
    modified = true;
  }
}

// ==================== INSERT NEW LINE ====================
void TextEditor::insertNewLine() {
  if (!fileOpen)
    return;

  ensureLineExists(cursorRow);
  String &line = lines[cursorRow];

  // Split line at cursor
  String newLine = line.substring(cursorCol);

  // Carry the current line's leading whitespace onto the new line.
  String indent;
  if (AUTO_INDENT) {
    for (int i = 0; i < cursorCol && i < (int)line.length(); i++) {
      const char c = line[i];
      if (c != ' ' && c != '\t') {
        break;
      }
      indent += c;
    }
    newLine = indent + newLine;
  }

  line = line.substring(0, cursorCol);
  lines.insert(lines.begin() + cursorRow + 1, newLine);

  addUndoAction(UndoAction::INSERT, cursorRow, cursorCol, "\n", "");
  // The indent is a separate step so undo removes the split as one action and
  // the inserted whitespace as another, both consistent with the buffer.
  if (indent.length() > 0) {
    addUndoAction(UndoAction::DELETE, cursorRow + 1, 0, indent, "");
    totalChars += indent.length();
  }

  cursorRow++;
  cursorCol = indent.length();
  modified = true;

  adjustScroll();
}

// ==================== MOVE CURSOR ====================
void TextEditor::moveCursor(int dr, int dc) {
  cursorRow += dr;
  cursorCol += dc;
  ensureCursorInBounds();
  adjustScroll();
}

void TextEditor::moveCursorUp() {
  if (cursorRow > 0) {
    cursorRow--;
    ensureCursorInBounds();
    adjustScroll();
  }
}

void TextEditor::moveCursorDown() {
  // (int) matters: on an empty buffer lines.size() - 1 is SIZE_MAX.
  if (cursorRow < (int)lines.size() - 1) {
    cursorRow++;
    ensureCursorInBounds();
    adjustScroll();
  }
}

void TextEditor::moveCursorLeft() {
  if (cursorCol > 0) {
    cursorCol--;
  } else if (cursorRow > 0) {
    cursorRow--;
    ensureLineExists(cursorRow);
    cursorCol = lines[cursorRow].length();
    adjustScroll();
  }
}

void TextEditor::moveCursorRight() {
  ensureLineExists(cursorRow);
  if (cursorCol < (int)lines[cursorRow].length()) {
    cursorCol++;
  } else if (cursorRow < (int)lines.size() - 1) {
    cursorRow++;
    cursorCol = 0;
    adjustScroll();
  }
}

void TextEditor::moveCursorToLineStart() { cursorCol = 0; }

void TextEditor::moveCursorToLineEnd() {
  ensureLineExists(cursorRow);
  cursorCol = lines[cursorRow].length();
}

void TextEditor::moveCursorToFileStart() {
  cursorRow = 0;
  cursorCol = 0;
  scrollRow = 0;
}

void TextEditor::moveCursorToFileEnd() {
  cursorRow = max(0, (int)lines.size() - 1);
  ensureLineExists(cursorRow);
  cursorCol = lines[cursorRow].length();
  adjustScroll();
}

// ==================== UNDO / REDO ====================
// Applies one action. `reverse` undoes it, otherwise it replays it. Every
// vector access is bounds-checked: the old code indexed lines[action.row]
// directly, and Replace All rewrote lines without recording anything, so the
// stack could hold rows that no longer existed and Fn+Z crashed.
void TextEditor::applyUndoAction(const UndoAction &action, bool reverse) {
  if (action.row < 0 || action.row >= (int)lines.size()) {
    return;
  }

  const bool isInsert = (action.type == UndoAction::INSERT);
  // Undoing an insert removes text; redoing one adds it. Deletes are the
  // mirror image, so one flag decides which half runs.
  const bool addText = isInsert ? !reverse : reverse;

  if (action.type == UndoAction::REPLACE) {
    lines[action.row] = reverse ? action.oldText : action.text;
    cursorRow = action.row;
    cursorCol = 0;
    modified = true;
    return;
  }

  if (action.text == "\n" || action.text.startsWith("\n")) {
    // A line split or a line join.
    if (addText) {
      const int col = constrain(action.col, 0, (int)lines[action.row].length());
      const String tail = (action.text == "\n") ? lines[action.row].substring(col)
                                                : action.text.substring(1);
      lines[action.row] = lines[action.row].substring(0, col);
      lines.insert(lines.begin() + action.row + 1, tail);
      cursorRow = action.row + 1;
      cursorCol = 0;
    } else if (action.row + 1 < (int)lines.size()) {
      lines[action.row] += lines[action.row + 1];
      lines.erase(lines.begin() + action.row + 1);
      cursorRow = action.row;
      cursorCol = constrain(action.col, 0, (int)lines[action.row].length());
    }
    modified = true;
    return;
  }

  String &line = lines[action.row];
  const int col = constrain(action.col, 0, (int)line.length());

  if (addText) {
    line = line.substring(0, col) + action.text + line.substring(col);
    cursorRow = action.row;
    cursorCol = col + action.text.length();
  } else {
    const int endCol =
        constrain(col + (int)action.text.length(), col, (int)line.length());
    line = line.substring(0, col) + line.substring(endCol);
    cursorRow = action.row;
    cursorCol = col;
  }
  modified = true;
}

void TextEditor::undo() {
  if (undoStack.empty())
    return;

  const uint16_t group = undoStack.back().group;
  do {
    UndoAction action = undoStack.back();
    undoStack.pop_back();
    applyUndoAction(action, true);
    redoStack.push_back(action);
    // Grouped actions (Replace All) undo as one step.
  } while (group != 0 && !undoStack.empty() && undoStack.back().group == group);

  recomputeTotalChars();
  ensureCursorInBounds();
  adjustScroll();
}

void TextEditor::redo() {
  if (redoStack.empty())
    return;

  const uint16_t group = redoStack.back().group;
  do {
    UndoAction action = redoStack.back();
    redoStack.pop_back();
    applyUndoAction(action, false);
    undoStack.push_back(action);
  } while (group != 0 && !redoStack.empty() && redoStack.back().group == group);

  recomputeTotalChars();
  ensureCursorInBounds();
  adjustScroll();
}

// ==================== FIND TEXT ====================
bool TextEditor::findText(const String &query, bool fromStart) {
  if (query.length() == 0)
    return false;

  int startRow = fromStart ? 0 : cursorRow;
  int startCol = fromStart ? 0 : cursorCol + 1;

  String lowerQuery = query;
  lowerQuery.toLowerCase();

  for (int i = startRow; i < (int)lines.size(); i++) {
    String lowerLine = lines[i];
    lowerLine.toLowerCase();

    int col = (i == startRow) ? startCol : 0;
    int pos = lowerLine.indexOf(lowerQuery, col);

    if (pos >= 0) {
      cursorRow = i;
      cursorCol = pos;
      lastSearchRow = i;
      lastSearchCol = pos;
      adjustScroll();
      return true;
    }
  }

  return false;
}

// ==================== REPLACE TEXT ====================
int TextEditor::replaceText(const String &find, const String &replace,
                            bool replaceAll) {
  if (find.length() == 0)
    return 0;

  int count = 0;
  String lowerFind = find;
  lowerFind.toLowerCase();

  // One group for the whole operation, so a Replace All undoes in a single
  // Fn+Z. Previously nothing was recorded at all: the replacement could not be
  // undone, and the stale rows left on the undo stack could crash a later undo.
  const uint16_t group = nextUndoGroup++;
  if (nextUndoGroup == 0) {
    nextUndoGroup = 1; // 0 means "not grouped".
  }

  for (size_t i = 0; i < lines.size(); i++) {
    String &line = lines[i];
    const String originalLine = line;

    String lowerLine = line;
    lowerLine.toLowerCase();

    int pos = 0;
    bool lineChanged = false;

    while ((pos = lowerLine.indexOf(lowerFind, pos)) >= 0) {
      line = line.substring(0, pos) + replace +
             line.substring(pos + find.length());
      lowerLine = line;
      lowerLine.toLowerCase();

      count++;
      lineChanged = true;
      pos += replace.length();

      if (!replaceAll) {
        break;
      }
    }

    if (lineChanged) {
      addUndoAction(UndoAction::REPLACE, (int)i, 0, line, originalLine, group);
    }
    if (count > 0 && !replaceAll) {
      break;
    }
  }

  if (count > 0) {
    modified = true;
    recomputeTotalChars();
  }

  return count;
}

// ==================== RENDER ====================
void TextEditor::render() {
  if (!fileOpen)
    return;

  // Draw header to canvas
  String fileName = FileOps::getFileName(filePath);
  String headerText = fileName + " [" + String(cursorRow + 1) + "/" +
                      String(lines.size()) + "]";
  if (modified)
    headerText += " *";

  UI::drawHeader(headerText, true, lastBatteryLevel);

  // Content area
  int visibleLines = getVisibleLines();
  int y = HEADER_HEIGHT + 2;
  const bool showLineNumbers = SHOW_LINE_NUMBERS;
  const int lineNumWidth = showLineNumbers ? 20 : 0;

  for (int i = 0; i < visibleLines && (i + scrollRow) < (int)lines.size(); i++) {
    int lineIndex = i + scrollRow;

    // Clear line background on canvas
    UI::canvas.fillRect(0, y, SCREEN_WIDTH, LINE_HEIGHT, BG_COLOR);

    // Line number
    if (showLineNumbers) {
      UI::canvas.setTextColor(SECONDARY_COLOR, BG_COLOR);
      String lineNum = String(lineIndex + 1) + " ";
      UI::canvas.drawString(lineNum, 4, y + 2);
    }

    // Line text with horizontal scrolling
    UI::canvas.setTextColor(TEXT_COLOR, BG_COLOR);
    String displayLine = lines[lineIndex];
    const int maxChars = (SCREEN_WIDTH - lineNumWidth - 4) / CHAR_WIDTH;

    // Horizontal scroll calculation
    int scrollCol = 0;
    if (lineIndex == cursorRow && cursorCol > maxChars - 5) {
      scrollCol = cursorCol - maxChars + 10;
      if (scrollCol < 0)
        scrollCol = 0;
    }

    // Extract visible portion
    if ((int)displayLine.length() > scrollCol) {
      displayLine = displayLine.substring(scrollCol);
      if ((int)displayLine.length() > maxChars) {
        displayLine = displayLine.substring(0, maxChars);
      }
    } else {
      displayLine = "";
    }

    UI::canvas.drawString(displayLine, lineNumWidth + 2, y + 2);

    // Draw cursor
    if (lineIndex == cursorRow && cursorVisible) {
      int cursorX = lineNumWidth + 2 + ((cursorCol - scrollCol) * CHAR_WIDTH);
      if (cursorX >= lineNumWidth + 2 && cursorX < SCREEN_WIDTH - 2) {
        UI::canvas.drawLine(cursorX, y + 2, cursorX, y + 10, CURSOR_COLOR);
      }
    }

    y += LINE_HEIGHT;
  }

  // Clear remaining space to footer
  if (y < SCREEN_HEIGHT - 12) {
    UI::canvas.fillRect(0, y, SCREEN_WIDTH, (SCREEN_HEIGHT - 12) - y, BG_COLOR);
  }

  // File info at bottom
  int infoY = SCREEN_HEIGHT - 10;
  UI::canvas.fillRect(0, infoY, SCREEN_WIDTH, 10, BG_COLOR); // Clear info area
  UI::canvas.setTextColor(SECONDARY_COLOR, BG_COLOR);

  String posInfo =
      "Ln " + String(cursorRow + 1) + ", Col " + String(cursorCol + 1);
  UI::canvas.drawString(posInfo, 2, infoY);

  String sizeInfo = String((unsigned)totalChars) + " chars";
  int sizeX = SCREEN_WIDTH - (sizeInfo.length() * CHAR_WIDTH) - 2;
  UI::canvas.drawString(sizeInfo, sizeX, infoY);

  // Push complete frame
  UI::pushCanvas();
}

// ==================== UPDATE ====================
bool TextEditor::update() {
  bool changed = false;

  // Update battery every 30 seconds
  if (millis() - lastBatteryCheck > 30000) {
    lastBatteryLevel = M5Cardputer.Power.getBatteryLevel();
    lastBatteryCheck = millis();
  }

  // Autosave. AUTO_SAVE_INTERVAL has been in CardEX.ini since the start but
  // nothing ever read it; 0 keeps it off, which is the default.
  if (fileOpen && modified && AUTO_SAVE_INTERVAL > 0 &&
      millis() - lastAutoSave > (unsigned long)AUTO_SAVE_INTERVAL) {
    lastAutoSave = millis();
    saveFile();
    changed = true;
  }

  // Blink cursor
  if (millis() - lastBlinkTime > (unsigned long)CURSOR_BLINK_MS) {
    cursorVisible = !cursorVisible;
    lastBlinkTime = millis();
    changed = true;
  }

  return changed;
}

// ==================== ENSURE CURSOR IN BOUNDS ====================
void TextEditor::ensureCursorInBounds() {
  if (lines.empty()) {
    lines.push_back("");
  }

  cursorRow = constrain(cursorRow, 0, (int)lines.size() - 1);
  ensureLineExists(cursorRow);
  cursorCol = constrain(cursorCol, 0, (int)lines[cursorRow].length());
}

// ==================== ENSURE LINE EXISTS ====================
void TextEditor::ensureLineExists(int row) {
  while (row >= (int)lines.size()) {
    lines.push_back("");
  }
}

// ==================== ADJUST SCROLL ====================
void TextEditor::adjustScroll() {
  int visibleLines = getVisibleLines();

  if (cursorRow < scrollRow) {
    scrollRow = cursorRow;
  } else if (cursorRow >= scrollRow + visibleLines) {
    scrollRow = cursorRow - visibleLines + 1;
  }

  scrollRow = constrain(scrollRow, 0, max(0, (int)lines.size() - visibleLines));
}

// ==================== ADD UNDO ACTION ====================
void TextEditor::addUndoAction(UndoAction::Type type, int row, int col,
                               const String &text, const String &oldText,
                               uint16_t group) {
  UndoAction action;
  action.type = type;
  action.row = row;
  action.col = col;
  action.text = text;
  action.oldText = oldText;
  action.group = group;

  undoStack.push_back(action);

  // Limit undo stack size. The cast matters: MAX_UNDO_LEVELS is an int, and an
  // unsigned comparison against a negative value never trims.
  const size_t limit = MAX_UNDO_LEVELS > 0 ? (size_t)MAX_UNDO_LEVELS : 1;
  while (undoStack.size() > limit) {
    // Drop whole groups, never half of one.
    const uint16_t oldestGroup = undoStack.front().group;
    undoStack.erase(undoStack.begin());
    if (oldestGroup != 0) {
      while (!undoStack.empty() && undoStack.front().group == oldestGroup) {
        undoStack.erase(undoStack.begin());
      }
    }
  }

  clearRedoStack();
}

// ==================== RECOMPUTE TOTAL CHARS ====================
void TextEditor::recomputeTotalChars() {
  totalChars = 0;
  for (const String &line : lines) {
    totalChars += line.length();
  }
}

// ==================== CLEAR REDO STACK ====================
void TextEditor::clearRedoStack() { redoStack.clear(); }

// ==================== GET VISIBLE LINES ====================
int TextEditor::getVisibleLines() const {
  return (CONTENT_HEIGHT - 4) / LINE_HEIGHT;
}
