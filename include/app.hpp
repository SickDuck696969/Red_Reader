#pragma once
#include "model.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <map>
#include <switch.h>
namespace rr {
#ifdef RR_DESKTOP
constexpr const char *romfsPrefix = "romfs/";
#else
constexpr const char *romfsPrefix = "romfs:/";
#endif
enum class Screen { Library, Reading, Settings, Contents, Bookmarks, Download, ModeRequired };
struct Chapter {
    std::string title;
    int page = 0, depth = 0;
};
struct Image {
    int width = 0, height = 0;
    std::vector<unsigned char> pixels;
};
struct RenderRequest {
    uint64_t id = 0;
    std::string path;
    Style style;
    int page = 0, previousTotal = 0;
    double fraction = 0;
    bool cover = false;
};
struct RenderResult {
    uint64_t id = 0;
    std::string path, error;
    bool cover = false;
    int page = 0, total = 0;
    Image image;
    std::vector<Chapter> chapters;
};
struct App {
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *pageTexture = nullptr, *oldPage = nullptr, *portraitTarget = nullptr;
    TTF_Font *small = nullptr, *body = nullptr, *heading = nullptr;
    Screen screen = Screen::Library, settingsReturn = Screen::Library;
    Style style;
    std::vector<Book> books;
    std::vector<int> visible;
    std::vector<Chapter> chapters;
    std::map<std::string, SDL_Texture *> covers;
    int cursor = 0, offset = 0, libraryCursor = 0, active = -1, page = 0, total = 0, sort = 0, pageWidth = 0,
        pageHeight = 0, requestedPage = 0;
    int pendingNavigation = 0;
    bool running = true, busy = false, hud = true, favorites = false;
    std::string query, lastRead, notice;
    uint64_t noticeUntil = 0, transition = 0, pageAnimation = 0, requestId = 0, coverAt = 0, saveAt = 0;
    uint64_t coverRequestId = 0;
    float focus = 0, zoom = 1, panX = 0, panY = 0, dt = .016f;
    int direction = 1;
};
extern App app;
void notify(const std::string &value);
void go(Screen screen);
void rebuild();
void scanLibrary();
void loadState();
void saveState();
void saveReading();
std::string keyboard(const char *prompt, const std::string &initial = "", int maxLength = 160,
                     bool emptyOnCancel = false);
bool startEngine();
void stopEngine();
void submit(RenderRequest request);
bool receive(RenderResult &result);
void openBook(int index);
void requestPage(int page, bool relayout = false);
void pollEngine();
void closeBook();
void toggleBookmark();
void jumpTo(int page, int previousTotal, double fraction);
void input(u64 down, u64 held);
void completeNavigation();
void touch(const SDL_Event &event);
void render();
void clearUi();
void text(const std::string &value, int x, int y, int width, SDL_Color color, TTF_Font *font = nullptr);
} // namespace rr
