#define SDL_MAIN_HANDLED
#include "app.hpp"
#include "download.hpp"
#include <SDL2/SDL_image.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace rr {
App app;
DownloadView download;
void openDownloads() {
    go(Screen::Download);
}
void editDownload() {
    download.state = DownloadState::Running;
}
void cancelDownload() {
    download.state = DownloadState::Cancelled;
}

RenderRequest lastRequest;
RenderResult answer;
bool hasAnswer = false;
int saves = 0;
std::string keyboardAnswer;
void notify(const std::string &s) {
    app.notice = s;
}
void go(Screen s) {
    app.screen = s;
    app.cursor = app.offset = 0;
    app.focus = 0;
    app.transition = 0;
}
void rebuild() {
    app.visible = filtered(app.books, app.query, app.favorites, app.sort);
    app.cursor = std::clamp(app.cursor, 0, std::max(0, (int)app.visible.size() - 1));
    app.offset = 0;
    app.coverAt = UINT64_MAX;
}
void saveState() {
    ++saves;
    app.saveAt = 0;
}
void saveReading() {
    if (app.active >= 0 && app.total) {
        auto &b = app.books[app.active];
        b.page = app.page;
        b.total = app.total;
        b.fraction = app.total > 1 ? (double)app.page / (app.total - 1) : 0;
        app.lastRead = b.path;
    }
}
std::string keyboard(const char *, const std::string &, int, bool) {
    return keyboardAnswer;
}
void submit(RenderRequest r) {
    lastRequest = std::move(r);
}
bool receive(RenderResult &r) {
    if (!hasAnswer)
        return false;
    r = std::move(answer);
    hasAnswer = false;
    return true;
}
} // namespace rr
using namespace rr;
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                                                  \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
Image pageImage() {
    SDL_Surface *source = IMG_Load("build/epub-2.png");
    if (!source) {
        source = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 24, SDL_PIXELFORMAT_RGB24);
        SDL_FillRect(source, nullptr, SDL_MapRGB(source->format, 255, 255, 255));
        auto *f = TTF_OpenFont("romfs/fonts/CrimsonText-Regular.ttf", 24);
        const char *lines[] = {"Chapter One",
                               "",
                               "The current page is saved automatically.",
                               "Bookmarks and chapter navigation are available",
                               "from the reading settings menu.",
                               "",
                               "Use the shoulder buttons to turn the page.",
                               "Change the font, spacing and margins for EPUBs."};
        int y = 40;
        for (auto line : lines) {
            if (*line) {
                auto *label = TTF_RenderUTF8_Blended(f, line, {30, 30, 30, 255});
                SDL_Rect d{42, y, 0, 0};
                SDL_BlitSurface(label, nullptr, source, &d);
                SDL_FreeSurface(label);
            }
            y += 42;
        }
        TTF_CloseFont(f);
    }
    SDL_Surface *rgb = SDL_ConvertSurfaceFormat(source, SDL_PIXELFORMAT_RGB24, 0);
    Image image;
    image.width = rgb->w;
    image.height = rgb->h;
    image.pixels.resize(rgb->w * rgb->h * 3);
    for (int y = 0; y < rgb->h; ++y)
        memcpy(image.pixels.data() + y * rgb->w * 3, (char *)rgb->pixels + y * rgb->pitch, rgb->w * 3);
    SDL_FreeSurface(rgb);
    SDL_FreeSurface(source);
    return image;
}
void complete(int total = 30) {
    answer = {};
    answer.id = lastRequest.id;
    answer.path = lastRequest.path;
    answer.total = total;
    answer.page = resolvePage(lastRequest.page, lastRequest.previousTotal, lastRequest.fraction, total);
    answer.image = pageImage();
    answer.chapters = {{"Chapter One", 0, 0}, {"Chapter Two", 15, 0}};
    hasAnswer = true;
    pollEngine();
}
int main() {
    SDL_SetMainReady();
    SDL_Init(0);
    TTF_Init();
    IMG_Init(IMG_INIT_PNG);
    auto *surface = SDL_CreateRGBSurfaceWithFormat(0, 1280, 720, 32, SDL_PIXELFORMAT_RGBA32);
    app.renderer = SDL_CreateSoftwareRenderer(surface);
    app.portraitTarget =
        SDL_CreateTexture(app.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 720, 1280);
    app.small = TTF_OpenFont("romfs/fonts/Atkinson-Regular.ttf", 20);
    app.body = TTF_OpenFont("romfs/fonts/Atkinson-Regular.ttf", 25);
    app.heading = TTF_OpenFont("romfs/fonts/Atkinson-Bold.ttf", 36);
    CHECK(app.renderer && app.portraitTarget && app.small && app.body && app.heading);
    app.style.motion = false;
    for (auto name : {"A Study in Scarlet", "Around the World in Eighty Days", "Collected Stories",
                      "Illustrated Atlas", "The Time Machine", "Travel Notes", "Working with Type"}) {
        Book b;
        b.title = name;
        b.path = "sdmc:/lib/" + b.title + ".epub";
        app.books.push_back(b);
    }
    app.books[0].page = 9;
    app.books[0].total = 30;
    app.books[0].fraction = 9. / 29;
    app.books[0].favorite = true;
    rebuild();
    for (auto name : {"Field Notes", "Collected Poems", "The Odyssey", "Mountain Trails", "Night Sky",
                      "Design Studies", "Botanical Atlas", "Short Stories", "Travel Journal"}) {
        Book b;
        b.title = name;
        b.path = "sdmc:/lib/" + b.title + ".epub";
        app.books.push_back(b);
    }
    rebuild();
    app.coverAt = 0;
    pollEngine();
    const int sizes[][2] = {{300, 400}, {400, 200}, {200, 500}, {400, 400}, {150, 200}, {640, 360}};
    for (int i = 0; i < libraryCapacity; ++i) {
        CHECK(lastRequest.cover && lastRequest.path == app.books[app.visible[i]].path);
        answer = {};
        answer.cover = true;
        answer.id = lastRequest.id;
        answer.path = lastRequest.path;
        answer.image.width = sizes[i % 6][0];
        answer.image.height = sizes[i % 6][1];
        answer.image.pixels.resize(sizes[i % 6][0] * sizes[i % 6][1] * 3);
        for (int y = 0; y < sizes[i % 6][1]; ++y)
            for (int x = 0; x < sizes[i % 6][0]; ++x) {
                auto n = (y * sizes[i % 6][0] + x) * 3;
                bool band = y > sizes[i % 6][1] / 3 && y < sizes[i % 6][1] / 2;
                answer.image.pixels[n] = band ? 230 : 45 + (i % 6) * 20;
                answer.image.pixels[n + 1] = band ? 225 : 65 + (i % 6) * 12;
                answer.image.pixels[n + 2] = band ? 210 : 100 + (i % 6) * 8;
            }
        hasAnswer = true;
        pollEngine();
    }
    CHECK(app.covers.size() == libraryCapacity && app.coverRequestId == 0);
    render();
    CHECK(IMG_SavePNG(surface, "build/library.png") == 0);
    input(HidNpadButton_Down, 0);
    CHECK(app.cursor == libraryColumns);
    input(HidNpadButton_Up, 0);
    CHECK(app.cursor == 0);
    Book extra;
    extra.title = "Z last book";
    extra.path = "sdmc:/lib/Z.epub";
    app.books.push_back(extra);
    rebuild();
    input(HidNpadButton_ZR, 0);
    CHECK(app.cursor == libraryCapacity && libraryFirst(app.cursor) == libraryCapacity);
    render();
    input(HidNpadButton_ZL, 0);
    CHECK(app.cursor == 0);
    app.books.resize(7);
    rebuild();
    input(HidNpadButton_Right, 0);
    CHECK(app.cursor == 1);
    input(HidNpadButton_Left, 0);
    CHECK(app.cursor == 0);
    SDL_Event cardTap{};
    cardTap.type = SDL_FINGERDOWN;
    cardTap.tfinger.fingerId = 42;
    cardTap.tfinger.x = 230.f / 1280;
    cardTap.tfinger.y = 250.f / 720;
    touch(cardTap);
    cardTap.type = SDL_FINGERUP;
    touch(cardTap);
    CHECK(app.cursor == 1 && app.screen == Screen::Library);
    input(HidNpadButton_Left, 0);
    input(HidNpadButton_StickR, 0);
    CHECK(app.screen == Screen::Download);
    input(HidNpadButton_A, 0);
    CHECK(download.state == DownloadState::Running);
    download.name = "Example book.epub";
    download.bytes = 12 * 1024 * 1024;
    download.total = 20 * 1024 * 1024;
    download.speed = 2 * 1024 * 1024;
    render();
    CHECK(IMG_SavePNG(surface, "build/download.png") == 0);
    input(HidNpadButton_X, 0);
    CHECK(download.state == DownloadState::Cancelled);
    input(HidNpadButton_B, 0);
    CHECK(app.screen == Screen::Library);
    input(HidNpadButton_L, 0);
    CHECK(app.visible.size() == 1);
    input(HidNpadButton_L, 0);
    CHECK(app.visible.size() == 7);
    keyboardAnswer = "atlas";
    input(HidNpadButton_Y, 0);
    CHECK(app.visible.size() == 1);
    input(HidNpadButton_B, 0);
    CHECK(app.visible.size() == 7);
    input(HidNpadButton_A, 0);
    CHECK(app.busy && app.screen == Screen::Reading);
    complete();
    CHECK(!app.busy && app.page == 9 && app.total == 30);
    CHECK(!app.hud);
    render();
    unsigned char edge[4]{};
    SDL_Rect pixel{1, 1, 1, 1};
    CHECK(SDL_RenderReadPixels(app.renderer, &pixel, SDL_PIXELFORMAT_RGBA32, edge, 4) == 0);
    CHECK(edge[0] == 255 && edge[1] == 255 && edge[2] == 255);

    input(HidNpadButton_Y, 0);
    CHECK(app.books[0].marks.size() == 1);
    input(HidNpadButton_Y, 0);
    CHECK(app.books[0].marks.empty());
    input(HidNpadButton_Y, 0);
    input(HidNpadButton_R, 0);
    CHECK(lastRequest.page == 10);
    complete();
    CHECK(app.page == 10);
    render();
    CHECK(IMG_SavePNG(surface, "build/reading.png") == 0);
    input(HidNpadButton_Plus, 0);
    CHECK(app.screen == Screen::Settings);
    app.cursor = 0;
    input(HidNpadButton_Right, 0);
    input(HidNpadButton_Right, 0);
    CHECK(app.style.font == 2);
    render();
    CHECK(IMG_SavePNG(surface, "build/settings.png") == 0);
    app.cursor = 4;
    input(HidNpadButton_Right, 0);
    input(HidNpadButton_Right, 0);
    CHECK(app.style.alignment == 2);
    app.cursor = 9;
    input(HidNpadButton_A, 0);
    CHECK(app.busy && app.pendingNavigation == 2);
    complete(60);
    CHECK(app.screen == Screen::Contents && app.page == 20);
    render();
    CHECK(IMG_SavePNG(surface, "build/chapters.png") == 0);
    input(HidNpadButton_Down, 0);
    input(HidNpadButton_A, 0);
    CHECK(lastRequest.page == 15);
    complete(60);
    CHECK(app.page == 15);
    input(HidNpadButton_Plus, 0);
    app.cursor = 10;
    input(HidNpadButton_A, 0);
    CHECK(app.screen == Screen::Bookmarks);
    input(HidNpadButton_A, 0);
    complete(60);
    CHECK(app.page == 18); // normalized bookmark after reflow
    auto current = app.page;
    answer = {};
    answer.id = app.requestId - 1;
    answer.page = 0;
    hasAnswer = true;
    pollEngine();
    CHECK(app.page == current);
    input(HidNpadButton_Plus, 0);
    app.cursor = 8;
    keyboardAnswer = "99999";
    input(HidNpadButton_A, 0);
    CHECK(!app.busy);
    input(HidNpadButton_Plus, 0);
    app.cursor = 8;
    keyboardAnswer = "2";
    input(HidNpadButton_A, 0);
    complete(60);
    CHECK(app.page == 1);
    app.style.portrait = true;
    render();
    CHECK(IMG_SavePNG(surface, "build/portrait.png") == 0);
    app.style.portrait = false;
    for (int theme = 0; theme < 3; ++theme) {
        app.style.theme = theme;
        for (auto s :
             {Screen::Reading, Screen::Library, Screen::Settings, Screen::Bookmarks, Screen::Contents}) {
            app.screen = s;
            app.cursor = 0;
            render();
        }
    }
    app.screen = Screen::Reading;
    input(HidNpadButton_B, 0);
    CHECK(app.active < 0 && app.screen == Screen::Library && saves > 0);
    app.books[0].path = "sdmc:/lib/test.pdf";
    openBook(0);
    complete();
    input(HidNpadButton_ZR, 0);
    CHECK(app.zoom == 1.25f && !app.busy);
    input(HidNpadButton_Plus, 0);
    app.cursor = 0;
    int font = app.style.font;
    input(HidNpadButton_A, 0);
    CHECK(app.style.font == font);
    input(HidNpadButton_B, 0);
    CHECK(app.screen == Screen::Reading);
    auto fingerEvent = [](Uint32 type, SDL_FingerID id, float x, float y) {
        SDL_Event e{};
        e.type = type;
        e.tfinger.fingerId = id;
        e.tfinger.x = x;
        e.tfinger.y = y;
        touch(e);
    };
    fingerEvent(SDL_FINGERDOWN, 1, .4f, .5f);
    fingerEvent(SDL_FINGERDOWN, 2, .6f, .5f);
    fingerEvent(SDL_FINGERMOTION, 2, .7f, .5f);
    CHECK(app.zoom > 1.8f);
    CHECK(app.panX > 60.f); // centroid moved right; zoom must follow it rather than stay centered

    fingerEvent(SDL_FINGERUP, 2, .7f, .5f);
    fingerEvent(SDL_FINGERUP, 1, .4f, .5f);
    CHECK(!app.busy);
    app.style.portrait = true;
    app.zoom = 1;
    app.panX = app.panY = 0;
    fingerEvent(SDL_FINGERDOWN, 1, .25f, .4f);
    fingerEvent(SDL_FINGERDOWN, 2, .25f, .6f);
    fingerEvent(SDL_FINGERMOTION, 2, .25f, .8f);
    CHECK(app.zoom > 1.9f && app.panX > 60.f &&
          app.panY == 0); // landscape page fits portrait height: clamp vertical pan

    fingerEvent(SDL_FINGERUP, 1, .25f, .4f);
    fingerEvent(SDL_FINGERUP, 2, .25f, .8f);
    app.style.portrait = false;
    for (int fontIndex = 0; fontIndex < fontCount; ++fontIndex) {
        app.style.font = fontIndex;
        auto fontPath = std::string("romfs/fonts/") + fontFamilies[fontIndex] + "-Regular.ttf";
        auto *font = TTF_OpenFont(fontPath.c_str(), 24);
        CHECK(font);
        TTF_CloseFont(font);
        app.screen = Screen::Settings;
        render();
    }
    app.screen = Screen::ModeRequired;
    render();
    CHECK(IMG_SavePNG(surface, "build/full-ram-required.png") == 0);
    input(HidNpadButton_A, 0);
    CHECK(!app.running);
    clearUi();
    TTF_CloseFont(app.small);
    TTF_CloseFont(app.body);
    TTF_CloseFont(app.heading);
    SDL_DestroyRenderer(app.renderer);
    SDL_FreeSurface(surface);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    puts("PASS: production input/reader/rendering, search, favorites, bookmarks, deferred navigation, reflow "
         "restoration, stale results, zoom, portrait, themes, full-RAM gate");
}
