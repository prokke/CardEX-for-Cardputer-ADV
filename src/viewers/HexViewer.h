#ifndef HEXVIEWER_H
#define HEXVIEWER_H

#include <Arduino.h>
#include <FS.h>

// ==================== HEX VIEWER ====================
// Paged hex dump, read straight from the card with seek(). Nothing is held in
// RAM beyond the visible page, so it works on files of any size - unlike the
// editor, which is capped at MAX_FILE_SIZE.
//
// This is where anything that is not text now goes. Binary files used to open
// in the editor, which filled the buffer with garbage and wrote that garbage
// back on save.
class HexViewer {
public:
  static void run(fs::FS &fs, const String &path);
};

#endif // HEXVIEWER_H
