#pragma once

#include <array>
#include <condition_variable>
#include <mutex>

template<size_t N>
class MultiGroupMutex {
public:
    static_assert(N >= 2, "N must be greater than or equal to 2");

    void lock(const size_t groupIndex) {
        std::unique_lock lock(_mutex);
        _cv.wait(lock, [this, groupIndex] { return !_isOtherGroupLocked(groupIndex); });
        ++_counters[groupIndex];
    }

    [[nodiscard]] bool owned(const size_t groupIndex) const {
        std::unique_lock lock(_mutex);
        return _counters[groupIndex] > 0;
    }

    void unlock(const size_t groupIndex) {
        std::unique_lock lock(_mutex);
        if (--_counters[groupIndex] == 0) {
            _cv.notify_all();
        }
    }

    void unlockAll(const size_t groupIndex) {
        std::unique_lock lock(_mutex);
        _counters[groupIndex] = 0;
        _cv.notify_all();
    }

private:
    std::mutex _mutex;
    std::condition_variable _cv;
    std::array<int, N> _counters{};

    [[nodiscard]] bool _isOtherGroupLocked(const size_t groupIndex) const {
        for (auto index = 0; index < N; ++index) {
            if (index != groupIndex && _counters[index] > 0) {
                return true;
            }
        }
        return false;
    }
};