#pragma once

#include <string>

#include <singleton_dclp.hpp>

#include <types/SiVersion.h>

namespace components {
    class Configurator : public SingletonDclp<Configurator> {
    public:
        Configurator();

        [[nodiscard]] types::SiVersion::Full version() const;

        [[nodiscard]] std::string reportVersion(const std::string&version) const;

    private:
        std::string _siVersionString;
        types::SiVersion::Full _siVersion;
    };
}
