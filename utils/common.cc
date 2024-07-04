#include <components/MemoryManipulator.h>
#include <components/WindowManager.h>
#include <utils/common.h>

using namespace components;
using namespace std;
using namespace types;
using namespace utils;

CaretDimension common::getCaretDimensions() {
    const auto [clientX, clientY] = WindowManager::GetInstance()->getClientPosition();

    auto [height, xPosition, yPosition] = MemoryManipulator::GetInstance()->getCaretDimension();
    while (!height) {
        std::this_thread::sleep_for(5ms);
        const auto [
            newHeight,
            newXPosition,
            newYPosition
        ] = MemoryManipulator::GetInstance()->getCaretDimension();
        height = newHeight;
        xPosition = newXPosition;
        yPosition = newYPosition;
    }

    return {
        height,
        clientX + xPosition,
        clientY + yPosition - 1,
    };
}