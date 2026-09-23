#ifndef QAPP_LOADER_H
#define QAPP_LOADER_H

#include <stdint.h>
#include <stdbool.h>
#include "qwatch_api.h"

#ifdef ARDUINO
#include <Arduino.h>
#include <LittleFS.h>
#endif

class QAppLoader {
public:
    QAppLoader();
    ~QAppLoader();

    // Validation & Header Inspection
    static QAppErrorCode validateFileHeader(const QAppFileHeader& hdr, uint32_t fileSize);
    static QAppErrorCode inspectFile(const char* path, QAppFileHeader* out_hdr);
    static QAppErrorCode inspectFile(const char* path, QAppHeader* out_hdr);

    // Lifecycle Management
    QAppErrorCode loadApp(const char* path);
    QAppErrorCode loadAppFromMemory(const uint8_t* buffer, uint32_t size);
    void update(float dt);
    void render();
    void handleButton(uint8_t btn, QButtonEvent evt);
    void unloadApp();

    bool isRunning() const { return is_running; }
    const QAppHeader* getActiveHeader() const { return is_running ? &active_header : nullptr; }
    const char* getActiveAppName() const { return is_running ? active_header.name : ""; }
    uint32_t getCodeAllocatedSize() const { return code_allocated_size; }
    uint32_t getDataAllocatedSize() const { return data_allocated_size; }
    uint32_t getAllocatedSize() const { return code_allocated_size + data_allocated_size; }
    bool isUsingPsram() const { return using_psram; }

    // Live API Initialization
    static void initLiveApi();
    static const QWatchAPI* getLiveApi();

private:
    bool is_running;
    QAppFileHeader active_file_header;
    QAppHeader active_header;
    uint8_t* code_memory;
    uint32_t code_allocated_size;
    uint8_t* data_memory;
    uint32_t data_allocated_size;
    bool using_psram;
};

extern QAppLoader qappLoader;

#endif // QAPP_LOADER_H
