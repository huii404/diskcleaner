#include "BrowserCleaner.h"
#include <iostream>
#include <vector>
#include <algorithm>

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

        // Opera dùng chính "Opera Stable/GX Stable" làm profile root thay vì
        // đặt cache dưới Default/Profile N như Chrome và Edge.
        std::string baseName = baseDir.filename().string();
        bool isOpera = (baseName == "Opera Stable" || baseName == "Opera GX Stable");
        if (isOpera) {
            for (const auto& cacheName : cacheFolderNames) {
                CleanerCore::wipeFolderContents(baseDir / cacheName, dryRun, stats);
            }
        }

        // Với Opera đã xử lý xong ở trên — bỏ qua vòng lặp profile để
        // tránh double-wipe các thư mục ShaderCache, GrShaderCache, DawnCache.
        if (!isOpera) {
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
        } // end if (!isOpera)
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

    // 3. Dọn rác chuyên sâu cho Zalo PC (Được tối ưu chính xác theo cấu trúc ổ cứng thực tế)
    // Loại bỏ hoàn toàn quét các app chat không dùng khác để tiết kiệm tài nguyên tối đa.
    // LƯU Ý BẢO VỆ: Giữ nguyên tuyệt đối Database chat (*.db), lịch sử tin nhắn và phiên đăng nhập.

    if (!baseLocal.empty()) {
        // A. Xóa các bản cài đặt cũ còn tồn đọng sau khi Zalo tự cập nhật (thường chiếm > 500MB)
        // Ví dụ: Zalo-26.8.20 cũ vẫn nằm cạnh Zalo-26.9.10 đang chạy.
        fs::path zaloPrograms = fs::path(baseLocal) / "Programs" / "Zalo";
        std::error_code ec;
        if (fs::exists(zaloPrograms, ec)) {
            std::vector<fs::path> versionDirs;
            for (const auto& entry : fs::directory_iterator(zaloPrograms, fs::directory_options::skip_permission_denied, ec)) {
                if (ec) { ec.clear(); continue; }
                if (entry.is_directory(ec)) {
                    std::string name = entry.path().filename().string();
                    if (name.rfind("Zalo-", 0) == 0) {
                        versionDirs.push_back(entry.path());
                    }
                }
            }

            // Nếu có từ 2 bản cài đặt trở lên, giữ lại bản mới nhất, xóa các bản cũ
            if (versionDirs.size() > 1) {
                auto latestIt = std::max_element(versionDirs.begin(), versionDirs.end(),
                    [](const fs::path& a, const fs::path& b) {
                        std::error_code e1, e2;
                        auto t1 = fs::last_write_time(a, e1);
                        auto t2 = fs::last_write_time(b, e2);
                        if (!e1 && !e2) return t1 < t2;
                        return a.filename().string() < b.filename().string();
                    });

                for (const auto& p : versionDirs) {
                    if (p != *latestIt) {
                        CleanerCore::forceDeleteFolder(p, dryRun, stats);
                    }
                }
            }
        }

        // B. Gói cài đặt update đã tải xong còn tồn dư của Zalo updater (chiếm ~180MB installer.exe)
        CleanerCore::wipeFolderContents(baseLocal + "\\zalo-updater", dryRun, stats);

        // C. Bộ đệm file tải tạm xem trước của Zalo (TempDownloads)
        CleanerCore::wipeFolderContents(baseLocal + "\\Temp\\Zalo Temp", dryRun, stats);

        // D. Thư mục temp và crash dumps phụ
        CleanerCore::wipeFolderContents(baseLocal + "\\ZaloPC\\temp", dryRun, stats);
        CleanerCore::wipeFolderContents(baseLocal + "\\ZaloPC\\CrashDumps", dryRun, stats);
    }

    if (!baseApp.empty()) {
        fs::path zaloDataDir = fs::path(baseApp) / "ZaloData";
        std::error_code ec;

        if (fs::exists(zaloDataDir, ec)) {
            // E. Gói zip cập nhật phiên bản Zalo còn sót lại trong media\update (chiếm ~260MB)
            CleanerCore::wipeFolderContents(zaloDataDir / "media" / "update", dryRun, stats);

            // F. File media tạm xem trước trong media\temp (chiếm ~120MB)
            CleanerCore::wipeFolderContents(zaloDataDir / "media" / "temp", dryRun, stats);

            // G. Log hành vi và telemetry trong media\action và media\qos
            CleanerCore::wipeFolderContents(zaloDataDir / "media" / "action", dryRun, stats);
            CleanerCore::wipeFolderContents(zaloDataDir / "media" / "qos", dryRun, stats);

            // H. Web/Electron & V8 Shader Cache của Zalo (ZaloData root)
            CleanerCore::wipeFolderContents(zaloDataDir / "Cache", dryRun, stats);
            CleanerCore::wipeFolderContents(zaloDataDir / "Code Cache", dryRun, stats);
            CleanerCore::wipeFolderContents(zaloDataDir / "DawnCache", dryRun, stats);
            CleanerCore::wipeFolderContents(zaloDataDir / "GPUCache", dryRun, stats);
            CleanerCore::wipeFolderContents(zaloDataDir / "resp_cache", dryRun, stats);
            CleanerCore::wipeFolderContents(zaloDataDir / "blob_storage", dryRun, stats);

            // I. Cache trong profile partition của Zalo (Partitions\zalo)
            fs::path partZalo = zaloDataDir / "Partitions" / "zalo";
            CleanerCore::wipeFolderContents(partZalo / "Cache", dryRun, stats);
            CleanerCore::wipeFolderContents(partZalo / "Code Cache", dryRun, stats);
            CleanerCore::wipeFolderContents(partZalo / "DawnCache", dryRun, stats);
            CleanerCore::wipeFolderContents(partZalo / "GPUCache", dryRun, stats);
            CleanerCore::wipeFolderContents(partZalo / "blob_storage", dryRun, stats);
            CleanerCore::wipeFolderContents(partZalo / "Service Worker" / "CacheStorage", dryRun, stats);
            CleanerCore::wipeFolderContents(partZalo / "Service Worker" / "ScriptCache", dryRun, stats);

            // J. File log văn bản tích tụ qua thời gian
            static const std::vector<std::string> zaloLogFiles = {
                "call.log", "log.log", "startup.log", "update.log"
            };
            for (const auto& logFile : zaloLogFiles) {
                CleanerCore::safeDeleteFile(zaloDataDir / logFile, dryRun, stats);
            }
            CleanerCore::safeDeleteFile(zaloDataDir / "media" / "timeonapp.txt", dryRun, stats);
        }

        // K. Bộ đệm nếu có dưới %APPDATA%\Zalo
        CleanerCore::wipeFolderContents(baseApp + "\\Zalo\\Cache", dryRun, stats);
        CleanerCore::wipeFolderContents(baseApp + "\\Zalo\\Code Cache", dryRun, stats);
        CleanerCore::wipeFolderContents(baseApp + "\\Zalo\\GPUCache", dryRun, stats);
        CleanerCore::wipeFolderContents(baseApp + "\\Zalo\\DawnCache", dryRun, stats);
    }

    return stats;
}
