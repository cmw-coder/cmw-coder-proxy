#include <codecvt>
#include <format>

#include <compact_enc_det/compact_enc_det.h>
#include <magic_enum.hpp>

#include <components/ConfigManager.h>
#include <utils/iconv.h>

#include <windows.h>

using namespace components;
using namespace magic_enum;
using namespace std;
using namespace types;
using namespace utils;

namespace {
    string decode(const string& source, const Encoding encoding = CHINESE_GB) {
        switch (encoding) {
            case ISO_8859_1:
            case UTF8:
            case ASCII_7BIT: {
                return source;
            }
            default: {
                auto len = MultiByteToWideChar(CP_ACP, 0, source.c_str(), -1, nullptr, 0);
                vector<wchar_t> wstr(len + 1, 0);
                MultiByteToWideChar(CP_ACP, 0, source.c_str(), -1, wstr.data(), len);
                len = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), -1, nullptr, 0, nullptr, nullptr);
                vector<char> str(len + 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, wstr.data(), -1, str.data(), len, nullptr, nullptr);
                string strTemp = str.data();
                return strTemp;
            }
        }
    }

    string encode(const string& source, const Encoding encoding = CHINESE_GB) {
        switch (encoding) {
            case CHINESE_GB: {
                auto len = MultiByteToWideChar(CP_UTF8, 0, source.c_str(), -1, nullptr, 0);
                vector<wchar_t> wszGBK(len + 1, 0);
                MultiByteToWideChar(CP_UTF8, 0, source.c_str(), -1, wszGBK.data(), len);
                len = WideCharToMultiByte(CP_ACP, 0, wszGBK.data(), -1, nullptr, 0, nullptr, nullptr);
                vector<char> szGBK(len + 1, 0);
                WideCharToMultiByte(CP_ACP, 0, wszGBK.data(), -1, szGBK.data(), len, nullptr, nullptr);
                string strTemp = szGBK.data();
                return strTemp;
            }
            default: {
                return source;
            }
        }
    }
}

string iconv::autoDecode(const string& source) {
    bool is_reliable;
    int bytes_consumed;
    return decode(
        source,
        DetectEncoding(
            source.c_str(), static_cast<int>(source.length()),
            nullptr, nullptr, nullptr,
            CHINESE_GB,
            CHINESE,
            CompactEncDet::EMAIL_CORPUS,
            false,
            &bytes_consumed,
            &is_reliable
        )
    );
}

string iconv::autoEncode(const string& source) {
    return encode(source, ConfigManager::GetInstance()->version().first == SiVersion::Major::V35 ? CHINESE_GB : UTF8);
}
