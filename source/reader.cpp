#include "app.hpp"
#include <algorithm>
#include <set>
namespace rr {
namespace {
SDL_Texture *texture(const Image &image) {
    if (image.pixels.empty())
        return nullptr;
    auto *surface = SDL_CreateRGBSurfaceWithFormatFrom((void *)image.pixels.data(), image.width, image.height,
                                                       24, image.width * 3, SDL_PIXELFORMAT_RGB24);
    if (!surface)
        return nullptr;
    auto *t = SDL_CreateTextureFromSurface(app.renderer, surface);
    SDL_FreeSurface(surface);
    return t;
}
} // namespace
void openBook(int index) {
    if (index < 0 || index >= (int)app.books.size())
        return;
    saveReading();
    app.libraryCursor = app.cursor;
    app.active = index;
    app.total = 0;
    app.page = app.books[index].page;
    app.zoom = 1;
    app.panX = app.panY = 0;
    app.hud = false;
    if (app.pageTexture) {
        SDL_DestroyTexture(app.pageTexture);
        app.pageTexture = nullptr;
    }
    if (app.oldPage) {
        SDL_DestroyTexture(app.oldPage);
        app.oldPage = nullptr;
    }
    go(Screen::Reading);
    const auto &b = app.books[index];
    app.busy = true;
    app.coverRequestId = 0;
    submit({++app.requestId, b.path, app.style, b.page, b.total, b.fraction, false});
}
void requestPage(int page, bool relayout) {
    if (app.active < 0)
        return;
    app.requestedPage = std::clamp(page, 0, std::max(0, app.total - 1));
    app.direction = app.requestedPage >= app.page ? 1 : -1;
    app.busy = true;
    double fraction = app.total > 1 ? (double)app.page / (app.total - 1) : 0;
    submit({++app.requestId, app.books[app.active].path, app.style, app.requestedPage, app.total,
            relayout ? fraction : (app.total > 1 ? (double)app.requestedPage / (app.total - 1) : 0), false});
}
void jumpTo(int page, int previousTotal, double fraction) {
    app.busy = true;
    submit({++app.requestId, app.books[app.active].path, app.style, page, previousTotal, fraction, false});
    go(Screen::Reading);
}
void pollEngine() {
    RenderResult r;
    if (receive(r) && ((r.cover && r.id == app.coverRequestId) || (!r.cover && r.id == app.requestId))) {
        if (r.cover) {
            app.coverRequestId = 0;
            if (!app.covers.count(r.path)) {
                if (app.covers.size() >= libraryCapacity + 4) {
                    std::set<std::string> visiblePaths;
                    int first = libraryFirst(app.cursor);
                    for (int i = first; i < std::min(first + libraryCapacity, (int)app.visible.size()); ++i)
                        visiblePaths.insert(app.books[app.visible[i]].path);
                    auto old = std::find_if(app.covers.begin(), app.covers.end(),
                                            [&](const auto &c) { return !visiblePaths.count(c.first); });
                    if (old == app.covers.end())
                        old = app.covers.begin();
                    SDL_DestroyTexture(old->second);
                    app.covers.erase(old);
                }
                app.covers[r.path] = texture(r.image);
            }
        } else {
            app.busy = false;
            if (!r.error.empty() || r.image.pixels.empty()) {
                app.pendingNavigation = 0;
                notify(r.error.empty() ? "Page unavailable" : r.error);
                if (!app.pageTexture) {
                    app.active = -1;
                    go(Screen::Library);
                    rebuild();
                }
                return;
            }
            auto *t = texture(r.image);
            if (!t) {
                notify("Could not allocate page texture");
                return;
            }
            SDL_DestroyTexture(app.oldPage);
            app.oldPage = app.pageTexture;
            app.pageTexture = t;
            app.pageWidth = r.image.width;
            app.pageHeight = r.image.height;
            app.page = r.page;
            app.requestedPage = r.page;
            app.total = r.total;
            app.chapters = std::move(r.chapters);
            app.pageAnimation = SDL_GetTicks64();
            app.zoom = 1;
            app.panX = app.panY = 0;
            saveReading();
            completeNavigation();
        }
    }
    if (app.screen == Screen::Library && !app.visible.empty() && !app.coverRequestId &&
        SDL_GetTicks64() >= app.coverAt) {
        int first = libraryFirst(app.cursor);
        std::vector<int> candidates{app.visible[app.cursor]};
        for (int i = first; i < std::min(first + libraryCapacity, (int)app.visible.size()); ++i)
            candidates.push_back(app.visible[i]);
        for (int index : candidates)
            if (!app.covers.count(app.books[index].path)) {
                app.coverRequestId = ++app.requestId;
                submit({app.coverRequestId, app.books[index].path, app.style, 0, 0, 0, true});
                break;
            }
    }
    if (app.saveAt && SDL_GetTicks64() >= app.saveAt)
        saveState();
}
void closeBook() {
    app.pendingNavigation = 0;
    saveReading();
    saveState();
    ++app.requestId;
    app.busy = false;
    app.active = -1;
    SDL_DestroyTexture(app.pageTexture);
    app.pageTexture = nullptr;
    SDL_DestroyTexture(app.oldPage);
    app.oldPage = nullptr;
    go(Screen::Library);
    app.cursor = app.libraryCursor;
    rebuild();
}
void toggleBookmark() {
    if (app.active < 0 || !app.total || app.busy)
        return;
    auto &marks = app.books[app.active].marks;
    auto it = std::find_if(marks.begin(), marks.end(), [](const Mark &m) {
        return resolvePage(m.page, m.total, m.fraction, app.total) == app.page;
    });
    if (it != marks.end()) {
        marks.erase(it);
        notify("Bookmark removed");
    } else if (marks.size() < 100) {
        marks.push_back({app.page, app.total, app.total > 1 ? (double)app.page / (app.total - 1) : 0});
        notify("Bookmark added");
    } else
        notify("Bookmark limit reached");
    saveState();
}
} // namespace rr
