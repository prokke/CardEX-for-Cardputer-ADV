# Changelog

All notable changes to CardEX are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[semantic versioning](https://semver.org/).

## [2.0.0] - 2026-09-09

The release that makes CardEX actually work on the Cardputer ADV.

### Breaking

- **M5Cardputer 1.2.0 or newer is now required.** In 1.2.0 the library leaves
  `KeysState::word` empty whenever `Fn` is held, so every `Fn`+letter shortcut in v1.0.0 was
  undetectable: save, quit, find, replace, undo, redo, new, delete, rename, copy, cut, paste,
  properties, search, and the USB mass storage toggle. `del` also moved to `Fn+Backspace`, so
  plain Backspace did nothing. Input is now read from the raw key matrix instead.
- **Typing `; . , /` changed.** These stay bare navigation keys; the characters are typed with
  `Fn+;` `Fn+.` `Fn+,` `Fn+/`. The old `Ctrl` workaround is gone — it could not work anyway,
  because `getKey()` treats `Ctrl` as `Shift`, so `Ctrl+;` produced `:`.
- The sketch entry point moved to `src/main.cpp`; `CardEX.ino` is now only a marker for the
  Arduino IDE.

### Added

- **Viewers.** Files open by type: BMP/JPEG/PNG/QOI in an image viewer with fit, zoom and pan;
  binaries in a hex viewer; text over 32KB in a read-only pager. None of the three has a size
  limit.
- **Settings screen** (`Fn+O`) covering appearance, battery thresholds, file manager behaviour,
  editor, keyboard repeat, sound and the status LED. Colours are chosen from a palette.
- **Multi-select** with `Space`, `Fn+A` for all; copy, cut and delete act on the selection.
- **Bookmarks** (`Fn+B`), marked with a star and always sorted to the top of the list whatever
  the sort mode. Stored in `/CardEX.bookmarks`.
- **Status LED** on G21: a colour per state (idle, clipboard, unsaved edits, USB) and a flash on
  success or failure. All colours, brightness and on/off are configurable.
- **Audio feedback** through the ADV's ES8311 codec, with an optional key click.
- Sorting by name, size or date, ascending or descending, and an option to show hidden files.
- `Esc` cancels dialogs, `Tab` indents, `Fn+Backspace` deletes forward.
- PlatformIO build, GitHub Actions CI, and host-side unit tests for the path helpers.

### Fixed

- **Copying could destroy a file.** `copyFile` ignored the result of `write()`, so on a full card
  the copy was truncated but still reported success — and a move then deleted the original.
- **Saving could destroy a file.** The editor opened the target with `FILE_WRITE`, which
  truncates, then wrote without checking anything and reported "File saved" regardless. Saves are
  now written to a temp file, verified, and swapped in.
- **Deleting a non-empty folder never worked.** `File::name()` returns only the base name, but
  the recursive walk used it as a path, so every child delete failed and the final `rmdir` failed
  with it.
- **Search was not recursive** and its results could not be opened from a subdirectory — the same
  `name()` versus `path()` confusion.
- **USB mass storage silently corrupted data.** Both callbacks ignored `offset` and dropped
  `bufsize % sectorSize`, yet reported the full transfer as written. The card is also remounted
  on exit, since the host rewrites the FAT underneath the mounted filesystem.
- Undo could crash: rows were indexed without bounds checks, and Replace All rewrote lines
  without recording anything, leaving stale entries on the stack.
- Characters were duplicated when two keys overlapped while typing.
- Autorepeat ran away at full speed after any dialog.
- Line endings were rewritten: CRLF and CR files were parsed but always saved as LF, and a
  trailing newline was always dropped.
- Binary files opened in the editor as garbage, and saving wrote that garbage back.
- Toasts were invisible in the editor, so "File saved" never appeared.
- Renaming silently overwrote an existing entry.
- Folders could not be copied or moved at all.
- Input dialogs could not be cancelled.
- Selection indices ran past the end of empty lists (`size() - 1` on an unsigned zero).
- Four `KEY_*` macros in `Config.h` collided with the library's own, leaving their values
  dependent on include order.

### Changed

- Repainting happens only when something changes, instead of pushing the 64,800-byte sprite every
  10ms — which on the ADV also overflowed the keyboard controller's 10-event FIFO and dropped
  keystrokes. Free space is cached instead of walking the FAT every frame.
- Opening a file reads in 1KB blocks rather than a byte at a time.
- Settings are validated and clamped on load, and `/CardEX.ini` is rewritten preserving comments
  and unknown keys.
- `TAB_SIZE`, `AUTO_SAVE_INTERVAL`, `KEY_REPEAT_DELAY` and `KEY_REPEAT_RATE` finally do
  something; they were documented in the config file but never read.
- The battery percentage changes colour at configurable thresholds and while charging.
- No SD card offers a retry screen instead of hanging forever.

## [1.0.0] - 2026-01-26

Initial release.
