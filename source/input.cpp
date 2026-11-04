#include "app.hpp"
#include "download.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
namespace rr {
namespace {
Style previousStyle;
int previousCursor = 0;
void settings() {
    previousStyle = app.style;
    previousCursor = app.cursor;
    app.settingsReturn = app.screen;
    go(Screen::Settings);
}
void applySettings() {
    saveState();
    go(app.settingsReturn);
    app.cursor = previousCursor;
    if (app.active >= 0 &&
        (styleKey(previousStyle) != styleKey(app.style) || previousStyle.theme != app.style.theme))
        requestPage(app.page, true);
}
void move(int delta, int count) {
    int old = app.cursor;
    app.cursor = std::clamp(app.cursor + delta, 0, std::max(0, count - 1));
    if (old != app.cursor)
        app.coverAt = SDL_GetTicks64() + 180;
}
void turn(int amount) {
    if (!app.busy && app.total > 0 &&
        (!app.style.motion || !app.oldPage || SDL_GetTicks64() - app.pageAnimation >= 220))
        requestPage(app.page + amount);
}
void jump() {
    if (app.active < 0 || app.busy)
        return;
    auto s = keyboard("Page number (1 - total)", std::to_string(app.page + 1));
    if (s.empty())
        return;
    char *end = nullptr;
    long n = strtol(s.c_str(), &end, 10);
    if (*end || n < 1 || n > app.total) {
        notify("Page number is out of range");
        return;
    }
    go(Screen::Reading);
    requestPage(n - 1);
}
} // namespace
void completeNavigation() {
    if (app.busy || !app.pendingNavigation)
        return;
    int action = app.pendingNavigation;
    app.pendingNavigation = 0;
    if (action == 1)
        jump();
    else
        go(action == 2 ? Screen::Contents : Screen::Bookmarks);
}
void input(u64 down, u64 held) {
    if (app.screen == Screen::ModeRequired) {
        if (down & (HidNpadButton_A | HidNpadButton_B | HidNpadButton_Plus))
            app.running = false;
        return;
    }
    if (app.screen == Screen::Download) {
        if (down & HidNpadButton_B) {
            go(Screen::Library);
            app.cursor = app.libraryCursor;
            rebuild();
        } else if (down & HidNpadButton_X) {
            if (download.state == DownloadState::Running)
                cancelDownload();
            else if (download.state == DownloadState::Complete) {
                auto it = std::find_if(app.books.begin(), app.books.end(),
                                       [](const Book &b) { return b.path == download.path; });
                if (it != app.books.end())
                    openBook(it - app.books.begin());
            }
        } else if (down & HidNpadButton_A)
            editDownload();
        return;
    }
    int delta = (down & (HidNpadButton_Down | HidNpadButton_StickLDown) ? 1 : 0) -
                (down & (HidNpadButton_Up | HidNpadButton_StickLUp) ? 1 : 0);
    if (app.screen == Screen::Settings) {
        move(delta, settingsCount);
        if (down & (HidNpadButton_B | HidNpadButton_Plus)) {
            applySettings();
            return;
        }
        if (down & (HidNpadButton_A | HidNpadButton_Left | HidNpadButton_Right)) {
            int sign = down & HidNpadButton_Left ? -1 : 1;
            bool fixed = app.active >= 0 && fixedLayout(app.books[app.active].path);
            if (app.cursor < 5 && fixed) {
                notify("Text formatting is available for EPUB, FB2 and HTML books");
                return;
            }
            switch (app.cursor) {
            case 0:
                app.style.font = (app.style.font + sign + fontCount) % fontCount;
                break;
            case 1:
                app.style.size = std::clamp(app.style.size + sign * 2, 12, 42);
                break;
            case 2:
                app.style.spacing = std::clamp(app.style.spacing + sign, 10, 22);
                break;
            case 3:
                app.style.margin = std::clamp(app.style.margin + sign * 8, 0, 88);
                break;
            case 4:
                app.style.alignment = (app.style.alignment + sign + alignmentCount) % alignmentCount;
                break;
            case 5:
                app.style.theme = (app.style.theme + sign + 3) % 3;
                break;
            case 6:
                app.style.portrait = !app.style.portrait;
                break;
            case 7:
                app.style.motion = !app.style.motion;
                break;
            case 8:
                if (app.active >= 0) {
                    applySettings();
                    app.pendingNavigation = 1;
                    completeNavigation();
                }
                break;
            case 9:
                if (app.active >= 0) {
                    applySettings();
                    app.pendingNavigation = 2;
                    completeNavigation();
                }
                break;
            case 10:
                if (app.active >= 0) {
                    applySettings();
                    app.pendingNavigation = 3;
                    completeNavigation();
                }
                break;
            case 11:
                if (app.active >= 0)
                    closeBook();
                else
                    go(Screen::Library);
                saveState();
                break;
            case 12:
                saveReading();
                saveState();
                app.running = false;
                break;
            }
        }
        return;
    }
    if (down & HidNpadButton_Plus) {
        if (!app.busy)
            settings();
        return;
    }
    if (app.screen == Screen::Library) {
        if (down & HidNpadButton_StickR) {
            app.libraryCursor = app.cursor;
            openDownloads();
            return;
        }
        delta *= libraryColumns;
        if (down & (HidNpadButton_Right | HidNpadButton_StickLRight))
            delta = 1;
        if (down & (HidNpadButton_Left | HidNpadButton_StickLLeft))
            delta = -1;
        move(delta, app.visible.size());
        if (down & HidNpadButton_ZL)
            move(-libraryCapacity, app.visible.size());
        if (down & HidNpadButton_ZR)
            move(libraryCapacity, app.visible.size());
        if ((down & HidNpadButton_A) && !app.visible.empty())
            openBook(app.visible[app.cursor]);
        if (down & HidNpadButton_X) {
            auto i = std::find_if(app.books.begin(), app.books.end(),
                                  [](const Book &b) { return b.path == app.lastRead; });
            if (i != app.books.end())
                openBook(i - app.books.begin());
            else
                notify("No previous book available");
        }
        if (down & HidNpadButton_Y) {
            app.query = keyboard("Search library", app.query);
            app.cursor = 0;
            rebuild();
        }
        if (down & HidNpadButton_L) {
            app.favorites = !app.favorites;
            app.cursor = 0;
            rebuild();
        }
        if (down & HidNpadButton_R) {
            app.sort = (app.sort + 1) % 3;
            rebuild();
            saveState();
        }
        if ((down & HidNpadButton_Minus) && !app.visible.empty()) {
            auto &b = app.books[app.visible[app.cursor]];
            b.favorite = !b.favorite;
            saveState();
            rebuild();
        }
        if (down & HidNpadButton_B) {
            app.query.clear();
            app.favorites = false;
            rebuild();
        }
    } else if (app.screen == Screen::Reading) {
        if (down & HidNpadButton_B) {
            closeBook();
            return;
        }
        if (down & HidNpadButton_X)
            app.hud = !app.hud;
        if (down & HidNpadButton_Y)
            toggleBookmark();
        if (!app.busy) {
            if (down & (HidNpadButton_A | HidNpadButton_Right | HidNpadButton_R))
                turn(1);
            else if (down & (HidNpadButton_Left | HidNpadButton_L))
                turn(-1);
            if (down & HidNpadButton_Minus) {
                app.style.theme = (app.style.theme + 1) % 3;
                requestPage(app.page);
                saveState();
            }
            if (down & (HidNpadButton_ZL | HidNpadButton_ZR)) {
                int sign = down & HidNpadButton_ZL ? -1 : 1;
                if (fixedLayout(app.books[app.active].path)) {
                    app.zoom = std::clamp(app.zoom + sign * .25f, 1.f, 3.f);
                    if (app.zoom == 1)
                        app.panX = app.panY = 0;
                } else {
                    app.style.size = std::clamp(app.style.size + sign * 2, 12, 42);
                    requestPage(app.page, true);
                    saveState();
                }
            }
        }
        if (app.zoom > 1) {
            float d = 420 * app.dt;
            app.panX +=
                ((held & HidNpadButton_StickLLeft ? 1 : 0) - (held & HidNpadButton_StickLRight ? 1 : 0)) * d;
            app.panY += ((held & (HidNpadButton_StickLUp | HidNpadButton_Up) ? 1 : 0) -
                         (held & (HidNpadButton_StickLDown | HidNpadButton_Down) ? 1 : 0)) *
                        d;
        }
    } else if (app.screen == Screen::Contents || app.screen == Screen::Bookmarks) {
        bool marks = app.screen == Screen::Bookmarks;
        int count = marks ? app.books[app.active].marks.size() : app.chapters.size();
        move(delta, count);
        if (down & HidNpadButton_B) {
            go(Screen::Reading);
            return;
        }
        if ((down & HidNpadButton_A) && count) {
            if (marks) {
                auto m = app.books[app.active].marks[app.cursor];
                jumpTo(m.page, m.total, m.fraction);
            } else {
                int page = app.chapters[app.cursor].page;
                go(Screen::Reading);
                requestPage(page);
            }
        }
        if (marks && (down & HidNpadButton_X) && count) {
            app.books[app.active].marks.erase(app.books[app.active].marks.begin() + app.cursor);
            move(0, count - 1);
            saveState();
        }
    }
}
void touch(const SDL_Event &event) {
    static std::map<SDL_FingerID, SDL_FPoint> fingers;
    static bool pinching = false;
    static float pinchDistance = 1, pinchZoom = 1;
    static Point pinchMid, pinchPan;
    static bool tracking = false;
    static SDL_FingerID finger = 0;
    static float startX = 0, startY = 0, lastX = 0, lastY = 0;
    static uint64_t since = 0;
    if (event.type != SDL_FINGERDOWN && event.type != SDL_FINGERUP && event.type != SDL_FINGERMOTION)
        return;
    float x = event.tfinger.x * 1280, y = event.tfinger.y * 720;
    if (app.style.portrait && app.screen == Screen::Reading) {
        float ox = x;
        x = y;
        y = 1280 - ox;
    }
    if (event.type == SDL_FINGERUP)
        fingers.erase(event.tfinger.fingerId);
    else
        fingers[event.tfinger.fingerId] = {x, y};
    if (fingers.size() >= 2 && app.screen == Screen::Reading && app.active >= 0 &&
        format(app.books[app.active].path) != "epub") {
        auto first = fingers.begin(), second = std::next(first);
        float dx = first->second.x - second->second.x, dy = first->second.y - second->second.y;
        float distance = std::max(1.f, std::sqrt(dx * dx + dy * dy));
        Point midpoint{(first->second.x + second->second.x) / 2, (first->second.y + second->second.y) / 2};
        if (!pinching) {
            pinchMid = midpoint;
            pinchPan = {app.panX, app.panY};
            pinchDistance = distance;
            pinchZoom = app.zoom;
            pinching = true;
        }
        app.zoom = std::clamp(pinchZoom * distance / pinchDistance, 1.f, 3.f);
        float width = app.style.portrait ? 720.f : 1280.f, height = app.style.portrait ? 1280.f : 720.f;
        auto pan = anchoredPan(pinchMid, midpoint, pinchPan, pinchZoom, app.zoom, width, height);
        float fit = std::min(width / std::max(1, app.pageWidth), height / std::max(1, app.pageHeight));
        float maxX = std::max(0.f, (app.pageWidth * fit * app.zoom - width) / 2);
        float maxY = std::max(0.f, (app.pageHeight * fit * app.zoom - height) / 2);
        app.panX = std::clamp(pan.x, -maxX, maxX);
        app.panY = std::clamp(pan.y, -maxY, maxY);
        tracking = false;
        return;
    }
    if (pinching) {
        if (fingers.empty())
            pinching = false;
        tracking = false;
        return;
    }
    if (event.type == SDL_FINGERDOWN) {
        if (tracking)
            return;
        tracking = true;
        finger = event.tfinger.fingerId;
        startX = lastX = x;
        startY = lastY = y;
        since = SDL_GetTicks64();
        return;
    }
    if (!tracking || event.tfinger.fingerId != finger)
        return;
    if (event.type == SDL_FINGERMOTION) {
        if (app.screen == Screen::Reading && app.zoom > 1) {
            app.panX += x - lastX;
            app.panY += y - lastY;
        }
        lastX = x;
        lastY = y;
        return;
    }
    tracking = false;
    float dx = x - startX, dy = y - startY;
    if (app.screen == Screen::Reading) {
        if (app.zoom > 1)
            return;
        if (std::abs(dx) > 80 && std::abs(dx) > std::abs(dy)) {
            turn(dx < 0 ? 1 : -1);
            return;
        }
        if (std::abs(dx) < 24 && std::abs(dy) < 24 && SDL_GetTicks64() - since < 600) {
            float w = app.style.portrait ? 720 : 1280;
            if (x < w * .25f)
                turn(-1);
            else if (x > w * .75f)
                turn(1);
            else
                app.hud = !app.hud;
        }
    } else if (app.screen == Screen::Download) {
        if (std::abs(dx) < 24 && std::abs(dy) < 24 && y >= 540 && y < 610) {
            if (x >= 80 && x < 420)
                input(HidNpadButton_A, 0);
            else if (x >= 450 && x < 790)
                input(HidNpadButton_X, 0);
            else if (x >= 820 && x < 1200)
                input(HidNpadButton_B, 0);
        }
    } else if (app.screen == Screen::Library) {
        if (std::abs(dx) < 24 && std::abs(dy) < 24 && x >= 820 && x < 1040 && y >= 20 && y < 70) {
            app.libraryCursor = app.cursor;
            openDownloads();
            return;
        }
        if (std::abs(dx) < 24 && std::abs(dy) < 24 && x >= 856 && x < 1240 && y >= 120 && y < 640 &&
            !app.visible.empty()) {
            openBook(app.visible[app.cursor]);
            return;
        }
        if (std::abs(dx) > 60 || std::abs(dy) > 60) {
            move((std::abs(dx) > std::abs(dy) ? dx < 0 : dy < 0) ? libraryCapacity : -libraryCapacity,
                 app.visible.size());
            return;
        }
        if (std::abs(dx) < 24 && std::abs(dy) < 24 && x >= libraryX &&
            x < libraryX + libraryColumns * libraryPitchX && y >= libraryY &&
            y < libraryY + libraryRows * libraryPitchY) {
            int gx = (int)(x - libraryX), gy = (int)(y - libraryY);
            if (gx % libraryPitchX >= libraryCardW || gy % libraryPitchY >= libraryCardH)
                return;
            int row = libraryFirst(app.cursor) + gy / libraryPitchY * libraryColumns + gx / libraryPitchX;
            if (row < (int)app.visible.size()) {
                if (row == app.cursor)
                    openBook(app.visible[row]);
                else {
                    app.cursor = row;
                    app.coverAt = SDL_GetTicks64() + 150;
                }
            }
        }
    }
}
} // namespace rr
