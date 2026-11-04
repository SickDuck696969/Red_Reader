#include "model.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>
namespace rr {
Point anchoredPan(Point startMid, Point currentMid, Point startPan, float startZoom, float zoom, float width,
                  float height) {
    float ratio = zoom / std::max(.01f, startZoom);
    return {currentMid.x - width / 2 - (startMid.x - width / 2 - startPan.x) * ratio,
            currentMid.y - height / 2 - (startMid.y - height / 2 - startPan.y) * ratio};
}
bool validDownloadUrl(const std::string &url) {
    auto prefix = url.rfind("https://", 0) == 0 ? 8u : url.rfind("http://", 0) == 0 ? 7u : 0u;
    if (!prefix || url.size() > 2048)
        return false;
    auto end = url.find_first_of("/?#", prefix);
    auto host = url.substr(prefix, end == std::string::npos ? end : end - prefix);
    if (host.empty() || host.find('@') != std::string::npos)
        return false;
    for (unsigned char c : url)
        if (c <= 32 || c == 127 || c == '\\')
            return false;
    return true;
}
bool validDownloadName(const std::string &name) {
    if (name.empty() || name.size() > 160 || name[0] == '.' || name.back() == ' ' || !supported(name))
        return false;
    for (unsigned char c : name)
        if (c < 32 || c == 127 || std::string("/\\:*?\"<>|").find(c) != std::string::npos)
            return false;
    return true;
}
std::string downloadName(const std::string &url) {
    auto path = url.substr(0, url.find_first_of("?#"));
    auto name = filename(path);
    return validDownloadName(name) ? name : "Book.epub";
}
CoverCrop coverCrop(int width, int height) {
    if (width <= 0 || height <= 0)
        return {};
    // All library covers use a 3:4 frame; crop, never stretch the source.
    int cropWidth = width, cropHeight = height;
    if ((int64_t)width * 4 > (int64_t)height * 3)
        cropWidth = std::max(1, height * 3 / 4);
    else
        cropHeight = std::max(1, width * 4 / 3);
    return {(width - cropWidth) / 2, (height - cropHeight) / 2, cropWidth, cropHeight};
}
std::string readingCss(const Style &s, const std::string &fontRoot) {
    std::string css = "body { margin: " + std::to_string(s.margin) +
                      "px !important; } body, p, li { line-height: " + std::to_string(s.spacing / 10.f) +
                      " !important; }";
    if (s.alignment > 0 && s.alignment < alignmentCount) {
        const char *align[] = {"", "left", "center", "right", "justify"};
        css += "body, p, li, blockquote { text-align: " + std::string(align[s.alignment]) + " !important; }";
        if (s.alignment == 2 || s.alignment == 3)
            css += "p { text-indent: 0 !important; }";
    }
    if (!s.font)
        return css;
    for (int bold = 0; bold < 2; ++bold)
        for (int italic = 0; italic < 2; ++italic) {
            std::string face = bold ? "Bold" : "Regular";
            if (italic && s.font != 3)
                face = bold ? "BoldItalic" : "Italic";
            if (s.font == 5 && bold && italic)
                face = "Bold";
            css += "@font-face { font-family: Reader; font-weight: " + std::string(bold ? "bold" : "normal") +
                   "; font-style: " + (italic ? "italic" : "normal") + "; src: url('" + fontRoot +
                   fontFamilies[s.font] + "-" + face + ".ttf'); }";
        }
    css += "body, p, div, span, li, h1, h2, h3, h4, blockquote { font-family: Reader !important; }";
    return css;
}
std::string lower(std::string v) {
    for (char &c : v)
        if (c >= 'A' && c <= 'Z')
            c += 32;
    return v;
}
std::string filename(const std::string &p) {
    return p.substr(p.find_last_of("/\\") + 1);
}
std::string format(const std::string &p) {
    auto n = filename(p);
    auto i = n.find_last_of('.');
    return i == std::string::npos ? "" : lower(n.substr(i + 1));
}
bool supported(const std::string &p) {
    auto f = format(p);
    return f == "epub" || f == "pdf" || f == "cbz" || f == "cbr" || f == "fb2" || f == "html" || f == "htm" ||
           f == "xhtml" || f == "xps" || f == "oxps";
}
bool fixedLayout(const std::string &p) {
    auto f = format(p);
    return f != "epub" && f != "fb2" && f != "html" && f != "htm" && f != "xhtml";
}
std::string styleKey(const Style &s) {
    return std::to_string(s.font) + ":" + std::to_string(s.size) + ":" + std::to_string(s.spacing) + ":" +
           std::to_string(s.margin) + ":" + std::to_string(s.portrait) + ":" + std::to_string(s.alignment);
}
int resolvePage(int page, int savedTotal, double fraction, int total) {
    if (total <= 0)
        return 0;
    if (!std::isfinite(fraction))
        fraction = 0;
    return std::clamp(savedTotal == total || savedTotal == 0
                          ? page
                          : (int)std::lround(std::clamp(fraction, 0., 1.) * std::max(0, total - 1)),
                      0, total - 1);
}
std::vector<int> filtered(const std::vector<Book> &books, const std::string &query, bool favorites,
                          int sort) {
    std::vector<int> result;
    for (size_t i = 0; i < books.size(); ++i)
        if ((!favorites || books[i].favorite) &&
            lower(books[i].title).find(lower(query)) != std::string::npos)
            result.push_back(i);
    std::stable_sort(result.begin(), result.end(), [&](int a, int b) {
        if (sort == 2 && books[a].opened != books[b].opened)
            return books[a].opened > books[b].opened;
        return sort == 1 ? lower(books[a].title) > lower(books[b].title)
                         : lower(books[a].title) < lower(books[b].title);
    });
    return result;
}
} // namespace rr
