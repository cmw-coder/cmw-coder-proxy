#include <unordered_map>

#include <types/Configurator.h>
#include <utils/system.h>

using namespace std;
using namespace types;
using namespace utils;

namespace {
    const unordered_map<SiVersion, tuple<uint64_t, uint64_t>> addressMap = {
            {SiVersion::V350076, {0x1CBEFC, 0x1CBF00}},
            {SiVersion::V350086, {0x1BE0CC, 0x1CD3E0}},
            {SiVersion::V400113, {0x27D040, 0x27E35C}},
    };
}

Configurator::Configurator() : _userName(system::getUserName()) {
    const auto [major, minor, build, _] = system::getVersion();
    if (major == 3 && minor == 5) {
        if (build == 76) {
            _version = SiVersion::V350076;
        } else if (build == 86) {
            _version = SiVersion::V350086;
        }
    } else if (major == 4 && minor == 0) {
        if (build == 113) {
            _version = SiVersion::V400113;
        }
    }
}

string Configurator::username() const {
    return _userName;
}

SiVersion Configurator::version() const {
    return _version;
}

std::string Configurator::pluginVersion() const {
    switch (_version) {
        case SiVersion::V350076:
            return _pluginVersion + "_3.50.0076";
        case SiVersion::V350086:
            return _pluginVersion + "_3.50.0086";
        case SiVersion::V400084:
            return _pluginVersion + "_4.00.0084";
        case SiVersion::V400096:
            return _pluginVersion + "_4.00.0096";
        case SiVersion::V400099:
            return _pluginVersion + "_4.00.0099";
        case SiVersion::V400113:
            return _pluginVersion + "_4.00.0113";
        case SiVersion::V400116:
            return _pluginVersion + "_4.00.0116";
        default:
            return _pluginVersion + "_0.00.0000";
    }
}
