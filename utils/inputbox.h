#pragma once

namespace utils {
    char *InputBox(const char *Prompt, const char *Title = (char *) "", const char *Default = (char *) "");

    char *PasswordBox(const char *Prompt, const char *Title = (char *) "", const char *Default = (char *) "");
}
