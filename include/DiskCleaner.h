#ifndef DISK_CLEANER_H
#define DISK_CLEANER_H

#include "CleanerCore.h"
#include "TempCleaner.h"
#include "BrowserCleaner.h"
#include "SystemDeepCleaner.h"
#include "DevCleaner.h"
#include "DownloadsCleaner.h"

enum class CleanScope {
    SurfaceAndTemp,
    BrowserAndApps,
    DeepSystem,
    DevEnvironments,
    DownloadsSmart,
    All
};

class DiskCleaner {
public:
    static void printBanner();
    static void printScopePreview(CleanScope scope, bool dryRun);

    // Chạy dọn dẹp theo phạm vi cụ thể
    static CleanStats runScope(CleanScope scope, bool dryRun);

    // Chạy toàn diện (tất cả các module)
    static CleanStats runAll(bool dryRun);

    // Quét phân tích toàn diện không xóa (Dry-Run / Scan Only)
    static void runScanAnalysis();

    // Menu tương tác người dùng
    static void runInteractiveMenu();
};

#endif // DISK_CLEANER_H
