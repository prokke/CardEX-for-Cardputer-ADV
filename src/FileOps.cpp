#include "FileOps.h"

// ==================== LIST DIRECTORY ====================
bool FileOps::listDirectory(fs::FS &fs, const String &path,
                            std::vector<FileEntry> &files) {
  files.clear();

  File dir = fs.open(path);
  if (!dir || !dir.isDirectory()) {
    return false;
  }

  File file = dir.openNextFile();
  while (file && files.size() < MAX_FILES_IN_LIST) {
    FileEntry entry;
    entry.name = String(file.name());
    entry.isDirectory = file.isDirectory();
    entry.size = file.size();
    entry.modified = file.getLastWrite();

    // Remove path prefix, keep only filename
    int lastSlash = entry.name.lastIndexOf('/');
    if (lastSlash >= 0) {
      entry.name = entry.name.substring(lastSlash + 1);
    }

    // Skip hidden files and current directory
    if (entry.name.length() > 0 && entry.name[0] != '.') {
      files.push_back(entry);
    }

    file = dir.openNextFile();
  }

  dir.close();

  // Sort: directories first, then alphabetically
  std::sort(files.begin(), files.end());

  return true;
}

// ==================== COPY FILE ====================
bool FileOps::copyFile(fs::FS &srcFS, const String &srcPath, fs::FS &dstFS,
                       const String &dstPath, void (*progressCallback)(int)) {
  File srcFile = srcFS.open(srcPath, FILE_READ);
  if (!srcFile) {
    return false;
  }

  File dstFile = dstFS.open(dstPath, FILE_WRITE);
  if (!dstFile) {
    srcFile.close();
    return false;
  }

  // Copy in chunks (4KB buffer)
  uint8_t buffer[4096];
  size_t totalBytes = srcFile.size();
  size_t copiedBytes = 0;
  
  while (srcFile.available()) {
    size_t bytesRead = srcFile.read(buffer, sizeof(buffer));
    dstFile.write(buffer, bytesRead);
    
    copiedBytes += bytesRead;
    if (progressCallback && totalBytes > 0) {
      progressCallback((copiedBytes * 100) / totalBytes);
    }
  }

  srcFile.close();
  dstFile.close();
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

  // Otherwise copy and delete
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
bool FileOps::deleteDirectory(fs::FS &fs, const String &path) {
  File dir = fs.open(path);
  if (!dir || !dir.isDirectory()) {
    return false;
  }

  // Delete all files in directory
  File file = dir.openNextFile();
  while (file) {
    String filePath = String(file.name());
    if (file.isDirectory()) {
      deleteDirectory(fs, filePath);
    } else {
      fs.remove(filePath);
    }
    file = dir.openNextFile();
  }
  dir.close();

  return fs.rmdir(path);
}

// ==================== FILE EXISTS ====================
bool FileOps::fileExists(fs::FS &fs, const String &path) {
  return fs.exists(path);
}

// ==================== GET FILE SIZE ====================
size_t FileOps::getFileSize(fs::FS &fs, const String &path) {
  File file = fs.open(path);
  if (!file)
    return 0;
  size_t size = file.size();
  file.close();
  return size;
}

// ==================== FORMAT BYTES ====================
String FileOps::formatBytes(uint64_t bytes) {
  if (bytes < 1024)
    return String((int)bytes) + "B";
  if (bytes < 1048576)
    return String((int)(bytes / 1024)) + "KB";
  if (bytes < 1073741824)
    return String((int)(bytes / 1048576)) + "MB";
  return String((int)(bytes / 1073741824)) + "GB";
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

// ==================== GET PARENT PATH ====================
String FileOps::getParentPath(const String &path) {
  int lastSlash = path.lastIndexOf('/');
  if (lastSlash <= 0) {
    return "/";
  }
  return path.substring(0, lastSlash);
}

// ==================== GET FILENAME ====================
String FileOps::getFileName(const String &path) {
  int lastSlash = path.lastIndexOf('/');
  if (lastSlash < 0) {
    return path;
  }
  return path.substring(lastSlash + 1);
}

// ==================== JOIN PATH ====================
String FileOps::joinPath(const String &dir, const String &file) {
  String result = dir;
  if (!result.endsWith("/")) {
    result += "/";
  }
  result += file;
  return normalizePath(result);
}

// ==================== NORMALIZE PATH ====================
String FileOps::normalizePath(const String &path) {
  String result = path;

  // Ensure starts with /
  if (!result.startsWith("/")) {
    result = "/" + result;
  }

  // Remove double slashes
  while (result.indexOf("//") >= 0) {
    result.replace("//", "/");
  }

  // Remove trailing slash (except root)
  if (result.length() > 1 && result.endsWith("/")) {
    result = result.substring(0, result.length() - 1);
  }

  return result;
}

// ==================== SEARCH FILES ====================
void FileOps::searchFiles(fs::FS &fs, const String &path, const String &query,
                          std::vector<FileEntry> &results) {
  results.clear();
  searchRecursive(fs, path, query, results, 0); // Start with depth 0
}

// ==================== SEARCH RECURSIVE ====================
// Added depth limit to prevent stack overflow
void FileOps::searchRecursive(fs::FS &fs, const String &path,
                              const String &query,
                              std::vector<FileEntry> &results, int depth) {
  if (results.size() >= MAX_FILES_IN_LIST || depth > 5) { // Limit depth to 5
    return;
  }

  File dir = fs.open(path);
  if (!dir || !dir.isDirectory()) {
    return;
  }

  File file = dir.openNextFile();
  while (file && results.size() < MAX_FILES_IN_LIST) {
    String fileName = String(file.name());

    // Extract just the filename
    int lastSlash = fileName.lastIndexOf('/');
    String baseName =
        (lastSlash >= 0) ? fileName.substring(lastSlash + 1) : fileName;

    // Skip hidden files
    if (baseName.length() > 0 && baseName[0] != '.') {
      // Check if matches query (case-insensitive)
      String lowerName = baseName;
      lowerName.toLowerCase();
      String lowerQuery = query;
      lowerQuery.toLowerCase();

      if (lowerName.indexOf(lowerQuery) >= 0) {
        FileEntry entry;
        entry.name = fileName; // Full path
        entry.isDirectory = file.isDirectory();
        entry.size = file.size();
        entry.modified = file.getLastWrite();
        results.push_back(entry);
      }

      // Recurse into subdirectories
      if (file.isDirectory()) {
        searchRecursive(fs, fileName, query, results, depth + 1);
      }
    }

    file = dir.openNextFile();
  }

  dir.close();
}
