#include "TextViewer.h"

#include "../Config.h"
#include "../FileOps.h"
#include "../Input.h"
#include "../UI.h"

#include <M5Cardputer.h>
#include <vector>

namespace {

// Line starts are held as 4-byte offsets. 20,000 of them is 80KB, far more
// than the heap has once the 64,800-byte canvas is allocated, so the index is
// capped and the viewer reports that it stopped early rather than crashing.
constexpr size_t kMaxIndexedLines = 8000;

int rowsPerPage() { return (CONTENT_HEIGHT - 2) / LINE_HEIGHT; }

// Builds the offset of every line start by streaming the file once.
bool buildIndex(File &file, std::vector<uint32_t> &offsets, bool &truncated) {
  offsets.clear();
  truncated = false;

  if (!file.seek(0)) {
    return false;
  }
  offsets.push_back(0);

  static uint8_t buffer[1024];
  uint32_t position = 0;
  bool pendingCR = false;

  while (file.available()) {
    const size_t count = file.read(buffer, sizeof(buffer));
    if (count == 0) {
      break;
    }

    for (size_t i = 0; i < count; i++) {
      const uint8_t c = buffer[i];
      position++;

      if (pendingCR && c != '\n') {
        // A lone CR ended the previous line; this byte starts the next one.
        offsets.push_back(position - 1);
        pendingCR = false;
      }

      if (c == '\r') {
        pendingCR = true;
      } else if (c == '\n') {
        pendingCR = false;
        offsets.push_back(position);
      }
    }

    if (offsets.size() >= kMaxIndexedLines) {
      truncated = true;
      break;
    }
  }

  // A trailing terminator leaves an empty final entry; drop it.
  if (offsets.size() > 1 && offsets.back() >= file.size()) {
    offsets.pop_back();
  }
  return true;
}

// Reads one line starting at `start`, stopping at the terminator or at `limit`
// characters.
String readLine(File &file, uint32_t start, int limit) {
  if (!file.seek(start)) {
    return String();
  }

  String line;
  line.reserve(limit + 1);

  while (file.available() && (int)line.length() < limit) {
    const char c = (char)file.read();
    if (c == '\n' || c == '\r') {
      break;
    }
    line += (c == '\t') ? ' ' : c;
  }
  return line;
}

} // namespace

void TextViewer::run(fs::FS &fs, const String &path) {
  Input::flush();

  File file = fs.open(path, FILE_READ);
  if (!file) {
    UI::showToast("Cannot open file", ERROR_COLOR);
    return;
  }

  UI::clearScreen();
  UI::drawCenteredText("Indexing...", SCREEN_HEIGHT / 2 - 4, SECONDARY_COLOR);
  UI::pushCanvas();

  std::vector<uint32_t> offsets;
  bool truncated = false;
  if (!buildIndex(file, offsets, truncated) || offsets.empty()) {
    file.close();
    UI::showToast("Cannot read file", ERROR_COLOR);
    return;
  }

  const uint32_t fileSize = file.size();
  const int rows = rowsPerPage();
  const int maxChars = (SCREEN_WIDTH - 4) / CHAR_WIDTH;
  const int totalLines = (int)offsets.size();

  int top = 0;
  int column = 0; // Horizontal scroll, in characters.
  bool running = true;
  bool needsDraw = true;

  while (running) {
    if (needsDraw) {
      needsDraw = false;

      String title = FileOps::getFileName(path);
      if (truncated) {
        title += " [part]";
      }
      UI::drawHeader(title, true, M5Cardputer.Power.getBatteryLevel());

      int y = HEADER_HEIGHT + 2;
      for (int row = 0; row < rows; row++) {
        UI::canvas.fillRect(0, y, SCREEN_WIDTH, LINE_HEIGHT, BG_COLOR);

        const int lineIndex = top + row;
        if (lineIndex < totalLines) {
          // Read past the horizontal scroll so panning right still shows text.
          String text = readLine(file, offsets[lineIndex], column + maxChars);
          if ((int)text.length() > column) {
            text = text.substring(column);
          } else {
            text = "";
          }
          UI::canvas.setTextColor(TEXT_COLOR, BG_COLOR);
          UI::canvas.drawString(text, 2, y + 2);
        }
        y += LINE_HEIGHT;
      }

      const int percent =
          totalLines > 1 ? (top * 100) / max(1, totalLines - 1) : 100;
      UI::drawFooter("Ln " + String(top + 1) + "/" + String(totalLines) + "  " +
                     FileOps::formatBytes(fileSize) + "  " + String(percent) +
                     "%");
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

      int nextTop = top;
      int nextColumn = column;

      switch (event.key()) {
        case ';': nextTop = top - 1; break;
        case '.': nextTop = top + 1; break;
        case ',':
          // Shift pans; unshifted pages, which is what a reader wants most.
          if (event.shift) {
            nextColumn = column - 8;
          } else {
            nextTop = top - rows;
          }
          break;
        case '/':
          if (event.shift) {
            nextColumn = column + 8;
          } else {
            nextTop = top + rows;
          }
          break;
        default: break;
      }
      if (event.letter() == 'g') {
        nextTop = 0;
      } else if (event.letter() == 'e') {
        nextTop = totalLines - rows;
      }

      nextTop = constrain(nextTop, 0, max(0, totalLines - 1));
      nextColumn = max(0, nextColumn);
      if (nextTop != top || nextColumn != column) {
        top = nextTop;
        column = nextColumn;
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
