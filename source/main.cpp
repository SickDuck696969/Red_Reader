#include "app.hpp"
#include "download.hpp"
#include <algorithm>
using namespace rr;
int main(int, char **) {
    romfsInit();
    if (SDL_Init(SDL_INIT_VIDEO) != 0 || TTF_Init() != 0)
        return 1;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    auto *window = SDL_CreateWindow("Red Reader", 0, 0, 1280, 720, 0);
    app.renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    app.small = TTF_OpenFont("romfs:/fonts/Atkinson-Regular.ttf", 20);
    app.body = TTF_OpenFont("romfs:/fonts/Atkinson-Regular.ttf", 25);
    app.heading = TTF_OpenFont("romfs:/fonts/Atkinson-Bold.ttf", 36);
    if (!app.renderer || !app.small || !app.body || !app.heading)
        return 1;
    app.portraitTarget =
        SDL_CreateTexture(app.renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, 720, 1280);
    if (!app.portraitTarget)
        return 1;
    auto type = appletGetAppletType();
    bool full = type == AppletType_Application || type == AppletType_SystemApplication;
    if (!full)
        go(Screen::ModeRequired);
    else {
        loadState();
        scanLibrary();
        if (!startEngine()) {
            notify("Could not start document renderer");
            app.running = false;
        }
    }
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);
    uint64_t last = SDL_GetTicks64(), repeatAt = 0;
    u64 repeating = 0;
    const u64 directions = HidNpadButton_Up | HidNpadButton_Down | HidNpadButton_Left | HidNpadButton_Right |
                           HidNpadButton_StickLUp | HidNpadButton_StickLDown;
    while (appletMainLoop() && app.running) {
        auto now = SDL_GetTicks64();
        app.dt = std::min(.05f, (now - last) / 1000.f);
        last = now;
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad), held = padGetButtons(&pad);
        if (down & directions) {
            repeating = down & directions;
            repeatAt = now + 360;
        } else if (repeating && (held & repeating) && now >= repeatAt) {
            down |= repeating;
            repeatAt = now + 100;
        } else if (!(held & repeating))
            repeating = 0;
        input(down, held);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT)
                app.running = false;
            else
                touch(event);
        }
        if (full) {
            pollEngine();
            pollDownload();
        }
        render();
    }
    if (full) {
        saveReading();
        saveState();
        stopDownloads();
        stopEngine();
    }
    clearUi();
    TTF_CloseFont(app.small);
    TTF_CloseFont(app.body);
    TTF_CloseFont(app.heading);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    romfsExit();
    return 0;
}
