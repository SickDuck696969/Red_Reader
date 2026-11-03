#pragma once
#include <cstdint>
#include <string>
namespace rr {
enum class DownloadState { Idle, Running, Complete, Failed, Cancelled };
struct DownloadView {
    DownloadState state = DownloadState::Idle;
    std::string url, name, message, path;
    uint64_t bytes = 0, total = 0;
    double speed = 0;
};
extern DownloadView download;
void openDownloads();
void editDownload();
void cancelDownload();
void pollDownload();
void stopDownloads();
} // namespace rr
