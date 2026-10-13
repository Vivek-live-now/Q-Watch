#include "file_manager.h"

FileManager fileManager;

FileManager::FileManager() {
}

void FileManager::ensureParentDir(const String& path) {
    int lastSlash = path.lastIndexOf('/');
    if (lastSlash <= 0) return;
    String dir = path.substring(0, lastSlash);
    int idx = 1;
    while (idx < (int)dir.length()) {
        int nextSlash = dir.indexOf('/', idx);
        String sub = (nextSlash == -1) ? dir : dir.substring(0, nextSlash);
        LittleFS.mkdir(sub);
        if (nextSlash == -1) break;
        idx = nextSlash + 1;
    }
    LittleFS.mkdir(dir);
}

bool FileManager::begin() {
    // Attempt mount without formatting first; if failed (unformatted partition), format and mount
    if (!LittleFS.begin(false)) {
        Serial.println("LittleFS Mount Failed. Formatting partition...");
        if (!LittleFS.begin(true)) {
            Serial.println("LittleFS Mount Failed after format");
            return false;
        }
        Serial.println("LittleFS Formatted & Mounted successfully");
    } else {
        Serial.println("LittleFS Mounted successfully");
    }

    // Ensure standard directories exist
    LittleFS.mkdir("/config");
    LittleFS.mkdir("/apps");
    LittleFS.mkdir("/anim");
    LittleFS.mkdir("/boot");
    LittleFS.mkdir("/ir");
    LittleFS.mkdir("/mochi");
    LittleFS.mkdir("/sounds");
    LittleFS.mkdir("/photos");
    LittleFS.mkdir("/images");

    // Automatically recover and relocate any files previously saved with URL-encoded names (e.g. /%2Fphotos%2Fphoto.bmp -> /photos/photo.bmp)
    File rootDir = LittleFS.open("/");
    if (rootDir && rootDir.isDirectory()) {
        File f = rootDir.openNextFile();
        while (f) {
            String fname = f.name();
            bool isDir = f.isDirectory();
            f.close();
            if (!isDir && (fname.indexOf("%2F") >= 0 || fname.indexOf("%2f") >= 0)) {
                String fullOld = fname.startsWith("/") ? fname : ("/" + fname);
                String fullNew = normalizePath(fullOld);
                if (fullNew != fullOld && isPathSafe(fullNew)) {
                    ensureParentDir(fullNew);
                    if (LittleFS.exists(fullNew)) {
                        LittleFS.remove(fullNew);
                    }
                    LittleFS.rename(fullOld, fullNew);
                }
            }
            f = rootDir.openNextFile();
        }
        rootDir.close();
    }
    return true;
}

bool FileManager::format() {
    return LittleFS.format();
}

void FileManager::end() {
    LittleFS.end();
}

String FileManager::urlDecode(const String& str) {
    if (str.indexOf('%') == -1 && str.indexOf('+') == -1) return str;
    String decoded = "";
    decoded.reserve(str.length());
    for (size_t i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        if (c == '%' && i + 2 < str.length()) {
            char h1 = str.charAt(i + 1);
            char h2 = str.charAt(i + 2);
            int v1 = (h1 >= '0' && h1 <= '9') ? (h1 - '0') :
                     (h1 >= 'a' && h1 <= 'f') ? (h1 - 'a' + 10) :
                     (h1 >= 'A' && h1 <= 'F') ? (h1 - 'A' + 10) : -1;
            int v2 = (h2 >= '0' && h2 <= '9') ? (h2 - '0') :
                     (h2 >= 'a' && h2 <= 'f') ? (h2 - 'a' + 10) :
                     (h2 >= 'A' && h2 <= 'F') ? (h2 - 'A' + 10) : -1;
            if (v1 >= 0 && v2 >= 0) {
                decoded += (char)((v1 << 4) | v2);
                i += 2;
                continue;
            }
        } else if (c == '+') {
            decoded += ' ';
            continue;
        }
        decoded += c;
    }
    return decoded;
}

bool FileManager::isPathSafe(const String& path) {
    if (path.length() == 0) return false;
    String p = urlDecode(path);
    for (size_t i = 0; i < p.length(); i++) {
        char c = p.charAt(i);
        if (c == '\0' || c == '\\' || (uint8_t)c < 32 || (uint8_t)c == 127) {
            return false;
        }
    }
    // Reject path traversal
    if (p.indexOf("..") != -1) return false;
    return true;
}

String FileManager::normalizePath(const String& path) {
    if (path.length() == 0) return "/";
    String p = path;
    p.trim();
    if (p.length() == 0) return "/";
    p.replace("\\", "/");
    while (p.indexOf("//") != -1) {
        p.replace("//", "/");
    }
    if (p.charAt(0) != '/') {
        p = "/" + p;
    }
    return p;
}

bool FileManager::exists(const String& path) {
    if (!isPathSafe(path)) return false;
    return LittleFS.exists(normalizePath(path));
}

bool FileManager::create(const String& path) {
    if (!isPathSafe(path)) return false;
    String p = normalizePath(path);
    if (LittleFS.exists(p)) return true; // Already exists
    ensureParentDir(p);
    File file = LittleFS.open(p, FILE_WRITE);
    if (!file) return false;
    file.close();
    return true;
}

bool FileManager::remove(const String& path) {
    if (!isPathSafe(path)) return false;
    return LittleFS.remove(normalizePath(path));
}

bool FileManager::rename(const String& pathFrom, const String& pathTo) {
    if (!isPathSafe(pathFrom) || !isPathSafe(pathTo)) return false;
    return LittleFS.rename(normalizePath(pathFrom), normalizePath(pathTo));
}

String FileManager::read(const String& path) {
    if (!isPathSafe(path)) return String();
    File file = LittleFS.open(normalizePath(path), FILE_READ);
    if (!file) return String();

    String content = file.readString();
    file.close();
    return content;
}

size_t FileManager::read(const String& path, uint8_t* buffer, size_t maxSize) {
    if (!buffer || maxSize == 0 || !isPathSafe(path)) return 0;

    File file = LittleFS.open(normalizePath(path), FILE_READ);
    if (!file) return 0;

    size_t bytesRead = file.read(buffer, maxSize);
    file.close();
    return bytesRead;
}

size_t FileManager::readSeek(const String& path, size_t offset, uint8_t* buffer, size_t length) {
    if (!buffer || length == 0 || !isPathSafe(path)) return 0;

    File file = LittleFS.open(normalizePath(path), FILE_READ);
    if (!file) return 0;

    if (offset > 0 && !file.seek(offset, SeekSet)) {
        file.close();
        return 0;
    }

    size_t bytesRead = file.read(buffer, length);
    file.close();
    return bytesRead;
}

bool FileManager::write(const String& path, const String& data) {
    if (!isPathSafe(path)) return false;
    String p = normalizePath(path);
    ensureParentDir(p);
    File file = LittleFS.open(p, FILE_WRITE);
    if (!file) return false;

    size_t bytesWritten = file.print(data);
    file.close();
    return bytesWritten == data.length();
}

bool FileManager::write(const String& path, const uint8_t* data, size_t size) {
    if (!isPathSafe(path)) return false;
    if (!data || size == 0) return create(path); // Create empty file if no data

    String p = normalizePath(path);
    ensureParentDir(p);
    File file = LittleFS.open(p, FILE_WRITE);
    if (!file) return false;

    size_t bytesWritten = file.write(data, size);
    file.close();
    return bytesWritten == size;
}

bool FileManager::append(const String& path, const String& data) {
    if (!isPathSafe(path)) return false;
    String p = normalizePath(path);
    ensureParentDir(p);
    File file = LittleFS.open(p, FILE_APPEND);
    if (!file) return false;

    size_t bytesWritten = file.print(data);
    file.close();
    return bytesWritten == data.length();
}

bool FileManager::append(const String& path, const uint8_t* data, size_t size) {
    if (!isPathSafe(path)) return false;
    if (!data || size == 0) return true;

    File file = LittleFS.open(normalizePath(path), FILE_APPEND);
    if (!file) return false;

    size_t bytesWritten = file.write(data, size);
    file.close();
    return bytesWritten == size;
}

bool FileManager::mkdir(const String& path) {
    if (!isPathSafe(path)) return false;
    String p = normalizePath(path);
    if (LittleFS.exists(p)) return true;
    ensureParentDir(p);
    return LittleFS.mkdir(p);
}

size_t FileManager::listDir(const String& path, FileInfo* results, size_t maxResults) {
    if (!results || maxResults == 0 || !isPathSafe(path)) return 0;

    File root = LittleFS.open(normalizePath(path));
    if (!root || !root.isDirectory()) {
        if (root) root.close();
        return 0;
    }

    size_t count = 0;
    File file = root.openNextFile();

    while (file && count < maxResults) {
        String fname = String(file.name());
        int lastSlash = fname.lastIndexOf('/');
        if (lastSlash >= 0) {
            fname = fname.substring(lastSlash + 1);
        }
        results[count].name = fname;
        results[count].size = file.size();
        results[count].isDir = file.isDirectory();

        count++;
        file.close();
        file = root.openNextFile();
    }
    if (file) file.close();
    root.close();

    return count;
}

size_t FileManager::freeSpace() {
    return LittleFS.totalBytes() - LittleFS.usedBytes();
}

size_t FileManager::totalSpace() {
    return LittleFS.totalBytes();
}

size_t FileManager::fileSize(const String& path) {
    if (!isPathSafe(path)) return 0;
    String p = normalizePath(path);
    if (!LittleFS.exists(p)) return 0;
    File f = LittleFS.open(p, "r");
    if (!f) return 0;
    size_t s = f.size();
    f.close();
    return s;
}
