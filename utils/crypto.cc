#include <stdexcept>

#include <cryptopp/base64.h>
#include <cryptopp/hex.h>
#include <cryptopp/sha.h>

#include <utils/crypto.h>

using namespace std;
using namespace utils;

namespace {
    template<typename T>
    string _encode(const string &input) {
        string output;
        T encoder;
        encoder.Attach(new CryptoPP::StringSink(output));
        encoder.Put(reinterpret_cast<const CryptoPP::byte *>(input.data()), input.size());
        encoder.MessageEnd();
        return output;
    }

    template<typename T>
    string _decode(const string &input) {
        string output;
        T decoder;
        decoder.Attach(new CryptoPP::StringSink(output));
        decoder.Put(reinterpret_cast<const CryptoPP::byte *>(input.data()), input.size());
        decoder.MessageEnd();
        return output;
    }
}

string crypto::encode(const string &input, crypto::Encoding encoding) {
    switch (encoding) {
        case Encoding::Base64:
            return _encode<CryptoPP::Base64Encoder>(input);
        case Encoding::Hex:
            return _encode<CryptoPP::HexEncoder>(input);
            [[unlikely]]default:
            throw new runtime_error("Unsupported encoding.");
    }
}

std::string crypto::decode(const string &input, crypto::Encoding encoding) {
    switch (encoding) {
        case Encoding::Base64:
            return _decode<CryptoPP::Base64Decoder>(input);
        case Encoding::Hex:
            return _decode<CryptoPP::HexDecoder>(input);
            [[unlikely]]default:
            throw new runtime_error("Unsupported encoding.");
    }
}

string crypto::sha1(const string &input, crypto::Encoding encoding) {
    string output;
    CryptoPP::SHA1 hash;
    hash.Update(reinterpret_cast<const CryptoPP::byte *>(input.data()), input.size());
    output.resize(hash.DigestSize());
    hash.Final(reinterpret_cast<CryptoPP::byte *>(output.data()));
    return encode(output, encoding);
}