#ifndef FILEOPS_H
#define FILEOPS_H

#include "Config.h"
#include <Arduino.h>
#include <FS.h>
#include <vector>

// ==================== FILE ENTRY STRUCTURE ====================
struct FileEntry {
  String name;
  bool isDirectory;
  size_t size;
  time_t modified;

  // Get icon based on file type
  String getIcon() const {
    if (isDirectory)
      return ">";
    if (name.endsWith(".txt"))
      return "T";
    if (name.endsWith(".json") || name.endsWith(".cfg") ||
        name.endsWith(".conf"))
      return "C";
    if (name.endsWith(".cpp") || name.endsWith(".h") || name.endsWith(".py") ||
        name.endsWith(".js"))
      return "S";
    if (name.endsWith(".bin") || name.endsWith(".hex"))
      return "B";
    if (name.endsWith(".log"))
      return "L";
    return "F";
  }

  // Get formatted size string
  String getSizeStr() const {
    if (isDirectory)
      return "<DIR>";
    if (size < 1024)
      return String(size) + "B";
    if (size < 1048576)
      return String(size / 1024) + "KB";
    return String(size / 1048576) + "MB";
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
  // List files in directory
  static bool listDirectory(fs::FS &fs, const String &path,
                            std::vector<FileEntry> &files);

  // File operations
  static bool copyFile(fs::FS &srcFS, const String &srcPath, fs::FS &dstFS,
                       const String &dstPath, void (*progressCallback)(int) = nullptr);
  static bool moveFile(fs::FS &srcFS, const String &srcPath, fs::FS &dstFS,
                       const String &dstPath, void (*progressCallback)(int) = nullptr);
  static bool deleteFile(fs::FS &fs, const String &path);
  static bool renameFile(fs::FS &fs, const String &oldPath,
                         const String &newPath);

  // Directory operations
  static bool createDirectory(fs::FS &fs, const String &path);
  static bool deleteDirectory(fs::FS &fs, const String &path);

  // File info
  static bool fileExists(fs::FS &fs, const String &path);
  static size_t getFileSize(fs::FS &fs, const String &path);
  static bool isDirectory(fs::FS &fs, const String &path);
  static String formatBytes(uint64_t bytes);

  // Path utilities
  static String getParentPath(const String &path);
  static String getFileName(const String &path);
  static String joinPath(const String &dir, const String &file);
  static String normalizePath(const String &path);

  // Search
  static void searchFiles(fs::FS &fs, const String &path, const String &query,
                          std::vector<FileEntry> &results);

private:
  static void searchRecursive(fs::FS &fs, const String &path,
                              const String &query,
                              std::vector<FileEntry> &results, int depth);
};

#endif // FILEOPS_H
