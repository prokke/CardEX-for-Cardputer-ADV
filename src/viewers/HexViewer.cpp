#include "HexViewer.h"

#include "../Config.h"
#include "../FileOps.h"
#include "../Input.h"
#include "../UI.h"

#include <M5Cardputer.h>

namespace {

// 8 bytes per row keeps "OOOOOO  xx xx ...  ........" inside 240px at 6px per
// character: 6 offset + 2 + 24 hex + 2 + 8 ascii = 42 columns.
constexpr int kBytesPerRow = 8;

int rowsPerPage() { return (CONTENT_HEIGHT - 2) / LINE_HEIGHT; }

char hexDigit(uint8_t value) {
  return value < 10 ? (char)('0' + value) : (char)('A' + value - 10);
}

} // namespace

void HexViewer::run(fs::FS &fs, const String &path) {
  Input::flush();

  File file = fs.open(path, FILE_READ);
  if (!file) {
    UI::showToast("Cannot open file", ERROR_COLOR);
    return;
  }

  const uint32_t fileSize = file.size();
  const int rows = rowsPerPage();
  const uint32_t bytesPerPage = (uint32_t)rows * kBytesPerRow;

  uint32_t offset = 0;
  bool running = true;
  bool needsDraw = true;

  static uint8_t page[64 * kBytesPerRow]; // Comfortably larger than one screen.

  while (running) {
    if (needsDraw) {
      needsDraw = false;

      const uint32_t want = min(bytesPerPage, (uint32_t)sizeof(page));
      file.seek(offset);
      const size_t got = file.read(page, want);

      UI::drawHeader(FileOps::getFileName(path) + " [HEX]", true,
                     M5Cardputer.Power.getBatteryLevel());

      int y = HEADER_HEIGHT + 2;
      for (int row = 0; row < rows; row++) {
        UI::canvas.fillRect(0, y, SCREEN_WIDTH, LINE_HEIGHT, BG_COLOR);

        const uint32_t rowStart = (uint32_t)row * kBytesPerRow;
        if (rowStart < got) {
          // Build the whole row as one string: a drawString per byte would be
          // 24 calls a line.
          char text[48];
          int at = 0;
          const uint32_t address = offset + rowStart;
          for (int shift = 20; shift >= 0; shift -= 4) {
            text[at++] = hexDigit((address >> shift) & 0xF);
          }
          text[at++] = ' ';
          text[at++] = ' ';

          for (int i = 0; i < kBytesPerRow; i++) {
            if (rowStart + i < got) {
              const uint8_t value = page[rowStart + i];
              text[at++] = hexDigit(value >> 4);
              text[at++] = hexDigit(value & 0xF);
            } else {
              text[at++] = ' ';
              text[at++] = ' ';
            }
            text[at++] = ' ';
          }
          text[at++] = ' ';

          for (int i = 0; i < kBytesPerRow && rowStart + i < got; i++) {
            const uint8_t value = page[rowStart + i];
            text[at++] = (value >= 0x20 && value < 0x7F) ? (char)value : '.';
          }
          text[at] = '\0';

          UI::canvas.setTextColor(TEXT_COLOR, BG_COLOR);
          UI::canvas.drawString(text, 2, y + 2);
        }

        y += LINE_HEIGHT;
      }

      const int percent =
          fileSize > 0 ? (int)((uint64_t)offset * 100 / fileSize) : 100;
      UI::drawFooter(FileOps::formatBytes(fileSize) + "  " + String(percent) +
                     "%  Esc exit");
      UI::pushCanvas();
    }

    M5Cardputer.update();
    Input::update();

    if (Input::optJustPressed()) {
      break;
    }

    for (const KeyEvent &event : Input::events()) {
      if (event.isEsc() || event.isBackspace()) {
        running = false;
        break;
      }

      // The last page starts at the last whole page boundary, so the view
      // never scrolls past the end into blank rows.
      const uint32_t maxOffset =
          fileSize > bytesPerPage
              ? ((fileSize - 1) / bytesPerPage) * bytesPerPage
              : 0;

      uint32_t next = offset;
      switch (event.key()) {
        case ';': next = offset >= kBytesPerRow ? offset - kBytesPerRow : 0; break;
        case '.': next = offset + kBytesPerRow; break;
        case ',': next = offset >= bytesPerPage ? offset - bytesPerPage : 0; break;
        case '/': next = offset + bytesPerPage; break;
        default: break;
      }
      if (event.letter() == 'g') {
        next = 0; // Start of file.
      } else if (event.letter() == 'e') {
        next = maxOffset; // End of file.
      }

      if (next > maxOffset) {
        next = maxOffset;
      }
      if (next != offset) {
        offset = next;
        needsDraw = true;
      }
    }

    delay(10);
  }

  file.close();
  Input::flush();
  UI::clearScreen();
  UI::pushCanvas();
}
