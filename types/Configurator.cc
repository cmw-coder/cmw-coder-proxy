#include <format>

#include <magic_enum.hpp>

#include <types/Configurator.h>
#include <utils/logger.h>
#include <utils/system.h>

using namespace magic_enum;
using namespace std;
using namespace types;
using namespace utils;
using namespace WinToastLib;

void Configurator::WinToastHandler::toastActivated() const {
    logger::log("Toast activated");
}

void Configurator::WinToastHandler::toastActivated(int actionIndex) const {
    logger::log(format("Toast activated: {}", actionIndex));
}

void Configurator::WinToastHandler::toastDismissed(WinToastDismissalReason state) const {
    logger::log(format("Toast dismissed: {}", enum_name(state)));
}

void Configurator::WinToastHandler::toastFailed() const {
    logger::log("Toast failed");
}

Configurator::Configurator() {
    const auto [major, minor, build, _] = system::getVersion();
    if (major == 3 && minor == 5) {
        _siVersion = make_pair(
                SiVersion::Major::V35,
                enum_cast<SiVersion::Minor>(build).value_or(SiVersion::Minor::Unknown)
        );
        _siVersionString = "_3.50." + format("{:0>{}}", build, 4);
    } else {
        _siVersion = make_pair(
                SiVersion::Major::V40,
                enum_cast<SiVersion::Minor>(build).value_or(SiVersion::Minor::Unknown)
        );
        _siVersionString = "_4.00." + format("{:0>{}}", build, 4);
    }

    _initWinToast();
}

pair<SiVersion::Major, SiVersion::Minor> Configurator::version() const {
    return _siVersion;
}

string Configurator::reportVersion(const string &version) const {
    return version + _siVersionString;
}

bool Configurator::showToast(const wstring &title, const wstring &content) const {
    if (!_canToast) {
        return false;
    }
    WinToastHandler winToastHandler;
    auto toast = WinToastTemplate(WinToastTemplate::Text02);
    toast.setTextField(title, WinToastTemplate::FirstLine);
    toast.setTextField(content, WinToastTemplate::SecondLine);
    return WinToast::instance()->showToast(toast, &winToastHandler) > 0;
}

void Configurator::_initWinToast() {
    if (!WinToast::isCompatible()) {
        logger::log("WinToast is not compatible with current system");
        _canToast = false;
    }
    WinToast::instance()->setAppName(L"Comware Coder Proxy");
    WinToast::instance()->setAppUserModelId(WinToast::configureAUMI(
            L"h3c", L"cmw-coder", L"cmw-coder-proxy"
    ));
    WinToast::instance()->setShortcutPolicy(WinToast::SHORTCUT_POLICY_IGNORE);

    if (!WinToast::instance()->initialize()) {
        logger::log("Failed to initialize WinToast");
        _canToast = false;
    }

    _canToast = true;
}
