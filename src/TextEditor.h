#ifndef TEXTEDITOR_H
#define TEXTEDITOR_H

#include "Config.h"
#include <Arduino.h>
#include <FS.h>
#include <vector>

// ==================== UNDO ACTION ====================
struct UndoAction {
  enum Type { INSERT, DELETE, REPLACE } type;
  int row, col;
  String text;
  String oldText;
};

// ==================== TEXT EDITOR CLASS ====================
class TextEditor {
public:
  TextEditor();

  // File operations
  bool openFile(fs::FS &fs, const String &path);
  bool saveFile();
  void closeFile();

  // Editing
  void insertChar(char c);
  void deleteChar();        // Backspace
  void deleteCharForward(); // Delete
  void insertNewLine();

  // Cursor movement
  void moveCursor(int dr, int dc);
  void moveCursorUp();
  void moveCursorDown();
  void moveCursorLeft();
  void moveCursorRight();
  void moveCursorToLineStart();
  void moveCursorToLineEnd();
  void moveCursorToFileStart();
  void moveCursorToFileEnd();

  // Undo/Redo
  void undo();
  void redo();

  // Search
  bool findText(const String &query, bool fromStart = false);
  int replaceText(const String &find, const String &replace,
                  bool replaceAll = false);

  // Selection
  void selectAll();
  void copySelection();
  void cutSelection();
  void paste();

  // Display
  void render();
  void update();

  // State
  bool isModified() const { return modified; }
  bool isOpen() const { return fileOpen; }
  String getFilePath() const { return filePath; }
  int getCurrentLine() const { return cursorRow + 1; }
  int getTotalLines() const { return lines.size(); }

private:
  // File state
  fs::FS *fileSystem;
  String filePath;
  bool fileOpen;
  bool modified;

  // Text buffer
  std::vector<String> lines;

  // Cursor
  int cursorRow;
  int cursorCol;
  int scrollRow;
  unsigned long lastBlinkTime;
  bool cursorVisible;

  // Selection
  bool hasSelection;
  int selStartRow, selStartCol;
  int selEndRow, selEndCol;
  String clipboard;

  // Undo/Redo
  std::vector<UndoAction> undoStack;
  std::vector<UndoAction> redoStack;

  // Search
  int lastSearchRow;
  int lastSearchCol;

  // Settings
  bool showLineNumbers;

  // Battery caching
  int lastBatteryLevel;
  unsigned long lastBatteryCheck;

  // Helper methods
  void ensureCursorInBounds();
  void ensureLineExists(int row);
  void adjustScroll();
  void addUndoAction(UndoAction::Type type, int row, int col,
                     const String &text, const String &oldText = "");
  void clearRedoStack();
  void drawCursor();
  int getVisibleLines() const;
};

#endif // TEXTEDITOR_H
