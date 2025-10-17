//============================================================================
// ProjectTwo.cpp
// CS 300 - ABCU Advising Assistance Program 
// Author: Connor Martin
// Description:
//   Command-line tool that loads Computer Science course data from a CSV and
//   lets advisors print an alphanumeric course list or look up a course with
//   its prerequisites.
//============================================================================

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;

// Helpers (trim/uppercase/split)
static inline string ltrim(string s) {
    size_t i = 0;
    while (i < s.size() && isspace(static_cast<unsigned char>(s[i]))) ++i;
    return s.substr(i);
}

static inline string rtrim(string s) {
    if (s.empty()) return s;
    size_t i = s.size();
    while (i > 0 && isspace(static_cast<unsigned char>(s[i - 1]))) --i;
    return s.substr(0, i);
}

static inline string trim(string s) {
    return rtrim(ltrim(std::move(s)));
}

static inline string toUpper(string s) {
    for (char& c : s) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
    return s;
}

static vector<string> splitCSVLine(const string& line) {
    // CSV here is simple: fields are comma-separated with no quoted commas in the spec.
    // If your file had quoted commas, we’d need a more robust parser.
    vector<string> parts;
    string field;
    stringstream ss(line);
    while (getline(ss, field, ',')) {
        parts.push_back(trim(field));
    }
    return parts;
}

// Core model
struct Course {
    string number;                // e.g., "CSCI101"
    string title;                 // e.g., "Introduction to Programming in C++"
    vector<string> prerequisites; // e.g., {"CSCI100"}
};

class CourseCatalog {
public:
    // Load from CSV into the map; returns number of courses loaded.
    // Expected columns: courseNumber, title, [prereq1, prereq2, ...]
    size_t loadFromCSV(const string& filename, vector<string>& warnings) {
        warnings.clear();
        courses_.clear();

        ifstream in(filename);
        if (!in.is_open()) {
            throw runtime_error("Could not open file: " + filename);
        }

        string line;
        size_t lineNo = 0;
        while (std::getline(in, line)) {
            ++lineNo;

            // Skip empty/comment lines gracefully
            string raw = trim(line);
            if (raw.empty() || raw.rfind("//", 0) == 0 || raw.rfind("#", 0) == 0) {
                continue;
            }

            // Handle possible UTF-8 BOM on first line
            if (lineNo == 1 && raw.size() >= 3 &&
                static_cast<unsigned char>(raw[0]) == 0xEF &&
                static_cast<unsigned char>(raw[1]) == 0xBB &&
                static_cast<unsigned char>(raw[2]) == 0xBF) {
                raw = raw.substr(3);
            }

            auto tokens = splitCSVLine(raw);
            if (tokens.size() < 2) {
                warnings.push_back("Line " + to_string(lineNo) + ": format error (need course number and title).");
                continue;
            }

            Course c;
            c.number = toUpper(tokens[0]);
            c.title = tokens[1];

            // Remaining tokens (if any) are prerequisites by course number
            for (size_t i = 2; i < tokens.size(); ++i) {
                if (!tokens[i].empty()) {
                    c.prerequisites.push_back(toUpper(tokens[i]));
                }
            }

            // If duplicate course numbers appear, last one wins; warn so it’s visible.
            if (courses_.count(c.number)) {
                warnings.push_back("Line " + to_string(lineNo) + ": duplicate course '" + c.number +
                    "' (overwriting previous entry).");
            }
            courses_[c.number] = std::move(c);
        }

        return courses_.size();
    }

    bool empty() const { return courses_.empty(); }

    // Return sorted course numbers (alphanumeric)
    vector<string> sortedCourseNumbers() const {
        vector<string> keys;
        keys.reserve(courses_.size());
        for (auto& kv : courses_) keys.push_back(kv.first);
        sort(keys.begin(), keys.end()); // string compare is fine for these IDs
        return keys;
    }

    // Try to get a course by ID (case-insensitive external API; we uppercase inside).
    const Course* find(string courseNumber) const {
        courseNumber = toUpper(trim(courseNumber));
        auto it = courses_.find(courseNumber);
        if (it == courses_.end()) return nullptr;
        return &it->second;
    }

    // Lookup a title by ID, or return empty string if unknown
    string titleFor(const string& courseNumber) const {
        auto it = courses_.find(courseNumber);
        return (it == courses_.end()) ? string() : it->second.title;
    }

private:
    unordered_map<string, Course> courses_;
};

// Presentation helpers
static void printMenu() {
    cout << "\n1. Load Data Structure." << '\n'
        << "2. Print Course List." << '\n'
        << "3. Print Course." << '\n'
        << "9. Exit" << '\n'
        << "\nWhat would you like to do? ";
}

static void printCourseList(const CourseCatalog& catalog) {
    cout << "Here is a sample schedule:\n\n";
    for (const auto& id : catalog.sortedCourseNumbers()) {
        const Course* c = catalog.find(id);
        if (!c) continue;
        cout << c->number << ", " << c->title << '\n';
    }
    cout << '\n';
}

static void printOneCourse(const CourseCatalog& catalog) {
    cout << "What course do you want to know about? ";
    string query;
    // Use std::ws to consume any leftover newline before getline
    cout.flush();
    cin >> ws;
    getline(cin, query);
    query = toUpper(trim(query));

    const Course* c = catalog.find(query);
    if (!c) {
        cout << "Sorry, I don't have a course with ID '" << query << "'.\n";
        return;
    }

    cout << c->number << ", " << c->title << '\n';

    if (c->prerequisites.empty()) {
        cout << "Prerequisites: None\n";
    }
    else {
        cout << "Prerequisites: ";
        for (size_t i = 0; i < c->prerequisites.size(); ++i) {
            const string& pid = c->prerequisites[i];
            string ptitle = catalog.titleFor(pid);
            if (!ptitle.empty()) {
                cout << pid << " (" << ptitle << ")";
            }
            else {
                // If the prereq isn't present in the file, still show the ID.
                cout << pid;
            }
            if (i + 1 < c->prerequisites.size()) cout << ", ";
        }
        cout << '\n';
    }
}

// Program entry
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    cout << "Welcome to the course planner.\n";

    CourseCatalog catalog;
    bool dataLoaded = false;

    while (true) {
        printMenu();

        string choiceRaw;
        if (!getline(cin, choiceRaw)) break;            // EOF/stream error → exit
        choiceRaw = trim(choiceRaw);
        if (choiceRaw.empty()) continue;

        // Validate numeric menu choice; reject non-digits cleanly
        bool allDigits = !choiceRaw.empty() &&
            all_of(choiceRaw.begin(), choiceRaw.end(), [](char c) { return isdigit(static_cast<unsigned char>(c)); });
        if (!allDigits) {
            cout << choiceRaw << " is not a valid option.\n";
            continue;
        }

        int choice = stoi(choiceRaw);

        if (choice == 1) {
            cout << "Enter the name of the data file: ";
            string filename;
            cout.flush();
            getline(cin, filename);
            filename = trim(filename);

            try {
                vector<string> warnings;
                size_t n = catalog.loadFromCSV(filename, warnings);
                dataLoaded = (n > 0);

                if (dataLoaded) {
                    cout << "Loaded " << n << " courses from '" << filename << "'.\n";
                }
                else {
                    cout << "No courses were loaded from '" << filename << "'.\n";
                }

                // Show any non-fatal format issues so the user can fix the file if needed
                for (const auto& w : warnings) {
                    cout << "Warning: " << w << '\n';
                }
            }
            catch (const exception& ex) {
                cout << "Error: " << ex.what() << '\n';
                dataLoaded = false;
            }
        }
        else if (choice == 2) {
            if (!dataLoaded) {
                cout << "Please load the data first (Option 1) before printing the course list.\n";
                continue;
            }
            printCourseList(catalog);
        }
        else if (choice == 3) {
            if (!dataLoaded) {
                cout << "Please load the data first (Option 1) before printing a course.\n";
                continue;
            }
            printOneCourse(catalog);
        }
        else if (choice == 9) {
            cout << "Thank you for using the course planner!\n";
            break;
        }
        else {
            cout << choice << " is not a valid option.\n";
        }
    }

    return 0;
}
