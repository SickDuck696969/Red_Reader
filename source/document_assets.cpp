#include "document_assets.hpp"
#include "model.hpp"
#include <algorithm>
#include <archive.h>
#include <archive_entry.h>
#include <cstdio>
#include <cstring>
#include <expat.h>
#include <map>
namespace rr {
namespace {
constexpr size_t limit = 32 * 1024 * 1024;
struct Archive {
    archive *a = archive_read_new();
    Archive(const std::string &path) {
        if (!a)
            return;
        archive_read_support_format_zip(a);
        archive_read_support_format_rar(a);
        archive_read_support_format_rar5(a);
        if (archive_read_open_filename(a, path.c_str(), 65536) != ARCHIVE_OK) {
            archive_read_free(a);
            a = nullptr;
        }
    }
    ~Archive() {
        if (a)
            archive_read_free(a);
    }
};
bool imageName(const std::string &s) {
    auto f = format(s);
    return f == "jpg" || f == "jpeg" || f == "png" || f == "webp" || f == "bmp" || f == "tif" || f == "tiff";
}
bool naturalLess(const std::string &a, const std::string &b) {
    auto x = lower(a), y = lower(b);
    size_t i = 0, j = 0;
    while (i < x.size() && j < y.size()) {
        if (x[i] >= '0' && x[i] <= '9' && y[j] >= '0' && y[j] <= '9') {
            size_t ie = i, je = j;
            while (ie < x.size() && x[ie] >= '0' && x[ie] <= '9')
                ++ie;
            while (je < y.size() && y[je] >= '0' && y[je] <= '9')
                ++je;
            size_t iz = i, jz = j;
            while (iz < ie && x[iz] == '0')
                ++iz;
            while (jz < je && y[jz] == '0')
                ++jz;
            if (ie - iz != je - jz)
                return ie - iz < je - jz;
            int c = x.compare(iz, ie - iz, y, jz, je - jz);
            if (c)
                return c < 0;
            i = ie;
            j = je;
        } else {
            if (x[i] != y[j])
                return x[i] < y[j];
            ++i;
            ++j;
        }
    }
    if (i != x.size() || j != y.size())
        return i == x.size();
    return a < b;
}
struct Node {
    std::string tag;
    std::map<std::string, std::string> at;
};
struct Xml {
    std::vector<Node> nodes;
    XML_Parser parser = nullptr;
};
std::string local(const char *s) {
    std::string v = s;
    return v.substr(v.find_last_of(':') + 1);
}
void XMLCALL begin(void *user, const char *tag, const char **at) {
    auto &x = *static_cast<Xml *>(user);
    if (x.nodes.size() >= 20000) {
        XML_StopParser(x.parser, XML_FALSE);
        return;
    }
    Node n;
    n.tag = local(tag);
    for (int i = 0; at[i]; i += 2)
        n.at[local(at[i])] = at[i + 1];
    x.nodes.push_back(std::move(n));
}
void XMLCALL entity(void *user, const char *, int, const char *, int, const char *, const char *,
                    const char *, const char *) {
    auto &x = *static_cast<Xml *>(user);
    XML_StopParser(x.parser, XML_FALSE);
}
std::vector<Node> parse(const AssetBytes &b) {
    Xml x;
    if (b.data.empty() || b.data.size() > 2 * 1024 * 1024)
        return {};
    x.parser = XML_ParserCreate(nullptr);
    if (!x.parser)
        return {};
    XML_SetUserData(x.parser, &x);
    XML_SetStartElementHandler(x.parser, begin);
    XML_SetEntityDeclHandler(x.parser, entity);
    bool ok = XML_Parse(x.parser, reinterpret_cast<const char *>(b.data.data()), b.data.size(), XML_TRUE) ==
              XML_STATUS_OK;
    XML_ParserFree(x.parser);
    return ok ? std::move(x.nodes) : std::vector<Node>{};
}
std::string resolve(const std::string &base, std::string ref) {
    ref = ref.substr(0, ref.find_first_of("?#"));
    for (size_t i = 0; i + 2 < ref.size(); ++i)
        if (ref[i] == '%') {
            auto hex = [](char c) {
                if (c >= '0' && c <= '9')
                    return c - '0';
                c = lower(std::string(1, c))[0];
                return c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
            };
            int a = hex(ref[i + 1]), b = hex(ref[i + 2]);
            if (a >= 0 && b >= 0) {
                ref.replace(i, 3, 1, char(a * 16 + b));
            }
        }
    if (ref.empty() || ref.find(':') != std::string::npos || ref[0] == '/' ||
        ref.find('\0') != std::string::npos)
        return {};
    auto slash = base.find_last_of('/');
    std::string full = (slash == std::string::npos ? "" : base.substr(0, slash + 1)) + ref;
    std::vector<std::string> parts;
    size_t pos = 0;
    while (pos <= full.size()) {
        auto end = full.find('/', pos);
        if (end == std::string::npos)
            end = full.size();
        auto p = full.substr(pos, end - pos);
        if (p == "..") {
            if (parts.empty())
                return {};
            parts.pop_back();
        } else if (!p.empty() && p != ".")
            parts.push_back(p);
        pos = end + 1;
    }
    std::string out;
    for (auto &p : parts) {
        if (!out.empty())
            out += '/';
        out += p;
    }
    return out;
}
} // namespace
AssetBytes archiveImage(const std::string &path, const std::string &name) {
    AssetBytes out;
    out.name = name;
    Archive ar(path);
    if (!ar.a) {
        out.error = "Could not open archive";
        return out;
    }
    archive_entry *entry = nullptr;
    int status = 0, count = 0;
    while ((status = archive_read_next_header(ar.a, &entry)) == ARCHIVE_OK && ++count <= 20000) {
        const char *entryName = archive_entry_pathname(entry);
        if (!entryName || name != entryName) {
            archive_read_data_skip(ar.a);
            continue;
        }
        if (archive_entry_filetype(entry) != AE_IFREG || archive_entry_is_encrypted(entry) > 0 ||
            archive_entry_size(entry) > static_cast<int64_t>(limit)) {
            out.error = "Unsupported or oversized archive image";
            return out;
        }
        unsigned char block[65536];
        la_ssize_t n;
        while ((n = archive_read_data(ar.a, block, sizeof(block))) > 0) {
            if (out.data.size() + n > limit) {
                out.data.clear();
                out.error = "Archive image exceeds 32 MiB";
                return out;
            }
            out.data.insert(out.data.end(), block, block + n);
        }
        if (n < 0) {
            out.data.clear();
            out.error = "Could not decode archive image";
        }
        return out;
    }
    out.error = "Archive image not found";
    return out;
}
std::vector<std::string> comicPages(const std::string &path, std::string &error) {
    Archive ar(path);
    std::vector<std::string> pages;
    if (!ar.a) {
        error = "Could not open comic archive";
        return pages;
    }
    archive_entry *entry = nullptr;
    int status = 0, count = 0;
    while ((status = archive_read_next_header(ar.a, &entry)) == ARCHIVE_OK) {
        if (++count > 20000) {
            error = "Comic contains too many entries";
            return {};
        }
        const char *name = archive_entry_pathname(entry);
        if (name && archive_entry_filetype(entry) == AE_IFREG && imageName(name) &&
            std::string(name).find("__MACOSX/") == std::string::npos && filename(name).rfind("._", 0) != 0)
            pages.emplace_back(name);
        archive_read_data_skip(ar.a);
    }
    if (status != ARCHIVE_EOF) {
        error = "Could not read comic archive (encrypted or unsupported RAR)";
        return {};
    }
    std::sort(pages.begin(), pages.end(), naturalLess);
    if (pages.empty())
        error = "No supported images in comic";
    return pages;
}
AssetBytes epubCover(const std::string &path) {
    auto container = parse(archiveImage(path, "META-INF/container.xml"));
    std::string opf;
    for (auto &n : container)
        if (n.tag == "rootfile") {
            opf = n.at["full-path"];
            break;
        }
    auto nodes = parse(archiveImage(path, opf));
    std::string id;
    for (auto &n : nodes)
        if (n.tag == "meta" && n.at["name"] == "cover")
            id = n.at["content"];
    std::vector<std::string> candidates, wrappers;
    for (auto &n : nodes)
        if (n.tag == "item") {
            auto properties = " " + n.at["properties"] + " ";
            for (char &c : properties)
                if (c == '\t' || c == '\r' || c == '\n')
                    c = ' ';
            if (properties.find(" cover-image ") != std::string::npos || (!id.empty() && n.at["id"] == id))
                candidates.push_back(resolve(opf, n.at["href"]));
        }
    for (auto &n : nodes) {
        if (n.tag == "reference" && n.at["type"] == "cover")
            wrappers.push_back(resolve(opf, n.at["href"]));
        if (n.tag == "item" && lower(n.at["href"]).find("cover") != std::string::npos) {
            auto p = resolve(opf, n.at["href"]);
            if (imageName(p))
                candidates.push_back(p);
            else
                wrappers.push_back(p);
        }
    }
    auto pick = [&]() -> AssetBytes {
        for (auto &p : candidates)
            if (!p.empty()) {
                auto b = archiveImage(path, p);
                if (!b.data.empty())
                    return b;
            }
        candidates.clear();
        return {};
    };
    auto found = pick();
    if (!found.data.empty())
        return found;
    // The first spine document may be a cover wrapper without any cover metadata.
    for (auto &n : nodes)
        if (n.tag == "itemref") {
            for (auto &item : nodes)
                if (item.tag == "item" && item.at["id"] == n.at["idref"])
                    wrappers.push_back(resolve(opf, item.at["href"]));
            break;
        }
    for (auto &wrapper : wrappers) {
        if (imageName(wrapper)) {
            candidates.push_back(wrapper);
            continue;
        }
        for (auto &n : parse(archiveImage(path, wrapper))) {
            if (n.tag == "img")
                candidates.push_back(resolve(wrapper, n.at["src"]));
            if (n.tag == "image")
                candidates.push_back(resolve(wrapper, n.at["href"]));
        }
    }
    found = pick();
    if (!found.data.empty())
        return found;
    // Legacy producers sometimes omit cover metadata entirely.
    std::string error;
    auto images = comicPages(path, error);
    for (auto &p : images)
        if (lower(filename(p)).find("cover") != std::string::npos)
            candidates.push_back(p);
    return pick();
}
bool validBookFile(const std::string &path, const std::string &kind, std::string &error) {
    FILE *file = fopen(path.c_str(), "rb");
    if (!file) {
        error = "Cannot read downloaded file";
        return false;
    }
    unsigned char header[8]{};
    size_t size = fread(header, 1, sizeof(header), file);
    fclose(file);
    bool zip = size >= 4 && memcmp(header, "PK\003\004", 4) == 0;
    bool rar = size >= 7 && memcmp(header, "Rar!\032\007", 6) == 0;
    bool pdf = size >= 5 && memcmp(header, "%PDF-", 5) == 0;
    bool valid = false;
    if (kind == "pdf")
        valid = pdf;
    if (kind == "epub" && zip)
        valid = !archiveImage(path, "META-INF/container.xml").data.empty();
    if ((kind == "cbz" && zip) || (kind == "cbr" && rar))
        valid = !comicPages(path, error).empty();
    if (!valid && error.empty())
        error = "The link did not return a matching book file. Use a direct download URL.";
    return valid;
}
} // namespace rr
