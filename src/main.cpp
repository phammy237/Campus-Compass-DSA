#include <iostream>
#include <limits>

#include "CampusCompass.h"

using namespace std;

int main() {
    CampusCompass compass;

    compass.ParseCSV("data/edges.csv", "data/classes.csv");

    int no_of_lines = 0;
    if (!(cin >> no_of_lines)) {
        no_of_lines = 0; // malformed count line: nothing left to process
    }
    cin.ignore(numeric_limits<streamsize>::max(), '\n'); // discard rest of that line

    string command;
    for (int i = 0; i < no_of_lines; i++) {
        getline(cin, command);
        cout << compass.processCommand(command) << '\n';
    }
}
