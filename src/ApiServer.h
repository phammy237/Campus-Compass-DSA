#pragma once

#include <string>

// escapes a string for embedding as a JSON string literal (no surrounding quotes)
std::string jsonEscape(const std::string &s);
