#pragma once

namespace utils {
    char* InputBox(const char* Prompt, const char* Title = "", const char* Default = "");

    char* PasswordBox(const char* Prompt, const char* Title = "", const char* Default = "");
}
