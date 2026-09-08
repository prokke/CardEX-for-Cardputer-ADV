#ifndef TEXTVIEWER_H
#define TEXTVIEWER_H

#include <Arduino.h>
#include <FS.h>

// ==================== LARGE TEXT VIEWER ====================
// Read-only pager for text files too large to edit. It indexes line offsets on
// open and then reads only the visible lines by seek(), so the 32KB
// MAX_FILE_SIZE ceiling no longer means "cannot be opened at all".
class TextViewer {
public:
  static void run(fs::FS &fs, const String &path);
};

#endif // TEXTVIEWER_H
