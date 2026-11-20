// generate_data.cpp
// Synthetic data generator for the Nota Hospital simulation.
//
// Writes a larger, fully made-up dataset in the same CSV format the loader
// reads (see include/DataLoader.h), so the simulation and the B-Trees can be
// tried on hundreds of records instead of the 54 hand written ones.
//
//   ./generate_data [seed] [doctors] [patients] [days] [out_dir]
//
// Defaults: seed 201, 16 doctors, 400 patients, 3 days, out_dir data. Writes
// <out_dir>/doctors_large.csv and <out_dir>/patients_large.csv. The same seed
// always produces the same files, so results can be reproduced.
//
// What the data looks like:
//   - Doctors have skill 1-10 and shifts of 8 or 12 hours spread around the
//     clock so every hour of the day has someone on duty. Some shifts are
//     overnight (end earlier than start).
//   - Patients arrive on quarter-hour marks, more often in the daytime and
//     evening. Most conditions are mild, a few are severe.
//   - The more severe the condition, the shorter the time limit before the
//     patient goes critical.
//   - Some patients arrive in groups at the same time (accidents, food
//     poisoning, and so on).
//   - Some patients share a name and age but have different dates of birth,
//     like the hand written data, so name + age alone is not a unique key.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <vector>

namespace {

const std::vector<std::string> kFirst = {
    "Aaliyah", "Adrian", "Aiden", "Alana", "Amir", "Ana", "Andre", "Bianca", "Caleb", "Carmen", "Chloe", "Dante",
    "Elena", "Eli", "Farah", "Felix", "Gabriel", "Hana", "Ian", "Imani", "Jasper", "Jin", "Kai", "Keisha", "Leila",
    "Mateo", "Mei", "Nadia", "Noah", "Olivia", "Priya", "Quinn", "Rafael", "Reese", "Sana", "Tariq", "Uma", "Victor",
    "Wen", "Yusuf", "Zara", "Beth", "Carlos", "Dina", "Evan", "Fatima", "Greg", "Holly", "Ivan", "Julia"};

const std::vector<std::string> kLast = {
    "Abbott", "Aguilar", "Banerjee", "Barnes", "Castillo", "Chaudhry", "Dawson", "Delgado", "Ellis", "Fischer",
    "Foster", "Gallagher", "Hamid", "Hughes", "Ibrahim", "Jensen", "Khan", "Lambert", "Larsen", "Mahoney", "Morales",
    "Nakamura", "Novak", "Okoye", "Owens", "Pham", "Quintero", "Rahman", "Reyes", "Sandoval", "Shah", "Takahashi",
    "Tanaka", "Underwood", "Vasquez", "Walker", "Xu", "Yamada", "Young", "Zimmerman"};

struct Ailment {
    const char* text;
    int lo, hi;   // typical condition range
};

const std::vector<Ailment> kAilments = {
    {"Sore throat", 0, 2},       {"Minor cut", 0, 2},          {"Rash", 0, 2},           {"Cold symptoms", 0, 1},
    {"Sprained ankle", 2, 4},    {"Migraine", 2, 4},           {"Ear infection", 2, 4},  {"Back pain", 2, 4},
    {"High fever", 3, 5},        {"Broken wrist", 4, 6},       {"Kidney stone", 4, 6},   {"Deep laceration", 4, 6},
    {"Allergic reaction", 5, 7}, {"Asthma attack", 5, 8},      {"Fall injury", 5, 7},    {"Appendicitis", 6, 8},
    {"Chest pain", 7, 9},        {"Head injury", 7, 9},        {"Overdose", 8, 10},      {"Severe bleeding", 8, 10},
    {"Stroke symptoms", 9, 10},  {"Cardiac arrest", 10, 10}};

const std::vector<std::string> kGroups = {"Car accident", "Food poisoning", "Kitchen fire", "Bar fight",
                                         "Gym accident", "Bus collision"};

// Reference year used by the hand written data: age = 2026 - birth year.
constexpr int kRefYear = 2026;

struct Gen {
    std::mt19937 rng;
    explicit Gen(unsigned seed) : rng(seed) {}
    int between(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); }
    template <typename T>
    const T& pick(const std::vector<T>& v) { return v[between(0, static_cast<int>(v.size()) - 1)]; }
};

std::string hhmm(int minutes) {
    minutes = ((minutes % 1440) + 1440) % 1440;
    char buf[8];
    std::snprintf(buf, sizeof buf, "%02d:%02d", minutes / 60, minutes % 60);
    return buf;
}

// Date of birth for someone of the given age, as YYYY-MM-DD. Days go to 28 so
// every month is valid.
std::string makeDob(Gen& g, int age) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%04d-%02d-%02d", kRefYear - age, g.between(1, 12), g.between(1, 28));
    return buf;
}

// Time limit (minutes) before a patient of this condition turns critical.
int timeLimit(Gen& g, int condition) {
    int lo, hi;
    if (condition >= 9) { lo = 15; hi = 30; }
    else if (condition >= 7) { lo = 30; hi = 60; }
    else if (condition >= 5) { lo = 60; hi = 120; }
    else if (condition >= 3) { lo = 120; hi = 240; }
    else { lo = 240; hi = 480; }
    return (g.between(lo, hi) / 5) * 5;
}

// Mostly mild conditions, a few severe ones.
int rollCondition(Gen& g) {
    int r = g.between(1, 100);
    if (r <= 30) return g.between(0, 2);
    if (r <= 60) return g.between(3, 4);
    if (r <= 82) return g.between(5, 6);
    if (r <= 94) return g.between(7, 8);
    return g.between(9, 10);
}

// Arrival minute-of-day on a quarter hour. Busier from 08:00 to 22:00.
int rollArrivalMinute(Gen& g) {
    int hour = (g.between(1, 100) <= 80) ? g.between(8, 21) : (g.between(0, 1) ? g.between(22, 23) : g.between(0, 7));
    return hour * 60 + g.between(0, 3) * 15;
}

// Same, but on day 1 nobody arrives before 06:00 because the simulation starts then.
int rollArrivalMinuteOnDay(Gen& g, int day) {
    int minute;
    do {
        minute = rollArrivalMinute(g);
    } while (day == 1 && minute < 6 * 60);
    return minute;
}

struct Patient {
    std::string name, dob, ailment;
    int age, condition, day, minute, limit;
};

}  // namespace

int main(int argc, char* argv[]) {
    const unsigned seed = argc > 1 ? static_cast<unsigned>(std::atoi(argv[1])) : 201u;
    const int doctorCount = argc > 2 ? std::atoi(argv[2]) : 16;
    const int patientCount = argc > 3 ? std::atoi(argv[3]) : 400;
    const int days = argc > 4 ? std::atoi(argv[4]) : 3;
    const std::string outDir = argc > 5 ? argv[5] : "data";
    if (doctorCount < 1 || patientCount < 1 || days < 1 || days > 30) {
        std::cerr << "usage: generate_data [seed] [doctors>=1] [patients>=1] [days 1-30] [out_dir]\n";
        return 1;
    }

    Gen g(seed);

    // ---- doctors ----
    // Shift starts rotate through the day so coverage is even, with a mix of
    // 8 and 12 hour shifts.
    const int starts[] = {6, 8, 10, 14, 16, 18, 22, 0};
    std::set<std::string> doctorKeys;
    std::ofstream dout(outDir + "/doctors_large.csv");
    if (!dout) { std::cerr << "could not write to " << outDir << "\n"; return 1; }
    dout << "# Synthetic Nota Hospital doctors, made by tools/generate_data.cpp (seed " << seed << ").\n";
    dout << "# All names and records are made up.\n";
    dout << "name,age,dob,skill,shift_start,shift_end\n";
    for (int i = 0; i < doctorCount; ++i) {
        std::string name, dob;
        int age;
        do {
            name = g.pick(kFirst) + " " + g.pick(kLast);
            age = g.between(27, 66);
            dob = makeDob(g, age);
        } while (!doctorKeys.insert(name + "|" + std::to_string(age) + "|" + dob).second);
        const int start = starts[i % 8] * 60;
        const int length = (g.between(1, 100) <= 70 ? 8 : 12) * 60;
        dout << name << ',' << age << ',' << dob << ',' << g.between(1, 10) << ',' << hhmm(start) << ','
             << hhmm(start + length) << '\n';
    }

    // ---- patients ----
    std::vector<Patient> patients;
    std::set<std::string> patientKeys;
    std::vector<std::pair<std::string, int>> usedNameAges;   // for deliberate name + age repeats

    auto addPatient = [&](const std::string& ailmentOverride, int day, int minute, int forcedCondition) {
        Patient p;
        // About 1 in 12 patients reuses an earlier name and age with a new date of birth.
        do {
            if (!usedNameAges.empty() && g.between(1, 12) == 1) {
                const auto& na = g.pick(usedNameAges);
                p.name = na.first;
                p.age = na.second;
            } else {
                p.name = g.pick(kFirst) + " " + g.pick(kLast);
                p.age = g.between(1, 92);
            }
            p.dob = makeDob(g, p.age);
        } while (!patientKeys.insert(p.name + "|" + std::to_string(p.age) + "|" + p.dob).second);
        usedNameAges.push_back({p.name, p.age});

        p.condition = forcedCondition >= 0 ? forcedCondition : rollCondition(g);
        if (!ailmentOverride.empty()) {
            p.ailment = ailmentOverride;
        } else {
            // choose an ailment whose range contains the condition
            std::vector<const Ailment*> fits;
            for (const auto& a : kAilments)
                if (p.condition >= a.lo && p.condition <= a.hi) fits.push_back(&a);
            p.ailment = fits.empty() ? "Unspecified complaint" : fits[g.between(0, static_cast<int>(fits.size()) - 1)]->text;
        }
        p.day = day;
        p.minute = minute;
        p.limit = timeLimit(g, p.condition);
        patients.push_back(p);
    };

    // Roughly one group event per 40 patients, 2 to 4 people each.
    int remaining = patientCount;
    const int groupEvents = patientCount / 40;
    for (int e = 0; e < groupEvents && remaining >= 4; ++e) {
        const std::string label = g.pick(kGroups);
        const int day = g.between(1, days);
        const int minute = rollArrivalMinuteOnDay(g, day);
        const int size = g.between(2, 4);
        for (int k = 0; k < size; ++k) addPatient(label + " - injury", day, minute, g.between(3, 9));
        remaining -= size;
    }
    for (; remaining > 0; --remaining) {
        const int day = g.between(1, days);
        addPatient("", day, rollArrivalMinuteOnDay(g, day), -1);
    }

    std::stable_sort(patients.begin(), patients.end(), [](const Patient& a, const Patient& b) {
        return a.day != b.day ? a.day < b.day : a.minute < b.minute;
    });

    std::ofstream pout(outDir + "/patients_large.csv");
    if (!pout) { std::cerr << "could not write to " << outDir << "\n"; return 1; }
    pout << "# Synthetic Nota Hospital ER patients, made by tools/generate_data.cpp (seed " << seed << ").\n";
    pout << "# All names and records are made up. Some name + age pairs repeat with different dates of birth.\n";
    pout << "name,age,dob,condition,ailment,arrival_day,arrival_time,time_limit_min\n";
    for (const Patient& p : patients)
        pout << p.name << ',' << p.age << ',' << p.dob << ',' << p.condition << ',' << p.ailment << ',' << p.day
             << ',' << hhmm(p.minute) << ',' << p.limit << '\n';

    std::cout << "Wrote " << doctorCount << " doctors and " << patients.size() << " patients over " << days
              << " day(s) to " << outDir << "/ (seed " << seed << ")\n";
    return 0;
}
