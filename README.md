# CardEX - File Manager & Text Editor for M5Stack Cardputer

CardEX is a powerful, native file manager and text editor designed specifically for the M5Stack Cardputer. It provides a desktop-like experience for managing files on the SD card, along with a full-featured text editor for on-the-go coding and note-taking.

---

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-M5Stack-orange.svg)
![Status](https://img.shields.io/badge/status-stable-green.svg)

## 🚀 Features

### File Management
- **SD Card Support:** Full management of files on the SD card.
- **Full Operations:** Copy, Move, Rename, Delete, and Create new files/folders.
- **Search:** Recursive file search to quickly find what you need.
- **Mass Storage Mode:** Connect to a PC via USB to manage SD card files directly (`Fn+M`).
- **Properties:** View detailed file information (Size, Path).

### Text Editor
- **File Support:** Edit files up to 32KB.
- **Search & Replace:** Full find and replace functionality.
- **Undo/Redo:** Multiple undo steps supported.
- **Line Numbers:** Toggleable line numbering.

### User Interface
- **Visual Feedback:** Toast notifications for actions and errors.
- **Customization:** Customizable color theme.
- **Context Help:** Built-in shortcut reference (`Opt` key).

## 🎮 Controls

CardEX optimizes the Cardputer's keyboard for navigation. Note that the physical arrow keys (`;`, `.`, `,`, `/`) are used for navigation by default.

---

### Global Shortcuts
| Key | Action |
|-----|--------|
| `Opt` | Show Fullscreen Help |
| `Fn + M` | **USB Mass Storage Mode** |

### File Manager
| Key | Action |
|-----|--------|
| `;` (Up) | Move Selection Up |
| `.` (Down) | Move Selection Down |
| `Enter` | Open File / Enter Folder |
| `Del` / `Backspace` | Go Back / Parent Folder |
| `Fn + N` | New File / Folder Dialog |
| `Fn + C` | Copy Selected |
| `Fn + X` | Cut (Move) Selected |
| `Fn + V` | Paste |
| `Fn + D` | Delete Selected |
| `Fn + R` | Rename Selected |
| `Fn + F` | Search Files |
| `Fn + P` | View Properties |

### Text Editor
| Key | Action |
|-----|--------|
| `;` / `.` | Move Cursor Up / Down |
| `,` / `/` | Move Cursor Left / Right |
| `Ctrl` + `key` | Type `;` `.` `,` `/` characters |
| `Fn + S` | Save File |
| `Fn + Q` | Quit (Prompts save) |
| `Fn + F` | Find Text |
| `Fn + R` | Replace Text |
| `Fn + Z` | Undo |
| `Fn + Y` | Redo |
| `Enter` | New Line |

## 🛠 Build & Flash

1. **Required Libraries:**
   - M5Cardputer (install via Library Manager)

2. **Setup:**
   - Open `CardEX.ino` in Arduino IDE.
   - Select Board: `M5Stack Cardputer` or `ESP32S3 Dev Module`.
   - Settings:
     - USB CDC On Boot: **Enabled**
     - Flash Size: **8MB**
     - Partition Scheme: **8MB (3MB APP/1.5MB SPIFFS)**


## ⚙️ Configuration

You can customize colors and limits in `src/Config.h`:

```cpp
#define ACCENT_COLOR    0x07E0  // Main accent color (Green)
#define BG_COLOR        0x3186  // Background color
#define MAX_FILE_SIZE   32768   // Max edit size (32KB)
```
