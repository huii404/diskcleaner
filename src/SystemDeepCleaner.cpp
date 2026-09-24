#include "SystemDeepCleaner.h"
#include <iostream>
#include <vector>

CleanStats SystemDeepCleaner::clean(bool dryRun, bool runDismCleanup) {
    CleanStats stats;

    if (!CleanerCore::isElevated()) {
        std::cout << CleanerCore::C_RED << " [!] LƯU Ý: Module dọn dẹp hệ thống chuyên sâu yêu cầu quyền Administrator!\n" << CleanerCore::C_RESET;
        return stats;
    }

    std::string sysDrive = CleanerCore::getSystemDriveRoot();
    char* sysRootEnv = std::getenv("SYSTEMROOT");
    char* progDataEnv = std::getenv("PROGRAMDATA");
    std::string sysRoot = sysRootEnv ? std::string(sysRootEnv) : "C:\\Windows";
    std::string progData = progDataEnv ? std::string(progDataEnv) : "C:\\ProgramData";

    // 1. Dọn dẹp cache tải về của Windows Update
    std::string wuDownloadPath = sysRoot + "\\SoftwareDistribution\\Download";
    if (!dryRun) {
        // Tạm dừng dịch vụ Windows Update để tránh file locked
        CleanerCore::runCommand("net stop wuauserv >nul 2>&1", true);
        CleanerCore::runCommand("net stop bits >nul 2>&1", true);
    }

    CleanerCore::wipeFolderContents(wuDownloadPath, dryRun, stats);

    if (!dryRun) {
        CleanerCore::runCommand("net start bits >nul 2>&1", true);
        CleanerCore::runCommand("net start wuauserv >nul 2>&1", true);
    }

    // 2. Dọn Delivery Optimization Cache
    CleanerCore::wipeFolderContents(progData + "\\Microsoft\\Windows\\DeliveryOptimization\\Cache", dryRun, stats);

    // 3. Tồn dư các bản cập nhật lớn: $WINDOWS.~BT, $WINDOWS.~WS, Windows.old
    std::vector<std::string> upgradeRemnants = {
        sysDrive + "$WINDOWS.~BT",
        sysDrive + "$WINDOWS.~WS",
        sysDrive + "Windows.old"
    };

    for (const auto& remDirStr : upgradeRemnants) {
        fs::path remPath(remDirStr);
        std::error_code ec;
        if (fs::exists(remPath, ec)) {
            if (!dryRun) {
                CleanerCore::takeOwnershipAndGrantAdmin(remPath);
            }
            CleanerCore::forceDeleteFolder(remPath, dryRun, stats);
        }
    }

    // 4. Log cài đặt Windows & Kernel Dumps
    std::vector<std::string> logAndDumpFolders = {
        sysRoot + "\\Panther",
        sysRoot + "\\LiveKernelReports",
        sysRoot + "\\Minidump",
        sysRoot + "\\Logs\\CBS",
        sysRoot + "\\Logs\\DISM"
    };

    for (const auto& folder : logAndDumpFolders) {
        CleanerCore::wipeFolderContents(folder, dryRun, stats);
    }

    // Single dump and log files
    std::vector<std::string> singleFiles = {
        sysRoot + "\\MEMORY.DMP",
        sysRoot + "\\WindowsUpdate.log"
    };

    for (const auto& file : singleFiles) {
        CleanerCore::safeDeleteFile(file, dryRun, stats);
    }

    // 5. Chạy DISM Component Store Cleanup (Thu hồi dung lượng WinSxS)
    if (runDismCleanup && !dryRun) {
        std::cout << CleanerCore::C_CYAN << " [*] Đang thực thi DISM Component Cleanup (WinSxS /resetbase)... Có thể mất vài phút...\n" << CleanerCore::C_RESET;
        CleanerCore::runCommand("dism.exe /online /cleanup-image /startcomponentcleanup /resetbase", false);
    }

    return stats;
}
