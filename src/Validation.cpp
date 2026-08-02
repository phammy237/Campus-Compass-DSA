#include "Validation.h"

#include <cctype>

namespace {
bool isDigitChar(char c) { return isdigit(static_cast<unsigned char>(c)) != 0; }
bool isUpperChar(char c) { return isupper(static_cast<unsigned char>(c)) != 0; }
bool isAlphaChar(char c) { return isalpha(static_cast<unsigned char>(c)) != 0; }
} // namespace

bool isInteger(const string &s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[0] == '+' || s[0] == '-') i = 1;
    if (i == s.size()) return false; // sign with no digits
    for (; i < s.size(); ++i) {
        if (!isDigitChar(s[i])) return false;
    }
    return true;
}

bool isValidUFID(const string &s) {
    if (s.size() != 8) return false;
    for (char c : s) {
        if (!isDigitChar(c)) return false;
    }
    return true;
}

bool isValidName(const string &s) {
    if (s.empty()) return false;
    bool hasLetter = false;
    for (char c : s) {
        if (isAlphaChar(c)) {
            hasLetter = true;
        } else if (c != ' ') {
            return false;
        }
    }
    return hasLetter;
}

bool isValidClassCode(const string &s) {
    if (s.size() != 7) return false;
    for (size_t i = 0; i < 3; ++i) {
        if (!isUpperChar(s[i])) return false;
    }
    for (size_t i = 3; i < 7; ++i) {
        if (!isDigitChar(s[i])) return false;
    }
    return true;
}
