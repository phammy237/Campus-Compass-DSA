#pragma once

#include <string>
#include <unordered_map>

#include "Models.h"

using namespace std;

class StudentManager {
public:
    bool hasStudent(const string &ufid) const;
    const Student *getStudent(const string &ufid) const; // nullptr if absent

    bool insertStudent(Student s);  // false if ufid already exists
    bool removeStudent(const string &ufid); // false if ufid not present

    // removes classCode from the student's list; if that leaves them with zero
    // classes, the student is removed entirely
    bool dropClass(const string &ufid, const string &classCode);

    // false if the student lacks oldCode, or already has newCode (would duplicate)
    bool replaceClass(const string &ufid, const string &oldCode, const string &newCode);

    // removes classCode from every student who has it, deleting any student left
    // with zero classes; returns how many students had classCode in their list
    int removeClass(const string &classCode);

private:
    unordered_map<string, Student> students_;
};
