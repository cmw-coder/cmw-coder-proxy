#include <components/ModificationManager.h>

using namespace components;
using namespace std;
using namespace types;


void ModificationManager::deleteInput(const CaretPosition position) {
    bool isBufferValid, isIncrement{}; {
        shared_lock lock(_bufferMutex);
        isBufferValid = _buffer.has_value();
    }

    if (isBufferValid) {
        unique_lock lock(_bufferMutex);
        isIncrement = _buffer.value().modifySingle(Modification::Type::Deletion, position);
    }
    if (!isIncrement) {
        if (isBufferValid) {
            unique_lock lock(_historyMutex);
            _historyQueue.push(_buffer.value());
        }
        // TODO: Deal with cross-line deletion
        if (position.character == 0) {
            {
                unique_lock lock(_historyMutex);
                _historyQueue.emplace(position, position);
            }
            unique_lock lock(_bufferMutex);
            _buffer.reset();
        }
        else {
            unique_lock lock(_bufferMutex);
            _buffer.emplace(Modification(position, position - CaretPosition{1, 0}));
        }
    }
}

void ModificationManager::normalInput(const CaretPosition position, const char character) {
    bool isBufferValid, isIncrement{}; {
        shared_lock lock(_bufferMutex);
        isBufferValid = _buffer.has_value();
    }

    if (isBufferValid) {
        unique_lock lock(_bufferMutex);
        isIncrement = _buffer.value().modifySingle(Modification::Type::Addition, position, character);
    }
    if (!isIncrement) {
        if (isBufferValid) {
            unique_lock lock(_historyMutex);
            _historyQueue.push(_buffer.value());
        }
        unique_lock lock(_bufferMutex);
        _buffer.emplace(Modification(position, string{1, character}));
    }
}
