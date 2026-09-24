#include "SystemDeepCleaner.h"
#include <iostream>
#include <vector>

namespace {
bool queryServiceRunning(const wchar_t* serviceName, bool& running) {
    running = false;
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    SC_HANDLE service = OpenServiceW(scm, serviceName, SERVICE_QUERY_STATUS);
    if (!service) {
        CloseServiceHandle(scm);
        return false;
    }

    SERVICE_STATUS_PROCESS status{};
    DWORD bytesNeeded = 0;
    bool ok = QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
                                   reinterpret_cast<LPBYTE>(&status), sizeof(status),
                                   &bytesNeeded) != FALSE;
    if (ok) running = status.dwCurrentState == SERVICE_RUNNING;
    CloseServiceHandle(service);
    CloseServiceHandle(scm);
    return ok;
}
}

CleanStats SystemDeepCleaner::clean(bool dryRun, bool runDismCleanup) {
    CleanStats stats;

    if (!dryRun && !CleanerCore::isElevated()) {
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
    
    // RAII Scope Guard: Đảm bảo các dịch vụ cập nhật luôn được khôi phục dù có sự cố xảy ra
    struct WuServiceGuard {
        bool restartBits = false;
        bool restartWuauserv = false;
        ~WuServiceGuard() {
            if (restartBits) {
                CleanerCore::runCommand("net start bits >nul 2>&1", true);
            }
            if (restartWuauserv) {
                CleanerCore::runCommand("net start wuauserv >nul 2>&1", true);
            }
        }
    } wuGuard;

    if (!dryRun) {
        // Chỉ khởi động lại những dịch vụ vốn đang chạy trước tác vụ.
        bool wuauservRunning = false;
        bool bitsRunning = false;
        if (queryServiceRunning(L"wuauserv", wuauservRunning) && wuauservRunning) {
            wuGuard.restartWuauserv = CleanerCore::runCommand("net stop wuauserv >nul 2>&1", true);
        }
        if (queryServiceRunning(L"bits", bitsRunning) && bitsRunning) {
            wuGuard.restartBits = CleanerCore::runCommand("net stop bits >nul 2>&1", true);
        }
    }

    CleanerCore::wipeFolderContents(wuDownloadPath, dryRun, stats);

    if (wuGuard.restartBits && CleanerCore::runCommand("net start bits >nul 2>&1", true)) {
        wuGuard.restartBits = false;
    }
    if (wuGuard.restartWuauserv && CleanerCore::runCommand("net start wuauserv >nul 2>&1", true)) {
        wuGuard.restartWuauserv = false;
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

    // 5. Chạy DISM Component Store Cleanup an toàn (vẫn cho phép gỡ bản cập nhật).
    if (runDismCleanup && !dryRun) {
        std::cout << CleanerCore::C_CYAN << " [*] Đang thực thi DISM Component Cleanup... Có thể mất vài phút...\n" << CleanerCore::C_RESET;
        if (!CleanerCore::runCommand(
                "dism.exe /online /cleanup-image /startcomponentcleanup", false)) {
            stats.errorsCount++;
        }
    }

    return stats;
}
