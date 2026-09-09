/*
 * CardEX - File Manager with Text Editor
 * For M5Stack Cardputer ADV
 *
 * Features:
 * - Browse SD card
 * - Full-featured text editor
 * - File operations (copy, move, delete, rename)
 * - Search functionality
 * - Context-sensitive help (Opt)
 *
 * Author: -Prokke
 * Version: see CARDEX_VERSION in src/Config.h
 */

#include "Config.h"
#include "Bookmarks.h"
#include "Feedback.h"
#include "FileManager.h"
#include "Input.h"
#include "Settings.h"
#include "StatusLed.h"
#include "UI.h"
#include <M5Cardputer.h>

// Global file manager instance
FileManager fileManager;

// ==================== SETUP ====================
void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  delay(100);

  Serial.println("==============");
  Serial.println("CardEX v" CARDEX_VERSION);
  Serial.println("==============");

  // Initialize M5Cardputer
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true); // Enable keyboard
  Input::begin();
  Feedback::begin();
  StatusLed::begin();

  // Set display rotation
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(128);

  Serial.println("M5Cardputer initialized");

  // Bring up the card and the configuration before anything is drawn. The
  // splash used to render in the default palette because loadConfig() ran
  // later, inside FileManager::init().
  const bool sdReady = FileManager::initStorage();
  if (sdReady) {
    Settings::load();
    Bookmarks::load();
  }

  // Initialize UI
  UI::init();
  UI::clearScreen();

  // Show splash screen
  M5Cardputer.Display.setTextColor(ACCENT_COLOR, BG_COLOR);
  UI::drawCenteredText("FILE MANAGER", 40, ACCENT_COLOR);
  M5Cardputer.Display.setTextColor(TEXT_COLOR, BG_COLOR);
  UI::drawCenteredText("v" CARDEX_VERSION, 55, TEXT_COLOR);
  M5Cardputer.Display.setTextColor(SECONDARY_COLOR, BG_COLOR);
  UI::drawCenteredText("Initializing...", 75, SECONDARY_COLOR);

  delay(1000);

  // No card: offer a retry instead of hanging forever. The old build sat in a
  // bare while(true) with "INIT FAILED!" on screen, so inserting a card meant
  // power-cycling the device.
  if (!sdReady) {
    while (!FileManager::initStorage()) {
      UI::clearScreen();
      UI::drawCenteredText("NO SD CARD", 45, ERROR_COLOR);
      UI::drawCenteredText("Insert a card and", 70, TEXT_COLOR);
      UI::drawCenteredText("press any key to retry", 82, TEXT_COLOR);
      UI::pushCanvas();
      UI::waitForAnyKey();
    }
    Settings::load();
    Bookmarks::load();
  }

  if (!fileManager.init()) {
    Serial.println("File manager initialization failed!");
    M5Cardputer.Display.setTextColor(ERROR_COLOR, BG_COLOR);
    UI::drawCenteredText("INIT FAILED!", 95, ERROR_COLOR);
    while (true) {
      delay(1000);
    }
  }

  Serial.println("File manager initialized");

  // Clear screen and show initial view
  UI::clearScreen();
  fileManager.render();

  Serial.println("Ready!");
}

// ==================== LOOP ====================
void loop() {
  // Update M5 system. This is what refreshes the keyboard's key list, and on
  // the ADV it drains exactly one event from the TCA8418's 10-deep FIFO per
  // call - so it must be reached often or keystrokes are silently lost.
  M5Cardputer.update();

  // Turn the raw key list into this frame's events.
  Input::update();

  // Update file manager
  fileManager.update();

  // Expires the acknowledgement flash back to the steady colour.
  StatusLed::update();

  // Repaint only when something changed. Pushing the 64,800-byte sprite
  // unconditionally at ~100Hz burnt the SPI bus and the battery for nothing.
  if (fileManager.needsRender()) {
    fileManager.render();
  }

  // Small delay to prevent excessive CPU usage
  delay(5);
}
