#include "model.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
using namespace rr;
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            std::cerr << "FAIL " << __LINE__ << ": " << #x << '\n';                                          \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
int main() {
    for (auto path : {"a.fb2", "a.HTML", "a.htm", "a.xhtml", "a.xps", "a.oxps"})
        CHECK(supported(path));
    CHECK(!fixedLayout("a.fb2") && !fixedLayout("a.html") && fixedLayout("a.xps"));
    auto pan = anchoredPan({800, 450}, {800, 450}, {0, 0}, 1, 2, 1280, 720);
    CHECK(pan.x == -160 && pan.y == -90); // touched point stays in the same place
    pan = anchoredPan({800, 450}, {830, 470}, {20, -10}, 2, 3, 1280, 720);
    CHECK(pan.x == -20 && pan.y == -40); // combines zoom and two-finger pan

    CHECK(validDownloadUrl("https://example.org/book.epub?token=abc"));
    CHECK(validDownloadUrl("http://192.168.1.2:8000/book.pdf"));
    CHECK(!validDownloadUrl("file:///etc/passwd") && !validDownloadUrl("https:///book.epub"));
    CHECK(!validDownloadUrl("https://user@host/book.epub") && !validDownloadUrl("https://host/a b"));
    CHECK(validDownloadName("My Book.CBR") && !validDownloadName("../book.epub"));
    CHECK(!validDownloadName("book.exe") && !validDownloadName("dir/book.epub"));
    CHECK(downloadName("https://host/book.pdf?token=1") == "book.pdf");
    CHECK(downloadName("https://host/download?id=1") == "Book.epub");
    auto portrait = coverCrop(600, 800);
    CHECK(portrait.x == 0 && portrait.y == 0 && portrait.width == 600 && portrait.height == 800);
    auto landscape = coverCrop(1200, 600);
    CHECK(landscape.x == 375 && landscape.y == 0 && landscape.width == 450 && landscape.height == 600);
    auto narrow = coverCrop(300, 1200);
    CHECK(narrow.x == 0 && narrow.y == 400 && narrow.width == 300 && narrow.height == 400);
    auto square = coverCrop(400, 400);
    CHECK(square.x == 50 && square.width == 300 && square.height == 400);
    CHECK(coverCrop(0, 400).width == 0);
    CHECK(coverCrop(3, 4).height == 4);
    CHECK(supported("Book.EPUB") && supported("a/b.CBZ") && supported("Comic.CBR") && !supported("a.txt"));
    CHECK(fixedLayout("book.pdf") && !fixedLayout("book.epub"));
    CHECK(resolvePage(30, 100, .3, 200) == 60);
    CHECK(resolvePage(30, 100, .3, 100) == 30);
    CHECK(resolvePage(500, 0, 0, 10) == 9);
    CHECK(resolvePage(-1, 0, 0, 10) == 0);
    CHECK(resolvePage(30, 100, std::numeric_limits<double>::quiet_NaN(), 200) == 0);
    std::vector<Book> books(3);
    books[0].title = "Alpha";
    books[1].title = "Café";
    books[2].title = "Beta";
    books[2].favorite = true;
    books[2].opened = 100;
    CHECK(filtered(books, "ALP", false, 0) == std::vector<int>{0});
    CHECK(filtered(books, "", true, 0) == std::vector<int>{2});
    CHECK(filtered(books, "", false, 2)[0] == 2);
    Style s;
    auto key = styleKey(s);
    s.theme = 2;
    CHECK(styleKey(s) == key);
    s.font = 2;
    CHECK(styleKey(s) != key);
    s.font = 0;
    s.margin = 0;
    CHECK(readingCss(s).find("margin: 0px") != std::string::npos);
    const char *alignments[] = {"", "left", "center", "right", "justify"};
    for (int a = 1; a < alignmentCount; ++a) {
        auto before = styleKey(s);
        s.alignment = a;
        CHECK(styleKey(s) != before);
        CHECK(readingCss(s).find(std::string("text-align: ") + alignments[a] + " !important") !=
              std::string::npos);
    }
    s.alignment = 0;
    CHECK(readingCss(s).find("text-align") == std::string::npos);
    for (int i = 0; i < fontCount; ++i) {
        s.font = i;
        auto css = readingCss(s, std::filesystem::absolute("romfs/fonts").generic_string() + "/");
        CHECK((css.find("@font-face") != std::string::npos) == (i != 0));
        if (i)
            CHECK(css.find("font-family: Reader !important") != std::string::npos);
        size_t pos = 0;
        while ((pos = css.find("url('", pos)) != std::string::npos) {
            auto end = css.find("')", pos);
            CHECK(std::filesystem::exists(css.substr(pos + 5, end - pos - 5)));
            pos = end + 2;
        }
        std::ofstream("build/reading-" + std::to_string(i) + ".css") << css;
    }
    std::cout << "PASS: supported formats, page restoration, filtering, sorting, reflow cache keys, bundled "
                 "font CSS\n";
}
