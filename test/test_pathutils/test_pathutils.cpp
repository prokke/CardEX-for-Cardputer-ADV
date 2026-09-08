// Host-side unit tests for src/PathUtils.cpp.
//
// These cover the traversal and validation rules that guard every destructive
// file operation, so they are the one part of CardEX that can be verified
// without the hardware. Run with: pio test -e native

#include <unity.h>

#include "PathUtils.h"

using PathUtils::extension;
using PathUtils::fileName;
using PathUtils::formatBytes;
using PathUtils::isInside;
using PathUtils::isValidFileName;
using PathUtils::join;
using PathUtils::normalize;
using PathUtils::parent;

static void assertEqualString(const char *expected, const std::string &actual) {
  TEST_ASSERT_EQUAL_STRING(expected, actual.c_str());
}

// ==================== normalize ====================

void test_normalize_basics(void) {
  assertEqualString("/", normalize(""));
  assertEqualString("/", normalize("/"));
  assertEqualString("/a", normalize("/a"));
  assertEqualString("/a", normalize("a"));       // relative becomes absolute
  assertEqualString("/a", normalize("/a/"));     // trailing slash stripped
  assertEqualString("/a/b", normalize("/a//b")); // duplicate separators
  assertEqualString("/a/b", normalize("///a///b///"));
}

void test_normalize_resolves_dot_segments(void) {
  assertEqualString("/a", normalize("/a/."));
  assertEqualString("/a/b", normalize("/a/./b"));
  assertEqualString("/", normalize("/a/.."));
  assertEqualString("/b", normalize("/a/../b"));
  assertEqualString("/a/c", normalize("/a/b/../c"));
}

// The whole point of resolving ".." ourselves: it must saturate at the root
// rather than walking above it.
void test_normalize_cannot_escape_root(void) {
  assertEqualString("/", normalize("/.."));
  assertEqualString("/", normalize("/../.."));
  assertEqualString("/", normalize("/a/../../.."));
  assertEqualString("/etc", normalize("/a/../../../etc"));
  assertEqualString("/CardEX.ini", normalize("/docs/../../CardEX.ini"));
}

// ==================== join ====================

void test_join(void) {
  assertEqualString("/a", join("/", "a"));
  assertEqualString("/a/b", join("/a", "b"));
  assertEqualString("/a/b", join("/a/", "b"));
  assertEqualString("/a", join("/a", ""));
  // join() resolves traversal rather than rejecting it - callers must validate
  // the component first. This documents that contract.
  assertEqualString("/", join("/a", ".."));
  assertEqualString("/evil", join("/a", "../evil"));
}

// ==================== parent / fileName / extension ====================

void test_parent(void) {
  assertEqualString("/", parent("/"));
  assertEqualString("/", parent("/a"));
  assertEqualString("/a", parent("/a/b"));
  assertEqualString("/a/b", parent("/a/b/c.txt"));
  assertEqualString("/a", parent("/a/b/"));
}

void test_filename(void) {
  assertEqualString("", fileName("/"));
  assertEqualString("a", fileName("/a"));
  assertEqualString("c.txt", fileName("/a/b/c.txt"));
  assertEqualString("b", fileName("/a/b/"));
}

void test_extension(void) {
  assertEqualString("txt", extension("/a/b.txt"));
  assertEqualString("txt", extension("/a/B.TXT")); // lower-cased
  assertEqualString("gz", extension("/a/b.tar.gz"));
  assertEqualString("", extension("/a/b"));
  assertEqualString("", extension("/a/.config")); // dotfile, not an extension
  assertEqualString("", extension("/a/b."));      // trailing dot, nothing after
  assertEqualString("", extension("/"));
}

// ==================== isValidFileName ====================

void test_valid_names_accepted(void) {
  TEST_ASSERT_TRUE(isValidFileName("a"));
  TEST_ASSERT_TRUE(isValidFileName("notes.txt"));
  TEST_ASSERT_TRUE(isValidFileName("my file (1).tar.gz"));
  TEST_ASSERT_TRUE(isValidFileName(".config")); // leading dot is fine
  TEST_ASSERT_TRUE(isValidFileName(std::string(64, 'x')));
}

void test_traversal_names_rejected(void) {
  TEST_ASSERT_FALSE(isValidFileName(""));
  TEST_ASSERT_FALSE(isValidFileName("."));
  TEST_ASSERT_FALSE(isValidFileName(".."));
  TEST_ASSERT_FALSE(isValidFileName("../evil.txt"));
  TEST_ASSERT_FALSE(isValidFileName("../../CardEX.ini"));
  TEST_ASSERT_FALSE(isValidFileName("a/b"));
  TEST_ASSERT_FALSE(isValidFileName("a\\b"));
}

void test_invalid_chars_rejected(void) {
  TEST_ASSERT_FALSE(isValidFileName("a:b"));
  TEST_ASSERT_FALSE(isValidFileName("a*b"));
  TEST_ASSERT_FALSE(isValidFileName("a?b"));
  TEST_ASSERT_FALSE(isValidFileName("a\"b"));
  TEST_ASSERT_FALSE(isValidFileName("a<b"));
  TEST_ASSERT_FALSE(isValidFileName("a>b"));
  TEST_ASSERT_FALSE(isValidFileName("a|b"));
  TEST_ASSERT_FALSE(isValidFileName(std::string("a\x01") + "b"));
  TEST_ASSERT_FALSE(isValidFileName("a\nb"));
}

void test_fat_edge_names_rejected(void) {
  TEST_ASSERT_FALSE(isValidFileName("name."));  // FAT strips trailing dot
  TEST_ASSERT_FALSE(isValidFileName("name "));  // and trailing space
  TEST_ASSERT_FALSE(isValidFileName(" name"));
  TEST_ASSERT_FALSE(isValidFileName(std::string(65, 'x'))); // too long
}

// ==================== isInside ====================

void test_is_inside_accepts_contained_paths(void) {
  TEST_ASSERT_TRUE(isInside("/", "/anything"));
  TEST_ASSERT_TRUE(isInside("/a", "/a"));       // the base itself
  TEST_ASSERT_TRUE(isInside("/a", "/a/b"));
  TEST_ASSERT_TRUE(isInside("/a", "/a/b/c.txt"));
  TEST_ASSERT_TRUE(isInside("/a/", "/a/b"));    // unnormalized base
}

void test_is_inside_rejects_escapes(void) {
  TEST_ASSERT_FALSE(isInside("/a", "/b"));
  TEST_ASSERT_FALSE(isInside("/a", "/"));
  TEST_ASSERT_FALSE(isInside("/a", "/a/../b"));
  TEST_ASSERT_FALSE(isInside("/a/b", "/a"));
}

// A prefix match without the separator would wrongly accept "/abc" as inside
// "/ab". This is the classic off-by-one in containment checks.
void test_is_inside_requires_separator(void) {
  TEST_ASSERT_FALSE(isInside("/ab", "/abc"));
  TEST_ASSERT_FALSE(isInside("/ab", "/abc/d"));
  TEST_ASSERT_TRUE(isInside("/ab", "/ab/c"));
}

// ==================== formatBytes ====================

void test_format_bytes(void) {
  assertEqualString("0B", formatBytes(0));
  assertEqualString("512B", formatBytes(512));
  assertEqualString("1023B", formatBytes(1023));
  assertEqualString("1.0KB", formatBytes(1024));
  assertEqualString("1.5KB", formatBytes(1536));
  // 3355443B is 3.19999MB - truncating instead of rounding reports "3.1MB".
  assertEqualString("3.2MB", formatBytes(3355443));
  assertEqualString("14.9GB", formatBytes(16000000000ULL));
  assertEqualString("1.8TB", formatBytes(2000000000000ULL));
}

// Rounding up must not leave the value in the unit below, i.e. never "1024.0KB".
void test_format_bytes_unit_boundary(void) {
  assertEqualString("1.0MB", formatBytes(1048575));   // rounds up across KB->MB
  assertEqualString("1.0MB", formatBytes(1048576));
  assertEqualString("1.0GB", formatBytes(1073741823));
  assertEqualString("1023.4KB", formatBytes(1048000)); // just below the boundary
}

// ==================== runner ====================

int main(int, char **) {
  UNITY_BEGIN();

  RUN_TEST(test_normalize_basics);
  RUN_TEST(test_normalize_resolves_dot_segments);
  RUN_TEST(test_normalize_cannot_escape_root);
  RUN_TEST(test_join);
  RUN_TEST(test_parent);
  RUN_TEST(test_filename);
  RUN_TEST(test_extension);
  RUN_TEST(test_valid_names_accepted);
  RUN_TEST(test_traversal_names_rejected);
  RUN_TEST(test_invalid_chars_rejected);
  RUN_TEST(test_fat_edge_names_rejected);
  RUN_TEST(test_is_inside_accepts_contained_paths);
  RUN_TEST(test_is_inside_rejects_escapes);
  RUN_TEST(test_is_inside_requires_separator);
  RUN_TEST(test_format_bytes);
  RUN_TEST(test_format_bytes_unit_boundary);

  return UNITY_END();
}

void setUp(void) {}
void tearDown(void) {}
