#pragma once

#include <memory>

#include <singleton_dclp.hpp>

#include <models/MemoryAddress.h>
#include <models/MemoryPayloads.h>
#include <types/CaretDimension.h>
#include <types/CaretPosition.h>
#include <types/SiVersion.h>

namespace components {
    class MemoryManipulator : public SingletonDclp<MemoryManipulator> {
    public:
        explicit MemoryManipulator(types::SiVersion::Full version);

        void deleteLineContent(uint32_t line) const;

        [[nodiscard]] types::CaretDimension getCaretDimension() const;

        [[nodiscard]] types::CaretDimension getCaretDimension(uint32_t line) const;

        [[nodiscard]] types::CaretPosition getCaretPosition() const;

        [[nodiscard]] std::string getFileName() const;

        [[nodiscard]] std::string getLineContent(uint32_t line) const;

        [[nodiscard]] std::string getProjectDirectory() const;

        [[nodiscard]] std::optional<models::SymbolName> getSymbolName() const;

        [[nodiscard]] std::optional<models::SymbolName> getSymbolName(uint32_t line) const;

        [[nodiscard]] std::optional<models::SymbolRecord> getSymbolRecord(models::SymbolName symbolName) const;

        void setCaretPosition(const types::CaretPosition& caretPosition) const;

        void setLineContent(uint32_t line, const std::string& content, bool isInsertion) const;

        void setSelectionContent(const std::string& content) const;

    private:
        const models::MemoryAddress _memoryAddress;
        const std::shared_ptr<void> _processHandle;

        [[nodiscard]] uint32_t _getHandle(models::MemoryAddress::HandleType handleType) const;
    };
}
