#include <components/InteractionMonitor.h>
#include <components/StatisticManager.h>
#include <components/WebsocketManager.h>
#include <components/WindowManager.h>
#include <utils/logger.h>

using namespace components;
using namespace std;
using namespace types;
using namespace utils;

StatisticManager::StatisticManager() {
    _threadReportEditedCompletions();

    logger::info("StatisticManager is initialized.");
}

StatisticManager::~StatisticManager() {
    _isRunning.store(false);
}

uint32_t StatisticManager::addLine(const uint32_t line, const uint32_t count) {
    uint32_t result{};
    if (_configCheckEditedCompletion.load()) {
        if (const auto currentWindowHandleOpt = WindowManager::GetInstance()->getCurrentWindowHandle();
            currentWindowHandleOpt.has_value()) {
            unique_lock lock(_editedCompletionMapMutex);
            for (auto& editedCompletion: _editedCompletionMap | views::values) {
                if (editedCompletion.windowHandle == currentWindowHandleOpt.value()) {
                    editedCompletion.addLine(line, count);
                    result++;
                }
            }
        }
    }
    return result;
}

bool StatisticManager::reactEditedCompletion(const std::string& actionId, const bool isAccept) {
    if (_configCheckEditedCompletion.load()) {
        unique_lock lock(_editedCompletionMapMutex);
        if (_editedCompletionMap.contains(actionId)) {
            _editedCompletionMap.at(actionId).react(isAccept);
            return true;
        }
    }
    return false;
}

uint32_t StatisticManager::removeLine(const uint32_t line, const uint32_t count) {
    uint32_t result{};
    if (_configCheckEditedCompletion.load()) {
        if (const auto currentWindowHandleOpt = WindowManager::GetInstance()->getCurrentWindowHandle();
            currentWindowHandleOpt.has_value()) {
            unique_lock lock(_editedCompletionMapMutex);
            for (auto& editedCompletion: _editedCompletionMap | views::values) {
                if (editedCompletion.windowHandle == currentWindowHandleOpt.value()) {
                    editedCompletion.removeLine(line, count);
                    result++;
                }
            }
        }
    }
    return result;
}

void StatisticManager::setEditedCompletion(const string& actionId, const uint32_t line, const string& completion) {
    if (_configCheckEditedCompletion.load()) {
        if (const auto currentWindowHandleOpt = WindowManager::GetInstance()->getCurrentWindowHandle();
            currentWindowHandleOpt.has_value()) {
            auto editedCompletion = EditedCompletion(actionId, currentWindowHandleOpt.value(), line, completion);
            unique_lock lock(_editedCompletionMapMutex);
            _editedCompletionMap.emplace(actionId, move(editedCompletion));
        }
    }
}

void StatisticManager::updateStatisticConfig(const models::StatisticConfig& completionConfig) {
    if (const auto checkEditedCompletionOpt = completionConfig.checkEditedCompletion;
        checkEditedCompletionOpt.has_value()) {
        const auto checkEditedCompletion = checkEditedCompletionOpt.value();
        logger::info(format("Update check edited completion: {}", checkEditedCompletion));
        _configCheckEditedCompletion.store(checkEditedCompletion);
        if (!checkEditedCompletion) {
            logger::info("Clear edited completion map");
            unique_lock lock(_editedCompletionMapMutex);
            _editedCompletionMap.clear();
        }
    }
}

void StatisticManager::_threadReportEditedCompletions() {
    thread([this] {
        while (_isRunning) {
            if (_configCheckEditedCompletion.load()) {
                vector<EditedCompletion> needReportCompletions{}; {
                    const shared_lock lock(_editedCompletionMapMutex);
                    for (const auto& editedCompletion: _editedCompletionMap | views::values) {
                        if (editedCompletion.canReport()) {
                            needReportCompletions.push_back(editedCompletion);
                        }
                    }
                } {
                    const auto interactionLock = InteractionMonitor::GetInstance()->getInteractionLock();
                    const unique_lock editedCompletionMapLock(_editedCompletionMapMutex);
                    for (const auto& needReportCompletion: needReportCompletions) {
                        _editedCompletionMap.erase(needReportCompletion.actionId);
                        WebsocketManager::GetInstance()->send(needReportCompletion.parse());
                    }
                }
            }
            this_thread::sleep_for(5s);
        }
    }).detach();
}
