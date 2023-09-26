#pragma once

#include <chrono>

#include <singleton_dclp.hpp>

#include <types/CursorPosition.h>

namespace types {
    class RegistryMonitor : public SingletonDclp<RegistryMonitor> {
    public:
        RegistryMonitor();

        ~RegistryMonitor() override;

        void cancelByCursorNavigate(CursorPosition, CursorPosition);

        void cancelByDeleteBackward(CursorPosition oldPosition, CursorPosition newPosition);

        void cancelByKeycodeNavigate(unsigned int);

        void cancelByModifyLine(unsigned int);

    private:
        const std::string _subKey = R"(SOFTWARE\Source Dynamics\Source Insight\3.0)";
        std::atomic<bool> _isRunning = true;
        std::atomic<std::chrono::time_point<std::chrono::high_resolution_clock>> _lastTriggerTime;

        std::optional<std::string> _generateCompletion(std::string &&editorInfo);
    };
}
