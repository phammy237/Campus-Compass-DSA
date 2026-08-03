#include "ScheduleBuilder.h"

#include <algorithm>
#include <exception>
#include <fstream>
#include <sstream>

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

bool overlaps(const Section &a, const Section &b) {
    return a.startMinutes < b.endMinutes && b.startMinutes < a.endMinutes;
}

} // namespace

bool ScheduleBuilder::load(const string &edgesPath, const string &sectionsPath) {
    ifstream edgesFile(edgesPath);
    if (!edgesFile.is_open()) return false;

    string line;
    getline(edgesFile, line); // discard header row
    while (getline(edgesFile, line)) {
        if (trim(line).empty()) continue;
        vector<string> tokens = splitCsvLine(line);
        if (tokens.size() < 5) continue;

        try {
            int id1 = stoi(tokens[0]);
            int id2 = stoi(tokens[1]);
            int time = stoi(tokens[4]);
            graph_.addEdge(id1, id2, time);
        } catch (const exception &) {
            continue;
        }
    }

    ifstream sectionsFile(sectionsPath);
    if (!sectionsFile.is_open()) return false;

    getline(sectionsFile, line); // discard header row
    while (getline(sectionsFile, line)) {
        if (trim(line).empty()) continue;
        vector<string> tokens = splitCsvLine(line);
        if (tokens.size() < 5) continue;

        try {
            string code = tokens[0];
            string sectionId = tokens[1];
            int locationId = stoi(tokens[2]);
            int startMinutes = parseTimeToMinutes(tokens[3]);
            int endMinutes = parseTimeToMinutes(tokens[4]);
            graph_.addVertex(locationId); // no-op if already registered by an edge
            sectionsByCourse_[code].push_back(Section{code, sectionId, locationId, startMinutes, endMinutes});
        } catch (const exception &) {
            continue;
        }
    }

    return true;
}

vector<string> ScheduleBuilder::availableCourses() const {
    vector<string> codes;
    codes.reserve(sectionsByCourse_.size());
    for (const auto &[code, sections] : sectionsByCourse_) codes.push_back(code);
    sort(codes.begin(), codes.end());
    return codes;
}

vector<Section> ScheduleBuilder::sectionsFor(const string &courseCode) const {
    auto it = sectionsByCourse_.find(courseCode);
    return it == sectionsByCourse_.end() ? vector<Section>{} : it->second;
}

const DijkstraResult &ScheduleBuilder::dijkstraCached(int source, unordered_map<int, DijkstraResult> &cache) const {
    auto it = cache.find(source);
    if (it != cache.end()) return it->second;
    // unordered_map guarantees reference stability across inserts (only iterators
    // may be invalidated by rehashing), so returning this reference is safe even
    // as later calls insert more entries into the same cache
    return cache.emplace(source, graph_.dijkstra(source)).first->second;
}

ScheduleRecommendation ScheduleBuilder::recommend(int homeLocationId, const vector<string> &courseCodes) const {
    ScheduleRecommendation best;
    if (courseCodes.empty() || !graph_.hasVertex(homeLocationId)) return best;

    vector<vector<Section>> options;
    for (const string &code : courseCodes) {
        vector<Section> sections = sectionsFor(code);
        if (sections.empty()) return best; // unknown course: cannot build a schedule
        options.push_back(move(sections));
    }

    vector<Section> chosen;
    unordered_map<int, DijkstraResult> distCache;
    search(options, 0, chosen, homeLocationId, distCache, best);
    return best;
}

void ScheduleBuilder::search(const vector<vector<Section>> &options, size_t idx, vector<Section> &chosen,
                              int homeLocationId, unordered_map<int, DijkstraResult> &distCache,
                              ScheduleRecommendation &best) const {
    if (idx == options.size()) {
        vector<Section> sorted = chosen;
        sort(sorted.begin(), sorted.end(),
             [](const Section &a, const Section &b) { return a.startMinutes < b.startMinutes; });

        vector<ScheduleLeg> legs;
        int totalTravel = 0;
        int prevLocation = homeLocationId;
        string prevLabel = "Home";
        for (const Section &s : sorted) {
            const DijkstraResult &dr = dijkstraCached(prevLocation, distCache);
            auto it = dr.dist.find(s.locationId);
            if (it == dr.dist.end()) return; // unreachable leg: this combination is infeasible

            int travel = it->second;
            legs.push_back(ScheduleLeg{prevLabel, s.courseCode, prevLocation, s.locationId, travel,
                                        s.startMinutes - travel});
            totalTravel += travel;
            prevLocation = s.locationId;
            prevLabel = s.courseCode;
        }

        if (!best.found || totalTravel < best.totalTravelMinutes) {
            best.found = true;
            best.sections = sorted;
            best.legs = legs;
            best.totalTravelMinutes = totalTravel;
        }
        return;
    }

    for (const Section &candidate : options[idx]) {
        bool conflict = false;
        for (const Section &c : chosen) {
            if (overlaps(candidate, c)) {
                conflict = true;
                break;
            }
        }
        if (conflict) continue;

        chosen.push_back(candidate);
        search(options, idx + 1, chosen, homeLocationId, distCache, best);
        chosen.pop_back();
    }
}
