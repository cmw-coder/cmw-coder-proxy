#pragma once

#include <string>

#include <singleton_dclp.hpp>
#include <wintoastlib.h>

#include <types/SiVersion.h>

namespace types {
    class Configurator : public SingletonDclp<Configurator> {
    private:
        class WinToastHandler : public WinToastLib::IWinToastHandler {
        public:
            void toastActivated() const override;

            void toastActivated(int actionIndex) const override;

            void toastDismissed(WinToastDismissalReason state) const override;

            void toastFailed() const override;
        };

    public:
        Configurator();

        [[nodiscard]] std::pair<SiVersion::Major, SiVersion::Minor> version() const;

        [[nodiscard]] std::string reportVersion(const std::string &version) const;

        bool showToast(const std::wstring &title, const std::wstring &content) const;

    private:
        bool _canToast{};
        std::string _siVersionString;
        std::pair<SiVersion::Major, SiVersion::Minor> _siVersion;

        void _initWinToast();
    };
}
