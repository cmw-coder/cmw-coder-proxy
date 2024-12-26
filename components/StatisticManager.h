#pragma once

#include <singleton_dclp.hpp>

#include <types/EditedCompletion.h>

namespace components {
    class StatisticManager : public SingletonDclp<StatisticManager> {
    public:
        StatisticManager();

        ~StatisticManager() override;

        uint32_t addLine(uint32_t line, uint32_t count = 1);

        bool reactEditedCompletion(const std::string& actionId, bool isAccept);

        uint32_t removeLine(uint32_t line, uint32_t count = 1);

        void setEditedCompletion(const std::string& actionId, uint32_t line, const std::string& completion);

        void updateStatisticConfig(const models::StatisticConfig& completionConfig);

    private:
        mutable std::shared_mutex _editedCompletionMapMutex;
        std::atomic<bool> _configCheckEditedCompletion{false}, _isRunning{true};
        std::unordered_map<std::string, types::EditedCompletion> _editedCompletionMap;

        void _threadReportEditedCompletions();
    };
}
