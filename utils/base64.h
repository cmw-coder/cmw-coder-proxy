#include <string>

namespace utils::base64 {
    std::string encode(
            const unsigned char *bytes_to_encode,
            size_t in_len,
            bool url_safe = false,
            bool padded = true
    );

    inline std::string base64Encode(std::string_view data, bool url_safe = false, bool padded = true) {
        return encode(
                (unsigned char *) data.data(),
                data.size(),
                url_safe,
                padded
        );
    }
    std::string decode(std::string_view encoded_string);

    bool isValid(std::string_view str);
}
