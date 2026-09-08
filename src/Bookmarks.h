#ifndef BOOKMARKS_H
#define BOOKMARKS_H

#include <Arduino.h>
#include <vector>

// ==================== BOOKMARKS ====================
// Favourite files and folders, marked with a star in the listing and always
// sorted to the top regardless of the sort mode.
//
// Stored one absolute path per line in /CardEX.bookmarks and kept in RAM, so
// the per-entry lookup that runs while listing a directory costs no I/O.
class Bookmarks {
public:
  // Most that will be held. Each is a String on a heap already carrying the
  // 64,800-byte canvas, so this is deliberately modest.
  static constexpr size_t kMaxBookmarks = 64;

  // Reads the file. Safe to call when it does not exist.
  static void load();

  static bool isBookmarked(const String &path);

  // Adds or removes `path`. Returns the new state. Writes the file
  // immediately - a bookmark that disappears on reboot is worse than none.
  static bool toggle(const String &path);

  // Drops a bookmark whose target no longer exists, after a rename or delete.
  static void remove(const String &path);

  // Rewrites the file. Returns false if it could not be written.
  static bool save();

  static const std::vector<String> &all();
  static bool isFull();
};

#endif // BOOKMARKS_H
