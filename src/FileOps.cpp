#include "FileOps.h"

#include "Bookmarks.h"
#include "Settings.h"

#include <algorithm>

namespace {

inline std::string toStd(const String &s) { return std::string(s.c_str()); }
inline String toArduino(const std::string &s) { return String(s.c_str()); }

// File::name() returns only the base name - VFSFileImpl::name() is
// pathToFileName(path()) on every ESP32 core we support. Code that needs a
// usable path must call path(); using name() produced a relative string that
// the VFS layer rejects outright, which is what silently broke recursive
// delete and recursive search.
inline String entryPath(File &file) { return String(file.path()); }

} // namespace

// ==================== LIST DIRECTORY ====================
bool FileOps::listDirectory(fs::FS &fs, const String &path,
                            std::vector<FileEntry> &files, bool *truncated) {
  files.clear();
  if (truncated) {
    *truncated = false;
  }

  File dir = fs.open(path);
  if (!dir || !dir.isDirectory()) {
    return false;
  }

  File file = dir.openNextFile();
  while (file) {
    if (files.size() >= MAX_FILES_IN_LIST) {
      if (truncated) {
        *truncated = true;
      }
      file.close();
      break;
    }

    FileEntry entry;
    entry.name = String(file.name());
    entry.fullPath = entryPath(file);
    entry.isDirectory = file.isDirectory();
    entry.size = file.size();
    entry.modified = file.getLastWrite();
    entry.bookmarked = Bookmarks::isBookmarked(entry.fullPath);

    // Hidden entries used to be filtered unconditionally, with no way to see
    // them at all.
    const bool hidden = entry.name.length() > 0 && entry.name[0] == '.';
    if (entry.name.length() > 0 && (SHOW_HIDDEN_FILES || !hidden)) {
      files.push_back(entry);
    }

    file.close();
    file = dir.openNextFile();
  }

  dir.close();

  sortEntries(files);
  return true;
}

// ==================== SORT ENTRIES ====================
// Bookmarks first, then directories, then files. SORT_DESCENDING reverses only
// the comparison within the innermost group, so neither favourites nor folders
// ever sink below plain files.
void FileOps::sortEntries(std::vector<FileEntry> &files) {
  std::sort(files.begin(), files.end(),
            [](const FileEntry &a, const FileEntry &b) {
              // Bookmarks outrank everything, including the folders-first rule
              // and the reverse-order switch: the point of a favourite is that
              // it is always at the top.
              if (a.bookmarked != b.bookmarked) {
                return a.bookmarked;
              }
              if (a.isDirectory != b.isDirectory) {
                return a.isDirectory;
              }

              bool less;
              switch (SORT_MODE) {
                case SORT_SIZE:
                  if (a.size != b.size) {
                    less = a.size < b.size;
                    break;
                  }
                  less = a.name.compareTo(b.name) < 0;
                  break;
                case SORT_DATE:
                  if (a.modified != b.modified) {
                    less = a.modified < b.modified;
                    break;
                  }
                  less = a.name.compareTo(b.name) < 0;
                  break;
                default:
                  less = a.name.compareTo(b.name) < 0;
                  break;
              }
              return SORT_DESCENDING ? !less : less;
            });
}

// ==================== COPY FILE ====================
bool FileOps::copyFile(fs::FS &srcFS, const String &srcPath, fs::FS &dstFS,
                       const String &dstPath, void (*progressCallback)(int)) {
  File srcFile = srcFS.open(srcPath, FILE_READ);
  if (!srcFile) {
    return false;
  }
  if (srcFile.isDirectory()) {
    srcFile.close();
    return false; // Directories go through copyDirectory().
  }

  File dstFile = dstFS.open(dstPath, FILE_WRITE);
  if (!dstFile) {
    srcFile.close();
    return false;
  }

  // Static rather than a 4KB stack frame: copyDirectory() recurses, and this
  // function is only ever active once at a time within that recursion.
  static uint8_t buffer[4096];
  const size_t totalBytes = srcFile.size();
  size_t copiedBytes = 0;
  int lastPercent = -1;
  bool ok = true;

  while (srcFile.available()) {
    const size_t bytesRead = srcFile.read(buffer, sizeof(buffer));
    if (bytesRead == 0) {
      ok = false; // Short read before EOF: the card or the file is bad.
      break;
    }
    // The return value was previously ignored, so a full card produced a
    // truncated copy that still reported success - and moveFile then deleted
    // the original.
    if (dstFile.write(buffer, bytesRead) != bytesRead) {
      ok = false;
      break;
    }

    copiedBytes += bytesRead;
    if (progressCallback && totalBytes > 0) {
      // Only report when the number actually changes: the callback repaints
      // and pushes the whole 64KB canvas, and firing it per 4KB block made
      // copies crawl.
      const int percent = (int)((copiedBytes * 100) / totalBytes);
      if (percent != lastPercent) {
        lastPercent = percent;
        progressCallback(percent);
      }
    }
  }

  if (ok && copiedBytes != totalBytes) {
    ok = false;
  }

  dstFile.flush();
  const size_t written = dstFile.size();
  dstFile.close();
  srcFile.close();

  if (ok && written != totalBytes) {
    ok = false;
  }

  if (!ok) {
    dstFS.remove(dstPath); // Never leave a half-written file behind.
    return false;
  }
  return true;
}

// ==================== MOVE FILE ====================
bool FileOps::moveFile(fs::FS &srcFS, const String &srcPath, fs::FS &dstFS,
                       const String &dstPath, void (*progressCallback)(int)) {
  // If same filesystem, try rename first
  if (&srcFS == &dstFS) {
    if (srcFS.rename(srcPath, dstPath)) {
      if (progressCallback) progressCallback(100);
      return true;
    }
  }

  // Otherwise copy and delete. The source is only removed once copyFile has
  // verified the destination byte count.
  if (copyFile(srcFS, srcPath, dstFS, dstPath, progressCallback)) {
    return deleteFile(srcFS, srcPath);
  }
  return false;
}

// ==================== DELETE FILE ====================
bool FileOps::deleteFile(fs::FS &fs, const String &path) {
  return fs.remove(path);
}

// ==================== RENAME FILE ====================
bool FileOps::renameFile(fs::FS &fs, const String &oldPath,
                         const String &newPath) {
  return fs.rename(oldPath, newPath);
}

// ==================== CREATE DIRECTORY ====================
bool FileOps::createDirectory(fs::FS &fs, const String &path) {
  return fs.mkdir(path);
}

// ==================== DELETE DIRECTORY ====================
bool FileOps::deleteDirectory(fs::FS &fs, const String &path, int depth) {
  if (depth > MAX_DIR_DEPTH) {
    return false;
  }

  File dir = fs.open(path);
  if (!dir || !dir.isDirectory()) {
    return false;
  }

  // Collect first, then delete. Removing entries while openNextFile() is
  // walking the directory makes FAT skip siblings, so the old in-loop delete
  // could leave the directory non-empty and the final rmdir would fail.
  std::vector<String> childFiles;
  std::vector<String> childDirs;

  File child = dir.openNextFile();
  while (child) {
    const String childPath = entryPath(child);
    if (child.isDirectory()) {
      childDirs.push_back(childPath);
    } else {
      childFiles.push_back(childPath);
    }
    child.close();
    child = dir.openNextFile();
  }
  dir.close();

  for (const String &file : childFiles) {
    if (!fs.remove(file)) {
      return false;
    }
  }
  for (const String &subdir : childDirs) {
    if (!deleteDirectory(fs, subdir, depth + 1)) {
      return false;
    }
  }

  return fs.rmdir(path);
}

// ==================== COPY DIRECTORY ====================
bool FileOps::copyDirectory(fs::FS &srcFS, const String &srcPath, fs::FS &dstFS,
                            const String &dstPath, int depth) {
  if (depth > MAX_DIR_DEPTH) {
    return false;
  }

  File dir = srcFS.open(srcPath);
  if (!dir || !dir.isDirectory()) {
    return false;
  }

  if (!dstFS.exists(dstPath) && !dstFS.mkdir(dstPath)) {
    dir.close();
    return false;
  }

  bool ok = true;
  File child = dir.openNextFile();
  while (child && ok) {
    const String childPath = entryPath(child);
    const String childName = String(child.name());
    const String target = joinPath(dstPath, childName);
    const bool childIsDir = child.isDirectory();
    child.close();

    ok = childIsDir ? copyDirectory(srcFS, childPath, dstFS, target, depth + 1)
                    : copyFile(srcFS, childPath, dstFS, target);

    child = dir.openNextFile();
  }
  if (child) {
    child.close();
  }
  dir.close();

  return ok;
}

// ==================== COUNT ENTRIES ====================
int FileOps::countEntries(fs::FS &fs, const String &path) {
  File dir = fs.open(path);
  if (!dir || !dir.isDirectory()) {
    return 0;
  }

  int count = 0;
  File child = dir.openNextFile();
  while (child) {
    count++;
    child.close();
    child = dir.openNextFile();
  }
  dir.close();
  return count;
}

// ==================== FILE EXISTS ====================
bool FileOps::fileExists(fs::FS &fs, const String &path) {
  return fs.exists(path);
}

// ==================== FORMAT BYTES ====================
String FileOps::formatBytes(uint64_t bytes) {
  return toArduino(PathUtils::formatBytes(bytes));
}

// ==================== IS DIRECTORY ====================
bool FileOps::isDirectory(fs::FS &fs, const String &path) {
  File file = fs.open(path);
  if (!file)
    return false;
  bool isDir = file.isDirectory();
  file.close();
  return isDir;
}

// ==================== PATH HELPERS ====================
String FileOps::getParentPath(const String &path) {
  return toArduino(PathUtils::parent(toStd(path)));
}

String FileOps::getFileName(const String &path) {
  return toArduino(PathUtils::fileName(toStd(path)));
}

String FileOps::joinPath(const String &dir, const String &file) {
  return toArduino(PathUtils::join(toStd(dir), toStd(file)));
}

String FileOps::normalizePath(const String &path) {
  return toArduino(PathUtils::normalize(toStd(path)));
}

bool FileOps::isValidFileName(const String &name) {
  return PathUtils::isValidFileName(toStd(name));
}

bool FileOps::isPathInside(const String &base, const String &path) {
  return PathUtils::isInside(toStd(base), toStd(path));
}

// ==================== SEARCH FILES ====================
void FileOps::searchFiles(fs::FS &fs, const String &path, const String &query,
                          std::vector<FileEntry> &results) {
  results.clear();
  searchRecursive(fs, path, query, results, 0);
  // Results arrive in walk order; apply the same ordering rules as a listing
  // so bookmarks stay at the top here too.
  sortEntries(results);
}

// ==================== SEARCH RECURSIVE ====================
void FileOps::searchRecursive(fs::FS &fs, const String &path,
                              const String &query,
                              std::vector<FileEntry> &results, int depth) {
  if (results.size() >= MAX_FILES_IN_LIST || depth > MAX_DIR_DEPTH) {
    return;
  }

  File dir = fs.open(path);
  if (!dir || !dir.isDirectory()) {
    return;
  }

  String lowerQuery = query;
  lowerQuery.toLowerCase();

  File file = dir.openNextFile();
  while (file && results.size() < MAX_FILES_IN_LIST) {
    // Base name for matching, full path for opening the result later. The old
    // code used name() for both, so it recursed into a relative path that
    // never opened - search only ever scanned the current directory - and the
    // results it did return could not be opened from a subdirectory.
    const String baseName = String(file.name());
    const String childPath = entryPath(file);
    const bool childIsDir = file.isDirectory();

    const bool hidden = baseName.length() > 0 && baseName[0] == '.';
    if (baseName.length() > 0 && (SHOW_HIDDEN_FILES || !hidden)) {
      String lowerName = baseName;
      lowerName.toLowerCase();

      if (lowerName.indexOf(lowerQuery) >= 0) {
        FileEntry entry;
        entry.name = baseName;
        entry.fullPath = childPath;
        entry.isDirectory = childIsDir;
        entry.size = file.size();
        entry.modified = file.getLastWrite();
        entry.bookmarked = Bookmarks::isBookmarked(childPath);
        results.push_back(entry);
      }

      file.close();

      if (childIsDir) {
        searchRecursive(fs, childPath, query, results, depth + 1);
      }
    } else {
      file.close();
    }

    file = dir.openNextFile();
  }
  if (file) {
    file.close();
  }

  dir.close();
}
