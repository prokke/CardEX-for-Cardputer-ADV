#include "TextEditor.h"
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
  hasSelection = false;
  selStartRow = selStartCol = selEndRow = selEndCol = 0;
  lastSearchRow = lastSearchCol = 0;
  lastSearchRow = lastSearchCol = 0;
  showLineNumbers = true;

  lastBatteryLevel = 0;
  lastBatteryCheck = 0;
}

// ==================== OPEN FILE ====================
bool TextEditor::openFile(fs::FS &fs, const String &path) {
  File file = fs.open(path, FILE_READ);
  if (!file) {
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

  // Read file line by line
  String currentLine = "";
  while (file.available()) {
    char c = file.read();
    
    if (c == '\n') {
      // Unix style or second char of Windows style
      lines.push_back(currentLine);
      currentLine = "";
    } else if (c == '\r') {
      // Mac style or first char of Windows style
      // Peek next char to see if it's \n
      if (file.peek() == '\n') {
        // Windows style \r\n, handle on next iteration (the \n)
        continue;
      }
      // Old Mac style \r only
      lines.push_back(currentLine);
      currentLine = "";
    } else {
      // Regular character
      currentLine += c;
    }
  }

  // Add last line if not empty or if file was empty
  if (currentLine.length() > 0 || lines.size() == 0) {
    if (lines.size() == 0 && currentLine.length() == 0 && fileSize > 0) {
        // Avoid adding empty line for non-empty file if not needed, 
        // but here we ensure at least one line exists.
        lines.push_back("");
    } else {
        lines.push_back(currentLine);
    }
  }

  file.close();

  // Initialize state
  fileSystem = &fs;
  filePath = path;
  fileOpen = true;
  modified = false;

  // Initial battery check
  lastBatteryLevel = M5Cardputer.Power.getBatteryLevel();
  lastBatteryCheck = millis();

  cursorRow = 0;
  cursorCol = 0;
  scrollRow = 0;
  hasSelection = false;

  return true;
}

// ==================== SAVE FILE ====================
bool TextEditor::saveFile() {
  if (!fileOpen || !fileSystem) {
    return false;
  }

  File file = fileSystem->open(filePath, FILE_WRITE);
  if (!file) {
    UI::showToast("Save failed!", ERROR_COLOR);
    return false;
  }

  // Write all lines
  for (int i = 0; i < lines.size(); i++) {
    file.print(lines[i]);
    if (i < lines.size() - 1) {
      file.print("\n");
    }
  }

  file.close();
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
  if (cursorCol >= line.length()) {
    line += c;
  } else {
    line = line.substring(0, cursorCol) + String(c) + line.substring(cursorCol);
  }

  addUndoAction(UndoAction::INSERT, cursorRow, cursorCol, String(c), "");

  cursorCol++;
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

    if (cursorCol <= line.length()) {
      char deletedChar = line[cursorCol - 1];
      line = line.substring(0, cursorCol - 1) + line.substring(cursorCol);
      addUndoAction(UndoAction::DELETE, cursorRow, cursorCol - 1,
                    String(deletedChar), "");
      cursorCol--;
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

  if (cursorCol < line.length()) {
    // Delete character at cursor
    char deletedChar = line[cursorCol];
    line = line.substring(0, cursorCol) + line.substring(cursorCol + 1);
    addUndoAction(UndoAction::DELETE, cursorRow, cursorCol, String(deletedChar),
                  "");
    modified = true;
  } else if (cursorRow < lines.size() - 1) {
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
  line = line.substring(0, cursorCol);

  lines.insert(lines.begin() + cursorRow + 1, newLine);

  addUndoAction(UndoAction::INSERT, cursorRow, cursorCol, "\n", "");

  cursorRow++;
  cursorCol = 0;
  modified = true;

  adjustScroll();
}

// ==================== MOVE CURSOR ====================
void TextEditor::moveCursor(int dr, int dc) {
  cursorRow += dr;
  cursorCol += dc;
  ensureCursorInBounds();
  adjustScroll();
  hasSelection = false;
}

void TextEditor::moveCursorUp() {
  if (cursorRow > 0) {
    cursorRow--;
    ensureCursorInBounds();
    adjustScroll();
  }
}

void TextEditor::moveCursorDown() {
  if (cursorRow < lines.size() - 1) {
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
  if (cursorCol < lines[cursorRow].length()) {
    cursorCol++;
  } else if (cursorRow < lines.size() - 1) {
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

// ==================== UNDO ====================
void TextEditor::undo() {
  if (undoStack.empty())
    return;

  UndoAction action = undoStack.back();
  undoStack.pop_back();

  // Perform reverse action
  if (action.type == UndoAction::INSERT) {
    // Remove inserted text
    if (action.text == "\n") {
      // Remove newline
      if (action.row < lines.size() - 1) {
        lines[action.row] += lines[action.row + 1];
        lines.erase(lines.begin() + action.row + 1);
      }
    } else {
      // Remove characters
      String &line = lines[action.row];
      line = line.substring(0, action.col) +
             line.substring(action.col + action.text.length());
    }
    cursorRow = action.row;
    cursorCol = action.col;
  } else if (action.type == UndoAction::DELETE) {
    // Restore deleted text
    if (action.text.startsWith("\n")) {
      // Restore newline
      String restOfLine = lines[action.row].substring(action.col);
      lines[action.row] = lines[action.row].substring(0, action.col);
      lines.insert(lines.begin() + action.row + 1, action.text.substring(1));
      cursorRow = action.row + 1;
      cursorCol = 0;
    } else {
      // Restore characters
      String &line = lines[action.row];
      line = line.substring(0, action.col) + action.text +
             line.substring(action.col);
      cursorRow = action.row;
      cursorCol = action.col + action.text.length();
    }
  }

  redoStack.push_back(action);
  ensureCursorInBounds();
  adjustScroll();
}

// ==================== REDO ====================
void TextEditor::redo() {
  if (redoStack.empty())
    return;

  UndoAction action = redoStack.back();
  redoStack.pop_back();

  // Perform action again
  if (action.type == UndoAction::INSERT) {
    if (action.text == "\n") {
      String restOfLine = lines[action.row].substring(action.col);
      lines[action.row] = lines[action.row].substring(0, action.col);
      lines.insert(lines.begin() + action.row + 1, restOfLine);
      cursorRow = action.row + 1;
      cursorCol = 0;
    } else {
      String &line = lines[action.row];
      line = line.substring(0, action.col) + action.text +
             line.substring(action.col);
      cursorRow = action.row;
      cursorCol = action.col + action.text.length();
    }
  } else if (action.type == UndoAction::DELETE) {
    if (action.text.startsWith("\n")) {
      if (action.row < lines.size() - 1) {
        lines[action.row] += lines[action.row + 1];
        lines.erase(lines.begin() + action.row + 1);
      }
    } else {
      String &line = lines[action.row];
      line = line.substring(0, action.col) +
             line.substring(action.col + action.text.length());
    }
    cursorRow = action.row;
    cursorCol = action.col;
  }

  undoStack.push_back(action);
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

  for (int i = startRow; i < lines.size(); i++) {
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

  for (int i = 0; i < lines.size(); i++) {
    String &line = lines[i];
    String lowerLine = line;
    lowerLine.toLowerCase();

    int pos = 0;
    while ((pos = lowerLine.indexOf(lowerFind, pos)) >= 0) {
      line = line.substring(0, pos) + replace +
             line.substring(pos + find.length());
      lowerLine = line;
      lowerLine.toLowerCase();

      count++;
      pos += replace.length();

      if (!replaceAll) {
        modified = true;
        return count;
      }
    }
  }

  if (count > 0) {
    modified = true;
  }

  return count;
}

// ==================== SELECT ALL ====================
void TextEditor::selectAll() {
  hasSelection = true;
  selStartRow = 0;
  selStartCol = 0;
  selEndRow = lines.size() - 1;
  ensureLineExists(selEndRow);
  selEndCol = lines[selEndRow].length();
}

// ==================== COPY SELECTION ====================
void TextEditor::copySelection() {
  if (!hasSelection)
    return;

  clipboard = "";
  for (int i = selStartRow; i <= selEndRow; i++) {
    if (i >= lines.size())
      break;

    int startCol = (i == selStartRow) ? selStartCol : 0;
    int endCol = (i == selEndRow) ? selEndCol : lines[i].length();

    clipboard += lines[i].substring(startCol, endCol);
    if (i < selEndRow) {
      clipboard += "\n";
    }
  }
}

// ==================== CUT SELECTION ====================
void TextEditor::cutSelection() {
  copySelection();
  // TODO: Implement deletion of selection
  hasSelection = false;
}

// ==================== PASTE ====================
void TextEditor::paste() {
  if (clipboard.length() == 0)
    return;

  for (char c : clipboard) {
    if (c == '\n') {
      insertNewLine();
    } else {
      insertChar(c);
    }
  }
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

  if (modified)
    headerText += " *";

  UI::drawHeader(headerText, true, lastBatteryLevel);

  // Content area
  int visibleLines = getVisibleLines();
  int y = HEADER_HEIGHT + 2;
  int lineNumWidth = showLineNumbers ? 20 : 0;

  for (int i = 0; i < visibleLines && (i + scrollRow) < lines.size(); i++) {
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
    int maxChars = (SCREEN_WIDTH - lineNumWidth - 4) / CHAR_WIDTH;

    // Horizontal scroll calculation
    int scrollCol = 0;
    if (lineIndex == cursorRow && cursorCol > maxChars - 5) {
      scrollCol = cursorCol - maxChars + 10;
      if (scrollCol < 0)
        scrollCol = 0;
    }

    // Extract visible portion
    if (displayLine.length() > scrollCol) {
      displayLine = displayLine.substring(scrollCol);
      if (displayLine.length() > maxChars) {
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

  int totalChars = 0;
  for (const String &line : lines) {
    totalChars += line.length();
  }
  String sizeInfo = String(totalChars) + " chars";
  int sizeX = SCREEN_WIDTH - (sizeInfo.length() * CHAR_WIDTH) - 2;
  UI::canvas.drawString(sizeInfo, sizeX, infoY);

  // Push complete frame
  UI::pushCanvas();
}

// ==================== UPDATE ====================
void TextEditor::update() {
  // Update battery every 30 seconds
  if (millis() - lastBatteryCheck > 30000) {
    lastBatteryLevel = M5Cardputer.Power.getBatteryLevel();
    lastBatteryCheck = millis();
  }

  // Blink cursor
  if (millis() - lastBlinkTime > CURSOR_BLINK_MS) {
    cursorVisible = !cursorVisible;
    lastBlinkTime = millis();
  }
}

// ==================== ENSURE CURSOR IN BOUNDS ====================
void TextEditor::ensureCursorInBounds() {
  if (lines.empty()) {
    lines.push_back("");
  }

  cursorRow = constrain(cursorRow, 0, lines.size() - 1);
  ensureLineExists(cursorRow);
  cursorCol = constrain(cursorCol, 0, lines[cursorRow].length());
}

// ==================== ENSURE LINE EXISTS ====================
void TextEditor::ensureLineExists(int row) {
  while (row >= lines.size()) {
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
                               const String &text, const String &oldText) {
  UndoAction action;
  action.type = type;
  action.row = row;
  action.col = col;
  action.text = text;
  action.oldText = oldText;

  undoStack.push_back(action);

  // Limit undo stack size
  if (undoStack.size() > MAX_UNDO_LEVELS) {
    undoStack.erase(undoStack.begin());
  }

  clearRedoStack();
}

// ==================== CLEAR REDO STACK ====================
void TextEditor::clearRedoStack() { redoStack.clear(); }

// ==================== GET VISIBLE LINES ====================
int TextEditor::getVisibleLines() const {
  return (CONTENT_HEIGHT - 4) / LINE_HEIGHT;
}
