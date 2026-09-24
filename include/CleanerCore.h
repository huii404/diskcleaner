#ifndef CLEANER_CORE_H
#define CLEANER_CORE_H

#include <windows.h>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

// Kết quả và thống kê của một tác vụ dọn dẹp
struct CleanStats {
    long long bytesFreed = 0;
    long long bytesRecycled = 0;
    int filesDeleted = 0;
    int filesRecycled = 0;
    int dirsDeleted = 0;
    int itemsSkipped = 0;
    int errorsCount = 0;

    void add(const CleanStats& other) {
        bytesFreed += other.bytesFreed;
        bytesRecycled += other.bytesRecycled;
        filesDeleted += other.filesDeleted;
        filesRecycled += other.filesRecycled;
        dirsDeleted += other.dirsDeleted;
        itemsSkipped += other.itemsSkipped;
        errorsCount += other.errorsCount;
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
    static const char* C_CYAN;

    static void initConsole();
    static void cls();
    static void waitEnter();
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
    // Kiểm tra đường dẫn nguy hiểm - KHÔNG BAO GIỜ xóa thư mục gốc hệ thống
    static bool isCriticalPath(const fs::path& p);

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
};

#endif // CLEANER_CORE_H
