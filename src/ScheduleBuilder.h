#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "Graph.h"

using namespace std;

struct Section {
    string courseCode;
    string sectionId;
    int locationId;
    int startMinutes;
    int endMinutes;
};

struct ScheduleLeg {
    string fromLabel; // "Home" or the previous leg's course code
    string toCode;
    int fromLocationId;
    int toLocationId;
    int travelMinutes;
    int leaveByMinutes; // minutes-after-midnight the student must leave fromLocationId
};

struct ScheduleRecommendation {
    bool found = false;
    vector<Section> sections;   // chosen sections, sorted by start time
    vector<ScheduleLeg> legs;   // home->first section, then each consecutive pair
    int totalTravelMinutes = 0;
};

// Builds a student's schedule from a synthetic, expanded campus graph and a
// multi-section course catalog — separate from the graded CampusCompass/Graph
// data so it can never affect the assignment's own tests.
class ScheduleBuilder {
public:
    bool load(const string &edgesPath, const string &sectionsPath);

    vector<string> availableCourses() const;
    vector<Section> sectionsFor(const string &courseCode) const;

    // backtracking search over one section per requested course, pruning any
    // combination with overlapping section times, minimizing total walking
    // time from `homeLocationId` through all chosen sections in start-time order
    ScheduleRecommendation recommend(int homeLocationId, const vector<string> &courseCodes) const;

private:
    Graph graph_;
    unordered_map<string, vector<Section>> sectionsByCourse_;

    void search(const vector<vector<Section>> &options, size_t idx, vector<Section> &chosen,
                int homeLocationId, unordered_map<int, DijkstraResult> &distCache,
                ScheduleRecommendation &best) const;

    const DijkstraResult &dijkstraCached(int source, unordered_map<int, DijkstraResult> &cache) const;
};
