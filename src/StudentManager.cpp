#include "StudentManager.h"

#include <algorithm>
#include <utility>

namespace {
vector<string>::iterator findCode(vector<string> &codes, const string &code) {
    return find(codes.begin(), codes.end(), code);
}
} // namespace

bool StudentManager::hasStudent(const string &ufid) const {
    return students_.count(ufid) > 0;
}

const Student *StudentManager::getStudent(const string &ufid) const {
    auto it = students_.find(ufid);
    return it == students_.end() ? nullptr : &it->second;
}

bool StudentManager::insertStudent(Student s) {
    if (students_.count(s.ufid) > 0) return false;
    string ufid = s.ufid; // copy the key before moving s into the map's value
    students_.emplace(move(ufid), move(s));
    return true;
}

bool StudentManager::removeStudent(const string &ufid) {
    return students_.erase(ufid) > 0;
}

bool StudentManager::dropClass(const string &ufid, const string &classCode) {
    auto it = students_.find(ufid);
    if (it == students_.end()) return false;

    vector<string> &codes = it->second.classCodes;
    auto pos = findCode(codes, classCode);
    if (pos == codes.end()) return false;

    codes.erase(pos);
    if (codes.empty()) students_.erase(it);
    return true;
}

bool StudentManager::replaceClass(const string &ufid, const string &oldCode, const string &newCode) {
    auto it = students_.find(ufid);
    if (it == students_.end()) return false;

    vector<string> &codes = it->second.classCodes;
    auto pos = findCode(codes, oldCode);
    if (pos == codes.end()) return false;
    if (findCode(codes, newCode) != codes.end()) return false; // would duplicate

    *pos = newCode;
    return true;
}

int StudentManager::removeClass(const string &classCode) {
    int affected = 0;
    for (auto it = students_.begin(); it != students_.end();) {
        vector<string> &codes = it->second.classCodes;
        auto pos = findCode(codes, classCode);
        if (pos == codes.end()) {
            ++it;
            continue;
        }

        codes.erase(pos);
        ++affected;
        if (codes.empty()) {
            it = students_.erase(it);
        } else {
            ++it;
        }
    }
    return affected;
}
