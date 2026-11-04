#include "app.hpp"
#include "document_assets.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mupdf/fitz.h>
namespace rr {
namespace {
Thread worker;
Mutex lock;
std::atomic<bool> stopping{false};
bool started = false, pending = false, ready = false;
RenderRequest request;
RenderResult result;
fz_context *context = nullptr;
fz_document *document = nullptr;
std::string documentPath, layoutKey;
int total = 0;
std::vector<Chapter> chapters;
std::map<int, Image> cache;
void outline(fz_outline *node, int depth) {
    if (depth > 12)
        return;
    for (; node && chapters.size() < 2000; node = node->next) {
        int page = node->page;
        fz_var(page);
        if (page < 0 && node->uri) {
            fz_try(context) {
                auto location = fz_resolve_link(context, document, node->uri, nullptr, nullptr);
                if (location.chapter >= 0 && location.page >= 0)
                    page = fz_page_number_from_location(context, document, location);
            }
            fz_catch(context) {
                page = -1;
            }
        }
        if (node->title && page >= 0 && page < total)
            chapters.push_back({node->title, page, depth});
        if (node->down)
            outline(node->down, depth + 1);
    }
}
Image raster(fz_document *doc, int page, int width, int height, bool sharp) {
    Image image;
    fz_page *loaded = nullptr;
    fz_pixmap *pix = nullptr;
    fz_var(loaded);
    fz_var(pix);
    fz_try(context) {
        loaded = fz_load_page(context, doc, page);
        fz_rect bounds = fz_bound_page(context, loaded);
        float w = std::max(1.f, bounds.x1 - bounds.x0), h = std::max(1.f, bounds.y1 - bounds.y0);
        float scale = std::min((float)width / w, (float)height / h) * (sharp ? 1.7f : 1.f);
        scale = std::min(scale, std::min(2048.f / w, 2048.f / h));
        pix = fz_new_pixmap_from_page(context, loaded, fz_scale(scale, scale), fz_device_rgb(context), 0);
        image.width = fz_pixmap_width(context, pix);
        image.height = fz_pixmap_height(context, pix);
        int stride = fz_pixmap_stride(context, pix), n = fz_pixmap_components(context, pix);
        if (n != 3 || image.width <= 0 || image.height <= 0)
            fz_throw(context, FZ_ERROR_GENERIC, "Unsupported page pixels");
        image.pixels.resize(image.width * image.height * 3);
        auto *data = fz_pixmap_samples(context, pix);
        for (int y = 0; y < image.height; ++y)
            memcpy(image.pixels.data() + y * image.width * 3, data + y * stride, image.width * 3);
    }
    fz_always(context) {
        fz_drop_pixmap(context, pix);
        fz_drop_page(context, loaded);
    }
    fz_catch(context) {
        image = {};
    }
    return image;
}
Image assetRaster(const AssetBytes &asset, int width, int height, bool sharp) {
    Image image;
    fz_buffer *buffer = nullptr;
    fz_stream *stream = nullptr;
    fz_document *doc = nullptr;
    fz_var(buffer);
    fz_var(stream);
    fz_var(doc);
    fz_try(context) {
        if (!asset.data.empty()) {
            buffer = fz_new_buffer_from_shared_data(context, asset.data.data(), asset.data.size());
            stream = fz_open_buffer(context, buffer);
            doc = fz_open_document_with_stream(context, asset.name.c_str(), stream);
            image = raster(doc, 0, width, height, sharp);
        }
    }
    fz_always(context) {
        fz_drop_document(context, doc);
        fz_drop_stream(context, stream);
        fz_drop_buffer(context, buffer);
    }
    fz_catch(context) {
        image = {};
    }
    return image;
}
std::string comicPath, comicLayout;
std::vector<std::string> comicEntries;
void trimCache(int current) {
    size_t bytes = 0;
    for (const auto &p : cache)
        bytes += p.second.pixels.size();
    while (cache.size() > 3 || bytes > 32 * 1024 * 1024) {
        auto it = cache.begin();
        for (auto i = cache.begin(); i != cache.end(); ++i)
            if (std::abs(i->first - current) > std::abs(it->first - current))
                it = i;
        bytes -= it->second.pixels.size();
        cache.erase(it);
    }
}
void tint(Image &image, int theme) {
    if (!theme)
        return;
    for (size_t i = 0; i + 2 < image.pixels.size(); i += 3) {
        for (int c = 0; c < 3; ++c) {
            auto &p = image.pixels[i + c];
            if (theme == 2)
                p = 24 + (255 - p) * 208 / 255;
            else
                p = p * (c == 0 ? 246 : c == 1 ? 236 : 215) / 255;
        }
    }
}
void entry(void *) {
    context = fz_new_context(nullptr, nullptr, 96 * 1024 * 1024);
    if (context) {
        fz_register_document_handlers(context);
        fz_disable_icc(context);
    }
    while (!stopping) {
        RenderRequest r;
        bool work = false;
        mutexLock(&lock);
        if (pending) {
            r = request;
            pending = false;
            work = true;
        }
        mutexUnlock(&lock);
        if (!work) {
            svcSleepThread(8000000);
            continue;
        }
        RenderResult out;
        out.id = r.id;
        out.path = r.path;
        out.cover = r.cover;
        if (!context)
            out.error = "Could not allocate document memory";
        else if (r.cover) {
            fz_document *coverDoc = nullptr;
            fz_var(coverDoc);
            fz_try(context) {
                auto kind = format(r.path);
                if (kind == "epub")
                    out.image = assetRaster(epubCover(r.path), 384, 512, false);
                else if (kind == "cbz" || kind == "cbr") {
                    auto pages = comicPages(r.path, out.error);
                    if (!pages.empty())
                        out.image = assetRaster(archiveImage(r.path, pages.front()), 384, 512, false);
                }
                if (out.image.pixels.empty() && kind != "cbr") {
                    fz_set_user_css(context, "");
                    fz_set_use_document_css(context, 1);
                    coverDoc = fz_open_document(context, r.path.c_str());
                    if (fz_needs_password(context, coverDoc))
                        fz_throw(context, FZ_ERROR_GENERIC, "Password protected book");
                    fz_layout_document(context, coverDoc, 420, 600, 22);
                    out.image = raster(coverDoc, 0, 384, 512, false);
                }
                if (out.image.pixels.empty() && out.error.empty())
                    out.error = "No usable cover image";
            }
            fz_always(context) {
                fz_drop_document(context, coverDoc);
            }
            fz_catch(context) {
                out.error = fz_caught_message(context);
            }
        } else if (format(r.path) == "cbr" || format(r.path) == "cbz") {
            // Keep only the page index; RAR images are decoded on demand, never extracted to SD.
            if (document) {
                fz_drop_document(context, document);
                document = nullptr;
                documentPath.clear();
                cache.clear();
                chapters.clear();
            }
            if (comicPath != r.path || comicLayout != styleKey(r.style))
                cache.clear();
            comicLayout = styleKey(r.style);
            if (comicPath != r.path) {
                comicEntries = comicPages(r.path, out.error);
                comicPath = comicEntries.empty() ? "" : r.path;
            }
            out.total = comicEntries.size();
            if (out.total) {
                out.page = resolvePage(r.page, r.previousTotal, r.fraction, out.total);
                if (!cache.count(out.page)) {
                    auto asset = archiveImage(r.path, comicEntries[out.page]);
                    auto image = assetRaster(asset, r.style.portrait ? 720 : 1280,
                                             r.style.portrait ? 1280 : 720, true);
                    if (image.pixels.empty())
                        out.error = asset.error.empty() ? "Could not decode comic page" : asset.error;
                    else
                        cache[out.page] = std::move(image);
                }
                if (cache.count(out.page)) {
                    out.image = cache[out.page];
                    tint(out.image, r.style.theme);
                    trimCache(out.page);
                }
            } else if (out.error.empty())
                out.error = "No readable comic pages";
        } else {
            if (!comicPath.empty()) {
                comicPath.clear();
                comicEntries.clear();
                cache.clear();
            }
            std::string key = styleKey(r.style), userCss = readingCss(r.style);
            fz_outline *tree = nullptr;
            fz_var(tree);
            fz_try(context) {
                if (!document || documentPath != r.path || layoutKey != key) {
                    fz_drop_document(context, document);
                    document = nullptr;
                    cache.clear();
                    chapters.clear();
                    documentPath.clear();
                    fz_set_user_css(context, userCss.c_str());
                    fz_set_use_document_css(context, r.style.font == 0);
                    document = fz_open_document(context, r.path.c_str());
                    if (fz_needs_password(context, document))
                        fz_throw(context, FZ_ERROR_GENERIC, "Password-protected books are not supported");
                    fz_layout_document(context, document, r.style.portrait ? 720 : 1280,
                                       r.style.portrait ? 1280 : 720, r.style.size);
                    total = fz_count_pages(context, document);
                    if (total <= 0)
                        fz_throw(context, FZ_ERROR_GENERIC, "This document has no pages");
                    tree = fz_load_outline(context, document);
                    outline(tree, 0);
                    documentPath = r.path;
                    layoutKey = key;
                }
                out.total = total;
                out.page = resolvePage(r.page, r.previousTotal, r.fraction, total);
                out.chapters = chapters;
                if (!cache.count(out.page))
                    cache[out.page] = raster(document, out.page, r.style.portrait ? 720 : 1280,
                                             r.style.portrait ? 1280 : 720, fixedLayout(r.path));
                if (cache[out.page].pixels.empty()) {
                    fz_empty_store(context);
                    cache[out.page] = raster(document, out.page, r.style.portrait ? 360 : 640,
                                             r.style.portrait ? 640 : 360, false);
                }
                out.image = cache[out.page];
                if (out.image.pixels.empty())
                    cache.erase(out.page);
                trimCache(out.page);
                if (out.image.pixels.empty())
                    out.error = "Could not render this page";
                else
                    tint(out.image, r.style.theme);
            }
            fz_always(context) {
                fz_drop_outline(context, tree);
            }
            fz_catch(context) {
                out.error = fz_caught_message(context);
                fz_drop_document(context, document);
                document = nullptr;
                cache.clear();
            }
        }
        bool prefetch = !r.cover && format(r.path) != "cbr" && format(r.path) != "cbz" && out.error.empty();
        int current = out.page;
        mutexLock(&lock);
        if (!pending || request.id <= r.id) {
            result = std::move(out);
            ready = true;
        }
        mutexUnlock(&lock);
        if (prefetch) {
            for (int next : {current + 1, current - 1}) {
                mutexLock(&lock);
                bool waiting = pending;
                mutexUnlock(&lock);
                if (waiting || stopping)
                    break;
                if (next >= 0 && next < total && !cache.count(next)) {
                    auto image = raster(document, next, r.style.portrait ? 720 : 1280,
                                        r.style.portrait ? 1280 : 720, fixedLayout(r.path));
                    if (!image.pixels.empty())
                        cache[next] = std::move(image);
                    trimCache(current);
                }
            }
        }
    }
    cache.clear();
    if (context) {
        fz_drop_document(context, document);
        document = nullptr;
        fz_drop_context(context);
        context = nullptr;
    }
}
} // namespace
bool startEngine() {
    mutexInit(&lock);
    stopping = false;
    if (R_FAILED(threadCreate(&worker, entry, nullptr, nullptr, 1024 * 1024, 0x3B, -2)))
        return false;
    if (R_FAILED(threadStart(&worker))) {
        threadClose(&worker);
        return false;
    }
    started = true;
    return true;
}
void stopEngine() {
    if (started) {
        stopping = true;
        threadWaitForExit(&worker);
        threadClose(&worker);
        started = false;
    }
}
void submit(RenderRequest r) {
    mutexLock(&lock);
    request = std::move(r);
    pending = true;
    mutexUnlock(&lock);
}
bool receive(RenderResult &out) {
    mutexLock(&lock);
    bool has = ready;
    if (has) {
        out = std::move(result);
        ready = false;
    }
    mutexUnlock(&lock);
    return has;
}
} // namespace rr
