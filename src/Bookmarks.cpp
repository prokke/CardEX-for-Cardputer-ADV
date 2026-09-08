#include "Bookmarks.h"

#include "Config.h"

#include <SD.h>

namespace {

std::vector<String> gBookmarks;

int indexOfPath(const String &path) {
  for (size_t i = 0; i < gBookmarks.size(); i++) {
    if (gBookmarks[i] == path) {
      return (int)i;
    }
  }
  return -1;
}

} // namespace

void Bookmarks::load() {
  gBookmarks.clear();

  if (!SD.exists(BOOKMARKS_FILE_PATH)) {
    return;
  }

  File file = SD.open(BOOKMARKS_FILE_PATH, FILE_READ);
  if (!file) {
    return;
  }

  while (file.available() && gBookmarks.size() < kMaxBookmarks) {
    String line = file.readStringUntil('\n');
    line.replace("\r", "");
    line.trim();
    // Absolute paths only, and no duplicates - a hand-edited file should not
    // be able to produce two stars on one entry.
    if (line.startsWith("/") && indexOfPath(line) < 0) {
      gBookmarks.push_back(line);
    }
  }
  file.close();
}

bool Bookmarks::isBookmarked(const String &path) {
  return indexOfPath(path) >= 0;
}

bool Bookmarks::toggle(const String &path) {
  const int existing = indexOfPath(path);
  if (existing >= 0) {
    gBookmarks.erase(gBookmarks.begin() + existing);
    save();
    return false;
  }

  if (gBookmarks.size() >= kMaxBookmarks) {
    return false;
  }
  gBookmarks.push_back(path);
  save();
  return true;
}

void Bookmarks::remove(const String &path) {
  const int existing = indexOfPath(path);
  if (existing >= 0) {
    gBookmarks.erase(gBookmarks.begin() + existing);
    save();
  }
}

bool Bookmarks::save() {
  // Same temp-and-rename swap the editor and the settings writer use: a
  // failure part-way through must not leave a truncated list.
  const String tempPath = String(BOOKMARKS_FILE_PATH) + ".tmp";
  SD.remove(tempPath);

  File file = SD.open(tempPath, FILE_WRITE);
  if (!file) {
    return false;
  }

  bool ok = true;
  for (const String &path : gBookmarks) {
    if (file.println(path) == 0) {
      ok = false;
      break;
    }
  }

  file.flush();
  file.close();

  if (!ok) {
    SD.remove(tempPath);
    return false;
  }

  SD.remove(BOOKMARKS_FILE_PATH);
  return SD.rename(tempPath, BOOKMARKS_FILE_PATH);
}

const std::vector<String> &Bookmarks::all() { return gBookmarks; }

bool Bookmarks::isFull() { return gBookmarks.size() >= kMaxBookmarks; }
