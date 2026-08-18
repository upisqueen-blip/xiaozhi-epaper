#include "sd_card_storage.h"

#include <dirent.h>
#include <sys/stat.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "audio_codec.h"
#include "config.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"

namespace {
constexpr const char* kTag = "SdCardStorage";
constexpr const char* kRoot = "/sdcard/xiaozhi";

struct WavHeader {
    char riff[4] = {'R', 'I', 'F', 'F'};
    uint32_t file_size = 0;
    char wave[4] = {'W', 'A', 'V', 'E'};
    char fmt[4] = {'f', 'm', 't', ' '};
    uint32_t fmt_size = 16;
    uint16_t format = 1;
    uint16_t channels = 1;
    uint32_t sample_rate = 24000;
    uint32_t byte_rate = 48000;
    uint16_t block_align = 2;
    uint16_t bits = 16;
    char data[4] = {'d', 'a', 't', 'a'};
    uint32_t data_size = 0;
};

void EnsureDirectories() {
    mkdir(kRoot, 0775);
    mkdir("/sdcard/xiaozhi/images", 0775);
    mkdir("/sdcard/xiaozhi/audio", 0775);
    mkdir("/sdcard/xiaozhi/recordings", 0775);
    mkdir("/sdcard/xiaozhi/cache", 0775);
}
}  // namespace

SdCardStorage::~SdCardStorage() {
    if (mounted_) {
        esp_vfs_fat_sdcard_unmount("/sdcard", static_cast<sdmmc_card_t*>(card_));
        spi_bus_free(SD_SPI_NUM);
    }
}

bool SdCardStorage::Mount() {
    spi_bus_config_t bus = {};
    bus.mosi_io_num = SD_MOSI_PIN;
    bus.miso_io_num = SD_MISO_PIN;
    bus.sclk_io_num = SD_SCK_PIN;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.max_transfer_sz = 16 * 1024;
    esp_err_t err = spi_bus_initialize(SD_SPI_NUM, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "SPI bus init failed: %s", esp_err_to_name(err));
        return false;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_NUM;
    sdspi_device_config_t slot = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot.host_id = SD_SPI_NUM;
    slot.gpio_cs = SD_CS_PIN;
    esp_vfs_fat_sdmmc_mount_config_t mount = {};
    mount.format_if_mount_failed = false;
    mount.max_files = 6;
    mount.allocation_unit_size = 16 * 1024;
    sdmmc_card_t* card = nullptr;
    err = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot, &mount, &card);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "No usable TF card: %s", esp_err_to_name(err));
        spi_bus_free(SD_SPI_NUM);
        return false;
    }
    card_ = card;
    mounted_ = true;
    EnsureDirectories();
    ESP_LOGI(kTag, "TF card mounted at /sdcard");
    return true;
}

bool SdCardStorage::SafeName(const std::string& name) {
    return !name.empty() && name.size() <= 96 && name != "." && name != ".." &&
           name.find('/') == std::string::npos && name.find('\\') == std::string::npos &&
           name.find("..") == std::string::npos;
}

std::string SdCardStorage::Directory(const std::string& category) {
    if (category == "images") return std::string(kRoot) + "/images";
    if (category == "audio") return std::string(kRoot) + "/audio";
    if (category == "recordings") return std::string(kRoot) + "/recordings";
    return {};
}

std::string SdCardStorage::Path(const std::string& category, const std::string& name) {
    const auto dir = Directory(category);
    return dir.empty() || !SafeName(name) ? std::string() : dir + "/" + name;
}

std::string SdCardStorage::Status() const {
    if (!mounted_) return "TF card not mounted";
    uint64_t total = 0, free = 0;
    if (esp_vfs_fat_info("/sdcard", &total, &free) != ESP_OK) return "TF card mounted";
    char text[128];
    std::snprintf(text, sizeof(text), "TF mounted; total=%llu MB, free=%llu MB",
                  total / 1048576ULL, free / 1048576ULL);
    return text;
}

std::string SdCardStorage::List(const std::string& category) const {
    if (!mounted_) return "TF card not mounted";
    const auto dir = Directory(category);
    if (dir.empty()) return "invalid category";
    DIR* handle = opendir(dir.c_str());
    if (!handle) return "directory unavailable";
    std::string result;
    while (auto* item = readdir(handle)) {
        if (item->d_name[0] == '.') continue;
        if (!result.empty()) result += "\n";
        result += item->d_name;
        if (result.size() > 1800) { result += "\n..."; break; }
    }
    closedir(handle);
    return result.empty() ? "(empty)" : result;
}

bool SdCardStorage::Load(const std::string& category, const std::string& name,
                         std::vector<uint8_t>& data, size_t limit, std::string& error) const {
    if (!mounted_) { error = "TF card not mounted"; return false; }
    const auto path = Path(category, name);
    if (path.empty()) { error = "invalid file name"; return false; }
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) { error = "file not found"; return false; }
    std::fseek(file, 0, SEEK_END);
    const long size = std::ftell(file);
    std::rewind(file);
    if (size <= 0 || static_cast<size_t>(size) > limit) {
        std::fclose(file); error = "file is empty or too large"; return false;
    }
    data.resize(size);
    const bool ok = std::fread(data.data(), 1, data.size(), file) == data.size();
    std::fclose(file);
    if (!ok) { data.clear(); error = "read failed"; }
    return ok;
}

bool SdCardStorage::RecordWav(AudioCodec* codec, const std::string& name, int seconds,
                              std::string& error) {
    if (!mounted_ || !codec) { error = "TF card or codec unavailable"; return false; }
    const auto path = Path("recordings", name);
    if (path.empty()) { error = "invalid file name"; return false; }
    const std::string temp = path + ".tmp";
    FILE* file = std::fopen(temp.c_str(), "wb+");
    if (!file) { error = "cannot create recording"; return false; }
    WavHeader header;
    header.sample_rate = codec->input_sample_rate();
    header.byte_rate = header.sample_rate * 2;
    std::fwrite(&header, 1, sizeof(header), file);
    codec->Start();
    codec->EnableInput(true);
    std::vector<int16_t> pcm(480);
    uint32_t samples = 0;
    const uint32_t target = header.sample_rate * seconds;
    while (samples < target) {
        pcm.resize(std::min<uint32_t>(480, target - samples));
        if (!codec->InputData(pcm)) { error = "microphone read failed"; break; }
        if (std::fwrite(pcm.data(), sizeof(int16_t), pcm.size(), file) != pcm.size()) {
            error = "TF write failed"; break;
        }
        samples += pcm.size();
    }
    header.data_size = samples * sizeof(int16_t);
    header.file_size = header.data_size + sizeof(header) - 8;
    std::rewind(file);
    std::fwrite(&header, 1, sizeof(header), file);
    std::fflush(file);
    std::fclose(file);
    if (!error.empty()) { std::remove(temp.c_str()); return false; }
    std::remove(path.c_str());
    if (std::rename(temp.c_str(), path.c_str()) != 0) { error = "rename failed"; return false; }
    return true;
}

bool SdCardStorage::PlayWav(AudioCodec* codec, const std::string& name, std::string& error) const {
    if (!mounted_ || !codec) { error = "TF card or codec unavailable"; return false; }
    const auto path = Path("audio", name);
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file) { error = "file not found"; return false; }
    WavHeader header;
    if (std::fread(&header, 1, sizeof(header), file) != sizeof(header) ||
        std::memcmp(header.riff, "RIFF", 4) || std::memcmp(header.wave, "WAVE", 4) ||
        header.format != 1 || header.bits != 16 || header.channels != 1 ||
        header.sample_rate != static_cast<uint32_t>(codec->output_sample_rate())) {
        std::fclose(file); error = "requires mono 16-bit PCM WAV at codec sample rate"; return false;
    }
    codec->Start();
    codec->EnableOutput(true);
    std::vector<int16_t> pcm(480);
    while (true) {
        const size_t count = std::fread(pcm.data(), sizeof(int16_t), pcm.size(), file);
        if (!count) break;
        pcm.resize(count);
        codec->OutputData(pcm);
        pcm.resize(480);
    }
    std::fclose(file);
    return true;
}
