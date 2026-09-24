#ifndef SYSTEM_DEEP_CLEANER_H
#define SYSTEM_DEEP_CLEANER_H

#include "CleanerCore.h"

/**
 * @brief Module dọn dẹp hệ thống chuyên sâu & Tồn dư cập nhật Windows
 * (Hầu hết tác vụ yêu cầu quyền Administrator)
 * - Bộ đệm tải về Windows Update (%SystemRoot%\SoftwareDistribution\Download)
 * - Xóa tồn dư cài đặt/nâng cấp Windows cũ: C:\Windows.old, C:\$WINDOWS.~BT, C:\$WINDOWS.~WS
 * - Log cài đặt & lỗi hệ thống: Panther, LiveKernelReports, Minidump, MEMORY.DMP, CBS, DISM, WindowsUpdate.log
 * - Cache giao hàng cập nhật (DeliveryOptimization Cache)
 * - Tùy chọn chạy DISM Component Cleanup an toàn (/online /cleanup-image /startcomponentcleanup)
 */
class SystemDeepCleaner {
public:
    static CleanStats clean(bool dryRun = false, bool runDismCleanup = false);
};

#endif // SYSTEM_DEEP_CLEANER_H
