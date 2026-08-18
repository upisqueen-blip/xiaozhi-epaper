#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class AudioCodec;

class SdCardStorage {
public:
    SdCardStorage() = default;
    ~SdCardStorage();

    bool Mount();
    bool mounted() const { return mounted_; }
    std::string Status() const;
    std::string List(const std::string& category) const;
    bool Load(const std::string& category, const std::string& name,
              std::vector<uint8_t>& data, size_t limit, std::string& error) const;
    bool RecordWav(AudioCodec* codec, const std::string& name, int seconds, std::string& error);
    bool PlayWav(AudioCodec* codec, const std::string& name, std::string& error) const;

private:
    bool mounted_ = false;
    void* card_ = nullptr;

    static bool SafeName(const std::string& name);
    static std::string Directory(const std::string& category);
    static std::string Path(const std::string& category, const std::string& name);
};
