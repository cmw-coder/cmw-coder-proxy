#include <codecvt>
#include <format>
#include <locale>

#include <compact_enc_det/compact_enc_det.h>
#include <magic_enum.hpp>

#include <utils/iconv.h>
#include <utils/logger.h>

#include <windows.h>

using namespace magic_enum;
using namespace std;
using namespace utils;

namespace {
    string decode(const string& source, const Encoding encoding = CHINESE_GB) {
        switch (encoding) {
            case CHINESE_GB: {
                auto len = MultiByteToWideChar(CP_ACP, 0, source.c_str(), -1, nullptr, 0);
                vector<wchar_t> wstr(len + 1, 0);
                MultiByteToWideChar(CP_ACP, 0, source.c_str(), -1, wstr.data(), len);
                len = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), -1, nullptr, 0, nullptr, nullptr);
                vector<char> str(len + 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, wstr.data(), -1, str.data(), len, nullptr, nullptr);
                string strTemp = str.data();
                return strTemp;
            }
            case ISO_8859_1:
            case UTF8:
            case ASCII_7BIT: {
                return source;
            }
            default: {
                logger::warn(format("Decode: Unsupported encoding: {}", enum_name(encoding)));
                return source;
            }
        }
    }

    string encode(const string& source, const Encoding encoding = CHINESE_GB) {
        switch (encoding) {
            case CHINESE_GB: {
                int len = MultiByteToWideChar(CP_UTF8, 0, source.c_str(), -1, nullptr, 0);
                const auto wszGBK = new wchar_t[len + 1];
                memset(wszGBK, 0, len * 2 + 2);
                MultiByteToWideChar(CP_UTF8, 0, source.c_str(), -1, wszGBK, len);
                len = WideCharToMultiByte(CP_ACP, 0, wszGBK, -1, nullptr, 0, nullptr, nullptr);
                const auto szGBK = new char[len + 1];
                memset(szGBK, 0, len + 1);
                WideCharToMultiByte(CP_ACP, 0, wszGBK, -1, szGBK, len, nullptr, nullptr);
                string strTemp(szGBK);
                delete[] wszGBK;
                delete[] szGBK;
                return strTemp;
            }
            default: {
                logger::warn(format("Encode: Unsupported encoding: {}", enum_name(encoding)));
                return source;
            }
        }
    }
}

string iconv::autoDecode(const string& source) {
    bool is_reliable;
    int bytes_consumed;
    const auto encoding = DetectEncoding(
        source.c_str(), static_cast<int>(source.length()),
        nullptr, nullptr, nullptr,
        CHINESE_GB,
        CHINESE,
        CompactEncDet::EMAIL_CORPUS,
        false,
        &bytes_consumed,
        &is_reliable);
    if (!is_reliable) {
        logger::warn("AutoDecode: Unreliable encoding detected");
    }
    return decode(source, encoding);
}

string iconv::autoEncode(const string& source) {
    return encode(source);
}
