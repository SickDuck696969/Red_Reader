#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace rr {
constexpr int libraryColumns = 5, libraryRows = 2, libraryCapacity = libraryColumns * libraryRows;
constexpr int libraryX = 40, libraryY = 156, libraryCardW = 132, libraryCardH = 176;
constexpr int libraryPitchX = 156, libraryPitchY = 222;
inline int libraryFirst(int cursor) {
    return cursor < 0 ? 0 : cursor / libraryCapacity * libraryCapacity;
}
struct Point {
    float x = 0, y = 0;
};
Point anchoredPan(Point startMid, Point currentMid, Point startPan, float startZoom, float zoom,
                  float viewportWidth, float viewportHeight);
struct Style {
    int font = 0, size = 22, spacing = 14, margin = 16, theme = 0, alignment = 0;
    bool portrait = false, motion = true;
};
struct Mark {
    int page = 0, total = 0;
    double fraction = 0;
};
struct Book {
    std::string path, title;
    int page = 0, total = 0;
    double fraction = 0;
    uint64_t opened = 0;
    bool favorite = false;
    std::vector<Mark> marks;
};
bool validDownloadUrl(const std::string &url);
bool validDownloadName(const std::string &name);
std::string downloadName(const std::string &url);
std::string lower(std::string value);
bool supported(const std::string &name);
bool fixedLayout(const std::string &name);
struct CoverCrop {
    int x = 0, y = 0, width = 0, height = 0;
};
CoverCrop coverCrop(int sourceWidth, int sourceHeight);
std::string filename(const std::string &path);
std::string format(const std::string &path);
std::string styleKey(const Style &style);
std::string readingCss(const Style &style, const std::string &fontRoot = "romfs:/fonts/");
int resolvePage(int savedPage, int savedTotal, double fraction, int total);
std::vector<int> filtered(const std::vector<Book> &books, const std::string &query, bool favorites, int sort);
constexpr const char *fontNames[] = {
    "Publisher", "Atkinson Hyperlegible", "Crimson Text", "Lusitana", "Spectral",
    "Cardo",     "Gentium Book Plus"};
constexpr const char *fontFamilies[] = {"CrimsonText", "Atkinson", "CrimsonText",    "Lusitana",
                                        "Spectral",    "Cardo",    "GentiumBookPlus"};
constexpr const char *fontSuggestions[] = {"Book's original typography",  "Clear sans-serif",
                                           "Classics / literary fiction", "Romance / literary fiction",
                                           "Contemporary / mystery",      "Fantasy / historical fiction",
                                           "Long novels / classics"};
constexpr int fontCount = sizeof(fontNames) / sizeof(fontNames[0]);
constexpr const char *alignmentNames[] = {"Publisher", "Left", "Center", "Right", "Justify"};
constexpr int alignmentCount = 5, settingsCount = 13;
constexpr const char *themeNames[] = {"Paper", "Sepia", "Night"};
} // namespace rr
