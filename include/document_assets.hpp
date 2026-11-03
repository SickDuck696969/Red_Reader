#pragma once
#include <string>
#include <vector>
namespace rr {
struct AssetBytes {
    std::vector<unsigned char> data;
    std::string name, error;
};
std::vector<std::string> comicPages(const std::string &path, std::string &error);
AssetBytes archiveImage(const std::string &path, const std::string &name);
bool validBookFile(const std::string &path, const std::string &kind, std::string &error);
AssetBytes epubCover(const std::string &path);
} // namespace rr
