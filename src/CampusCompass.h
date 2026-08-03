#pragma once

#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "Graph.h"
#include "Models.h"
#include "StudentManager.h"

using namespace std;

struct ShortestPathResult {
    vector<int> path;    // ordered vertex ids from source to target; empty if unreachable
    int totalTime = -1;  // -1 if unreachable
};

struct StudentInfo {
    bool found = false;
    string name;
    string ufid;
    int residenceId = -1;
    vector<string> classCodes;
};

struct ZoneResult {
    bool found = false;
    int cost = -1;
    vector<int> vertices; // residence plus every vertex on a shortest path to any of the student's classes
};

class CampusCompass {
public:
    bool ParseCSV(const string &edges_filepath, const string &classes_filepath);

    // parses and executes one command line, returning the exact text to print
    // (e.g. "successful", "unsuccessful", "3", "DNE") with no trailing newline
    string processCommand(const string &command);

    // read-only queries used by the API layer, bypassing the string command grammar
    bool locationExists(int id) const;
    ShortestPathResult shortestPath(int from, int to) const;
    StudentInfo getStudentInfo(const string &ufid) const;
    vector<StudentInfo> allStudents() const;
    ZoneResult studentZone(const string &ufid) const;

private:
    Graph graph_;
    StudentManager students_;
    unordered_map<string, ClassInfo> classes_;

    // each handler consumes its arguments from `args` (positioned right after
    // the command name) and returns the exact text to print
    string handleInsert(istringstream &args);
    string handleRemove(istringstream &args);
    string handleDropClass(istringstream &args);
    string handleReplaceClass(istringstream &args);
    string handleRemoveClass(istringstream &args);

    string handleToggleEdgesClosure(istringstream &args);
    string handleCheckEdgeStatus(istringstream &args);
    string handleIsConnected(istringstream &args);

    string handlePrintShortestEdges(istringstream &args);
    string handlePrintStudentZone(istringstream &args);
    string handleVerifySchedule(istringstream &args);

    // residence plus every vertex on a shortest path from it to one of `s`'s classes;
    // shared by handlePrintStudentZone and the structured studentZone() query
    set<int> buildZoneVertices(const Student &s) const;

    // parses one UFID argument (no trailing tokens) and looks it up;
    // nullptr on any parse/validation/lookup failure
    const Student *resolveStudentArg(istringstream &args) const;

    // nullptr if `code` is not a known class
    const ClassInfo *findClass(const string &code) const;
};
