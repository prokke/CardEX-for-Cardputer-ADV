#include "MassStorage.h"
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
        // Check for exit key (Fn+M)
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange()) {
            auto status = M5Cardputer.Keyboard.keysState();
            if (status.fn) {
                for (char c : status.word) {
                    if (tolower(c) == 'm') {
                        return; // Exit loop
                    }
                }
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
    int startY = (SCREEN_HEIGHT - bodyH) / 2;
    
    // Draw connector
    int connX = startX + bodyW;
    int connY = startY + (bodyH - connH) / 2;
    
    UI::canvas.fillRoundRect(connX, connY, connW, connH, 4, 0xBDF7);
    UI::canvas.fillRect(connX + 8, connY + 5, 8, 5, 0x7BEF);
    UI::canvas.fillRect(connX + 8, connY + 21, 8, 5, 0x7BEF);
    
    // Draw body
    UI::canvas.fillRoundRect(startX, startY, bodyW, bodyH, 8, 0x0410);
    
    // Draw LED
    uint16_t ledColor = connected ? 0x07E0 : 0x8410; // Green : White
    int ledX = startX + 10;
    int ledY = startY + (bodyH / 2);
    UI::canvas.fillRect(ledX, ledY - 20, 6, 40, ledColor);
    
    // Draw "USB" text - centered and larger
    UI::canvas.setTextColor(0xFFFF, 0x0410);
    UI::canvas.setTextSize(2);
    int textX = startX + (bodyW / 2) - 18; // Center approximately  
    int textY = startY + (bodyH / 2) - 8;
    UI::canvas.drawString("USB", textX, textY);
    UI::canvas.setTextSize(1); // Reset

    UI::pushCanvas();
}

// Write callback - with memcpy buffer as per reference
int32_t usbWriteCallback(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize) {
    // Verify freespace
    uint64_t freeSpace = SD.totalBytes() - SD.usedBytes();
    if (bufsize > freeSpace) {
        return -1;
    }

    // Verify sector size
    const uint32_t secSize = SD.sectorSize();
    if (secSize == 0) return -1;

    // Write blocks with buffer copy
    for (uint32_t x = 0; x < bufsize / secSize; ++x) {
        uint8_t blkBuffer[secSize];
        memcpy(blkBuffer, buffer + secSize * x, secSize);
        if (!SD.writeRAW(blkBuffer, lba + x)) {
            return -1;
        }
    }
    return bufsize;
}

// Read callback
int32_t usbReadCallback(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize) {
    const uint32_t secSize = SD.sectorSize();
    if (secSize == 0) return -1;

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
