#ifndef MASS_STORAGE_H
#define MASS_STORAGE_H

#include <Arduino.h>
#include "Config.h"
#include <USB.h>
#include <USBMSC.h>
#include <M5Cardputer.h>

class MassStorage {
public:
    static bool shouldStop;

    MassStorage();
    ~MassStorage();

    void begin();
    void loop();
    void end();
    void drawIcon(bool connected);
    
    bool isActive() const { return active; }

    static void setShouldStop(bool value) { shouldStop = value; }

private:
    USBMSC msc;
    bool active;

    void setupUsbCallbacks();
    void setupUsbEvents();
};

// Global callbacks
void usbEventCallback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
int32_t usbWriteCallback(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize);
int32_t usbReadCallback(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize);
bool usbStartStopCallback(uint8_t power_condition, bool start, bool load_eject);

#endif
