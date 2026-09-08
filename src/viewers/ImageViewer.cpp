#include "ImageViewer.h"

#include "../Config.h"
#include "../Input.h"
#include "../PathUtils.h"
#include "../UI.h"

#include <M5Cardputer.h>

namespace {

enum ImageFormat { FMT_NONE, FMT_BMP, FMT_JPEG, FMT_PNG, FMT_QOI };

ImageFormat formatOf(const String &path) {
  const std::string ext = PathUtils::extension(std::string(path.c_str()));
  if (ext == "bmp") return FMT_BMP;
  if (ext == "jpg" || ext == "jpeg") return FMT_JPEG;
  if (ext == "png") return FMT_PNG;
  if (ext == "qoi") return FMT_QOI;
  return FMT_NONE;
}

uint16_t readBE16(File &file) {
  const uint8_t hi = file.read();
  const uint8_t lo = file.read();
  return (uint16_t)((hi << 8) | lo);
}

uint32_t readBE32(File &file) {
  uint32_t value = 0;
  for (int i = 0; i < 4; i++) {
    value = (value << 8) | (uint8_t)file.read();
  }
  return value;
}

uint32_t readLE32(File &file) {
  uint32_t value = 0;
  for (int i = 0; i < 4; i++) {
    value |= ((uint32_t)(uint8_t)file.read()) << (8 * i);
  }
  return value;
}

// Walks the JPEG marker chain to the first SOF segment, which is where the
// dimensions live. Markers carry a big-endian length except for the handful of
// standalone ones skipped below.
bool jpegSize(File &file, int &width, int &height) {
  if (readBE16(file) != 0xFFD8) {
    return false;
  }

  while (file.available()) {
    uint8_t byte = file.read();
    if (byte != 0xFF) {
      continue; // Resynchronise on the next marker prefix.
    }
    while (file.available() && (byte = file.read()) == 0xFF) {
      // Fill bytes between markers.
    }

    // Standalone markers: no length field follows.
    if (byte == 0xD8 || byte == 0x01 || (byte >= 0xD0 && byte <= 0xD7)) {
      continue;
    }
    if (byte == 0xD9) {
      return false; // End of image, no SOF found.
    }

    const uint16_t length = readBE16(file);
    if (length < 2) {
      return false;
    }

    const bool isSOF = (byte >= 0xC0 && byte <= 0xCF) && byte != 0xC4 &&
                       byte != 0xC8 && byte != 0xCC;
    if (isSOF) {
      file.read(); // Sample precision.
      height = readBE16(file);
      width = readBE16(file);
      return width > 0 && height > 0;
    }

    if (!file.seek(file.position() + length - 2)) {
      return false;
    }
  }
  return false;
}

} // namespace

bool ImageViewer::handles(const String &path) {
  return formatOf(path) != FMT_NONE;
}

bool ImageViewer::imageSize(fs::FS &fs, const String &path, int &width,
                            int &height) {
  const ImageFormat format = formatOf(path);
  if (format == FMT_NONE) {
    return false;
  }

  File file = fs.open(path, FILE_READ);
  if (!file) {
    return false;
  }

  bool ok = false;
  switch (format) {
    case FMT_BMP:
      // BITMAPINFOHEADER: signed 32-bit width at 18, height at 22. A negative
      // height means a top-down bitmap.
      if (file.size() >= 26 && file.seek(18)) {
        width = (int)(int32_t)readLE32(file);
        height = (int)(int32_t)readLE32(file);
        if (height < 0) height = -height;
        ok = width > 0 && height > 0;
      }
      break;

    case FMT_PNG:
      // IHDR is always the first chunk, so width/height sit at 16 and 20.
      if (file.size() >= 24 && file.seek(16)) {
        width = (int)readBE32(file);
        height = (int)readBE32(file);
        ok = width > 0 && height > 0;
      }
      break;

    case FMT_QOI:
      // qoi_header: magic(4), width(4 BE), height(4 BE).
      if (file.size() >= 14 && file.seek(4)) {
        width = (int)readBE32(file);
        height = (int)readBE32(file);
        ok = width > 0 && height > 0;
      }
      break;

    case FMT_JPEG:
      ok = jpegSize(file, width, height);
      break;

    default:
      break;
  }

  file.close();
  return ok;
}

void ImageViewer::run(fs::FS &fs, const String &path) {
  Input::flush();

  int imageW = 0;
  int imageH = 0;
  const bool haveSize = imageSize(fs, path, imageW, imageH);

  // Fit on open, never enlarging past 1:1 - upscaling a small icon to fill the
  // screen is not what anyone wants from a file viewer.
  float scale = 1.0f;
  if (haveSize && (imageW > SCREEN_WIDTH || imageH > SCREEN_HEIGHT)) {
    const float fitX = (float)SCREEN_WIDTH / (float)imageW;
    const float fitY = (float)SCREEN_HEIGHT / (float)imageH;
    scale = fitX < fitY ? fitX : fitY;
  }
  const float fitScale = scale;

  int panX = 0;
  int panY = 0;
  bool running = true;
  bool needsDraw = true;
  bool decodeFailed = false;

  const ImageFormat format = formatOf(path);

  while (running) {
    if (needsDraw) {
      needsDraw = false;
      UI::canvas.fillScreen(BG_COLOR);

      // Centred, then offset by the pan. datum_t::middle_center does the
      // centring for us at any scale.
      const int x = SCREEN_WIDTH / 2 + panX;
      const int y = SCREEN_HEIGHT / 2 + panY;

      bool drawn = false;
      switch (format) {
        case FMT_BMP:
          drawn = UI::canvas.drawBmpFile(fs, path.c_str(), x, y, 0, 0, 0, 0,
                                         scale, scale,
                                         datum_t::middle_center);
          break;
        case FMT_JPEG:
          drawn = UI::canvas.drawJpgFile(fs, path.c_str(), x, y, 0, 0, 0, 0,
                                         scale, scale,
                                         datum_t::middle_center);
          break;
        case FMT_PNG:
          drawn = UI::canvas.drawPngFile(fs, path.c_str(), x, y, 0, 0, 0, 0,
                                         scale, scale,
                                         datum_t::middle_center);
          break;
        case FMT_QOI:
          drawn = UI::canvas.drawQoiFile(fs, path.c_str(), x, y, 0, 0, 0, 0,
                                         scale, scale,
                                         datum_t::middle_center);
          break;
        default:
          break;
      }

      if (!drawn) {
        decodeFailed = true;
        UI::drawCenteredText("Cannot decode image", SCREEN_HEIGHT / 2 - 4,
                             ERROR_COLOR);
      }

      // Footer over the image rather than beside it: the screen is 135px tall
      // and the picture is the point.
      if (!decodeFailed) {
        String info = String((int)(scale * 100.0f)) + "%";
        if (haveSize) {
          info = String(imageW) + "x" + String(imageH) + "  " + info;
        }
        UI::canvas.fillRect(0, SCREEN_HEIGHT - 10, SCREEN_WIDTH, 10, MENU_BG);
        UI::canvas.setTextColor(SECONDARY_COLOR, MENU_BG);
        UI::canvas.drawString(info, 2, SCREEN_HEIGHT - 9);
        UI::canvas.drawString("Esc", SCREEN_WIDTH - 22, SCREEN_HEIGHT - 9);
      }

      UI::pushCanvas();
    }

    M5Cardputer.update();
    Input::update();

    if (Input::optJustPressed()) {
      break;
    }

    for (const KeyEvent &event : Input::events()) {
      if (event.isEsc() || event.isBackspace() || event.isEnter()) {
        running = false;
        break;
      }

      // Same four keys as everywhere else: pan with ';' '.' and zoom with
      // ',' '/'. Shift makes them pan horizontally.
      const int panStep = 16;
      switch (event.key()) {
        case ';': panY += panStep; needsDraw = true; break;
        case '.': panY -= panStep; needsDraw = true; break;
        case ',':
          if (event.shift) {
            panX += panStep;
          } else {
            scale *= 0.8f;
            if (scale < 0.05f) scale = 0.05f;
          }
          needsDraw = true;
          break;
        case '/':
          if (event.shift) {
            panX -= panStep;
          } else {
            scale *= 1.25f;
            if (scale > 8.0f) scale = 8.0f;
          }
          needsDraw = true;
          break;
        default:
          break;
      }

      // 'f' refits, '1' goes to 1:1.
      if (event.letter() == 'f') {
        scale = fitScale;
        panX = panY = 0;
        needsDraw = true;
      } else if (event.key() == '1') {
        scale = 1.0f;
        panX = panY = 0;
        needsDraw = true;
      }
    }

    delay(10);
  }

  Input::flush();
  UI::clearScreen();
  UI::pushCanvas();
}
