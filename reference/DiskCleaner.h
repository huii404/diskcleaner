#ifndef DISKCLEANER_H
#define DISKCLEANER_H

#include "SystemCore.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <filesystem>

/**
 * @brief Phân hệ Dọn Rác & Giải Phóng Ổ Đĩa (Disk Cleaner)
 * Quản lý dọn rác tạm, cache trình duyệt, file tồn dư Windows Update,
 * artifacts môi trường lập trình và dọn dẹp thư mục Downloads thông minh.
 */
class DiskCleaner {
private:
    SystemCore &sc;

    // Các hàm phụ trợ nội bộ
    static void wipeFolderContents(const std::filesystem::path &dirPath);
    static int cleanDirectoryArtifacts(const std::filesystem::path &rootPath, 
                                       const std::vector<std::string> &targetDirNames, 
                                       const std::vector<std::string> &targetExtensions, 
                                       long long &freedBytes);
    static bool forceDeleteFolder(const std::filesystem::path &path);
    static std::string getSystemDriveRoot();
    static bool moveToRecycleBin(const std::filesystem::path &filePath);

    // Helpers cho Dọn dẹp thư mục Downloads (chỉ áp dụng cho file exe/msi và rác tải hỏng)
    static std::string getDownloadsPath();
    static std::unordered_set<std::string> getInstalledAppNames();
    static std::string getExeProductName(const std::string &exePath);
    static std::string cleanAppName(const std::string &raw);

public:
    DiskCleaner(SystemCore &core);
    ~DiskCleaner() = default;

    // 1. Dọn rác bề mặt & Cache người dùng (Temp, CrashDumps, WER, INetCache, RecycleBin, DNS)
    long long cleanSurfaceAndUserTemp();

    // 2. Dọn rác Trình duyệt & Ứng dụng (Chrome, Edge, Discord, Telegram, Shader cache...)
    long long cleanBrowserAndAppCache();

    // 3. Dọn dẹp Chuyên sâu & Tồn dư Cập nhật (DISM, Windows.old, EventLogs, WinUpdate cache)
    long long cleanDeepSystemAndUpdates();

    // 4. Dọn rác Môi trường lập trình (node_modules, Pip, Gradle, VS Code workspace...)
    long long cleanDevArtifactsAndCaches();

    // 5. Dọn file cài đặt Exe & Rác tải về trong Downloads (Exe đã cài, Exe trùng lặp, .crdownload hỏng)
    long long cleanDownloadsExesAndDuplicates();

    // Dọn cache môi trường dev
    void cleanDevCaches(bool interactive = false);
    void clearBrowserCache();

    // Điều phối 2 nhóm: 1 = Plus (nhiệm vụ 1, 2, 3, 5), 2 = Pro (toàn bộ 5 nhiệm vụ)
    void runCleanChoice(int choice);
};

#endif // DISKCLEANER_H
