#include "Locations.h"

#include <exception>
#include <fstream>
#include <sstream>
#include <vector>

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

} // namespace

unordered_map<int, Location> loadLocations(const string &filepath) {
    unordered_map<int, Location> locations;

    ifstream file(filepath);
    if (!file.is_open()) return locations;

    string line;
    getline(file, line); // discard header row
    while (getline(file, line)) {
        if (trim(line).empty()) continue;
        vector<string> tokens = splitCsvLine(line);
        if (tokens.size() < 4) continue; // malformed row, skip

        try {
            int id = stoi(tokens[0]);
            string name = tokens[1];
            double lat = stod(tokens[2]);
            double lng = stod(tokens[3]);
            locations[id] = Location{id, name, lat, lng};
        } catch (const exception &) {
            continue; // malformed row, skip
        }
    }

    return locations;
}
