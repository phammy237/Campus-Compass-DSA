#include "CampusCompass.h"

#include <algorithm>
#include <exception>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>

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

} // namespace

bool CampusCompass::ParseCSV(const string &edges_filepath, const string &classes_filepath) {
    ifstream edgesFile(edges_filepath);
    if (!edgesFile.is_open()) return false;

    string line;
    getline(edgesFile, line); // discard header row
    while (getline(edgesFile, line)) {
        if (trim(line).empty()) continue;
        vector<string> tokens = splitCsvLine(line);
        if (tokens.size() < 5) continue; // malformed row, skip

        try {
            int id1 = stoi(tokens[0]);
            int id2 = stoi(tokens[1]);
            int time = stoi(tokens[4]);
            graph_.addEdge(id1, id2, time);
        } catch (const exception &) {
            continue; // malformed row, skip
        }
    }

    ifstream classesFile(classes_filepath);
    if (!classesFile.is_open()) return false;

    getline(classesFile, line); // discard header row
    while (getline(classesFile, line)) {
        if (trim(line).empty()) continue;
        vector<string> tokens = splitCsvLine(line);
        if (tokens.size() < 4) continue; // malformed row, skip

        try {
            string code = tokens[0];
            int locationId = stoi(tokens[1]);
            int startMinutes = parseTimeToMinutes(tokens[2]);
            int endMinutes = parseTimeToMinutes(tokens[3]);

            graph_.addVertex(locationId); // register isolated class locations too
            classes_[code] = ClassInfo{locationId, startMinutes, endMinutes};
        } catch (const exception &) {
            continue; // malformed row, skip
        }
    }

    return true;
}

bool CampusCompass::locationExists(int id) const {
    return graph_.hasVertex(id);
}

ShortestPathResult CampusCompass::shortestPath(int from, int to) const {
    ShortestPathResult result;
    if (!graph_.hasVertex(from) || !graph_.hasVertex(to)) return result;

    DijkstraResult dijkstraResult = graph_.dijkstra(from);
    auto it = dijkstraResult.dist.find(to);
    if (it == dijkstraResult.dist.end()) return result; // unreachable

    result.path = graph_.reconstructPath(dijkstraResult, from, to);
    result.totalTime = it->second;
    return result;
}

namespace {
StudentInfo toStudentInfo(const Student &s) {
    StudentInfo info;
    info.found = true;
    info.name = s.name;
    info.ufid = s.ufid;
    info.residenceId = s.residenceId;
    info.classCodes = s.classCodes;
    return info;
}
} // namespace

StudentInfo CampusCompass::getStudentInfo(const string &ufid) const {
    const Student *s = students_.getStudent(ufid);
    if (!s) return StudentInfo{};
    return toStudentInfo(*s);
}

vector<StudentInfo> CampusCompass::allStudents() const {
    vector<StudentInfo> result;
    for (const Student &s : students_.allStudents()) result.push_back(toStudentInfo(s));
    return result;
}

ZoneResult CampusCompass::studentZone(const string &ufid) const {
    const Student *s = students_.getStudent(ufid);
    if (!s) return ZoneResult{};

    set<int> vertices = buildZoneVertices(*s);
    ZoneResult result;
    result.found = true;
    result.cost = graph_.studentZoneMST(vertices);
    result.vertices.assign(vertices.begin(), vertices.end());
    return result;
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
    string trailing;
    if (args >> trailing) return "unsuccessful"; // extra tokens beyond `count`

    set<string> seen;
    for (const string &code : codes) {
        if (!isValidClassCode(code)) return "unsuccessful";
        if (classes_.find(code) == classes_.end()) return "unsuccessful"; // must exist in classes.csv
        if (!seen.insert(code).second) return "unsuccessful"; // duplicate within this insert
    }

    Student s{studentName, ufid, residence, codes};
    if (!students_.insertStudent(s)) return "unsuccessful";
    graph_.addVertex(residence); // register even if residence has no edges yet
    return "successful";
}

string CampusCompass::handleRemove(istringstream &args) {
    string ufid;
    if (!(args >> ufid)) return "unsuccessful";
    string trailing;
    if (args >> trailing) return "unsuccessful";
    if (!isValidUFID(ufid)) return "unsuccessful";

    return students_.removeStudent(ufid) ? "successful" : "unsuccessful";
}

string CampusCompass::handleDropClass(istringstream &args) {
    string ufid, code;
    if (!(args >> ufid >> code)) return "unsuccessful";
    string trailing;
    if (args >> trailing) return "unsuccessful";
    if (!isValidUFID(ufid) || !isValidClassCode(code)) return "unsuccessful";

    return students_.dropClass(ufid, code) ? "successful" : "unsuccessful";
}

string CampusCompass::handleReplaceClass(istringstream &args) {
    string ufid, oldCode, newCode;
    if (!(args >> ufid >> oldCode >> newCode)) return "unsuccessful";
    string trailing;
    if (args >> trailing) return "unsuccessful";
    if (!isValidUFID(ufid) || !isValidClassCode(oldCode) || !isValidClassCode(newCode)) return "unsuccessful";
    if (classes_.find(newCode) == classes_.end()) return "unsuccessful"; // new class must exist in classes.csv

    const Student *s = students_.getStudent(ufid);
    if (!s) return "unsuccessful";
    if (find(s->classCodes.begin(), s->classCodes.end(), oldCode) == s->classCodes.end()) return "unsuccessful";
    if (find(s->classCodes.begin(), s->classCodes.end(), newCode) != s->classCodes.end()) return "unsuccessful"; // would duplicate

    return students_.replaceClass(ufid, oldCode, newCode) ? "successful" : "unsuccessful";
}

string CampusCompass::handleRemoveClass(istringstream &args) {
    string code;
    if (!(args >> code)) return "unsuccessful";
    string trailing;
    if (args >> trailing) return "unsuccessful";
    if (!isValidClassCode(code)) return "unsuccessful";

    int affected = students_.removeClass(code);
    if (affected == 0) return "unsuccessful";
    return to_string(affected);
}

string CampusCompass::handleToggleEdgesClosure(istringstream &args) {
    string countTok;
    if (!(args >> countTok)) return "unsuccessful";
    int count;
    if (!tryParseInt(countTok, count) || count < 1) return "unsuccessful";

    vector<pair<int, int>> pairs;
    for (int i = 0; i < count; ++i) {
        string tok1, tok2;
        if (!(args >> tok1 >> tok2)) return "unsuccessful"; // fewer than `count` pairs supplied
        int id1, id2;
        if (!tryParseInt(tok1, id1) || !tryParseInt(tok2, id2)) return "unsuccessful";
        if (graph_.getEdgeStatus(id1, id2) == EdgeStatus::DNE) return "unsuccessful";
        pairs.emplace_back(id1, id2);
    }
    string trailing;
    if (args >> trailing) return "unsuccessful"; // extra tokens beyond `count` pairs

    for (const auto &[id1, id2] : pairs) graph_.toggleEdge(id1, id2);
    return "successful";
}

string CampusCompass::handleCheckEdgeStatus(istringstream &args) {
    string tok1, tok2;
    if (!(args >> tok1 >> tok2)) return "unsuccessful";
    string trailing;
    if (args >> trailing) return "unsuccessful";

    int id1, id2;
    if (!tryParseInt(tok1, id1) || !tryParseInt(tok2, id2)) return "unsuccessful";

    switch (graph_.getEdgeStatus(id1, id2)) {
        case EdgeStatus::Open: return "open";
        case EdgeStatus::Closed: return "closed";
        default: return "DNE";
    }
}

string CampusCompass::handleIsConnected(istringstream &args) {
    string trailing;
    if (args >> trailing) return "unsuccessful"; // malformed: isConnected takes no arguments

    return graph_.isConnected() ? "true" : "false";
}

string CampusCompass::handlePrintShortestEdges(istringstream &args) {
    string ufid;
    if (!(args >> ufid)) return "unsuccessful";
    string trailing;
    if (args >> trailing) return "unsuccessful";
    if (!isValidUFID(ufid)) return "unsuccessful";

    const Student *s = students_.getStudent(ufid);
    if (!s) return "unsuccessful";

    DijkstraResult result = graph_.dijkstra(s->residenceId);

    vector<string> sortedCodes = s->classCodes;
    sort(sortedCodes.begin(), sortedCodes.end());

    ostringstream out;
    out << "Time For Shortest Edges: " << s->name;
    for (const string &code : sortedCodes) {
        auto classIt = classes_.find(code);
        if (classIt == classes_.end()) {
            out << "\n" << code << ": " << -1;
            continue;
        }
        auto distIt = result.dist.find(classIt->second.locationId);
        int time = (distIt != result.dist.end()) ? distIt->second : -1;
        out << "\n" << code << ": " << time;
    }
    return out.str();
}

string CampusCompass::handlePrintStudentZone(istringstream &args) {
    string ufid;
    if (!(args >> ufid)) return "unsuccessful";
    string trailing;
    if (args >> trailing) return "unsuccessful";
    if (!isValidUFID(ufid)) return "unsuccessful";

    const Student *s = students_.getStudent(ufid);
    if (!s) return "unsuccessful";

    set<int> zoneVertices = buildZoneVertices(*s);
    int cost = graph_.studentZoneMST(zoneVertices);

    ostringstream out;
    out << "Student Zone Cost For " << s->name << ": " << cost;
    return out.str();
}

set<int> CampusCompass::buildZoneVertices(const Student &s) const {
    DijkstraResult result = graph_.dijkstra(s.residenceId);

    set<int> zoneVertices;
    zoneVertices.insert(s.residenceId);
    for (const string &code : s.classCodes) {
        auto classIt = classes_.find(code);
        if (classIt == classes_.end()) continue;
        vector<int> path = graph_.reconstructPath(result, s.residenceId, classIt->second.locationId);
        for (int v : path) zoneVertices.insert(v); // unreachable classes yield an empty path, contributing nothing
    }
    return zoneVertices;
}

string CampusCompass::handleVerifySchedule(istringstream &args) {
    string ufid;
    if (!(args >> ufid)) return "unsuccessful";
    string trailing;
    if (args >> trailing) return "unsuccessful";
    if (!isValidUFID(ufid)) return "unsuccessful";

    const Student *s = students_.getStudent(ufid);
    if (!s) return "unsuccessful";
    if (s->classCodes.size() < 2) return "unsuccessful"; // no consecutive pair to check

    vector<string> byStart = s->classCodes;
    stable_sort(byStart.begin(), byStart.end(), [this](const string &a, const string &b) {
        return classes_.at(a).startMinutes < classes_.at(b).startMinutes;
    });

    ostringstream out;
    for (size_t i = 0; i + 1 < byStart.size(); ++i) {
        const ClassInfo &current = classes_.at(byStart[i]);
        const ClassInfo &next = classes_.at(byStart[i + 1]);
        int gap = next.startMinutes - current.endMinutes;

        DijkstraResult result = graph_.dijkstra(current.locationId);
        auto it = result.dist.find(next.locationId);
        bool reachable = it != result.dist.end();

        bool ok = reachable && gap >= it->second;
        if (i > 0) out << "\n";
        out << (ok ? "successful" : "unsuccessful");
    }
    return out.str();
}
