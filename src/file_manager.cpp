#include "file_manager.h"

FileManager fileManager;

FileManager::FileManager() {
}

bool FileManager::begin() {
    // begin(formatOnFail, basePath, maxOpenFiles, partitionLabel)
    // formatOnFail = false
    if (!LittleFS.begin(false)) {
        Serial.println("LittleFS Mount Failed");
        return false;
    }
    Serial.println("LittleFS Mounted successfully");
    return true;
}

bool FileManager::format() {
    return LittleFS.format();
}

void FileManager::end() {
    LittleFS.end();
}

String FileManager::normalizePath(const String& path) {
    if (path.length() == 0) return "/";
    if (path.charAt(0) != '/') {
        return "/" + path;
    }
    return path;
}

bool FileManager::exists(const String& path) {
    return LittleFS.exists(normalizePath(path));
}

bool FileManager::create(const String& path) {
    String p = normalizePath(path);
    if (LittleFS.exists(p)) return true; // Already exists
    File file = LittleFS.open(p, FILE_WRITE);
    if (!file) return false;
    file.close();
    return true;
}

bool FileManager::remove(const String& path) {
    return LittleFS.remove(normalizePath(path));
}

bool FileManager::rename(const String& pathFrom, const String& pathTo) {
    return LittleFS.rename(normalizePath(pathFrom), normalizePath(pathTo));
}

String FileManager::read(const String& path) {
    File file = LittleFS.open(normalizePath(path), FILE_READ);
    if (!file) return String();

    String content = file.readString();
    file.close();
    return content;
}

size_t FileManager::read(const String& path, uint8_t* buffer, size_t maxSize) {
    if (!buffer || maxSize == 0) return 0;

    File file = LittleFS.open(normalizePath(path), FILE_READ);
    if (!file) return 0;

    size_t bytesRead = file.read(buffer, maxSize);
    file.close();
    return bytesRead;
}

bool FileManager::write(const String& path, const String& data) {
    File file = LittleFS.open(normalizePath(path), FILE_WRITE);
    if (!file) return false;

    size_t bytesWritten = file.print(data);
    file.close();
    return bytesWritten == data.length();
}

bool FileManager::write(const String& path, const uint8_t* data, size_t size) {
    if (!data || size == 0) return create(path); // Create empty file if no data

    File file = LittleFS.open(normalizePath(path), FILE_WRITE);
    if (!file) return false;

    size_t bytesWritten = file.write(data, size);
    file.close();
    return bytesWritten == size;
}

bool FileManager::append(const String& path, const String& data) {
    File file = LittleFS.open(normalizePath(path), FILE_APPEND);
    if (!file) return false;

    size_t bytesWritten = file.print(data);
    file.close();
    return bytesWritten == data.length();
}

bool FileManager::append(const String& path, const uint8_t* data, size_t size) {
    if (!data || size == 0) return true;

    File file = LittleFS.open(normalizePath(path), FILE_APPEND);
    if (!file) return false;

    size_t bytesWritten = file.write(data, size);
    file.close();
    return bytesWritten == size;
}

size_t FileManager::listDir(const String& path, FileInfo* results, size_t maxResults) {
    if (!results || maxResults == 0) return 0;

    File root = LittleFS.open(normalizePath(path));
    if (!root || !root.isDirectory()) {
        return 0;
    }

    size_t count = 0;
    File file = root.openNextFile();

    while (file && count < maxResults) {
        results[count].name = String(file.name());
        results[count].size = file.size();
        results[count].isDir = file.isDirectory();

        count++;
        file = root.openNextFile();
    }

    return count;
}

size_t FileManager::freeSpace() {
    return LittleFS.totalBytes() - LittleFS.usedBytes();
}

size_t FileManager::totalSpace() {
    return LittleFS.totalBytes();
}
