/*
 * File Manager with Text Editor
 * For M5Stack Cardputer / Cardputer ADV
 *
 * Features:
 * - Browse SD card
 * - Full-featured text editor
 * - File operations (copy, move, delete, rename)
 * - Search functionality
 * - Context-sensitive help (Opt)
 *
 * Author: -Prokke
 * Version: 1.0.0
 * Date: 2026-01-26
 */

#include "src/Config.h"
#include "src/FileManager.h"
#include "src/UI.h"
#include <M5Cardputer.h>

// Global file manager instance
FileManager fileManager;

// ==================== SETUP ====================
void setup() {
  // Initialize serial for debugging
  Serial.begin(115200);
  delay(100);

  Serial.println("==============");
  Serial.println("CardEX v1.0.0");
  Serial.println("==============");

  // Initialize M5Cardputer
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true); // Enable keyboard

  // Set display rotation
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setBrightness(128);

  Serial.println("M5Cardputer initialized");

  // Initialize UI
  UI::init();
  UI::clearScreen();

  // Show splash screen
  M5Cardputer.Display.setTextColor(ACCENT_COLOR, BG_COLOR);
  UI::drawCenteredText("FILE MANAGER", 40, ACCENT_COLOR);
  M5Cardputer.Display.setTextColor(TEXT_COLOR, BG_COLOR);
  UI::drawCenteredText("v1.0.0", 55, TEXT_COLOR);
  M5Cardputer.Display.setTextColor(SECONDARY_COLOR, BG_COLOR);
  UI::drawCenteredText("Initializing...", 75, SECONDARY_COLOR);

  delay(1000);

  // Initialize file manager
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
  // Update M5 system
  M5Cardputer.update();

  // Update file manager
  fileManager.update();

  // No throttling needed with double buffering
  fileManager.render();

  // Small delay to prevent excessive CPU usage
  delay(10);
}
