#include "PathUtils.h"

#include <cstdio>
#include <vector>

namespace PathUtils {

namespace {

// Characters FAT/exFAT reject in a name, plus the separators we use ourselves.
bool isForbiddenNameChar(char c) {
  switch (c) {
    case '/':
    case '\\':
    case ':':
    case '*':
    case '?':
    case '"':
    case '<':
    case '>':
    case '|':
      return true;
    default:
      // Control characters and DEL.
      return static_cast<unsigned char>(c) < 0x20 ||
             static_cast<unsigned char>(c) == 0x7F;
  }
}

}  // namespace

std::string normalize(const std::string &path) {
  std::vector<std::string> parts;
  std::string segment;

  // Walk the input one character at a time, emitting a segment at each
  // separator. Handling "." and ".." here - rather than with string replaces -
  // is what stops "/a/../../etc" from escaping the root.
  for (size_t i = 0; i <= path.size(); ++i) {
    if (i < path.size() && path[i] != '/') {
      segment += path[i];
      continue;
    }

    if (segment == "..") {
      if (!parts.empty()) {
        parts.pop_back();
      }
      // Popping an empty stack is a no-op: ".." can never rise above root.
    } else if (!segment.empty() && segment != ".") {
      parts.push_back(segment);
    }
    segment.clear();
  }

  std::string result;
  for (const std::string &part : parts) {
    result += '/';
    result += part;
  }
  return result.empty() ? "/" : result;
}

std::string join(const std::string &dir, const std::string &name) {
  if (name.empty()) {
    return normalize(dir);
  }
  return normalize(dir + "/" + name);
}

std::string parent(const std::string &path) {
  const std::string normalized = normalize(path);
  const size_t lastSlash = normalized.find_last_of('/');
  if (lastSlash == std::string::npos || lastSlash == 0) {
    return "/";
  }
  return normalized.substr(0, lastSlash);
}

std::string fileName(const std::string &path) {
  const std::string normalized = normalize(path);
  if (normalized == "/") {
    return "";
  }
  return normalized.substr(normalized.find_last_of('/') + 1);
}

std::string extension(const std::string &path) {
  const std::string name = fileName(path);
  const size_t dot = name.find_last_of('.');
  // npos means no dot; 0 means a dotfile such as ".config", which has no
  // extension for our purposes.
  if (dot == std::string::npos || dot == 0 || dot + 1 >= name.size()) {
    return "";
  }

  std::string ext = name.substr(dot + 1);
  for (char &c : ext) {
    if (c >= 'A' && c <= 'Z') {
      c = static_cast<char>(c - 'A' + 'a');
    }
  }
  return ext;
}

bool isValidFileName(const std::string &name) {
  if (name.empty() || name.size() > kMaxFileNameLen) {
    return false;
  }
  if (name == "." || name == "..") {
    return false;
  }
  // FAT silently strips these, which would make the created name differ from
  // what the user typed - and a later delete would then target the wrong entry.
  if (name.back() == '.' || name.back() == ' ' || name.front() == ' ') {
    return false;
  }
  for (char c : name) {
    if (isForbiddenNameChar(c)) {
      return false;
    }
  }
  return true;
}

bool isInside(const std::string &base, const std::string &path) {
  const std::string normalizedBase = normalize(base);
  const std::string normalizedPath = normalize(path);

  if (normalizedPath == normalizedBase) {
    return true;
  }
  if (normalizedBase == "/") {
    return true;  // Everything is inside the root.
  }
  // The trailing slash matters: without it "/abc" would count as inside "/ab".
  return normalizedPath.compare(0, normalizedBase.size() + 1,
                                normalizedBase + "/") == 0;
}

std::string formatBytes(uint64_t bytes) {
  static const char *const kUnits[] = {"B", "KB", "MB", "GB", "TB"};

  if (bytes < 1024) {
    return std::to_string(bytes) + "B";
  }

  // Keep one decimal so a 14.9GB card does not read "14GB". The divisor is
  // scaled first and the rounding applied once, against the original byte
  // count - dividing step by step would truncate twice (3.2MB -> "3.1MB").
  size_t unit = 0;          // index of the unit *below* the one we print
  uint64_t divisor = 1024;  // bytes per kUnits[unit + 1]
  while (bytes / divisor >= 1024 && unit + 1 < 4) {
    divisor *= 1024;
    ++unit;
  }

  uint64_t tenths = (bytes * 10 + divisor / 2) / divisor;  // rounded, not truncated
  // Rounding up can cross the unit boundary (1048575B would print "1024.0KB"),
  // so step up once more when it does.
  if (tenths >= 10240 && unit + 1 < 4) {
    ++unit;
    divisor *= 1024;
    tenths = (bytes * 10 + divisor / 2) / divisor;
  }

  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%llu.%llu%s",
                static_cast<unsigned long long>(tenths / 10),
                static_cast<unsigned long long>(tenths % 10), kUnits[unit + 1]);
  return buffer;
}

}  // namespace PathUtils
