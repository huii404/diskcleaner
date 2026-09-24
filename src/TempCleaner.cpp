#include "TempCleaner.h"
#include <iostream>
#include <vector>

CleanStats TempCleaner::clean(bool dryRun) {
    CleanStats stats;

    char* tempEnv = std::getenv("TEMP");
    char* localAppEnv = std::getenv("LOCALAPPDATA");
    char* appEnv = std::getenv("APPDATA");
    char* sysRootEnv = std::getenv("SYSTEMROOT");
    char* progDataEnv = std::getenv("PROGRAMDATA");

    std::string tempDir = tempEnv ? std::string(tempEnv) : "";
    std::string localApp = localAppEnv ? std::string(localAppEnv) : "";
    std::string appData = appEnv ? std::string(appEnv) : "";
    std::string sysRoot = sysRootEnv ? std::string(sysRootEnv) : "C:\\Windows";
    std::string progData = progDataEnv ? std::string(progDataEnv) : "C:\\ProgramData";

    // 1. Thư mục Temp người dùng
    if (!tempDir.empty()) {
        CleanerCore::wipeFolderContents(tempDir, dryRun, stats);
    }

    // 2. Thư mục Temp hệ thống & Prefetch (Nếu có quyền Admin)
    if (CleanerCore::isElevated()) {
        CleanerCore::wipeFolderContents(sysRoot + "\\Temp", dryRun, stats);
        CleanerCore::wipeFolderContents(sysRoot + "\\Prefetch", dryRun, stats);
    }

    // 3. Recent Items & Quick Access Cache
    if (!appData.empty()) {
        CleanerCore::wipeFolderContents(appData + "\\Microsoft\\Windows\\Recent", dryRun, stats);
    }

    // 4. Caches đồ họa & Bảo mật Windows
    if (!localApp.empty()) {
        CleanerCore::wipeFolderContents(localApp + "\\D3DSCache", dryRun, stats);
        CleanerCore::wipeFolderContents(localApp + "\\Low\\Microsoft\\CryptnetUrlCache", dryRun, stats);
        CleanerCore::wipeFolderContents(localApp + "\\CrashDumps", dryRun, stats);
        CleanerCore::wipeFolderContents(localApp + "\\Microsoft\\Windows\\WER\\Temp", dryRun, stats);
        CleanerCore::wipeFolderContents(localApp + "\\Microsoft\\Windows\\WER\\ReportArchive", dryRun, stats);
        CleanerCore::wipeFolderContents(localApp + "\\Microsoft\\Windows\\WER\\ReportQueue", dryRun, stats);
        CleanerCore::wipeFolderContents(localApp + "\\Microsoft\\Windows\\INetCache", dryRun, stats);

        // Thumbnail cache
        std::error_code ec;
        fs::path explorerDir = fs::path(localApp) / "Microsoft" / "Windows" / "Explorer";
        if (fs::exists(explorerDir, ec)) {
            for (const auto& entry : fs::directory_iterator(explorerDir, fs::directory_options::skip_permission_denied, ec)) {
                if (ec) { ec.clear(); continue; }
                if (entry.is_regular_file(ec)) {
                    std::string fn = entry.path().filename().string();
                    if (fn.rfind("thumbcache_", 0) == 0 && fn.size() >= 3 && fn.substr(fn.size() - 3) == ".db") {
                        CleanerCore::safeDeleteFile(entry.path(), dryRun, stats);
                    }
                }
            }
        }
    }

    // 5. ProgramData WER Temp
    if (CleanerCore::isElevated() && !progData.empty()) {
        CleanerCore::wipeFolderContents(progData + "\\Microsoft\\Windows\\WER\\Temp", dryRun, stats);
    }

    // 6. Xóa sạch cache phân giải tên miền DNS
    if (!dryRun) {
        CleanerCore::flushDns();
    }

    return stats;
}
