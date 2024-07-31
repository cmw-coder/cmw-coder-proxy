#include <components/MemoryManipulator.h>
#include <components/WindowManager.h>
#include <utils/common.h>

#include <combaseapi.h>

using namespace components;
using namespace std;
using namespace types;
using namespace utils;

CaretDimension common::getCaretDimensions(const bool waitTillAvailable) {
    const auto [clientX, clientY] = WindowManager::GetInstance()->getClientPosition();

    auto [height, xPosition, yPosition] = MemoryManipulator::GetInstance()->getCaretDimension();
    while (waitTillAvailable && !height) {
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

string common::uuid() {
    GUID gidReference;
    HRESULT result;
    do {
        result = CoCreateGuid(&gidReference);
        if (result != S_OK) {
            std::this_thread::sleep_for(1ms);
        }
    } while (result != S_OK);
    return format(
        "{:08x}-{:04x}-{:04x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
        gidReference.Data1, gidReference.Data2, gidReference.Data3,
        gidReference.Data4[0], gidReference.Data4[1],
        gidReference.Data4[2], gidReference.Data4[3], gidReference.Data4[4],
        gidReference.Data4[5], gidReference.Data4[6], gidReference.Data4[7]
    );
}
