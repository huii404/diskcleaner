#include "DevCleaner.h"
#include <iostream>
#include <vector>
#include <string>
#include <unordered_set>

std::vector<fs::path> DevCleaner::detectDevScanRoots() {
    std::vector<fs::path> scanRoots;
    fs::path currentRoot = fs::current_path();
    scanRoots.push_back(currentRoot);

    char* userProfileEnv = std::getenv("USERPROFILE");
    std::string baseUser = userProfileEnv ? std::string(userProfileEnv) : "";

    // Duyệt qua các ổ đĩa cố định (Fixed Drives)
    DWORD driveMask = GetLogicalDrives();
    for (char c = 'C'; c <= 'Z'; ++c) {
        if (driveMask & (1 << (c - 'A'))) {
            std::string rootDrive = std::string(1, c) + ":\\";
            if (GetDriveTypeA(rootDrive.c_str()) == DRIVE_FIXED) {
                static const std::vector<std::string> commonDevFolders = {
                    "Code", "Projects", "Source", "Repos", "Dev", "Web", "Workspace"
                };
                for (const auto& df : commonDevFolders) {
                    fs::path devPath = rootDrive + df;
                    std::error_code ec;
                    if (fs::exists(devPath, ec) && fs::is_directory(devPath, ec)) {
                        bool dup = false;
                        for (const auto& sr : scanRoots) {
                            if (fs::equivalent(sr, devPath, ec)) { dup = true; break; }
                        }
                        if (!dup) scanRoots.push_back(devPath);
                    }
                }
            }
        }
    }

    if (!baseUser.empty()) {
        static const std::vector<std::string> userDevFolders = {
            "Desktop", "Documents", "Projects", "source\\repos"
        };
        for (const auto& uf : userDevFolders) {
            fs::path uPath = fs::path(baseUser) / uf;
            std::error_code ec;
            if (fs::exists(uPath, ec) && fs::is_directory(uPath, ec)) {
                bool dup = false;
                for (const auto& sr : scanRoots) {
                    if (fs::equivalent(sr, uPath, ec)) { dup = true; break; }
                }
                if (!dup) scanRoots.push_back(uPath);
            }
        }
    }

    return scanRoots;
}

int DevCleaner::cleanDirectoryArtifacts(const fs::path& rootPath,
                                       const std::vector<std::string>& targetDirNames,
                                       const std::vector<std::string>& targetExtensions,
                                       bool dryRun,
                                       CleanStats& stats) {
    std::error_code ec;
    if (!fs::exists(rootPath, ec) || !fs::is_directory(rootPath, ec)) return 0;

    int deletedCount = 0;
    std::vector<fs::path> dirsToDelete;
    std::vector<fs::path> filesToDelete;

    try {
        for (auto it = fs::recursive_directory_iterator(rootPath, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator();) {
            if (ec) { ec.clear(); try { it++; } catch (...) { break; } continue; }

            try {
                const auto& entry = *it;
                std::string filename = entry.path().filename().string();

                // TUYỆT ĐỐI BẢO VỆ CÁC THƯ MỤC VÀ FILE QUẢN LÝ MÃ NGUỒN GIT
                if (_stricmp(filename.c_str(), ".git") == 0 ||
                    _stricmp(filename.c_str(), ".github") == 0 ||
                    _stricmp(filename.c_str(), ".gitignore") == 0 ||
                    _stricmp(filename.c_str(), ".gitattributes") == 0) {
                    if (entry.is_directory(ec)) {
                        it.disable_recursion_pending();
                    }
                    it.increment(ec);
                    continue;
                }

                if (entry.is_directory(ec)) {
                    bool matchDir = false;
                    for (const auto& td : targetDirNames) {
                        if (_stricmp(filename.c_str(), td.c_str()) == 0) {
                            matchDir = true;
                            break;
                        }
                    }
                    if (matchDir) {
                        dirsToDelete.push_back(entry.path());
                        it.disable_recursion_pending();
                    }
                } else if (entry.is_regular_file(ec)) {
                    std::string ext = entry.path().extension().string();
                    for (const auto& te : targetExtensions) {
                        if (_stricmp(ext.c_str(), te.c_str()) == 0) {
                            filesToDelete.push_back(entry.path());
                            break;
                        }
                    }
                }
            } catch (...) {}
            it.increment(ec);
        }
    } catch (...) {}

    for (const auto& f : filesToDelete) {
        if (CleanerCore::safeDeleteFile(f, dryRun, stats)) {
            deletedCount++;
        }
    }

    for (const auto& d : dirsToDelete) {
        if (CleanerCore::forceDeleteFolder(d, dryRun, stats)) {
            deletedCount++;
        }
    }

    return deletedCount;
}

CleanStats DevCleaner::clean(bool dryRun) {
    CleanStats stats;

    char* localAppEnv = std::getenv("LOCALAPPDATA");
    char* appEnv = std::getenv("APPDATA");
    char* userProfileEnv = std::getenv("USERPROFILE");

    std::string baseLocal = localAppEnv ? std::string(localAppEnv) : "";
    std::string baseApp   = appEnv ? std::string(appEnv) : "";
    std::string baseUser  = userProfileEnv ? std::string(userProfileEnv) : "";

    std::vector<fs::path> scanRoots = detectDevScanRoots();

    // 1. Python Caches (pip)
    if (!baseLocal.empty()) {
        CleanerCore::wipeFolderContents(baseLocal + "\\pip\\cache", dryRun, stats);
    }

    // 2. Node.js / JavaScript / Web Caches (global cache folders)
    if (!baseLocal.empty()) {
        CleanerCore::wipeFolderContents(baseLocal + "\\npm-cache", dryRun, stats);
        CleanerCore::wipeFolderContents(baseLocal + "\\Yarn\\Cache", dryRun, stats);
        CleanerCore::wipeFolderContents(baseLocal + "\\pnpm\\store", dryRun, stats);
        CleanerCore::wipeFolderContents(baseLocal + "\\pnpm\\cache", dryRun, stats);
        CleanerCore::wipeFolderContents(baseLocal + "\\electron\\Cache", dryRun, stats);
        CleanerCore::wipeFolderContents(baseLocal + "\\Microsoft\\TypeScript", dryRun, stats);
        CleanerCore::wipeFolderContents(baseLocal + "\\deno\\deps", dryRun, stats);
    }
    if (!baseApp.empty()) {
        CleanerCore::wipeFolderContents(baseApp + "\\npm-cache", dryRun, stats);
    }
    if (!baseUser.empty()) {
        CleanerCore::wipeFolderContents(baseUser + "\\.turbo", dryRun, stats);
        CleanerCore::wipeFolderContents(baseUser + "\\.npm", dryRun, stats);
        CleanerCore::wipeFolderContents(baseUser + "\\.yarn", dryRun, stats);
        CleanerCore::wipeFolderContents(baseUser + "\\.pnpm-store", dryRun, stats);
    }

    // Quét dự án (Python & Web build/cache) trên scanRoots trong 1 lần duyệt duy nhất để tiết kiệm 50% I/O
    std::vector<std::string> devTargetFolders = {
        "__pycache__", ".pytest_cache", ".mypy_cache", ".ruff_cache", ".tox",
        ".turbo", ".parcel-cache", ".next", ".nuxt", ".vite"
    };
    std::vector<std::string> devTargetExts = {
        ".pyc", ".pyo"
    };
    for (const auto& sr : scanRoots) {
        cleanDirectoryArtifacts(sr, devTargetFolders, devTargetExts, dryRun, stats);
    }

    // 3. Java Gradle & Android
    if (!baseUser.empty()) {
        CleanerCore::wipeFolderContents(baseUser + "\\.gradle\\caches", dryRun, stats);
        CleanerCore::wipeFolderContents(baseUser + "\\.gradle\\daemon", dryRun, stats);
        CleanerCore::wipeFolderContents(baseUser + "\\.android\\cache", dryRun, stats);
    }

    // 4. Code Editors & Compilers (VS Code, Cursor, Go, Rust, NuGet)
    if (!baseApp.empty()) {
        CleanerCore::wipeFolderContents(baseApp + "\\Code\\Cache", dryRun, stats);
        CleanerCore::wipeFolderContents(baseApp + "\\Code\\CachedData", dryRun, stats);
        CleanerCore::wipeFolderContents(baseApp + "\\Code\\CachedExtensionVSIXs", dryRun, stats);
        CleanerCore::wipeFolderContents(baseApp + "\\Cursor\\Cache", dryRun, stats);
        CleanerCore::wipeFolderContents(baseApp + "\\Cursor\\CachedData", dryRun, stats);
    }
    if (!baseLocal.empty()) {
        CleanerCore::wipeFolderContents(baseLocal + "\\NuGet\\v3-cache", dryRun, stats);
        CleanerCore::wipeFolderContents(baseLocal + "\\go-build", dryRun, stats);
    }
    if (!baseUser.empty()) {
        CleanerCore::wipeFolderContents(baseUser + "\\.cargo\\registry\\cache", dryRun, stats);
        CleanerCore::wipeFolderContents(baseUser + "\\.rustup\\downloads", dryRun, stats);
    }

    return stats;
}
