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

    // 两份数据
    std::vector<ScoreEntry> spScores;  // 单机榜
    std::vector<ScoreEntry> netScores; // 网络榜

    const std::string spFileName = "highscores_sp.dat";
    const std::string netFileName = "highscores_net.dat";

    HighScoreManager() {
        LoadScores(); // 启动时加载所有榜单
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

    // 统一加载
    void LoadScores() {
        LoadList(spFileName, spScores);
        LoadList(netFileName, netScores);
    }

    // 统一保存
    void SaveScores() {
        SaveList(spFileName, spScores);
        SaveList(netFileName, netScores);
    }

    // 获取对应的列表引用
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
        SaveScores(); // 保存更改
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
        // 再次排序确保安全
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