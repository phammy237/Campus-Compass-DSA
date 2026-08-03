#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

#include "CampusCompass.h"

using namespace std;

namespace {
// runs a full command script (same format as stdin: a line count followed by
// that many command lines) and concatenates processCommand()'s output the
// same way main.cpp would print it, one line per command
string runScript(CampusCompass &app, const string &script) {
    istringstream in(script);
    int count = 0;
    in >> count;
    in.ignore();

    string out;
    string line;
    for (int i = 0; i < count; ++i) {
        getline(in, line);
        out += app.processCommand(line);
        out += "\n";
    }
    return out;
}
} // namespace

TEST_CASE("Incorrect commands are rejected", "[validation]") {
    CampusCompass app;
    REQUIRE(app.ParseCSV("data/edges.csv", "data/classes.csv"));

    SECTION("unknown/misspelled command name") {
        REQUIRE(app.processCommand("frobnicate 123") == "unsuccessful");
    }
    SECTION("insert missing required arguments") {
        REQUIRE(app.processCommand("insert \"Only Name\" 12345678") == "unsuccessful");
    }
    SECTION("insert with a UFID that is not 8 digits") {
        REQUIRE(app.processCommand("insert \"Bad UFID\" 1234567 1 1 COP3502") == "unsuccessful");
    }
    SECTION("insert with a non-integer class count") {
        REQUIRE(app.processCommand("insert \"Bad Count\" 12345678 1 abc COP3502") == "unsuccessful");
    }
    SECTION("insert referencing a class code not in classes.csv") {
        REQUIRE(app.processCommand("insert \"Bad Class\" 12345678 1 1 ZZZ9999") == "unsuccessful");
    }
    SECTION("removeClass with a malformed (lowercase) class code") {
        REQUIRE(app.processCommand("removeClass cop3502") == "unsuccessful");
    }
}

TEST_CASE("Edge cases", "[edge-cases]") {
    CampusCompass app;
    REQUIRE(app.ParseCSV("data/edges.csv", "data/classes.csv"));

    SECTION("class count boundary: 6 classes allowed, 7 rejected") {
        REQUIRE(app.processCommand(
                    "insert \"Six Classes\" 10101010 1 6 COP3502 COP3503 COP3504 COT3100 COP3530 CDA3101") ==
                "successful");
        REQUIRE(app.processCommand("insert \"Seven Classes\" 20202020 1 7 COP3502 COP3503 COP3504 "
                                    "COT3100 COP3530 CDA3101 MAC2311") == "unsuccessful");
    }
    SECTION("duplicate class codes within a single insert are rejected") {
        REQUIRE(app.processCommand("insert \"Dup Codes\" 30303030 1 2 COP3502 COP3502") == "unsuccessful");
    }
    SECTION("checkEdgeStatus on a nonexistent edge reports DNE") {
        REQUIRE(app.processCommand("checkEdgeStatus 1 99999") == "DNE");
    }
    SECTION("toggling an edge twice returns it to its original state") {
        REQUIRE(app.processCommand("toggleEdgesClosure 1 1 2") == "successful");
        REQUIRE(app.processCommand("checkEdgeStatus 1 2") == "closed");
        REQUIRE(app.processCommand("toggleEdgesClosure 1 1 2") == "successful");
        REQUIRE(app.processCommand("checkEdgeStatus 1 2") == "open");
    }
}

TEST_CASE("dropClass, removeClass, remove, and replaceClass", "[student-commands]") {
    CampusCompass app;
    REQUIRE(app.ParseCSV("data/edges.csv", "data/classes.csv"));

    SECTION("dropClass removes the student entirely when it was their only class") {
        REQUIRE(app.processCommand("insert \"Only Class\" 11111111 1 1 COP3502") == "successful");
        REQUIRE(app.processCommand("dropClass 11111111 COP3502") == "successful");
        REQUIRE(app.processCommand("remove 11111111") == "unsuccessful"); // already gone
    }

    SECTION("dropClass on a class the student does not have fails") {
        REQUIRE(app.processCommand("insert \"Has One\" 22222222 1 1 COP3502") == "successful");
        REQUIRE(app.processCommand("dropClass 22222222 MAC2311") == "unsuccessful");
    }

    SECTION("replaceClass swaps a class and rejects creating a duplicate") {
        REQUIRE(app.processCommand("insert \"Swap Student\" 33333333 1 2 COP3502 MAC2311") == "successful");
        REQUIRE(app.processCommand("replaceClass 33333333 MAC2311 COP3530") == "successful");
        REQUIRE(app.processCommand("replaceClass 33333333 COP3530 COP3502") == "unsuccessful"); // COP3502 already present
    }

    SECTION("removeClass reports the number of affected students and removes those left with zero classes") {
        REQUIRE(app.processCommand("insert \"Student A\" 44444444 1 1 COP3502") == "successful");
        REQUIRE(app.processCommand("insert \"Student B\" 55555555 1 2 COP3502 MAC2311") == "successful");
        REQUIRE(app.processCommand("removeClass COP3502") == "2");
        REQUIRE(app.processCommand("remove 44444444") == "unsuccessful"); // emptied and auto-removed
        REQUIRE(app.processCommand("dropClass 55555555 MAC2311") == "successful"); // B still exists
    }

    SECTION("removeClass with no matching student fails") {
        REQUIRE(app.processCommand("removeClass ZZZ0000") == "unsuccessful");
    }

    SECTION("remove deletes a student; a second remove on the same UFID fails") {
        REQUIRE(app.processCommand("insert \"Removable\" 66666666 1 1 COP3502") == "successful");
        REQUIRE(app.processCommand("remove 66666666") == "successful");
        REQUIRE(app.processCommand("remove 66666666") == "unsuccessful");
    }
}

TEST_CASE("printShortestEdges reflects road closures", "[shortest-path][closures]") {
    CampusCompass app;
    REQUIRE(app.ParseCSV("data/edges.csv", "data/classes.csv"));

    REQUIRE(app.processCommand("insert \"Closure Student\" 87654321 1 1 COP3502") == "successful");

    // initially reachable: COP3502 sits at location 23, 25 minutes from residence 1
    REQUIRE(app.processCommand("printShortestEdges 87654321") ==
            "Time For Shortest Edges: Closure Student\nCOP3502: 25");

    // close every edge touching location 23, isolating it completely
    REQUIRE(app.processCommand("toggleEdgesClosure 1 13 23") == "successful");
    REQUIRE(app.processCommand("toggleEdgesClosure 1 14 23") == "successful");
    REQUIRE(app.processCommand("toggleEdgesClosure 1 22 23") == "successful");
    REQUIRE(app.processCommand("toggleEdgesClosure 1 23 24") == "successful");

    // now unreachable
    REQUIRE(app.processCommand("printShortestEdges 87654321") ==
            "Time For Shortest Edges: Closure Student\nCOP3502: -1");
}

TEST_CASE("End-to-end script matches the assignment's example scenario", "[integration]") {
    CampusCompass app;
    REQUIRE(app.ParseCSV("data/edges.csv", "data/classes.csv"));

    string input = R"(6
insert "Student A" 10000001 1 1 COP3502
insert "Student B" 10000002 1 1 COP3502
insert "Student C" 10000003 1 2 COP3502 MAC2311
dropClass 10000001 COP3502
remove 10000001
removeClass COP3502
)";

    string expectedOutput = R"(successful
successful
successful
successful
unsuccessful
2
)";

    REQUIRE(runScript(app, input) == expectedOutput);
}
