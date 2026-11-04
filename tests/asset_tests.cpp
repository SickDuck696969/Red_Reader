#include "document_assets.hpp"
#include <fstream>
#include <iostream>
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            std::cerr << "FAIL " << __LINE__ << ": " << #x << "\n";                                          \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
int main(int argc, char **argv) {
    using namespace rr;
    std::string validationError;
    CHECK(validBookFile("build/asset-fixtures/sample.fb2", "fb2", validationError));
    CHECK(validBookFile("build/asset-fixtures/sample.html", "html", validationError));
    CHECK(validBookFile("build/asset-fixtures/sample.xps", "xps", validationError));
    CHECK(!validBookFile("build/asset-fixtures/sample.html", "fb2", validationError));
    validationError.clear();
    CHECK(validBookFile("build/asset-fixtures/epub2.epub", "epub", validationError));
    CHECK(validBookFile("build/asset-fixtures/comic.cbz", "cbz", validationError));
    CHECK(validBookFile("build/asset-fixtures/comic5.cbr", "cbr", validationError));
    CHECK(!validBookFile("build/asset-fixtures/comic.cbz", "epub", validationError));
    validationError.clear();
    CHECK(!validBookFile("build/asset-fixtures/epub2.epub", "pdf", validationError));
    for (auto name : {"epub2", "epub3", "guide", "spine"}) {
        auto b = epubCover(std::string("build/asset-fixtures/") + name + ".epub");
        CHECK(b.name == "OPS/Images/art one.png");
        CHECK(b.data.size() > 8 && b.data[0] == 137);
    }
    CHECK(epubCover("build/asset-fixtures/unsafe.epub").data.empty());
    CHECK(epubCover("missing.epub").data.empty());
    for (auto name : {"comic.cbz", "comic5.cbr"}) {
        std::string error, path = std::string("build/asset-fixtures/") + name;
        auto pages = comicPages(path, error);
        CHECK(error.empty());
        CHECK(pages.size() == 3);
        CHECK(pages[0] == "1.png" && pages[1] == "2.png" && pages[2] == "10.png");
        for (auto &p : pages) {
            auto b = archiveImage(path, p);
            CHECK(b.error.empty());
            CHECK(b.data.size() > 8 && b.data[0] == 137);
        }
        CHECK(!archiveImage(path, "missing.png").error.empty());
    }
    if (argc > 1) {
        auto b = epubCover(argv[1]);
        CHECK(b.name == "cover.jpg");
        CHECK(b.data.size() > 1000);
        CHECK(b.data[0] == 255 && b.data[1] == 216);
        std::cout << "User EPUB: declared cover.jpg found, " << b.data.size() << " bytes\n";
    }
    std::cout << "EPUB metadata/wrapper/path and ZIP/RAR5 ordering/decoding tests passed\n";
}
