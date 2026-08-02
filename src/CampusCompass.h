#pragma once

#include <sstream>
#include <string>
#include <unordered_map>

#include "Graph.h"
#include "Models.h"
#include "StudentManager.h"

using namespace std;

class CampusCompass {
public:
    bool ParseCSV(const string &edges_filepath, const string &classes_filepath);

    // parses and executes one command line, returning the exact text to print
    // (e.g. "successful", "unsuccessful", "3", "DNE") with no trailing newline
    string processCommand(const string &command);

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

    // parses one UFID argument (no trailing tokens) and looks it up;
    // nullptr on any parse/validation/lookup failure
    const Student *resolveStudentArg(istringstream &args) const;

    // nullptr if `code` is not a known class
    const ClassInfo *findClass(const string &code) const;
};
