#include <filesystem>
#include <fstream>
#include <regex>

#include <utils/fs.h>

using namespace std;
using namespace utils;

string fs::readFile(const string& path) {
    if (path.empty()) {
        return {};
    }
    constexpr auto read_size = size_t{4096};
    auto stream = ifstream{path.data()};
    stream.exceptions(ios_base::badbit);

    auto out = string{};
    auto buf = string(read_size, '\0');
    while (stream.read(&buf[0], read_size)) {
        out.append(buf, 0, stream.gcount());
    }
    out.append(buf, 0, stream.gcount());
    // Remove \r using ranges::remove
    erase(out, '\r');
    return out;
}

string fs::getExtension(const string& path) {
    if (path.empty()) {
        return {};
    }
    return filesystem::path(path).extension().string();
}
