#pragma once

#include <string>

#include <singleton_dclp.hpp>

#include <types/SiVersion.h>

namespace types {
    class Configurator : public SingletonDclp<Configurator> {
    public:
        Configurator();

        [[nodiscard]] std::pair<SiVersion::Major, SiVersion::Minor> version() const;

        [[nodiscard]] std::string reportVersion(const std::string&version) const;

    private:
        std::string _siVersionString;
        std::pair<SiVersion::Major, SiVersion::Minor> _siVersion;
    };
}
