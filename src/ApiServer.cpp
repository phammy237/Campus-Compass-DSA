#include "ApiServer.h"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "httplib.h"

#include "CampusCompass.h"
#include "Locations.h"
#include "ScheduleBuilder.h"

using namespace std;

namespace {

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

struct EdgeRecord {
    int id1, id2, time;
};

// re-reads data/edges.csv for topology (id pairs + travel time); open/closed
// status is queried live from CampusCompass so it reflects any toggles
vector<EdgeRecord> loadEdgeList(const string &filepath) {
    vector<EdgeRecord> edges;
    ifstream file(filepath);
    if (!file.is_open()) return edges;

    string line;
    getline(file, line); // discard header row
    while (getline(file, line)) {
        if (trim(line).empty()) continue;
        vector<string> tokens = splitCsvLine(line);
        if (tokens.size() < 5) continue;

        try {
            int id1 = stoi(tokens[0]);
            int id2 = stoi(tokens[1]);
            int time = stoi(tokens[4]);
            edges.push_back(EdgeRecord{id1, id2, time});
        } catch (const exception &) {
            continue;
        }
    }
    return edges;
}

vector<int> sortedKeys(const unordered_map<int, Location> &locations) {
    vector<int> ids;
    ids.reserve(locations.size());
    for (const auto &[id, loc] : locations) ids.push_back(id);
    sort(ids.begin(), ids.end());
    return ids;
}

struct ClassRecord {
    string code;
    int locationId;
    string startText, endText;
    int startMinutes, endMinutes;
};

// "HH:MM" -> minutes after midnight; -1 on malformed input
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

// minutes-after-midnight -> "HH:MM"
string minutesToClock(int minutes) {
    if (minutes < 0) return "?";
    int h = (minutes / 60) % 24;
    int m = minutes % 60;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
    return string(buf);
}

string locationNameOr(const unordered_map<int, Location> &locations, int id) {
    auto it = locations.find(id);
    return it == locations.end() ? ("#" + to_string(id)) : it->second.name;
}

// re-reads data/classes.csv; the catalog is immutable after startup (removeClass
// only unenrolls students, it never deletes the catalog entry), so this stays fresh
vector<ClassRecord> loadClassList(const string &filepath) {
    vector<ClassRecord> classes;
    ifstream file(filepath);
    if (!file.is_open()) return classes;

    string line;
    getline(file, line); // discard header row
    while (getline(file, line)) {
        if (trim(line).empty()) continue;
        vector<string> tokens = splitCsvLine(line);
        if (tokens.size() < 4) continue;

        try {
            string code = tokens[0];
            int locationId = stoi(tokens[1]);
            string startText = tokens[2];
            string endText = tokens[3];
            classes.push_back(ClassRecord{code, locationId, startText, endText,
                                           parseTimeToMinutes(startText), parseTimeToMinutes(endText)});
        } catch (const exception &) {
            continue;
        }
    }
    return classes;
}

string studentInfoToJson(const StudentInfo &info) {
    ostringstream out;
    out << "{\"found\":" << (info.found ? "true" : "false");
    if (info.found) {
        out << ",\"name\":\"" << jsonEscape(info.name) << "\",\"ufid\":\"" << jsonEscape(info.ufid)
            << "\",\"residenceId\":" << info.residenceId << ",\"classCodes\":[";
        for (size_t i = 0; i < info.classCodes.size(); ++i) {
            if (i > 0) out << ",";
            out << "\"" << jsonEscape(info.classCodes[i]) << "\"";
        }
        out << "]";
    }
    out << "}";
    return out.str();
}

} // namespace

string jsonEscape(const string &s) {
    string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

int main() {
    CampusCompass compass;
    if (!compass.ParseCSV("data/edges.csv", "data/classes.csv")) {
        cerr << "Failed to load data/edges.csv or data/classes.csv" << endl;
        return 1;
    }

    unordered_map<int, Location> locations = loadLocations("data/locations.csv");
    vector<EdgeRecord> edgeList = loadEdgeList("data/edges.csv");
    vector<ClassRecord> classList = loadClassList("data/classes.csv");
    unordered_map<string, ClassRecord> classByCode;
    for (const ClassRecord &c : classList) classByCode[c.code] = c;

    ScheduleBuilder scheduleBuilder;
    if (!scheduleBuilder.load("data/synthetic_edges.csv", "data/course_sections.csv")) {
        cerr << "Failed to load data/synthetic_edges.csv or data/course_sections.csv" << endl;
        return 1;
    }

    httplib::Server svr;
    svr.set_mount_point("/", "web");

    svr.Get("/api/locations", [&](const httplib::Request &, httplib::Response &res) {
        ostringstream out;
        out << "[";
        bool first = true;
        for (int id : sortedKeys(locations)) {
            const Location &loc = locations.at(id);
            if (!first) out << ",";
            first = false;
            out << "{\"id\":" << loc.id << ",\"name\":\"" << jsonEscape(loc.name)
                << "\",\"lat\":" << loc.lat << ",\"lng\":" << loc.lng << "}";
        }
        out << "]";
        res.set_content(out.str(), "application/json");
    });

    svr.Get("/api/edges", [&](const httplib::Request &, httplib::Response &res) {
        ostringstream out;
        out << "[";
        bool first = true;
        for (const EdgeRecord &e : edgeList) {
            string statusText = compass.processCommand("checkEdgeStatus " + to_string(e.id1) + " " + to_string(e.id2));
            if (statusText == "DNE") continue;
            bool open = (statusText == "open");
            if (!first) out << ",";
            first = false;
            out << "{\"from\":" << e.id1 << ",\"to\":" << e.id2 << ",\"time\":" << e.time
                << ",\"open\":" << (open ? "true" : "false") << "}";
        }
        out << "]";
        res.set_content(out.str(), "application/json");
    });

    svr.Get("/api/shortest-path", [&](const httplib::Request &req, httplib::Response &res) {
        if (!req.has_param("from") || !req.has_param("to")) {
            res.status = 400;
            res.set_content("{\"error\":\"missing 'from' or 'to' query parameter\"}", "application/json");
            return;
        }
        int from, to;
        try {
            from = stoi(req.get_param_value("from"));
            to = stoi(req.get_param_value("to"));
        } catch (const exception &) {
            res.status = 400;
            res.set_content("{\"error\":\"'from' and 'to' must be integers\"}", "application/json");
            return;
        }

        ShortestPathResult result = compass.shortestPath(from, to);

        ostringstream out;
        out << "{\"path\":[";
        for (size_t i = 0; i < result.path.size(); ++i) {
            if (i > 0) out << ",";
            out << result.path[i];
        }
        out << "],\"totalTime\":" << result.totalTime << ",\"coordinates\":[";
        bool first = true;
        for (int id : result.path) {
            auto it = locations.find(id);
            if (it == locations.end()) continue;
            if (!first) out << ",";
            first = false;
            out << "[" << it->second.lat << "," << it->second.lng << "]";
        }
        out << "]}";
        res.set_content(out.str(), "application/json");
    });

    svr.Post("/api/edges/toggle", [&](const httplib::Request &req, httplib::Response &res) {
        if (!req.has_param("id1") || !req.has_param("id2")) {
            res.status = 400;
            res.set_content("{\"error\":\"missing 'id1' or 'id2' query parameter\"}", "application/json");
            return;
        }
        string id1 = req.get_param_value("id1");
        string id2 = req.get_param_value("id2");

        string toggleResult = compass.processCommand("toggleEdgesClosure 1 " + id1 + " " + id2);
        if (toggleResult != "successful") {
            res.status = 400;
            res.set_content("{\"error\":\"edge does not exist\"}", "application/json");
            return;
        }

        string statusText = compass.processCommand("checkEdgeStatus " + id1 + " " + id2);
        res.set_content("{\"status\":\"" + statusText + "\"}", "application/json");
    });

    svr.Get("/api/isConnected", [&](const httplib::Request &, httplib::Response &res) {
        string result = compass.processCommand("isConnected");
        res.set_content(string("{\"connected\":") + (result == "true" ? "true" : "false") + "}", "application/json");
    });

    svr.Get("/api/classes", [&](const httplib::Request &, httplib::Response &res) {
        ostringstream out;
        out << "[";
        bool first = true;
        for (const ClassRecord &c : classList) {
            if (!first) out << ",";
            first = false;
            out << "{\"code\":\"" << jsonEscape(c.code) << "\",\"locationId\":" << c.locationId
                << ",\"start\":\"" << jsonEscape(c.startText) << "\",\"end\":\"" << jsonEscape(c.endText)
                << "\",\"startMinutes\":" << c.startMinutes << ",\"endMinutes\":" << c.endMinutes << "}";
        }
        out << "]";
        res.set_content(out.str(), "application/json");
    });

    // DELETE, not POST: cpp-httplib has no way to know a bodyless POST/PUT/PATCH
    // has finished (it blocks reading an unbounded body when Content-Length is
    // absent), but it special-cases DELETE to skip that read entirely
    svr.Delete(R"(/api/classes/(\w+))", [&](const httplib::Request &req, httplib::Response &res) {
        string code = req.matches[1];
        string result = compass.processCommand("removeClass " + code);
        res.set_content("{\"result\":\"" + jsonEscape(result) + "\"}", "application/json");
    });

    svr.Get("/api/students", [&](const httplib::Request &, httplib::Response &res) {
        ostringstream out;
        out << "[";
        bool first = true;
        for (const StudentInfo &info : compass.allStudents()) {
            if (!first) out << ",";
            first = false;
            out << studentInfoToJson(info);
        }
        out << "]";
        res.set_content(out.str(), "application/json");
    });

    svr.Post("/api/students", [&](const httplib::Request &req, httplib::Response &res) {
        if (!req.has_param("name") || !req.has_param("ufid") || !req.has_param("residence") ||
            !req.has_param("codes")) {
            res.status = 400;
            res.set_content("{\"error\":\"missing required field(s): name, ufid, residence, codes\"}",
                             "application/json");
            return;
        }
        string name = req.get_param_value("name");
        string ufid = req.get_param_value("ufid");
        string residence = req.get_param_value("residence");
        vector<string> codes = splitCsvLine(req.get_param_value("codes"));

        ostringstream cmd;
        cmd << "insert " << quoted(name) << " " << ufid << " " << residence << " " << codes.size();
        for (const string &c : codes) cmd << " " << c;

        string result = compass.processCommand(cmd.str());
        res.set_content("{\"result\":\"" + jsonEscape(result) + "\"}", "application/json");
    });

    svr.Get(R"(/api/students/(\w+))", [&](const httplib::Request &req, httplib::Response &res) {
        string ufid = req.matches[1];
        StudentInfo info = compass.getStudentInfo(ufid);
        if (!info.found) {
            res.status = 404;
            res.set_content("{\"error\":\"student not found\"}", "application/json");
            return;
        }
        res.set_content(studentInfoToJson(info), "application/json");
    });

    svr.Delete(R"(/api/students/(\w+))", [&](const httplib::Request &req, httplib::Response &res) {
        string ufid = req.matches[1];
        string result = compass.processCommand("remove " + ufid);
        res.set_content("{\"result\":\"" + jsonEscape(result) + "\"}", "application/json");
    });

    svr.Post(R"(/api/students/(\w+)/dropClass)", [&](const httplib::Request &req, httplib::Response &res) {
        string ufid = req.matches[1];
        if (!req.has_param("code")) {
            res.status = 400;
            res.set_content("{\"error\":\"missing 'code' query parameter\"}", "application/json");
            return;
        }
        string code = req.get_param_value("code");
        string result = compass.processCommand("dropClass " + ufid + " " + code);
        res.set_content("{\"result\":\"" + jsonEscape(result) + "\"}", "application/json");
    });

    svr.Post(R"(/api/students/(\w+)/replaceClass)", [&](const httplib::Request &req, httplib::Response &res) {
        string ufid = req.matches[1];
        if (!req.has_param("oldCode") || !req.has_param("newCode")) {
            res.status = 400;
            res.set_content("{\"error\":\"missing 'oldCode' or 'newCode' query parameter\"}", "application/json");
            return;
        }
        string oldCode = req.get_param_value("oldCode");
        string newCode = req.get_param_value("newCode");
        string result = compass.processCommand("replaceClass " + ufid + " " + oldCode + " " + newCode);
        res.set_content("{\"result\":\"" + jsonEscape(result) + "\"}", "application/json");
    });

    svr.Get(R"(/api/students/(\w+)/shortest-edges)", [&](const httplib::Request &req, httplib::Response &res) {
        string ufid = req.matches[1];
        StudentInfo info = compass.getStudentInfo(ufid);
        if (!info.found) {
            res.status = 404;
            res.set_content("{\"error\":\"student not found\"}", "application/json");
            return;
        }
        ostringstream out;
        out << "{\"studentName\":\"" << jsonEscape(info.name) << "\",\"edges\":[";
        bool first = true;
        for (const string &code : info.classCodes) {
            auto it = classByCode.find(code);
            int time = -1;
            int locationId = -1;
            if (it != classByCode.end()) {
                locationId = it->second.locationId;
                time = compass.shortestPath(info.residenceId, locationId).totalTime;
            }
            if (!first) out << ",";
            first = false;
            out << "{\"code\":\"" << jsonEscape(code) << "\",\"locationId\":" << locationId
                << ",\"time\":" << time << "}";
        }
        out << "]}";
        res.set_content(out.str(), "application/json");
    });

    svr.Get(R"(/api/students/(\w+)/zone)", [&](const httplib::Request &req, httplib::Response &res) {
        string ufid = req.matches[1];
        ZoneResult zone = compass.studentZone(ufid);
        if (!zone.found) {
            res.status = 404;
            res.set_content("{\"error\":\"student not found\"}", "application/json");
            return;
        }
        ostringstream out;
        out << "{\"cost\":" << zone.cost << ",\"vertices\":[";
        for (size_t i = 0; i < zone.vertices.size(); ++i) {
            if (i > 0) out << ",";
            out << zone.vertices[i];
        }
        out << "],\"coordinates\":[";
        bool first = true;
        for (int id : zone.vertices) {
            auto it = locations.find(id);
            if (it == locations.end()) continue;
            if (!first) out << ",";
            first = false;
            out << "[" << it->second.lat << "," << it->second.lng << "]";
        }
        out << "]}";
        res.set_content(out.str(), "application/json");
    });

    svr.Get(R"(/api/students/(\w+)/verify-schedule)", [&](const httplib::Request &req, httplib::Response &res) {
        string ufid = req.matches[1];
        StudentInfo info = compass.getStudentInfo(ufid);
        if (!info.found) {
            res.status = 404;
            res.set_content("{\"error\":\"student not found\"}", "application/json");
            return;
        }

        // sort this student's classes by start time, mirroring handleVerifySchedule
        vector<string> byStart = info.classCodes;
        stable_sort(byStart.begin(), byStart.end(), [&](const string &a, const string &b) {
            auto ia = classByCode.find(a);
            auto ib = classByCode.find(b);
            int sa = (ia != classByCode.end()) ? ia->second.startMinutes : 0;
            int sb = (ib != classByCode.end()) ? ib->second.startMinutes : 0;
            return sa < sb;
        });

        ostringstream out;
        out << "{\"pairs\":[";
        bool first = true;
        for (size_t i = 0; i + 1 < byStart.size(); ++i) {
            auto ia = classByCode.find(byStart[i]);
            auto ib = classByCode.find(byStart[i + 1]);
            if (ia == classByCode.end() || ib == classByCode.end()) continue;
            const ClassRecord &current = ia->second;
            const ClassRecord &next = ib->second;
            int gap = next.startMinutes - current.endMinutes;
            ShortestPathResult sp = compass.shortestPath(current.locationId, next.locationId);
            bool reachable = sp.totalTime >= 0;
            bool ok = reachable && gap >= sp.totalTime;

            if (!first) out << ",";
            first = false;
            out << "{\"from\":\"" << jsonEscape(byStart[i]) << "\",\"to\":\"" << jsonEscape(byStart[i + 1])
                << "\",\"gap\":" << gap << ",\"travelTime\":" << sp.totalTime
                << ",\"ok\":" << (ok ? "true" : "false") << "}";
        }
        out << "]}";
        res.set_content(out.str(), "application/json");
    });

    svr.Get("/api/schedule-builder/courses", [&](const httplib::Request &, httplib::Response &res) {
        ostringstream out;
        out << "[";
        bool first = true;
        for (const string &code : scheduleBuilder.availableCourses()) {
            if (!first) out << ",";
            first = false;
            out << "{\"code\":\"" << jsonEscape(code) << "\",\"sections\":[";
            bool firstSection = true;
            for (const Section &s : scheduleBuilder.sectionsFor(code)) {
                if (!firstSection) out << ",";
                firstSection = false;
                out << "{\"sectionId\":\"" << jsonEscape(s.sectionId) << "\",\"locationId\":" << s.locationId
                    << ",\"locationName\":\"" << jsonEscape(locationNameOr(locations, s.locationId))
                    << "\",\"start\":\"" << minutesToClock(s.startMinutes) << "\",\"end\":\""
                    << minutesToClock(s.endMinutes) << "\"}";
            }
            out << "]}";
        }
        out << "]";
        res.set_content(out.str(), "application/json");
    });

    svr.Post("/api/schedule-builder/recommend", [&](const httplib::Request &req, httplib::Response &res) {
        if (!req.has_param("home") || !req.has_param("codes")) {
            res.status = 400;
            res.set_content("{\"error\":\"missing 'home' or 'codes' parameter\"}", "application/json");
            return;
        }
        int home;
        try {
            home = stoi(req.get_param_value("home"));
        } catch (const exception &) {
            res.status = 400;
            res.set_content("{\"error\":\"'home' must be an integer location id\"}", "application/json");
            return;
        }
        vector<string> codes = splitCsvLine(req.get_param_value("codes"));
        if (codes.empty()) {
            res.status = 400;
            res.set_content("{\"error\":\"'codes' must list at least one course code\"}", "application/json");
            return;
        }

        ScheduleRecommendation rec = scheduleBuilder.recommend(home, codes);

        ostringstream out;
        out << "{\"found\":" << (rec.found ? "true" : "false");
        if (rec.found) {
            out << ",\"totalTravelMinutes\":" << rec.totalTravelMinutes << ",\"sections\":[";
            bool first = true;
            for (const Section &s : rec.sections) {
                if (!first) out << ",";
                first = false;
                out << "{\"code\":\"" << jsonEscape(s.courseCode) << "\",\"sectionId\":\""
                    << jsonEscape(s.sectionId) << "\",\"locationId\":" << s.locationId
                    << ",\"locationName\":\"" << jsonEscape(locationNameOr(locations, s.locationId))
                    << "\",\"start\":\"" << minutesToClock(s.startMinutes) << "\",\"end\":\""
                    << minutesToClock(s.endMinutes) << "\"}";
            }
            out << "],\"legs\":[";
            first = true;
            for (const ScheduleLeg &leg : rec.legs) {
                if (!first) out << ",";
                first = false;
                out << "{\"fromLabel\":\"" << jsonEscape(leg.fromLabel) << "\",\"toCode\":\""
                    << jsonEscape(leg.toCode) << "\",\"fromLocationId\":" << leg.fromLocationId
                    << ",\"fromLocationName\":\"" << jsonEscape(locationNameOr(locations, leg.fromLocationId))
                    << "\",\"toLocationId\":" << leg.toLocationId << ",\"toLocationName\":\""
                    << jsonEscape(locationNameOr(locations, leg.toLocationId))
                    << "\",\"travelMinutes\":" << leg.travelMinutes << ",\"leaveBy\":\""
                    << minutesToClock(leg.leaveByMinutes) << "\"}";
            }
            out << "]";
        }
        out << "}";
        res.set_content(out.str(), "application/json");
    });

    int port = 8080;
    cout << "Campus Compass API server listening on http://localhost:" << port << endl;
    if (!svr.listen("0.0.0.0", port)) {
        cerr << "Failed to start server on port " << port << endl;
        return 1;
    }
}
