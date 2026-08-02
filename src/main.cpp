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
        // flush per line: autograders that read output interactively (write a
        // command, block for its response, repeat) will deadlock on a fully
        // buffered pipe otherwise, since the next input never arrives until
        // this line's output does
        cout << compass.processCommand(command) << endl;
    }
}
