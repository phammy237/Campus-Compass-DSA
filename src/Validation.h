#pragma once

#include <string>

using namespace std;

// pure syntactic checks — no knowledge of loaded students/classes.
// stateful rules (uniqueness, class existence, 1-6 class count, duplicates)
// are enforced in CampusCompass's command handlers (Phase 4), since they
// need access to already-loaded data.

bool isInteger(const string &s);        // optional leading +/-, then 1+ digits
bool isValidUFID(const string &s);      // exactly 8 digits
bool isValidName(const string &s);      // 1+ chars, letters and spaces only, at least one letter
bool isValidClassCode(const string &s); // exactly 3 capital letters then 4 digits
