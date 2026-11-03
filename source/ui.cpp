#include "app.hpp"
#include "download.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
namespace rr {
namespace {
struct Palette {
    SDL_Color background, panel, text, muted, line, accent;
};
Palette palette() {
    if (app.style.theme == 2)
        return {{23, 25, 29, 255},    {33, 36, 41, 255}, {234, 235, 239, 255},
                {173, 179, 188, 255}, {65, 71, 81, 255}, {246, 113, 112, 255}};
    if (app.style.theme == 1)
        return {{239, 228, 207, 255}, {249, 239, 221, 255}, {51, 42, 32, 255},
                {103, 88, 70, 255},   {205, 189, 163, 255}, {154, 49, 43, 255}};
    return {{242, 243, 245, 255}, {255, 255, 255, 255}, {30, 34, 41, 255},
            {94, 103, 116, 255},  {218, 222, 228, 255}, {178, 44, 51, 255}};
}
struct Label {
    SDL_Texture *texture;
    int w, h;
    uint64_t used;
};
std::map<std::string, Label> labels;
uint64_t serial = 0;
TTF_Font *preview[4]{};
void box(int x, int y, int w, int h, SDL_Color c) {
    SDL_SetRenderDrawColor(app.renderer, c.r, c.g, c.b, c.a);
    SDL_Rect r{x, y, w, h};
    SDL_RenderFillRect(app.renderer, &r);
}
void line(int x, int y, int w, SDL_Color c) {
    box(x, y, w, 1, c);
}
void footer(const std::string &first, const std::string &second = "") {
    auto p = palette();
    line(40, 654, 1200, p.line);
    text(first, 40, 665, 1200, p.text, app.small);
    if (!second.empty())
        text(second, 40, 694, 1200, p.muted, app.small);
}
int start(int count, int rows) {
    app.cursor = std::clamp(app.cursor, 0, std::max(0, count - 1));
    if (app.cursor < app.offset)
        app.offset = app.cursor;
    if (app.cursor >= app.offset + rows)
        app.offset = app.cursor - rows + 1;
    app.offset = std::clamp(app.offset, 0, std::max(0, count - rows));
    return app.offset;
}
void scrollbar(int x, int y, int h, int count, int rows) {
    auto p = palette();
    if (count <= rows)
        return;
    box(x, y, 3, h, p.line);
    int size = std::max(18, h * rows / count);
    box(x, y + (h - size) * app.offset / std::max(1, count - rows), 3, size, p.accent);
}
void cover(const Book &book, int x, int y, int width, int height) {
    auto p = palette();
    box(x + 2, y + 3, width, height, p.line);
    box(x, y, width, height, p.panel);
    auto found = app.covers.find(book.path);
    if (found != app.covers.end() && found->second) {
        int sourceWidth = 0, sourceHeight = 0;
        SDL_QueryTexture(found->second, nullptr, nullptr, &sourceWidth, &sourceHeight);
        auto crop = coverCrop(sourceWidth, sourceHeight);
        SDL_Rect source{crop.x, crop.y, crop.width, crop.height}, destination{x, y, width, height};
        if (crop.width > 0 && crop.height > 0)
            SDL_RenderCopy(app.renderer, found->second, &source, &destination);
    } else {
        box(x, y, width, height, p.background);
        box(x, y, width > 100 ? 5 : 2, height, p.accent);
        std::string label = format(book.path);
        for (char &c : label)
            if (c >= 'a' && c <= 'z')
                c -= 32;
        if (width < 100)
            label = label.substr(0, 1);
        text(label, x + (width < 100 ? 14 : 22), y + height / 2 - (width < 100 ? 12 : 22), width - 28,
             p.muted, width < 100 ? app.small : app.heading);
    }
}
void bookTitle(const std::string &title, int x, int y, int width) {
    int measured = 0;
    TTF_SizeUTF8(app.body, title.c_str(), &measured, nullptr);
    size_t split = title.size();
    while (measured > width && split != std::string::npos) {
        split = split ? title.find_last_of(' ', split - 1) : std::string::npos;
        if (split != std::string::npos)
            TTF_SizeUTF8(app.body, title.substr(0, split).c_str(), &measured, nullptr);
    }
    if (split == std::string::npos || split == title.size()) {
        text(title, x, y, width, palette().text);
        return;
    }
    text(title.substr(0, split), x, y, width, palette().text);
    text(title.substr(split + 1), x, y + 28, width, palette().text);
}
void library() {
    auto p = palette();
    text("Red Reader", 40, 24, 620, p.text, app.heading);
    box(820, 20, 220, 50, p.panel);
    text(download.state == DownloadState::Running ? "Downloading..." : "Download  [R3]", 834, 34, 200,
         p.accent, app.small);
    text(std::to_string(app.visible.size()) + " books", 1060, 34, 180, p.muted, app.small);
    line(40, 80, 1200, p.line);
    text(app.query.empty() ? (app.favorites ? "Favorites" : "Library") : "Search: " + app.query, 40, 96, 770,
         p.text, app.small);
    text(app.sort == 0   ? "Title A-Z"
         : app.sort == 1 ? "Title Z-A"
                         : "Recently read",
         1000, 96, 240, p.muted, app.small);
    app.cursor = std::clamp(app.cursor, 0, std::max(0, (int)app.visible.size() - 1));
    int first = libraryFirst(app.cursor);
    app.offset = first;
    // Keep the frame attached to the selected artwork; animate its emphasis, not its position.
    static int previousSelection = -1;
    static Uint64 selectionAt = 0;
    if (previousSelection != app.cursor) {
        previousSelection = app.cursor;
        selectionAt = SDL_GetTicks64();
    }
    if (!app.visible.empty()) {
        for (int i = first; i < std::min(first + libraryCapacity, (int)app.visible.size()); ++i) {
            const auto &b = app.books[app.visible[i]];
            int x = libraryX + (i - first) % libraryColumns * libraryPitchX;
            int y = libraryY + (i - first) / libraryColumns * libraryPitchY;
            cover(b, x, y, libraryCardW, libraryCardH);
            for (int row = 0; row < 64; ++row)
                box(x, y + libraryCardH - 64 + row, libraryCardW, 1, {0, 0, 0, (Uint8)(row * 3)});
            text(b.title, x + 8, y + libraryCardH - 34, libraryCardW - 16, {255, 255, 255, 255}, app.small);
            box(x + 7, y + 7, 60, 25, {15, 18, 24, 190});
            text(format(b.path), x + 11, y + 8, 52, {255, 255, 255, 255}, app.small);
            if (b.favorite) {
                box(x + libraryCardW - 24, y, 17, 27, p.accent);
                text("*", x + libraryCardW - 21, y + 1, 14, {255, 255, 255, 255}, app.small);
            }
            if (b.total > 1) {
                box(x, y + libraryCardH - 7, libraryCardW, 3, {0, 0, 0, 130});
                box(x, y + libraryCardH - 7, (int)(libraryCardW * b.fraction), 3, p.accent);
            }
        }
        int selectedX = libraryX + (app.cursor - first) % libraryColumns * libraryPitchX;
        int selectedY = libraryY + (app.cursor - first) / libraryColumns * libraryPitchY;
        float emphasis = app.style.motion ? std::min(1.f, (SDL_GetTicks64() - selectionAt) / 140.f) : 1.f;
        SDL_Color edge = p.accent;
        edge.a = (Uint8)(185 + 70 * emphasis);
        // Draw over the card so shadows, artwork and badges cannot hide any edge.
        SDL_SetRenderDrawColor(app.renderer, edge.r, edge.g, edge.b, edge.a);
        for (int inset = 0; inset < 3; ++inset) {
            SDL_Rect frame{selectedX + inset, selectedY + inset, libraryCardW - 2 * inset,
                           libraryCardH - 2 * inset};
            SDL_RenderDrawRect(app.renderer, &frame);
        }
        SDL_SetRenderDrawColor(app.renderer, 255, 255, 255, (Uint8)(65 + 45 * emphasis));
        SDL_Rect inner{selectedX + 3, selectedY + 3, libraryCardW - 6, libraryCardH - 6};
        SDL_RenderDrawRect(app.renderer, &inner);
        line(40, 541, 1200, p.line);
        const auto &b = app.books[app.visible[app.cursor]];
        bookTitle(b.title, 40, 557, 940);
        std::string info =
            b.total ? "Page " + std::to_string(b.page + 1) + " of " + std::to_string(b.total) : "Unread";
        text(info + "   /   " + std::to_string(b.marks.size()) + " bookmarks" +
                 (b.favorite ? "   /   Favorite" : ""),
             40, 619, 1000, p.muted, app.small);
        text(std::to_string(first / libraryCapacity + 1) + " / " +
                 std::to_string(((int)app.visible.size() + libraryCapacity - 1) / libraryCapacity),
             1100, 563, 140, p.text, app.body);
        text("Library page", 1090, 600, 150, p.muted, app.small);
    } else {
        text("No books found", 40, 244, 1100, p.text, app.heading);
        text(app.query.empty() ? "Add EPUB, PDF, CBZ or CBR files to sdmc:/lib"
                               : "Y: edit search   B: clear filters",
             40, 302, 1100, p.muted);
    }
    footer("A Open    X Resume    Y Search    L Favorites    R Sort    + Settings",
           "D-pad Select    - Favorite    ZL / ZR Library page    Touch: select, then open");
}
std::string dataSize(uint64_t bytes) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.1f MiB", bytes / (1024. * 1024.));
    return buffer;
}
void downloads() {
    auto p = palette();
    text("Download book", 80, 50, 1100, p.text, app.heading);
    line(80, 108, 1120, p.line);
    bool running = download.state == DownloadState::Running;
    text(download.name.empty() ? "EPUB / PDF / CBZ / CBR" : download.name, 80, 148, 1120, p.text);
    text("Save location: sdmc:/lib", 80, 195, 1120, p.muted, app.small);
    std::string status = running ? (download.bytes ? "Downloading" : "Connecting...")
                         : download.state == DownloadState::Complete  ? "Download complete"
                         : download.state == DownloadState::Failed    ? "Download failed"
                         : download.state == DownloadState::Cancelled ? "Cancelled"
                                                                      : "Enter a direct download link";
    text(status, 80, 264, 1120, p.text, app.heading);
    box(80, 333, 1120, 10, p.line);
    if (download.total) {
        double fraction = std::min(1., (double)download.bytes / download.total);
        box(80, 333, (int)(1120 * fraction), 10, p.accent);
        text(std::to_string((int)(fraction * 100)) + "%", 1080, 367, 120, p.text);
    } else if (running) {
        int shift = app.style.motion ? (SDL_GetTicks64() / 7) % 960 : 480;
        box(80 + shift, 333, 160, 10, p.accent);
    }
    if (download.state != DownloadState::Idle) {
        text(dataSize(download.bytes) +
                 (download.total ? " / " + dataSize(download.total) : " / size unknown"),
             80, 367, 700, p.text);
        if (running) {
            std::string speed = dataSize((uint64_t)download.speed) + "/s";
            if (download.total > download.bytes && download.speed > 1)
                speed += "   /   " +
                         std::to_string((int)((download.total - download.bytes) / download.speed)) +
                         "s remaining";
            text(speed, 80, 408, 1120, p.muted, app.small);
        }
    }
    text(download.message.empty() ? "Direct file links only; sign-in pages are not supported."
                                  : download.message,
         80, 465, 1120, p.muted, app.small);
    box(80, 550, 340, 54, p.panel);
    text(running ? "Download in progress" : "A  Enter link", 98, 565, 304, running ? p.muted : p.accent);
    if (running || download.state == DownloadState::Complete) {
        box(450, 550, 340, 54, p.panel);
        text(running ? "X  Cancel download" : "X  Open book", 468, 565, 304, p.accent);
    }
    box(820, 550, 380, 54, p.panel);
    text("B  Back to library", 838, 565, 344, p.text);
    footer(running ? "Downloads continue while you browse or read. X cancels the current download."
                   : "A Enter link    X Open completed book    B Library");
}
void drawPage(SDL_Texture *texture, float alpha, float slide, int width, int height) {
    if (!texture)
        return;
    int tw, th;
    SDL_QueryTexture(texture, nullptr, nullptr, &tw, &th);
    float fit = std::min((width - 64.f) / tw, (height - 116.f) / th);
    int w = tw * fit * app.zoom, h = th * fit * app.zoom;
    float maxX = std::max(0.f, (w - (width - 64.f)) / 2), maxY = std::max(0.f, (h - (height - 116.f)) / 2);
    app.panX = std::clamp(app.panX, -maxX, maxX);
    app.panY = std::clamp(app.panY, -maxY, maxY);
    SDL_Rect dest{(int)((width - w) / 2 + app.panX + slide), (int)((height - h) / 2 + app.panY), w, h};
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetTextureAlphaMod(texture, (Uint8)(alpha * 255));
    SDL_Rect clip{24, 48, width - 48, height - 96};
    SDL_RenderSetClipRect(app.renderer, &clip);
    SDL_RenderCopy(app.renderer, texture, nullptr, &dest);
    SDL_RenderSetClipRect(app.renderer, nullptr);
}
void reader() {
    auto p = palette();
    int width = app.style.portrait ? 720 : 1280, height = app.style.portrait ? 1280 : 720;
    if (app.style.portrait) {
        SDL_SetRenderTarget(app.renderer, app.portraitTarget);
        box(0, 0, width, height, p.background);
    }
    float t = app.style.motion ? std::min(1.f, (SDL_GetTicks64() - app.pageAnimation) / 220.f) : 1;
    float eased = t * t * (3.f - 2.f * t);
    drawPage(app.oldPage, 1, 0, width, height);
    drawPage(app.pageTexture, eased, 6 * (1 - eased) * app.direction, width, height);
    if (t >= 1 && app.oldPage) {
        SDL_DestroyTexture(app.oldPage);
        app.oldPage = nullptr;
    }
    if (app.hud && app.active >= 0) {
        text(app.books[app.active].title, 32, 14, width - 300, p.muted, app.small);
        text(app.busy ? "Rendering..." : std::to_string(app.page + 1) + " / " + std::to_string(app.total),
             width - 240, 14, 208, p.muted, app.small);
        line(32, height - 48, width - 64, p.line);
        if (app.total > 1)
            box(32, height - 48, (int)((width - 64) * (double)app.page / (app.total - 1)), 2, p.accent);
        text(
            app.style.portrait
                ? "L/R Page   Y Bookmark   + Settings"
                : "L/R Page   Y Bookmark   X Hide controls   ZL/ZR Text size / zoom   + Settings   B Library",
            32, height - 35, width - 64, p.muted, app.small);
    }
    if (app.busy) {
        int x = width / 2 - 60 + (int)((SDL_GetTicks64() / 12) % 100);
        box(x, height - 54, 20, 3, p.accent);
    }
    if (app.style.portrait) {
        SDL_SetRenderTarget(app.renderer, nullptr);
        SDL_Rect d{280, -280, 720, 1280};
        SDL_RenderCopyEx(app.renderer, app.portraitTarget, nullptr, &d, 90, nullptr, SDL_FLIP_NONE);
    }
}
void settings() {
    auto p = palette();
    text("Reading settings", 40, 28, 1100, p.text, app.heading);
    line(40, 91, 1200, p.line);
    const std::string names[] = {"Font",       "Text size",   "Line spacing", "Margins",
                                 "Appearance", "Orientation", "Animation",    "Go to page",
                                 "Chapters",   "Bookmarks",   "Library",      "Exit reader"};
    const std::string values[] = {fontNames[app.style.font],
                                  std::to_string(app.style.size),
                                  std::to_string(app.style.spacing / 10.f).substr(0, 3),
                                  std::to_string(app.style.margin),
                                  themeNames[app.style.theme],
                                  app.style.portrait ? "Portrait" : "Landscape",
                                  app.style.motion ? "On" : "Reduced motion",
                                  "",
                                  "",
                                  "",
                                  "",
                                  ""};
    int first = start(12, 8);
    for (int i = first; i < std::min(first + 8, 12); ++i) {
        int y = 119 + (i - first) * 65;
        box(40, y, 780, 59, i == app.cursor ? p.panel : p.background);
        if (i == app.cursor)
            box(40, y, 4, 59, p.accent);
        text(names[i], 60, y + 17, 310, p.text);
        text(values[i], 386, y + 17, 410, p.muted);
    }
    scrollbar(832, 119, 514, 12, 8);
    box(872, 119, 368, 514, p.panel);
    text("Font preview", 900, 148, 312, p.muted, app.small);
    if (!preview[app.style.font]) {
        const char *paths[] = {"fonts/CrimsonText-Regular.ttf", "fonts/Atkinson-Regular.ttf",
                               "fonts/CrimsonText-Regular.ttf", "fonts/Lusitana-Regular.ttf"};
        preview[app.style.font] =
            TTF_OpenFont((std::string(romfsPrefix) + paths[app.style.font]).c_str(), 32);
    }
    text("Aa Bb Cc Dd", 900, 201, 310, p.text, preview[app.style.font]);
    text("0123456789", 900, 250, 310, p.text, preview[app.style.font]);
    text("EPUB", 900, 342, 310, p.text);
    text("Font, size, spacing, margins", 900, 379, 310, p.muted, app.small);
    text("PDF / CBZ / CBR", 900, 438, 310, p.text);
    text("Original layout; zoom and pan", 900, 475, 310, p.muted, app.small);
    text("B applies reading changes", 900, 569, 310, p.muted, app.small);
    footer("Up / Down Select    Left / Right Adjust    A Change / open    B Apply and close");
}
void navigation() {
    auto p = palette();
    bool marks = app.screen == Screen::Bookmarks;
    text(marks ? "Bookmarks" : "Chapters", 40, 28, 1100, p.text, app.heading);
    line(40, 91, 1200, p.line);
    int count = marks ? app.books[app.active].marks.size() : app.chapters.size();
    int first = start(count, 8);
    for (int i = first; i < std::min(first + 8, count); ++i) {
        int page =
            marks ? resolvePage(app.books[app.active].marks[i].page, app.books[app.active].marks[i].total,
                                app.books[app.active].marks[i].fraction, app.total)
                  : app.chapters[i].page;
        int y = 122 + (i - first) * 64;
        box(40, y, 1190, 58, i == app.cursor ? p.panel : p.background);
        if (i == app.cursor)
            box(40, y, 4, 58, p.accent);
        text(marks ? "Page " + std::to_string(page + 1) : app.chapters[i].title,
             64 + (marks ? 0 : std::min(4, app.chapters[i].depth) * 20), y + 16, 950, p.text);
        text(std::to_string(page + 1), 1090, y + 16, 110, p.muted);
    }
    if (!count) {
        text(marks ? "No bookmarks" : "No chapter list in this book", 64, 200, 1100, p.text);
        if (marks)
            text("Press Y while reading to add a bookmark", 64, 244, 1100, p.muted);
    }
    scrollbar(1237, 122, 512, count, 8);
    footer(marks ? "A Open bookmark    X Remove    B Back" : "A Open chapter    B Back");
}
} // namespace
void text(const std::string &value, int x, int y, int width, SDL_Color color, TTF_Font *font) {
    if (value.empty() || width < 1)
        return;
    if (!font)
        font = app.body;
    std::string shown = value;
    int tw = 0;
    TTF_SizeUTF8(font, shown.c_str(), &tw, nullptr);
    while (tw > width && !shown.empty()) {
        size_t end = shown.size() - 1;
        while (end && ((unsigned char)shown[end] & 0xc0) == 0x80)
            --end;
        shown.resize(end);
        TTF_SizeUTF8(font, (shown + "...").c_str(), &tw, nullptr);
    }
    if (shown != value)
        shown += "...";
    std::string key = std::to_string((uintptr_t)font) + shown;
    auto it = labels.find(key);
    if (it == labels.end()) {
        if (labels.size() >= 128) {
            auto old = std::min_element(labels.begin(), labels.end(), [](const auto &a, const auto &b) {
                return a.second.used < b.second.used;
            });
            SDL_DestroyTexture(old->second.texture);
            labels.erase(old);
        }
        auto *surface = TTF_RenderUTF8_Blended(font, shown.c_str(), {255, 255, 255, 255});
        if (!surface)
            return;
        auto *texture = SDL_CreateTextureFromSurface(app.renderer, surface);
        Label l{texture, surface->w, surface->h, ++serial};
        SDL_FreeSurface(surface);
        if (!texture)
            return;
        it = labels.emplace(key, l).first;
    }
    it->second.used = ++serial;
    SDL_SetTextureColorMod(it->second.texture, color.r, color.g, color.b);
    SDL_Rect d{x, y, it->second.w, it->second.h};
    SDL_RenderCopy(app.renderer, it->second.texture, nullptr, &d);
}
void render() {
    auto p = palette();
    SDL_SetRenderDrawBlendMode(app.renderer, SDL_BLENDMODE_BLEND);
    box(0, 0, 1280, 720, p.background);
    if (app.screen == Screen::ModeRequired) {
        text("Full RAM mode required", 80, 220, 1120, p.text, app.heading);
        text("Launch Homebrew Menu by holding R while opening a game.", 80, 292, 1120, p.muted);
        text("Then open Red Reader. Album / applet mode is not supported.", 80, 335, 1120, p.muted);
        text("A / B  Exit", 80, 430, 1120, p.accent);
    } else {
        int slide = app.style.motion && app.screen != Screen::Reading
                        ? (int)(12 * std::max(0., 1. - (SDL_GetTicks64() - app.transition) / 160.))
                        : 0;
        SDL_Rect viewport{0, slide, 1280, 720};
        SDL_RenderSetViewport(app.renderer, &viewport);
        switch (app.screen) {
        case Screen::Download:
            downloads();
            break;
        case Screen::Library:
            library();
            break;
        case Screen::Reading:
            reader();
            break;
        case Screen::Settings:
            settings();
            break;
        case Screen::Contents:
        case Screen::Bookmarks:
            navigation();
            break;
        default:
            break;
        }
        SDL_RenderSetViewport(app.renderer, nullptr);
    }
    if (SDL_GetTicks64() < app.noticeUntil) {
        box(160, 590, 960, 51, p.text);
        text(app.notice, 180, 603, 920, p.panel, app.small);
    }
    SDL_RenderPresent(app.renderer);
}
void clearUi() {
    for (auto &p : labels)
        SDL_DestroyTexture(p.second.texture);
    labels.clear();
    for (auto &p : app.covers)
        SDL_DestroyTexture(p.second);
    app.covers.clear();
    for (auto *&f : preview)
        if (f) {
            TTF_CloseFont(f);
            f = nullptr;
        }
    SDL_DestroyTexture(app.pageTexture);
    SDL_DestroyTexture(app.oldPage);
    SDL_DestroyTexture(app.portraitTarget);
}
} // namespace rr
