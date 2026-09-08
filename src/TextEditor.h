#ifndef TEXTEDITOR_H
#define TEXTEDITOR_H

#include "Config.h"
#include <Arduino.h>
#include <FS.h>
#include <vector>

// ==================== UNDO ACTION ====================
struct UndoAction {
  enum Type { INSERT, DELETE, REPLACE } type;
  int row = 0;
  int col = 0;
  String text;
  String oldText;
  // Actions sharing a non-zero group undo and redo as a single step. Replace
  // All touches many lines and must not need one Fn+Z per line.
  uint16_t group = 0;
};

// How the file being edited terminates its lines. Detected on open and
// reproduced on save - the editor used to read CRLF and CR happily but always
// wrote LF, silently rewriting every line ending in the file.
enum EolStyle { EOL_LF, EOL_CRLF, EOL_CR };

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
  EolStyle eolStyle;
  bool trailingNewline; // Whether the file ended with a line terminator.

  // Running character count, maintained incrementally. render() used to sum
  // every line on every frame just to print it.
  size_t totalChars;

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
  uint16_t nextUndoGroup;

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
                     const String &text, const String &oldText = "",
                     uint16_t group = 0);
  void applyUndoAction(const UndoAction &action, bool reverse);
  void recomputeTotalChars();
  void clearRedoStack();
  void drawCursor();
  int getVisibleLines() const;
};

#endif // TEXTEDITOR_H
