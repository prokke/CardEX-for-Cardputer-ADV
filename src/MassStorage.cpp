#include "MassStorage.h"
#include "Input.h"
#include "UI.h"
#include <SD.h>

bool MassStorage::shouldStop = false;
MassStorage* gMassStorageInstance = nullptr;

MassStorage::MassStorage() : active(false) {
    gMassStorageInstance = this;
}

MassStorage::~MassStorage() {
    end();
    if (gMassStorageInstance == this) {
        gMassStorageInstance = nullptr;
    }
}

void MassStorage::begin() {
    if (active) return;
    
    setShouldStop(false);
    
    // Set active early to allow event callbacks
    active = true;
    
    setupUsbCallbacks();
    setupUsbEvents();
    
    // Draw BEFORE USB.begin (critical!)
    drawIcon(false);
    
    USB.begin();
}

void MassStorage::setupUsbCallbacks() {
    uint32_t secSize = SD.sectorSize();
    uint32_t numSectors = SD.numSectors();

    msc.vendorID("ESP32");
    msc.productID("CardEX");
    msc.productRevision("1.0");

    msc.onRead(usbReadCallback);
    msc.onWrite(usbWriteCallback);
    msc.onStartStop(usbStartStopCallback);

    msc.mediaPresent(true);
    msc.begin(numSectors, secSize);
}

void MassStorage::setupUsbEvents() {
    USB.onEvent(usbEventCallback);
}

// Static USB event callback
void usbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == ARDUINO_USB_EVENTS) {
        // Get the instance via static pointer
        extern MassStorage* gMassStorageInstance;
        if (gMassStorageInstance && gMassStorageInstance->isActive()) {
            switch (event_id) {
                case ARDUINO_USB_STARTED_EVENT:
                    gMassStorageInstance->drawIcon(true);
                    break;
                case ARDUINO_USB_STOPPED_EVENT:
                    gMassStorageInstance->drawIcon(false);
                    break;
                default:
                    break;
            }
        }
    }
}

void MassStorage::loop() {
    if (!active) return;

    while (!shouldStop) {
        // Check for the exit chord (Fn+M). The old code scanned
        // KeysState::word, which library 1.2.0 leaves empty whenever Fn is
        // held - so this mode could not be left from the device at all, only
        // by ejecting from the host.
        M5Cardputer.update();
        Input::update();

        for (const KeyEvent &event : Input::events()) {
            if (event.fn && event.letter() == 'm') {
                return;
            }
        }
        yield(); // Critical: yield to USB/background tasks
    }
}

void MassStorage::end() {
    if (!active) return;
    msc.end();
    active = false;
}

void MassStorage::drawIcon(bool connected) {
    // Clear screen
    UI::canvas.fillScreen(BG_COLOR);

    // Icon dimensions
    int bodyW = 100;
    int bodyH = 50;
    int connW = 35;
    int connH = 30;

    int totalW = bodyW + connW;
    int startX = (SCREEN_WIDTH - totalW) / 2;
    int startY = (SCREEN_HEIGHT - bodyH) / 2 - 6;

    // Draw connector
    int connX = startX + bodyW;
    int connY = startY + (bodyH - connH) / 2;

    UI::canvas.fillRoundRect(connX, connY, connW, connH, 4, SECONDARY_COLOR);
    UI::canvas.fillRect(connX + 8, connY + 5, 8, 5, MENU_BG);
    UI::canvas.fillRect(connX + 8, connY + 21, 8, 5, MENU_BG);

    // Draw body. Uses the configured palette rather than hardcoded colours, so
    // this screen follows the theme like everything else.
    UI::canvas.fillRoundRect(startX, startY, bodyW, bodyH, 8, SELECTED_BG);

    // Draw LED
    uint16_t ledColor = connected ? ACCENT_COLOR : SECONDARY_COLOR;
    int ledX = startX + 10;
    int ledY = startY + (bodyH / 2);
    UI::canvas.fillRect(ledX, ledY - 20, 6, 40, ledColor);

    // Draw "USB" text - centered and larger
    UI::canvas.setTextColor(TEXT_COLOR, SELECTED_BG);
    UI::canvas.setTextSize(2);
    int textX = startX + (bodyW / 2) - 18; // Center approximately
    int textY = startY + (bodyH / 2) - 8;
    UI::canvas.drawString("USB", textX, textY);
    UI::canvas.setTextSize(1); // Reset

    // The way out was undiscoverable, and until the input rework it did not
    // work at all.
    UI::drawCenteredText(connected ? "Connected - Fn+M to exit"
                                   : "Waiting for host - Fn+M to exit",
                         SCREEN_HEIGHT - 14, SECONDARY_COLOR);

    UI::pushCanvas();
}

// Both callbacks address the card by whole sectors, so a request that is not
// sector-aligned cannot be served correctly. The previous code silently
// dropped the remainder of `bufsize` and ignored `offset` entirely, yet still
// returned `bufsize` - telling the host the data was stored when part of it
// had gone nowhere. Failing the transfer is recoverable; silent corruption of
// the user's card is not.
static bool sectorAlignedRequest(uint32_t offset, uint32_t bufsize, uint32_t secSize) {
    return secSize != 0 && offset == 0 && bufsize != 0 && (bufsize % secSize) == 0;
}

// Write callback
int32_t usbWriteCallback(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
    // Note: no free-space check. The old one rejected any write larger than the
    // filesystem's free space, but the host mostly rewrites sectors that are
    // already allocated - on a nearly full card that refused perfectly legal
    // writes, including the FAT updates that finish a delete.
    const uint32_t secSize = SD.sectorSize();
    if (!sectorAlignedRequest(offset, bufsize, secSize)) {
        return -1;
    }

    // Written straight from the host's buffer; the old bounce buffer was a
    // variable-length array on the TinyUSB task stack and copied for nothing.
    for (uint32_t x = 0; x < bufsize / secSize; ++x) {
        if (!SD.writeRAW(buffer + (secSize * x), lba + x)) {
            return -1;
        }
    }
    return bufsize;
}

// Read callback
int32_t usbReadCallback(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    const uint32_t secSize = SD.sectorSize();
    if (!sectorAlignedRequest(offset, bufsize, secSize)) {
        return -1;
    }

    for (uint32_t x = 0; x < bufsize / secSize; ++x) {
        if (!SD.readRAW(reinterpret_cast<uint8_t *>(buffer) + (x * secSize), lba + x)) {
            return -1;
        }
    }
    return bufsize;
}

// Start/Stop callback
bool usbStartStopCallback(uint8_t power_condition, bool start, bool load_eject) {
    if (!start && load_eject) {
        MassStorage::setShouldStop(true);
        return false;
    }
    return true;
}
