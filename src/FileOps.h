#ifndef FILEOPS_H
#define FILEOPS_H

#include "Config.h"
#include "PathUtils.h"
#include <Arduino.h>
#include <FS.h>
#include <vector>

// Recursion guard for the directory walks. FAT nesting this deep is
// pathological, and each level costs a File handle plus a stack frame.
#define MAX_DIR_DEPTH 8

// ==================== FILE ENTRY STRUCTURE ====================
struct FileEntry {
  String name;      // Display name, e.g. "notes.txt"
  String fullPath;  // Absolute path, e.g. "/docs/notes.txt"
  bool isDirectory = false;
  size_t size = 0;
  time_t modified = 0;

  // Get formatted size string
  String getSizeStr() const {
    if (isDirectory)
      return "<DIR>";
    return String(PathUtils::formatBytes(size).c_str());
  }

  // Comparison for sorting
  bool operator<(const FileEntry &other) const {
    // Directories first
    if (isDirectory != other.isDirectory) {
      return isDirectory;
    }
    // Then alphabetically
    return name.compareTo(other.name) < 0;
  }
};

// ==================== FILE OPERATIONS CLASS ====================
class FileOps {
public:
  // List files in directory. `truncated`, when given, reports whether the
  // listing hit MAX_FILES_IN_LIST and so is incomplete.
  static bool listDirectory(fs::FS &fs, const String &path,
                            std::vector<FileEntry> &files,
                            bool *truncated = nullptr);

  // File operations. copyFile verifies every write and removes a partial
  // destination on failure, so a false return means nothing was left behind.
  static bool copyFile(fs::FS &srcFS, const String &srcPath, fs::FS &dstFS,
                       const String &dstPath, void (*progressCallback)(int) = nullptr);
  static bool moveFile(fs::FS &srcFS, const String &srcPath, fs::FS &dstFS,
                       const String &dstPath, void (*progressCallback)(int) = nullptr);
  static bool deleteFile(fs::FS &fs, const String &path);
  static bool renameFile(fs::FS &fs, const String &oldPath,
                         const String &newPath);

  // Directory operations
  static bool createDirectory(fs::FS &fs, const String &path);
  static bool deleteDirectory(fs::FS &fs, const String &path, int depth = 0);
  static bool copyDirectory(fs::FS &srcFS, const String &srcPath, fs::FS &dstFS,
                            const String &dstPath, int depth = 0);

  // Sorts in place according to SORT_MODE / SORT_DESCENDING.
  static void sortEntries(std::vector<FileEntry> &files);

  // Number of entries directly inside a directory, for delete confirmations.
  static int countEntries(fs::FS &fs, const String &path);

  // File info
  static bool fileExists(fs::FS &fs, const String &path);
  static bool isDirectory(fs::FS &fs, const String &path);
  static String formatBytes(uint64_t bytes);

  // Path utilities. Thin String wrappers over PathUtils, which is where the
  // logic lives and where it is unit tested.
  static String getParentPath(const String &path);
  static String getFileName(const String &path);
  static String joinPath(const String &dir, const String &file);
  static String normalizePath(const String &path);
  static bool isValidFileName(const String &name);
  static bool isPathInside(const String &base, const String &path);

  // Search
  static void searchFiles(fs::FS &fs, const String &path, const String &query,
                          std::vector<FileEntry> &results);

private:
  static void searchRecursive(fs::FS &fs, const String &path,
                              const String &query,
                              std::vector<FileEntry> &results, int depth);
};

#endif // FILEOPS_H
