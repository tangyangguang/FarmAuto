#ifndef DEVICE_REGISTRY_SMOKE_ESP32BASE_H
#define DEVICE_REGISTRY_SMOKE_ESP32BASE_H

#include <stdint.h>
#include <stddef.h>

#include <map>
#include <string>
#include <vector>

class Esp32BaseFs {
public:
    static bool isReady() {
        return ready();
    }

    static bool mkdir(const char* path) {
        return path != nullptr && path[0] == '/';
    }

    static bool exists(const char* path) {
        if (path == nullptr) {
            return false;
        }
        if (std::string(path) == "/farmauto") {
            return true;
        }
        return files().find(path) != files().end();
    }

    static int64_t fileSize(const char* path) {
        if (path == nullptr) {
            return -1;
        }
        auto it = files().find(path);
        return it == files().end() ? -1 : static_cast<int64_t>(it->second.size());
    }

    static bool createFixedFile(const char* path, uint32_t size, uint8_t fillByte) {
        if (path == nullptr || path[0] != '/') {
            return false;
        }
        files()[path] = std::vector<uint8_t>(size, fillByte);
        return true;
    }

    static bool readBytesAt(const char* path, uint32_t offset, uint8_t* out, size_t maxLen, size_t* readLen) {
        if (readLen != nullptr) {
            *readLen = 0;
        }
        if (path == nullptr || out == nullptr) {
            return false;
        }
        auto it = files().find(path);
        if (it == files().end() || offset > it->second.size()) {
            return false;
        }
        const size_t available = it->second.size() - offset;
        const size_t toRead = available < maxLen ? available : maxLen;
        for (size_t i = 0; i < toRead; ++i) {
            out[i] = it->second[offset + i];
        }
        if (readLen != nullptr) {
            *readLen = toRead;
        }
        return true;
    }

    static bool writeBytesAt(const char* path, uint32_t offset, const uint8_t* data, size_t len) {
        if (path == nullptr || (data == nullptr && len != 0u)) {
            return false;
        }
        auto it = files().find(path);
        if (it == files().end() || offset + len > it->second.size()) {
            return false;
        }
        for (size_t i = 0; i < len; ++i) {
            it->second[offset + i] = data[i];
        }
        return true;
    }

    static void reset() {
        files().clear();
        ready() = true;
    }

private:
    static std::map<std::string, std::vector<uint8_t>>& files() {
        static std::map<std::string, std::vector<uint8_t>> instance;
        return instance;
    }

    static bool& ready() {
        static bool instance = true;
        return instance;
    }
};

#endif
