#include "CampusCompass.h"

#include <algorithm>
#include <exception>
#include <fstream>
#include <functional>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

#include "Validation.h"

using namespace std;

namespace {

// trims trailing '\r' (Windows line endings) and surrounding whitespace
string trim(const string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

vector<string> splitCsvLine(const string &line) {
    vector<string> tokens;
    stringstream ss(line);
    string token;
    while (getline(ss, token, ',')) {
        tokens.push_back(trim(token));
    }
    return tokens;
}

// "HH:MM" -> minutes after midnight; returns -1 on malformed input
int parseTimeToMinutes(const string &hhmm) {
    size_t colon = hhmm.find(':');
    if (colon == string::npos) return -1;
    try {
        int hours = stoi(hhmm.substr(0, colon));
        int minutes = stoi(hhmm.substr(colon + 1));
        return hours * 60 + minutes;
    } catch (const exception &) {
        return -1;
    }
}

// syntax-checks then converts; false (and untouched `out`) on any failure,
// including out-of-range values that isInteger's syntax check can't catch
bool tryParseInt(const string &token, int &out) {
    if (!isInteger(token)) return false;
    try {
        out = stoi(token);
        return true;
    } catch (const exception &) {
        return false;
    }
}

// true if there's anything left to read after a command's expected arguments
bool hasTrailingTokens(istringstream &args) {
    string extra;
    return static_cast<bool>(args >> extra);
}

// parses a single UFID argument with no trailing tokens; false on any failure
bool parseSingleUfidArg(istringstream &args, string &ufid) {
    if (!(args >> ufid)) return false;
    if (hasTrailingTokens(args)) return false;
    return isValidUFID(ufid);
}

// parses two integer location-ID arguments with no trailing tokens
bool parseVertexPair(istringstream &args, int &id1, int &id2) {
    string tok1, tok2;
    if (!(args >> tok1 >> tok2)) return false;
    if (hasTrailingTokens(args)) return false;
    return tryParseInt(tok1, id1) && tryParseInt(tok2, id2);
}

// opens `path`, discards the header row, then calls `handleRow` for each
// non-blank row with at least `minTokens` fields; rows with too few fields,
// or for which `handleRow` throws, are skipped. false if the file can't be opened.
bool readCsvRows(const string &path, size_t minTokens,
                  const function<void(const vector<string> &)> &handleRow) {
    ifstream file(path);
    if (!file.is_open()) return false;

    string line;
    getline(file, line); // discard header row
    while (getline(file, line)) {
        if (trim(line).empty()) continue;
        vector<string> tokens = splitCsvLine(line);
        if (tokens.size() < minTokens) continue; // malformed row, skip

        try {
            handleRow(tokens);
        } catch (const exception &) {
            continue; // malformed row, skip
        }
    }
    return true;
}

} // namespace

bool CampusCompass::ParseCSV(const string &edges_filepath, const string &classes_filepath) {
    bool edgesOk = readCsvRows(edges_filepath, 5, [this](const vector<string> &tokens) {
        int id1 = stoi(tokens[0]);
        int id2 = stoi(tokens[1]);
        int time = stoi(tokens[4]);
        graph_.addEdge(id1, id2, time);
    });
    if (!edgesOk) return false;

    return readCsvRows(classes_filepath, 4, [this](const vector<string> &tokens) {
        const string &code = tokens[0];
        int locationId = stoi(tokens[1]);
        int startMinutes = parseTimeToMinutes(tokens[2]);
        int endMinutes = parseTimeToMinutes(tokens[3]);

        graph_.addVertex(locationId); // register isolated class locations too
        classes_[code] = ClassInfo{locationId, startMinutes, endMinutes};
    });
}

string CampusCompass::processCommand(const string &command) {
    istringstream iss(command);
    string name;
    if (!(iss >> name)) return "unsuccessful";

    if (name == "insert") return handleInsert(iss);
    if (name == "remove") return handleRemove(iss);
    if (name == "dropClass") return handleDropClass(iss);
    if (name == "replaceClass") return handleReplaceClass(iss);
    if (name == "removeClass") return handleRemoveClass(iss);
    if (name == "toggleEdgesClosure") return handleToggleEdgesClosure(iss);
    if (name == "checkEdgeStatus") return handleCheckEdgeStatus(iss);
    if (name == "isConnected") return handleIsConnected(iss);
    if (name == "printShortestEdges") return handlePrintShortestEdges(iss);
    if (name == "printStudentZone") return handlePrintStudentZone(iss);
    if (name == "verifySchedule") return handleVerifySchedule(iss);

    return "unsuccessful"; // unknown/misspelled command
}

const Student *CampusCompass::resolveStudentArg(istringstream &args) const {
    string ufid;
    if (!parseSingleUfidArg(args, ufid)) return nullptr;
    return students_.getStudent(ufid);
}

const ClassInfo *CampusCompass::findClass(const string &code) const {
    auto it = classes_.find(code);
    return it == classes_.end() ? nullptr : &it->second;
}

string CampusCompass::handleInsert(istringstream &args) {
    string studentName;
    if (!(args >> quoted(studentName))) return "unsuccessful";
    if (!isValidName(studentName)) return "unsuccessful";

    string ufid, residenceTok, countTok;
    if (!(args >> ufid >> residenceTok >> countTok)) return "unsuccessful";
    if (!isValidUFID(ufid)) return "unsuccessful";
    if (students_.hasStudent(ufid)) return "unsuccessful";

    int residence, count;
    if (!tryParseInt(residenceTok, residence)) return "unsuccessful";
    if (!tryParseInt(countTok, count)) return "unsuccessful";
    if (count < 1 || count > 6) return "unsuccessful";

    vector<string> codes;
    for (int i = 0; i < count; ++i) {
        string code;
        if (!(args >> code)) return "unsuccessful"; // fewer than `count` codes supplied
        codes.push_back(code);
    }
    if (hasTrailingTokens(args)) return "unsuccessful"; // extra tokens beyond `count`

    set<string> seen;
    for (const string &code : codes) {
        if (!isValidClassCode(code)) return "unsuccessful";
        if (!findClass(code)) return "unsuccessful"; // must exist in classes.csv
        if (!seen.insert(code).second) return "unsuccessful"; // duplicate within this insert
    }

    Student s{move(studentName), move(ufid), residence, move(codes)};
    if (!students_.insertStudent(move(s))) return "unsuccessful";
    graph_.addVertex(residence); // register even if residence has no edges yet
    return "successful";
}

string CampusCompass::handleRemove(istringstream &args) {
    string ufid;
    if (!parseSingleUfidArg(args, ufid)) return "unsuccessful";

    return students_.removeStudent(ufid) ? "successful" : "unsuccessful";
}

string CampusCompass::handleDropClass(istringstream &args) {
    string ufid, code;
    if (!(args >> ufid >> code)) return "unsuccessful";
    if (hasTrailingTokens(args)) return "unsuccessful";
    if (!isValidUFID(ufid) || !isValidClassCode(code)) return "unsuccessful";

    return students_.dropClass(ufid, code) ? "successful" : "unsuccessful";
}

string CampusCompass::handleReplaceClass(istringstream &args) {
    string ufid, oldCode, newCode;
    if (!(args >> ufid >> oldCode >> newCode)) return "unsuccessful";
    if (hasTrailingTokens(args)) return "unsuccessful";
    if (!isValidUFID(ufid) || !isValidClassCode(oldCode) || !isValidClassCode(newCode)) return "unsuccessful";
    if (!findClass(newCode)) return "unsuccessful"; // new class must exist in classes.csv

    return students_.replaceClass(ufid, oldCode, newCode) ? "successful" : "unsuccessful";
}

string CampusCompass::handleRemoveClass(istringstream &args) {
    string code;
    if (!(args >> code)) return "unsuccessful";
    if (hasTrailingTokens(args)) return "unsuccessful";
    if (!isValidClassCode(code)) return "unsuccessful";

    int affected = students_.removeClass(code);
    if (affected == 0) return "unsuccessful";
    return to_string(affected);
}

string CampusCompass::handleToggleEdgesClosure(istringstream &args) {
    string countTok;
    if (!(args >> countTok)) return "unsuccessful";
    int count;
    if (!tryParseInt(countTok, count)) return "unsuccessful";
    if (count < 1) return "unsuccessful";

    vector<pair<int, int>> edges;
    for (int i = 0; i < count; ++i) {
        int id1, id2;
        string tok1, tok2;
        if (!(args >> tok1 >> tok2)) return "unsuccessful"; // fewer than `count` pairs supplied
        if (!tryParseInt(tok1, id1) || !tryParseInt(tok2, id2)) return "unsuccessful";
        edges.emplace_back(id1, id2);
    }
    if (hasTrailingTokens(args)) return "unsuccessful"; // extra tokens beyond `count`

    // every edge must exist before mutating any of them (all-or-nothing)
    for (const auto &[id1, id2] : edges) {
        if (graph_.getEdgeStatus(id1, id2) == EdgeStatus::DNE) return "unsuccessful";
    }
    for (const auto &[id1, id2] : edges) {
        graph_.toggleEdge(id1, id2);
    }
    return "successful";
}

string CampusCompass::handleCheckEdgeStatus(istringstream &args) {
    int id1, id2;
    if (!parseVertexPair(args, id1, id2)) return "unsuccessful";

    switch (graph_.getEdgeStatus(id1, id2)) {
        case EdgeStatus::Open: return "open";
        case EdgeStatus::Closed: return "closed";
        default: return "DNE";
    }
}

string CampusCompass::handleIsConnected(istringstream &args) {
    int id1, id2;
    if (!parseVertexPair(args, id1, id2)) return "unsuccessful";

    return graph_.isConnected(id1, id2) ? "successful" : "unsuccessful";
}

string CampusCompass::handlePrintShortestEdges(istringstream &args) {
    const Student *s = resolveStudentArg(args);
    if (!s) return "unsuccessful";

    DijkstraResult result = graph_.dijkstra(s->residenceId);

    vector<string> sortedCodes = s->classCodes;
    sort(sortedCodes.begin(), sortedCodes.end());

    ostringstream out;
    out << "Time For Shortest Edges: " << s->name;
    for (const string &code : sortedCodes) {
        const ClassInfo *info = findClass(code);
        if (!info) {
            out << "\n" << code << ": " << -1;
            continue;
        }
        auto distIt = result.dist.find(info->locationId);
        int time = (distIt != result.dist.end()) ? distIt->second : -1;
        out << "\n" << code << ": " << time;
    }
    return out.str();
}

string CampusCompass::handlePrintStudentZone(istringstream &args) {
    const Student *s = resolveStudentArg(args);
    if (!s) return "unsuccessful";

    DijkstraResult result = graph_.dijkstra(s->residenceId);

    unordered_set<int> zoneVertices;
    zoneVertices.insert(s->residenceId);
    for (const string &code : s->classCodes) {
        const ClassInfo *info = findClass(code);
        if (!info) continue;
        vector<int> path = graph_.reconstructPath(result, s->residenceId, info->locationId);
        for (int v : path) zoneVertices.insert(v); // unreachable classes yield an empty path, contributing nothing
    }

    int cost = graph_.inducedMST(zoneVertices);

    ostringstream out;
    out << "Student Zone Cost For " << s->name << ": " << cost;
    return out.str();
}

string CampusCompass::handleVerifySchedule(istringstream &args) {
    const Student *s = resolveStudentArg(args);
    if (!s) return "unsuccessful";
    if (s->classCodes.size() < 2) return "unsuccessful"; // no consecutive pair to check

    vector<string> byStart = s->classCodes;
    stable_sort(byStart.begin(), byStart.end(), [this](const string &a, const string &b) {
        return classes_.at(a).startMinutes < classes_.at(b).startMinutes;
    });

    ostringstream out;
    const ClassInfo *current = &classes_.at(byStart[0]);
    for (size_t i = 0; i + 1 < byStart.size(); ++i) {
        const ClassInfo &next = classes_.at(byStart[i + 1]);
        int gap = next.startMinutes - current->endMinutes;

        DijkstraResult result = graph_.dijkstra(current->locationId);
        auto it = result.dist.find(next.locationId);
        bool reachable = it != result.dist.end();

        bool ok = reachable && gap >= it->second;
        if (i > 0) out << "\n";
        out << (ok ? "successful" : "unsuccessful");

        current = &next;
    }
    return out.str();
}
