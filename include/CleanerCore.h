#ifndef CLEANER_CORE_H
#define CLEANER_CORE_H

#include <windows.h>
#include <string>
#include <vector>
#include <filesystem>
#include <functional>
#include <iostream>

namespace fs = std::filesystem;

// Kết quả và thống kê của một tác vụ dọn dẹp
struct CleanStats {
    long long bytesFreed = 0;
    long long bytesRecycled = 0;
    int filesDeleted = 0;
    int filesRecycled = 0;
    int dirsDeleted = 0;
    int errorsCount = 0;
    std::vector<std::string> messages;

    void add(const CleanStats& other) {
        bytesFreed += other.bytesFreed;
        bytesRecycled += other.bytesRecycled;
        filesDeleted += other.filesDeleted;
        filesRecycled += other.filesRecycled;
        dirsDeleted += other.dirsDeleted;
        errorsCount += other.errorsCount;
        messages.insert(messages.end(), other.messages.begin(), other.messages.end());
    }
};

class CleanerCore {
public:
    // Màu ANSI Neon hiển thị trên Windows Terminal / CMD
    static const char* C_RESET;
    static const char* C_BOLD;
    static const char* C_RED;
    static const char* C_GREEN;
    static const char* C_YELLOW;
    static const char* C_BLUE;
    static const char* C_CYAN;
    static const char* C_MAGENTA;
    static const char* C_WHITE;

    static void initConsole();
    static void cls();
    static void waitEnter();
    static bool confirm(const std::string& prompt, bool defaultYes = false);
    static std::string trim(const std::string& str);
    static std::string toLower(const std::string& str);
    static std::string formatSize(long long bytes);
    static std::string getSystemDriveRoot();
    static long long getAvailableDiskSpace(const std::string& drivePath = "");

    // Quyền hạn Windows
    static bool isElevated();
    static bool restartAsAdmin(const std::string& args = "");

    // Tác vụ thực thi dòng lệnh an toàn
    static bool runCommand(const std::string& cmd, bool hideWindow = true);
    static bool runAdminCommand(const std::string& cmd, bool silent = false);

    // Tác vụ File & Thư mục an toàn
    static bool wipeFolderContents(const fs::path& dirPath, bool dryRun, CleanStats& stats);
    static bool forceDeleteFolder(const fs::path& dirPath, bool dryRun, CleanStats& stats);
    static bool safeDeleteFile(const fs::path& filePath, bool dryRun, CleanStats& stats);
    static bool moveToRecycleBin(const fs::path& filePath, bool dryRun, CleanStats& stats);
    static bool emptyRecycleBin(bool dryRun, CleanStats& stats);
    static bool flushDns();

    // Quyền sở hữu file (Take ownership cho Windows.old, $WINDOWS.~BT)
    static bool takeOwnershipAndGrantAdmin(const fs::path& targetPath);

    // So sánh nhị phân 2 file để phát hiện trùng lặp chính xác
    static bool filesHaveSameContent(const fs::path& first, const fs::path& second);
    static uintmax_t calculateDirectorySize(const fs::path& dirPath);
};

#endif // CLEANER_CORE_H
