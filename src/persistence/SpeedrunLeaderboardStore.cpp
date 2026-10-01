#include "persistence/SpeedrunLeaderboardStore.hpp"
#include <algorithm>
#include <fstream>
#include <string>

namespace gdp::persistence {
namespace {
bool less(const gameplay::SpeedrunRecord& a, const gameplay::SpeedrunRecord& b) {
    return a.bestTicks != b.bestTicks ? a.bestTicks < b.bestTicks : a.seed < b.seed;
}
bool valid(const SpeedrunLeaderboard& board) {
    return board.size() <= 10U && std::is_sorted(board.begin(), board.end(), less)
        && std::all_of(board.begin(), board.end(), [](const auto& r) {
            return r.rulesVersion == gameplay::currentSpeedrunRulesVersion && r.bestTicks > 0U;
        });
}
}
bool addSpeedrunLeaderboardResult(SpeedrunLeaderboard& board, const gameplay::SessionState& session) {
    std::optional<gameplay::SpeedrunRecord> candidate;
    if (!gameplay::speedrunRecordChanged(gameplay::considerSpeedrunResult(session, candidate))) return false;
    const auto before = board;
    board.push_back(*candidate);
    std::stable_sort(board.begin(), board.end(), less);
    board.erase(std::unique(board.begin(), board.end()), board.end());
    if (board.size() > 10U) board.resize(10U);
    return board != before;
}
SpeedrunLeaderboard loadSpeedrunLeaderboard(const std::filesystem::path& path) noexcept {
    try {
        std::ifstream in(path);
        std::string magic; unsigned version = 0U; std::size_t count = 0U;
        if (!(in >> magic >> version >> count) || magic != "GDP_SPEEDRUN_BOARD" || version != 1U || count > 10U) return {};
        SpeedrunLeaderboard board(count);
        for (auto& r : board) if (!(in >> r.rulesVersion >> r.bestTicks >> r.seed)) return {};
        std::string extra;
        if (in >> extra || !in.eof() || !valid(board)) return {};
        return board;
    } catch (...) { return {}; }
}
bool saveSpeedrunLeaderboard(const std::filesystem::path& path, const SpeedrunLeaderboard& board) noexcept {
    if (!valid(board)) return false;
    std::filesystem::path temporary;
    try {
        temporary = path;
        temporary += ".tmp";
        std::error_code error;
        if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(), error);
        if (error) return false;
        {
            std::ofstream out(temporary, std::ios::trunc);
            out << "GDP_SPEEDRUN_BOARD 1 " << board.size() << '\n';
            for (const auto& r : board) out << r.rulesVersion << ' ' << r.bestTicks << ' ' << r.seed << '\n';
            out.flush();
            if (!out.good()) {
                out.close(); std::filesystem::remove(temporary, error); return false;
            }
        }
        if (loadSpeedrunLeaderboard(temporary) != board) { std::filesystem::remove(temporary, error); return false; }
        std::filesystem::rename(temporary, path, error);
        if (error) { std::filesystem::remove(temporary, error); return false; }
        return true;
    } catch (...) {
        std::error_code error;
        if (!temporary.empty()) std::filesystem::remove(temporary, error);
        return false;
    }
}
}
