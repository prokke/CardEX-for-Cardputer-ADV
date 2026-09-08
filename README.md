# CardEX — File Manager & Text Editor for M5Stack Cardputer ADV

CardEX is a native file manager and text editor for the M5Stack Cardputer ADV. It gives you a
desktop-like way to manage the SD card, plus an editor, image and hex viewers, and USB mass
storage — all from the device itself.

---

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Cardputer%20ADV-orange.svg)
![Library](https://img.shields.io/badge/M5Cardputer-%E2%89%A5%201.2.0-informational.svg)

## ⚠️ Library version

**M5Cardputer 1.2.0 or newer is required.** The keyboard API changed incompatibly in 1.2.0:
`KeysState::word` is left empty while `Fn` is held, and `del` moved from plain Backspace to
`Fn+Backspace`. CardEX reads the raw key matrix (`Keyboard::keyList()`) instead, which works on
every version, but the key map it relies on comes from the library — so the dependency is pinned
in `platformio.ini` and should not be floated.

If you build with an older M5Cardputer, expect the `Fn` shortcuts to misbehave.

## 🚀 Features

### File management
- **Browse, create, rename, delete** files and folders on the SD card.
- **Multi-select** with `Space` — copy, cut and delete act on everything ticked.
- **Bookmarks** (`Fn+B`) — favourites are marked with a `*` and always sort to the top of the
  list, whatever the sort mode.
- **Copy, cut and paste**, including whole folders.
- **Recursive search** by name, with results you can open directly.
- **Sorting** by name, size or date, ascending or descending; optional hidden files.
- **USB mass storage** (`Fn+M`) — mount the card on a PC. The card is remounted on exit.
- **Properties** — name, size, type, location.

### Viewers
Files open according to what they are:

| Type | Opens in |
|------|----------|
| `.bmp` `.jpg` `.png` `.qoi` | Image viewer — fit to screen, zoom and pan |
| Text under 32KB | Editor |
| Text over 32KB | Read-only pager (no size limit) |
| Anything else | Hex viewer (no size limit) |

### Text editor
- Files up to 32KB, with find, replace and multi-level undo/redo.
- Atomic saves: writes to a temp file and swaps it in, so a failure cannot destroy the original.
- Preserves the file's original line endings (LF, CRLF or CR) and trailing newline.
- Auto-indent, configurable tab width, optional autosave, line numbers.

### Status LED
The RGB LED reports what the app is doing: idle, something on the clipboard, unsaved edits in the
editor, USB mass storage active, plus a brief flash on success or failure. Every colour, the
brightness and the on/off switch are settings.

### Settings
Everything is editable on the device (`Fn+O`) and stored in `/CardEX.ini` — colours, brightness,
battery thresholds, sort order, editor behaviour, key repeat, sounds and the LED. Values are
range-checked on load, so a hand-edited file cannot leave the app unusable. Colours are picked
from a preset palette rather than typed as numbers.

## 🎮 Controls

Navigation stays on the bare `;` `.` `,` `/` keys. The `Fn` layer of those four keys types the
characters instead.

### Global
| Key | Action |
|-----|--------|
| `Opt` | Full-screen help |
| `Esc` (`Fn`+`` ` ``) | Cancel / go back |

### File manager
| Key | Action |
|-----|--------|
| `;` / `.` | Move selection up / down |
| `,` / `/` | Parent folder / open |
| `Enter` | Open file or folder |
| `Backspace` | Parent folder |
| `Space` | Tick / untick the entry |
| `Fn+A` | Select all / clear selection |
| `Fn+B` | Bookmark the entry |
| `Fn+N` | New file or folder |
| `Fn+C` / `Fn+X` / `Fn+V` | Copy / cut / paste |
| `Fn+D` / `Fn+R` | Delete / rename |
| `Fn+F` | Search (recursive) |
| `Fn+P` | Properties |
| `Fn+O` | Settings |
| `Fn+M` | USB mass storage |
| `Fn+H` | Help |

### Text editor
| Key | Action |
|-----|--------|
| `;` `.` `,` `/` | Move cursor |
| `Fn+;` `Fn+.` `Fn+,` `Fn+/` | Type `;` `.` `,` `/` |
| `Backspace` / `Fn+Backspace` | Delete back / forward |
| `Tab` | Indent |
| `Enter` | New line |
| `Fn+S` / `Fn+Q` | Save / quit |
| `Fn+F` / `Fn+R` | Find / replace |
| `Fn+Z` / `Fn+Y` | Undo / redo |

### Viewers
| Key | Action |
|-----|--------|
| `;` / `.` | Scroll |
| `,` / `/` | Page, or zoom in the image viewer |
| `Shift`+`,` / `/` | Pan horizontally |
| `F` / `1` | Fit / 1:1 (image viewer) |
| `G` / `E` | Start / end of file |
| `Esc` | Close |

### Settings
`;` `.` move, `,` `/` change the value, `R` resets the current category, `Esc` saves and exits.

## 📁 Files on the card

| Path | Purpose |
|------|---------|
| `/CardEX.ini` | Settings. Delete it to return to defaults. |
| `/CardEX.bookmarks` | Bookmarked paths, one per line. |

## 🛠 Build & flash

### PlatformIO (recommended)

```bash
pio run -e cardputer-adv -t upload   # build and flash
pio device monitor                   # serial log
pio test -e native                   # host-side unit tests
```

Everything — board, partition layout, USB mode and the pinned library version — is in
`platformio.ini`. Note that the Cardputer ADV board definition hardcodes `ARDUINO_USB_MODE=1`,
which builds out the TinyUSB stack that mass storage needs; `platformio.ini` overrides it back
to `0`.

### Arduino IDE

1. Install **M5Cardputer 1.2.0 or newer** via the Library Manager.
2. Open `CardEX.ino` — it is only a marker file; the code lives in `src/`, which the IDE
   compiles recursively.
3. Board: `M5Stack StampS3` or `ESP32S3 Dev Module`, with:
   - USB CDC On Boot: **Enabled**
   - USB Mode: **USB-OTG (TinyUSB)** — required for USB mass storage
   - Flash Size: **8MB**
   - Partition Scheme: **8M with spiffs (3MB APP/1.5MB SPIFFS)**

## ⚙️ Configuration

Settings live in `/CardEX.ini` on the SD card and are best changed from `Fn+O` in the app. The
file is written back with your comments and any unknown keys preserved, so it is safe to edit by
hand too:

```ini
// Appearance
BG_COLOR = 0x3186
ACCENT_COLOR = 0x07E0
SCREEN_BRIGHTNESS = 128

// Battery
BAT_WARN_LEVEL = 50   // percentage turns amber below this
BAT_CRIT_LEVEL = 25   // and red below this

// Editor
TAB_SIZE = 4
AUTO_SAVE_INTERVAL = 0  // milliseconds; 0 disables
```

If a setting makes the interface unreadable, delete `/CardEX.ini` — the defaults are rewritten on
the next boot.

## 📟 Hardware notes (Cardputer ADV)

| | |
|---|---|
| MCU | Stamp-S3A / ESP32-S3FN8, 8MB flash |
| Display | 1.14" 240×135 ST7789V2 |
| Keyboard | TCA8418 I²C matrix controller (INT on G11) |
| microSD | SPI — CS G12, MOSI G14, CLK G40, MISO G39 |
| Audio | ES8311 codec + NS4150B amplifier |
| RGB LED | WS2812 on G21 |
| Battery | 1750mAh |
| RTC | none — file timestamps are not meaningful until time is set |

## 📄 License

MIT — see [LICENSE](LICENSE).
