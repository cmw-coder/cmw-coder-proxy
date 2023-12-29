#pragma once

#include <string>

#include <singleton_dclp.hpp>

#include <types/common.h>

namespace components {
    class Configurator : public SingletonDclp<Configurator> {
    public:
        Configurator();

        [[nodiscard]] types::EditorVersion version() const;

        [[nodiscard]] std::string reportVersion(const std::string&version) const;

    private:
        std::string _siVersionString;
        types::EditorVersion _siVersion;
    };
}
