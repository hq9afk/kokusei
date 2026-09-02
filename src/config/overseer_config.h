#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

constexpr float kOverseerPad = 10.0f;
constexpr float kOverseerMenuPad = 14.0f;
constexpr float kOverseerBorderWidth = 2.0f;
constexpr float kOverseerMenuBorderWidth = 4.0f;
constexpr float kOverseerHighlightBorderWidth = 2.0f;
constexpr float kOverseerBulletSize = 25.0f;
constexpr float kOverseerBulletGap = kOverseerPad;
constexpr float kOverseerSearchHeight = 40.0f;
constexpr float kOverseerRowHeight = 44.0f;
constexpr float kOverseerRowSpacing = 10.0f;
constexpr float kRowPitch = kOverseerRowHeight + kOverseerRowSpacing;
constexpr float kOverseerListTop = 64.0f;
constexpr float kCaretW = 2.0f;
constexpr float kOverseerTwoLineGap = -3.0f;
constexpr float kOverseerListGap = 10.0f;
constexpr int kOverseerSurfaceWidth = 700;
constexpr int kOverseerMaxVisible = 6;
constexpr int kOverseerSearchDebounceMs = 120;
constexpr int kOverseerKillGraceMs = 50;
constexpr int kOverseerKillCheckMs = 5;
constexpr int kOverseerMaxResults = 20;
constexpr float kOverseerHeightAnimMs = 200.0f;
constexpr float kOverseerHighlightAnimMs = 140.0f;
constexpr uint64_t kOverseerHeightOwner = 100;
constexpr uint64_t kOverseerHighlightOwner = 101;
constexpr uint64_t kOverseerScrollOwner = 102;

constexpr uint64_t kOverseerQueryCharOwnerBase = 1000;

inline constexpr int kIconTargetSize = 18;

namespace overseer_detail {
constexpr size_t kMaxRowChars = 74;
}

struct DesktopEntry {
    std::string id;
    std::string name;
    std::string exec;
    std::string icon;
    bool terminal = false;
    bool no_display = false;
    bool hidden = false;
};

struct FileEntry {
    std::string name;
    std::string path;
    bool is_dir = false;
    float score = 0.0f;
};

struct ScoredApp {
    const DesktopEntry *entry;
    float score;
};

enum class OverseerMode { Drun, Run, Google, DuckDuckGo, YouTube, Url };

struct ModeQuery {
    OverseerMode mode;
    std::string query;
};

struct DrunResult {
    enum class Kind { App, Dir, File } kind;
    const DesktopEntry *app = nullptr;
    FileEntry file;
};

struct VisitStore {
    std::unordered_map<std::string, int> counts;
    std::string path;
};

enum class SubmenuScreen { Search, Browse, DirActions, FileActions };

struct SubmenuEntry {
    std::string name;
    std::string path;
    bool is_dir = false;

    const char *icon = nullptr;
    enum class Action {
        None,
        Back,
        OpenOptions,
        OpenContainingDir,
        PrevDir,
        DirOpenFileManager,
        DirOpenEditor,
        DirOpenTerminal,
        FileOpen,
    } action = Action::None;
};

struct SubmenuState {
    SubmenuScreen screen = SubmenuScreen::Search;
    std::string current_path;
    bool came_from_browse = false;
    std::vector<SubmenuEntry> items;
};

using DirLister = std::function<std::vector<FileEntry>(const std::string &path,
                                                       bool want_dirs)>;
