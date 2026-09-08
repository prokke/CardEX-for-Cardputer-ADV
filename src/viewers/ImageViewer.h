#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <Arduino.h>
#include <FS.h>

// ==================== IMAGE VIEWER ====================
// Displays BMP, JPEG, PNG and QOI using M5GFX's built-in decoders - the
// library is already a dependency, so this costs almost nothing in flash.
//
// Opens fitted to the 240x135 screen. Fitting needs the pixel dimensions,
// which the decoder does not expose, so imageSize() reads them out of the
// file header itself.
class ImageViewer {
public:
  // True if the extension is one of the supported formats.
  static bool handles(const String &path);

  // Reads width and height from the file header. Returns false when the file
  // is not a recognised image.
  static bool imageSize(fs::FS &fs, const String &path, int &width,
                        int &height);

  // Blocks until the user leaves.
  static void run(fs::FS &fs, const String &path);
};

#endif // IMAGEVIEWER_H
