#pragma once

#include <string>

#include <models/WsMessage.h>
#include <types/common.h>

namespace types {
    class EditedCompletion {
    public:
        std::string actionId;

        EditedCompletion(std::string actionId, uint32_t windowHandle, uint32_t line, std::string completion);

        void react(bool isAccept);

        [[nodiscard]] bool canReport() const;

        void addLine(uint32_t line);

        void addLines(const std::vector<uint32_t> &lines);

        void removeLine(uint32_t line);

        [[nodiscard]] models::CompletionEditClientMessage parse() const;

    private:
        bool _isAccept = false;
        uint32_t _windowHandle;
        std::optional<Time> _reactTime;
        std::string _completion;
        std::vector<uint32_t> _references;

        void _removeDuplicates();
    };
}
