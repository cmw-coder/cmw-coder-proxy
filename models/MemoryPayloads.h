#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <types/CaretPosition.h>

namespace models {
    template<typename Data>
    class PayloadBase {
    public:
        virtual ~PayloadBase() = default;

        [[nodiscard]] const void* data() const {
            return &_data;
        }

        [[nodiscard]] void* data() {
            return &_data;
        }

        [[nodiscard]] uint32_t size() const {
            return sizeof(_data);
        }

    protected:
        Data _data;
    };

    struct SimpleStringData {
        uint8_t lengthLow, lengthHigh;
        char content[4094];
    };

    struct SymbolBufferData {
        uint32_t data1, data2;
        uint32_t reserved1[2];
        uint32_t data3, data4, data5, data6;
        uint32_t reserved2[4];
        int32_t reserved3[8];
        uint32_t data7, data8;
        uint32_t reserved4[8];
        int32_t reserved5;
        uint32_t reserved6[10];
        int32_t reserved7;
        uint32_t reserved8[22];
    };

    struct SymbolListData {
        uint32_t count;
        uint8_t data[4092];
    };

    struct SymbolNameData {
        uint32_t nestingDepth; ///< The nesting depth of the symbol. Starts at 1.
        char name[4092];
    };

    class SimpleString : public PayloadBase<SimpleStringData> {
    public:
        SimpleString() = default;

        explicit SimpleString(std::string_view str);

        [[nodiscard]] uint32_t length() const;

        [[nodiscard]] std::string str() const;
    };

    using SymbolBuffer = PayloadBase<SymbolBufferData>;

    class SymbolList final : public PayloadBase<SymbolListData> {
    public:
        [[nodiscard]] uint32_t count() const;
    };

    using SymbolListHandle = SymbolList *;

    class SymbolName final : public PayloadBase<SymbolNameData> {
    public:
        [[nodiscard]] uint32_t depth() const;
        [[nodiscard]] std::string name() const;
    };

    class SymbolRecord final : public SimpleString {
    public:
        struct Record {
            std::string file, project, symbol, type;
            types::CaretPosition namePosition;
            uint32_t instanceIndex, lineEnd, lineStart;
        };

        [[nodiscard]] std::optional<Record> parse() const;
    };
}
