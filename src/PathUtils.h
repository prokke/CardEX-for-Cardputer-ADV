#ifndef PATHUTILS_H
#define PATHUTILS_H

#include <cstdint>
#include <string>

// ==================== PATH UTILITIES ====================
//
// Deliberately free of any Arduino / M5 / FS dependency so that the security
// critical parts (name validation and containment checks) can be unit tested on
// the host - see test/native/. FileOps wraps these for the Arduino String API.
//
namespace PathUtils {

// Longest accepted file name. Must stay in sync with MAX_FILENAME_LEN in
// Config.h; FileOps.cpp static_asserts that they agree.
constexpr size_t kMaxFileNameLen = 64;

// Longest accepted absolute path. Must stay in sync with MAX_PATH_LEN.
constexpr size_t kMaxPathLen = 256;

// Collapses "//", resolves "." and "..", strips any trailing slash and
// guarantees a leading slash. ".." can never escape the root: normalize("/..")
// is "/". This is what makes the traversal check in isInside() reliable.
std::string normalize(const std::string &path);

// Appends one path component to a directory and normalizes the result.
// Note that a `name` containing slashes or ".." is resolved by normalize(),
// so callers that accept user input MUST validate it with isValidFileName()
// first - joining alone is not a security boundary.
std::string join(const std::string &dir, const std::string &name);

// Parent directory of a path. parent("/") and parent("/a") are both "/".
std::string parent(const std::string &path);

// Last component of a path. fileName("/a/b.txt") is "b.txt", fileName("/") is "".
std::string fileName(const std::string &path);

// Lower-cased extension without the dot, or "" when there is none.
// extension("/a/B.TXT") is "txt". A leading dot does not count as an extension,
// so extension("/.config") is "".
std::string extension(const std::string &path);

// True when `name` is a single, safe path component: not empty, not "." or
// "..", no slashes or other characters FAT rejects, no control characters, no
// trailing dot or space, and at most kMaxFileNameLen bytes.
bool isValidFileName(const std::string &name);

// True when `path` is `base` itself or lies underneath it. Both are normalized
// first, so this is safe to call on unnormalized user input. Used to refuse
// deleting or overwriting outside the current directory, and to refuse moving
// a directory into its own subtree.
bool isInside(const std::string &base, const std::string &path);

// Human readable byte count, e.g. "512B", "1.5KB", "3.2MB", "14.9GB".
std::string formatBytes(uint64_t bytes);

}  // namespace PathUtils

#endif  // PATHUTILS_H
