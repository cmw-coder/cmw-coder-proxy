#pragma once

#include <string>

#include <singleton_dclp.hpp>

#include <types/SiVersion.h>

namespace components {
    class Configurator : public SingletonDclp<Configurator> {
    public:
        Configurator();

        [[nodiscard]] std::pair<types::SiVersion::Major, types::SiVersion::Minor> version() const;

        [[nodiscard]] std::string reportVersion(const std::string&version) const;

    private:
        std::string _siVersionString;
        std::pair<types::SiVersion::Major, types::SiVersion::Minor> _siVersion;
    };
}
