#include <codecvt>
#include <locale>
#include <utils/iconv.h>

#include <windows.h>

using namespace std;
using namespace utils;

string iconv::gbkToUtf8(const string& source) {
    auto len = MultiByteToWideChar(CP_ACP, 0, source.c_str(), -1, nullptr, 0);
    const auto wstr = new wchar_t[len + 1];
    memset(wstr, 0, len + 1);
    MultiByteToWideChar(CP_ACP, 0, source.c_str(), -1, wstr, len);
    len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    const auto str = new char[len + 1];
    memset(str, 0, len + 1);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, len, nullptr, nullptr);
    string strTemp = str;
    delete[] wstr;
    delete[] str;
    return strTemp;
}

string iconv::utf8ToGbk(const string& source) {
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
