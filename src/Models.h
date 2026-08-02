#pragma once

#include <string>
#include <vector>

using namespace std;

struct Edge {
    int to;
    int time;
    bool open;
};

struct ClassInfo {
    int locationId;
    int startMinutes; // minutes after midnight
    int endMinutes;   // minutes after midnight
};

struct Student {
    string name;
    string ufid;
    int residenceId;
    vector<string> classCodes; // insertion order preserved
};
