#include "download.hpp"
#include "app.hpp"
#include "document_assets.hpp"
#include <atomic>
#include <cstdio>
#include <curl/curl.h>
#include <sys/stat.h>
#include <unistd.h>
namespace rr {
DownloadView download;
namespace {
Thread worker;
bool started = false, sockets = false;
std::atomic<bool> cancelled{false}, finished{false};
std::atomic<uint64_t> received{0}, expected{0};
std::string taskUrl, taskPath, taskError;
DownloadState outcome = DownloadState::Failed;
size_t writeFile(char *data, size_t size, size_t count, void *file) {
    if (cancelled)
        return 0;
    return fwrite(data, 1, size * count, static_cast<FILE *>(file));
}
int progress(void *, curl_off_t total, curl_off_t now, curl_off_t, curl_off_t) {
    expected = total > 0 ? total : 0;
    received = now > 0 ? now : 0;
    return cancelled ? 1 : 0;
}
void transfer(void *) {
    const std::string temporary = taskPath + ".part";
    outcome = DownloadState::Failed;
    taskError.clear();
    // Exclusive staging prevents overwriting books or abandoned partial downloads.
    FILE *file = fopen(temporary.c_str(), "wbx");
    CURL *curl = nullptr;
    bool initialized = false;
    if (!file)
        taskError = "Cannot create download file. Check SD space or choose another name.";
    else {
        initialized = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
        if (initialized)
            curl = curl_easy_init();
        if (!curl)
            taskError = "Could not initialize networking";
        else {
            char error[CURL_ERROR_SIZE]{};
            curl_easy_setopt(curl, CURLOPT_URL, taskUrl.c_str());
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "RedReader-Switch/2.3");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 8L);
            curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
            // HTTPS links cannot redirect to unencrypted HTTP.
            curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS,
                             taskUrl.rfind("https://", 0) == 0 ? CURLPROTO_HTTPS
                                                               : CURLPROTO_HTTP | CURLPROTO_HTTPS);
            curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L);
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 128L);
            curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 30L);
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
            curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFile);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress);
            if (access("sdmc:/lib/save/cacert.pem", R_OK) == 0)
                curl_easy_setopt(curl, CURLOPT_CAINFO, "sdmc:/lib/save/cacert.pem");
            auto result = curl_easy_perform(curl);
            if (result != CURLE_OK)
                taskError = error[0] ? error : curl_easy_strerror(result);
            curl_easy_cleanup(curl);
        }
        if (fclose(file) != 0 && taskError.empty())
            taskError = "Could not finish writing to the SD card";
        if (initialized)
            curl_global_cleanup();
        // HTTP success alone is insufficient: reject HTML pages and mismatched book formats.
        if (taskError.empty() && !cancelled && validBookFile(temporary, format(taskPath), taskError) &&
            !cancelled) {
            if (access(taskPath.c_str(), F_OK) == 0)
                taskError = "A book with this name already exists";
            else if (rename(temporary.c_str(), taskPath.c_str()) != 0)
                taskError = "Could not save the downloaded book";
            else
                outcome = DownloadState::Complete;
        }
        if (outcome != DownloadState::Complete)
            remove(temporary.c_str());
    }
    if (cancelled && outcome != DownloadState::Complete)
        outcome = DownloadState::Cancelled;
    finished = true;
}
} // namespace
void openDownloads() {
    go(Screen::Download);
}
void editDownload() {
    if (started)
        return;
    auto url = keyboard("Direct download link", download.url, 2048, true);
    if (url.empty())
        return;
    if (!validDownloadUrl(url)) {
        notify("Enter a direct HTTP or HTTPS download link");
        return;
    }
    auto name = keyboard("Save book as (EPUB, PDF, CBZ or CBR)", downloadName(url), 160, true);
    if (name.empty())
        return;
    if (!validDownloadName(name)) {
        notify("Use a filename ending in .epub, .pdf, .cbz or .cbr");
        return;
    }
    auto path = "sdmc:/lib/" + name;
    if (access(path.c_str(), F_OK) == 0 || access((path + ".part").c_str(), F_OK) == 0) {
        notify("That filename already exists. Choose a different name.");
        return;
    }
    if (!sockets)
        sockets = R_SUCCEEDED(socketInitializeDefault());
    if (!sockets) {
        notify("Could not initialize networking");
        return;
    }
    taskUrl = url;
    taskPath = path;
    cancelled = false;
    finished = false;
    received = 0;
    expected = 0;
    download = {};
    download.url = url;
    download.name = name;
    download.path = path;
    download.state = DownloadState::Running;
    if (R_FAILED(threadCreate(&worker, transfer, nullptr, nullptr, 512 * 1024, 0x3B, -2))) {
        download.state = DownloadState::Failed;
        download.message = "Could not start download worker";
        return;
    }
    if (R_FAILED(threadStart(&worker))) {
        threadClose(&worker);
        download.state = DownloadState::Failed;
        download.message = "Could not start download worker";
        return;
    }
    started = true;
}
void cancelDownload() {
    if (started) {
        cancelled = true;
        download.message = "Cancelling...";
    }
}
void pollDownload() {
    if (!started)
        return;
    download.bytes = received;
    download.total = expected;
    static uint64_t lastAt = 0, lastBytes = 0;
    auto now = SDL_GetTicks64();
    if (download.bytes < lastBytes || !lastAt) {
        lastBytes = 0;
        lastAt = now;
    }
    if (now - lastAt >= 500) {
        download.speed = (download.bytes - lastBytes) * 1000. / (now - lastAt);
        lastAt = now;
        lastBytes = download.bytes;
    }
    if (!finished)
        return;
    threadWaitForExit(&worker);
    threadClose(&worker);
    started = false;
    lastAt = lastBytes = 0;
    download.state = outcome;
    download.message = outcome == DownloadState::Complete    ? "Saved to sdmc:/lib"
                       : outcome == DownloadState::Cancelled ? "Download cancelled"
                                                             : taskError;
    if (outcome == DownloadState::Complete) {
        Book b;
        b.path = download.path;
        b.title = download.name.substr(0, download.name.find_last_of('.'));
        app.books.push_back(std::move(b));
        int cursor = app.cursor, offset = app.offset;
        rebuild();
        // Finishing in the background must not move a settings/chapter/bookmark selection.
        if (app.screen != Screen::Library) {
            app.cursor = cursor;
            app.offset = offset;
        }
        download.total = download.bytes;
        notify("Downloaded " + download.name);
    }
}
void stopDownloads() {
    if (started) {
        cancelled = true;
        threadWaitForExit(&worker);
        threadClose(&worker);
        started = false;
    }
    if (sockets) {
        socketExit();
        sockets = false;
    }
}
} // namespace rr
