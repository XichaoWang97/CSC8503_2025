#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <sstream>

struct ScoreEntry {
    char name[20];
    float time;
};

class HighScoreManager {
public:
    static HighScoreManager& Instance() {
        static HighScoreManager instance;
        return instance;
    }

    // Two sets of data
    std::vector<ScoreEntry> spScores;  // Single Player Leaderboard
    std::vector<ScoreEntry> netScores; // Network Leaderboard

    const std::string spFileName = "highscores_sp.dat";
    const std::string netFileName = "highscores_net.dat";

    HighScoreManager() {
        LoadScores(); // Load all leaderboards on startup
        InitDefaults(spScores);
        InitDefaults(netScores);
    }

    void InitDefaults(std::vector<ScoreEntry>& list) {
        if (list.empty()) {
            AddScoreToList(list, "EMPTY", 999.0f);
            AddScoreToList(list, "EMPTY", 999.0f);
            AddScoreToList(list, "EMPTY", 999.0f);
        }
    }

    // Unified load
    void LoadScores() {
        LoadList(spFileName, spScores);
        LoadList(netFileName, netScores);
    }

    // Unified save
    void SaveScores() {
        SaveList(spFileName, spScores);
        SaveList(netFileName, netScores);
    }

    // Get reference to the corresponding list
    std::vector<ScoreEntry>& GetScores(bool isNetwork) {
        return isNetwork ? netScores : spScores;
    }

    bool IsNewRecord(float time, bool isNetwork) {
        auto& list = GetScores(isNetwork);
        if (list.size() < 5) return true;
        return time < list.back().time;
    }

    void AddScore(std::string name, float time, bool isNetwork) {
        auto& list = GetScores(isNetwork);
        AddScoreToList(list, name, time);
        SaveScores(); // Save changes
    }

private:
    void AddScoreToList(std::vector<ScoreEntry>& list, std::string name, float time) {
        ScoreEntry entry;
        strncpy_s(entry.name, name.c_str(), 19);
        entry.name[19] = '\0';
        entry.time = time;
        list.push_back(entry);

        std::sort(list.begin(), list.end(), [](const ScoreEntry& a, const ScoreEntry& b) {
            return a.time < b.time;
            });
        if (list.size() > 5) list.resize(5);
    }

    void LoadList(std::string file, std::vector<ScoreEntry>& list) {
        list.clear();
        std::ifstream in(file, std::ios::binary);
        if (in.is_open()) {
            ScoreEntry entry;
            while (in.read(reinterpret_cast<char*>(&entry), sizeof(ScoreEntry))) {
                list.push_back(entry);
            }
            in.close();
        }
        // Re-sort to ensure safety
        std::sort(list.begin(), list.end(), [](const ScoreEntry& a, const ScoreEntry& b) {
            return a.time < b.time;
            });
    }

    void SaveList(std::string file, std::vector<ScoreEntry>& list) {
        std::ofstream out(file, std::ios::binary);
        if (out.is_open()) {
            for (const auto& s : list) {
                out.write(reinterpret_cast<const char*>(&s), sizeof(ScoreEntry));
            }
            out.close();
        }
    }
};