#pragma once

#include <string>
#include <unordered_map>

using namespace std;

struct Location {
    int id;
    string name;
    double lat;
    double lng;
};

// loads data/locations.csv (LocationID,Name,Lat,Lng) into an id-keyed map;
// returns an empty map if the file cannot be opened
unordered_map<int, Location> loadLocations(const string &filepath);
