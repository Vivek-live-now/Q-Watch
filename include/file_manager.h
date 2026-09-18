#pragma once

#include <Arduino.h>
#include <LittleFS.h>

struct FileInfo {
    String name;
    size_t size;
    bool isDir;
};

class FileManager {
public:
    FileManager();

    // Core
    bool begin();
    bool format();
    void end();

    // File operations
    bool exists(const String& path);
    bool create(const String& path);
    bool remove(const String& path);
    bool rename(const String& pathFrom, const String& pathTo);

    // Read operations
    String read(const String& path);
    size_t read(const String& path, uint8_t* buffer, size_t maxSize);

    // Write operations
    bool write(const String& path, const String& data);
    bool write(const String& path, const uint8_t* data, size_t size);

    // Append operations
    bool append(const String& path, const String& data);
    bool append(const String& path, const uint8_t* data, size_t size);

    // Directory operations
    size_t listDir(const String& path, FileInfo* results, size_t maxResults);

    // Storage info
    size_t freeSpace();
    size_t totalSpace();

private:
    String normalizePath(const String& path);
};

extern FileManager fileManager;
