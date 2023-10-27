#pragma once

#include <string>

#include <singleton_dclp.hpp>

#include <types/SiVersion.h>

namespace types {
    class Configurator : public SingletonDclp<Configurator> {
    public:
        Configurator();

        [[nodiscard]] std::string username() const;

        [[nodiscard]] SiVersion version() const;

        [[nodiscard]] std::string pluginVersion(const std::string &version) const;

    private:
        std::string _userName;
        SiVersion _version = SiVersion::Unknown;
    };
}
