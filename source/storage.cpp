#include "app.hpp"
#define JSON_NOEXCEPTION 1
#include "json.hpp"
#include <algorithm>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <sys/stat.h>
namespace rr {
App app;
namespace {
const std::string statePath = "sdmc:/lib/save/reader.json";
nlohmann::json savedBooks = nlohmann::json::object();
int integer(const nlohmann::json &j, const char *k, int def, int lo, int hi) {
    auto it = j.find(k);
    return it != j.end() && it->is_number_integer() ? std::clamp(it->get<int>(), lo, hi) : def;
}
void walk(const std::string &path, int depth) {
    if (depth > 8 || app.books.size() >= 10000)
        return;
    DIR *d = opendir(path.c_str());
    if (!d)
        return;
    while (auto *e = readdir(d)) {
        std::string n = e->d_name;
        if (n == "." || n == ".." || n == "save")
            continue;
        std::string p = path + "/" + n;
        struct stat st{};
        if (lstat(p.c_str(), &st) != 0 || S_ISLNK(st.st_mode))
            continue;
        if (S_ISDIR(st.st_mode)) {
            walk(p, depth + 1);
            continue;
        }
        if (!S_ISREG(st.st_mode) || !supported(n))
            continue;
        Book b;
        b.path = p;
        b.title = n.substr(0, n.find_last_of('.'));
        auto j = savedBooks.find(p);
        if (j != savedBooks.end() && j->is_object()) {
            b.page = integer(*j, "page", 0, 0, 10000000);
            b.total = integer(*j, "total", 0, 0, 10000000);
            if (j->contains("fraction") && (*j)["fraction"].is_number())
                b.fraction = std::clamp((*j)["fraction"].get<double>(), 0., 1.);
            if (j->contains("opened") && (*j)["opened"].is_number_unsigned())
                b.opened = (*j)["opened"].get<uint64_t>();
            b.favorite =
                j->contains("favorite") && (*j)["favorite"].is_boolean() && (*j)["favorite"].get<bool>();
            if (j->contains("marks") && (*j)["marks"].is_array())
                for (const auto &mark : (*j)["marks"])
                    if (mark.is_object() && b.marks.size() < 100) {
                        int page = integer(mark, "page", 0, 0, 10000000),
                            total = integer(mark, "total", 0, 0, 10000000);
                        double f = mark.contains("fraction") && mark["fraction"].is_number()
                                       ? mark["fraction"].get<double>()
                                       : 0;
                        b.marks.push_back({page, total, std::clamp(f, 0., 1.)});
                    }
        } else {
            std::ifstream legacy("sdmc:/lib/save/" + n + ".save");
            legacy >> b.page;
            b.page = std::max(0, b.page);
        }
        app.books.push_back(std::move(b));
        if (app.books.size() >= 10000)
            break;
    }
    closedir(d);
}
} // namespace
void notify(const std::string &s) {
    app.notice = s;
    app.noticeUntil = SDL_GetTicks64() + 4000;
}
void go(Screen s) {
    app.screen = s;
    app.cursor = app.offset = 0;
    app.focus = 0;
    app.transition = SDL_GetTicks64();
}
void rebuild() {
    app.visible = filtered(app.books, app.query, app.favorites, app.sort);
    app.cursor = std::clamp(app.cursor, 0, std::max(0, (int)app.visible.size() - 1));
    app.offset = 0;
    app.coverAt = SDL_GetTicks64() + 150;
}
void loadState() {
    mkdir("sdmc:/lib", 0777);
    mkdir("sdmc:/lib/save", 0777);
    std::ifstream f(statePath);
    auto j = nlohmann::json::parse(f, nullptr, false);
    if (!j.is_object()) {
        std::ifstream backup(statePath + ".bak");
        j = nlohmann::json::parse(backup, nullptr, false);
    }
    if (j.is_object()) {
        app.style.font = integer(j, "font", 0, 0, 3);
        app.style.size = integer(j, "size", 22, 12, 42);
        app.style.spacing = integer(j, "spacing", 14, 10, 22);
        app.style.margin = integer(j, "margin", 32, 8, 88);
        app.style.theme = integer(j, "theme", 0, 0, 2);
        app.sort = integer(j, "sort", 0, 0, 2);
        if (j.contains("portrait") && j["portrait"].is_boolean())
            app.style.portrait = j["portrait"].get<bool>();
        if (j.contains("motion") && j["motion"].is_boolean())
            app.style.motion = j["motion"].get<bool>();
        if (j.contains("last") && j["last"].is_string())
            app.lastRead = j["last"].get<std::string>();
        if (j.contains("books") && j["books"].is_object())
            savedBooks = j["books"];
    } else {
        std::ifstream old("sdmc:/lib/global_setting.dat");
        float size = 22;
        int portrait = 0, dark = 0, bg = 0, asc = 1;
        if (old >> size >> portrait >> dark >> bg >> asc) {
            app.style.size = std::clamp((int)size, 12, 42);
            app.style.portrait = portrait != 0;
            app.style.theme = dark ? 2 : 0;
            old.ignore(1024, '\n');
            std::getline(old, app.lastRead);
        }
    }
}
void scanLibrary() {
    app.books.clear();
    walk("sdmc:/lib", 0);
    rebuild();
}
void saveReading() {
    if (app.active < 0 || app.total <= 0)
        return;
    auto &b = app.books[app.active];
    b.page = app.page;
    b.total = app.total;
    b.fraction = app.total > 1 ? (double)app.page / (app.total - 1) : 0;
    b.opened = (uint64_t)time(nullptr);
    app.lastRead = b.path;
    app.saveAt = SDL_GetTicks64() + 1000;
}
void saveState() {
    for (const auto &b : app.books) {
        nlohmann::json marks = nlohmann::json::array();
        for (auto m : b.marks)
            marks.push_back({{"page", m.page}, {"total", m.total}, {"fraction", m.fraction}});
        savedBooks[b.path] = {{"page", b.page},     {"total", b.total},       {"fraction", b.fraction},
                              {"opened", b.opened}, {"favorite", b.favorite}, {"marks", marks}};
    }
    nlohmann::json j = {{"font", app.style.font},       {"size", app.style.size},
                        {"spacing", app.style.spacing}, {"margin", app.style.margin},
                        {"theme", app.style.theme},     {"portrait", app.style.portrait},
                        {"motion", app.style.motion},   {"sort", app.sort},
                        {"last", app.lastRead},         {"books", savedBooks}};
    std::ofstream out(statePath + ".tmp");
    out << j.dump(2);
    out.close();
    if (!out) {
        notify("Could not save reading settings");
        return;
    }
    remove((statePath + ".bak").c_str());
    rename(statePath.c_str(), (statePath + ".bak").c_str());
    if (rename((statePath + ".tmp").c_str(), statePath.c_str()) != 0) {
        rename((statePath + ".bak").c_str(), statePath.c_str());
        notify("Could not save reading settings");
    }
    app.saveAt = 0;
}
std::string keyboard(const char *title, const std::string &initial, int maxLength, bool emptyOnCancel) {
    SwkbdConfig k;
    if (R_FAILED(swkbdCreate(&k, 0)))
        return emptyOnCancel ? "" : initial;
    swkbdConfigMakePresetDefault(&k);
    swkbdConfigSetHeaderText(&k, title);
    swkbdConfigSetInitialText(&k, initial.c_str());
    swkbdConfigSetStringLenMax(&k, std::clamp(maxLength, 1, 2048));
    char buffer[8193]{};
    auto result = swkbdShow(&k, buffer, sizeof(buffer));
    swkbdClose(&k);
    return R_SUCCEEDED(result) ? buffer : emptyOnCancel ? "" : initial;
}
} // namespace rr
