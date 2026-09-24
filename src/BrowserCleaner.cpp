#include "BrowserCleaner.h"
#include <iostream>
#include <vector>

CleanStats BrowserCleaner::clean(bool dryRun) {
    CleanStats stats;

    char* localAppEnv = std::getenv("LOCALAPPDATA");
    char* appEnv = std::getenv("APPDATA");
    if (!localAppEnv && !appEnv) return stats;

    std::string baseLocal = localAppEnv ? std::string(localAppEnv) : "";
    std::string baseApp   = appEnv ? std::string(appEnv) : "";

    // 1. Danh sách các trình duyệt Chromium phổ biến
    std::vector<std::string> chromiumBases;
    if (!baseLocal.empty()) {
        chromiumBases.push_back(baseLocal + "\\Google\\Chrome\\User Data");
        chromiumBases.push_back(baseLocal + "\\Microsoft\\Edge\\User Data");
        chromiumBases.push_back(baseLocal + "\\CocCoc\\Browser\\User Data");
        chromiumBases.push_back(baseLocal + "\\BraveSoftware\\Brave-Browser\\User Data");
        chromiumBases.push_back(baseLocal + "\\Vivaldi\\User Data");
        chromiumBases.push_back(baseLocal + "\\Opera Software\\Opera Stable");
        chromiumBases.push_back(baseLocal + "\\Opera Software\\Opera GX Stable");
    }
    if (!baseApp.empty()) {
        chromiumBases.push_back(baseApp + "\\Opera Software\\Opera Stable");
        chromiumBases.push_back(baseApp + "\\Opera Software\\Opera GX Stable");
    }

    // Các thư mục bộ đệm tiêu chuẩn trong profile Chromium
    static const std::vector<std::string> cacheFolderNames = {
        "Cache", "Code Cache", "GPUCache", "DawnCache", "ShaderCache",
        "GrShaderCache", "GraphiteDawnCache", "Service Worker\\CacheStorage",
        "Service Worker\\ScriptCache"
    };

    for (const auto& baseDirStr : chromiumBases) {
        fs::path baseDir(baseDirStr);
        std::error_code ec;
        if (!fs::exists(baseDir, ec)) continue;

        try {
            for (const auto& entry : fs::directory_iterator(baseDir, fs::directory_options::skip_permission_denied, ec)) {
                if (ec) { ec.clear(); continue; }
                if (!entry.is_directory(ec)) continue;

                std::string dirName = entry.path().filename().string();
                bool isProfile = (dirName == "Default" || 
                                  dirName.rfind("Profile", 0) == 0 ||
                                  dirName == "Guest Profile" || 
                                  dirName == "System Profile");

                if (isProfile) {
                    for (const auto& cacheName : cacheFolderNames) {
                        fs::path targetCache = entry.path() / cacheName;
                        if (fs::exists(targetCache, ec)) {
                            CleanerCore::wipeFolderContents(targetCache, dryRun, stats);
                        }
                    }
                } else if (dirName == "ShaderCache" || dirName == "GrShaderCache" || dirName == "DawnCache") {
                    CleanerCore::wipeFolderContents(entry.path(), dryRun, stats);
                }
            }
        } catch (...) {}
    }

    // 2. Mozilla Firefox (Roaming & Local)
    if (!baseApp.empty()) {
        fs::path ffRoaming = fs::path(baseApp) / "Mozilla" / "Firefox" / "Profiles";
        std::error_code ec;
        if (fs::exists(ffRoaming, ec)) {
            try {
                for (const auto& profile : fs::directory_iterator(ffRoaming, fs::directory_options::skip_permission_denied, ec)) {
                    if (profile.is_directory(ec)) {
                        CleanerCore::wipeFolderContents(profile.path() / "cache2", dryRun, stats);
                        CleanerCore::wipeFolderContents(profile.path() / "startupCache", dryRun, stats);
                        CleanerCore::wipeFolderContents(profile.path() / "jumpListCache", dryRun, stats);
                    }
                }
            } catch (...) {}
        }
    }
    if (!baseLocal.empty()) {
        fs::path ffLocal = fs::path(baseLocal) / "Mozilla" / "Firefox" / "Profiles";
        std::error_code ec;
        if (fs::exists(ffLocal, ec)) {
            try {
                for (const auto& profile : fs::directory_iterator(ffLocal, fs::directory_options::skip_permission_denied, ec)) {
                    if (profile.is_directory(ec)) {
                        CleanerCore::wipeFolderContents(profile.path() / "cache2", dryRun, stats);
                        CleanerCore::wipeFolderContents(profile.path() / "startupCache", dryRun, stats);
                    }
                }
            } catch (...) {}
        }
    }

    // 3. Cache ứng dụng chat: Discord, Telegram Desktop
    // (Đã loại bỏ hoàn toàn xóa NVIDIA GLCache để đảm bảo an toàn đồ họa hệ thống)
    if (!baseApp.empty()) {
        CleanerCore::wipeFolderContents(baseApp + "\\discord\\Cache", dryRun, stats);
        CleanerCore::wipeFolderContents(baseApp + "\\discord\\Code Cache", dryRun, stats);
        CleanerCore::wipeFolderContents(baseApp + "\\Telegram Desktop\\tdata\\user_data\\cache", dryRun, stats);
    }

    return stats;
}
