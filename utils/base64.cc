#include <utils/base64.h>

using namespace std;
using namespace utils;

namespace {
    const std::string base64Chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";

    const std::string urlBase64Chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789-_";

    constexpr size_t base64EncodedLength(size_t in_len, bool padded = true) {
        return padded ? ((in_len + 3 - 1) / 3) * 4 : (in_len * 8 + 6 - 1) / 6;
    }

    constexpr size_t base64DecodedLength(size_t in_len) {
        return (in_len * 3) / 4;
    }

    inline bool _isBase64(unsigned char c) {
        if (isalnum(c))
            return true;
        switch (c) {
            case '+':
            case '/':
            case '-':
            case '_':
                return true;
        }
        return false;
    }

    class Base64CharMap {
    public:
        Base64CharMap() {
            char index = 0;
            for (int c = 'A'; c <= 'Z'; ++c) {
                charMap_[c] = index++;
            }
            for (int c = 'a'; c <= 'z'; ++c) {
                charMap_[c] = index++;
            }
            for (int c = '0'; c <= '9'; ++c) {
                charMap_[c] = index++;
            }
            charMap_[static_cast<int>('+')] = charMap_[static_cast<int>('-')] =
                    index++;
            charMap_[static_cast<int>('/')] = charMap_[static_cast<int>('_')] =
                    index;
            charMap_[0] = char(0xff);
        }

        char getIndex(const char c) const noexcept {
            return charMap_[static_cast<int>(c)];
        }

    private:
        char charMap_[256]{0};
    };

    const Base64CharMap base64CharMap;
}

std::string base64::encode(
        const unsigned char *bytes_to_encode,
        size_t in_len,
        bool url_safe,
        bool padded
) {
    std::string ret;
    ret.reserve(base64EncodedLength(in_len, padded));
    int i = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    const std::string &charSet = url_safe ? urlBase64Chars : base64Chars;

    while (in_len--) {
        char_array_3[i++] = *(bytes_to_encode++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) +
                              ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) +
                              ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for (i = 0; (i < 4); ++i)
                ret += charSet[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j < 3; ++j)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] =
                ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] =
                ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (int j = 0; (j <= i); ++j)
            ret += charSet[char_array_4[j]];

        if (padded)
            while ((++i < 4))
                ret += '=';
    }
    return ret;
}

std::string base64::decode(std::string_view encoded_string) {
    auto in_len = encoded_string.size();
    int i = 0;
    int in_{0};
    unsigned char char_array_4[4], char_array_3[3];
    std::string ret;
    ret.reserve(base64DecodedLength(in_len));

    while (in_len-- && (encoded_string[in_] != '=')) {
        if (!_isBase64(encoded_string[in_])) {
            ++in_;
            continue;
        }

        char_array_4[i++] = encoded_string[in_];
        ++in_;
        if (i == 4) {
            for (i = 0; i < 4; ++i) {
                char_array_4[i] = base64CharMap.getIndex(char_array_4[i]);
            }
            char_array_3[0] =
                    (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) +
                              ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; (i < 3); ++i)
                ret += char_array_3[i];
            i = 0;
        }
    }

    if (i) {
        for (int j = i; j < 4; ++j)
            char_array_4[j] = 0;

        for (int j = 0; j < 4; ++j) {
            char_array_4[j] = base64CharMap.getIndex(char_array_4[j]);
        }

        char_array_3[0] =
                (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] =
                ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        --i;
        for (int j = 0; (j < i); ++j)
            ret += char_array_3[j];
    }

    return ret;
}

bool base64::isValid(std::string_view str) {
    for (auto c: str)
        if (!_isBase64(c))
            return false;
    return true;
}
