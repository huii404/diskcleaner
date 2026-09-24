#ifndef TEMP_CLEANER_H
#define TEMP_CLEANER_H

#include "CleanerCore.h"

/**
 * @brief Module dọn dẹp file tạm, bộ nhớ đệm người dùng và log lỗi
 * - %TEMP% của User
 * - %SYSTEMROOT%\Temp (yêu cầu Admin)
 * - %SYSTEMROOT%\Prefetch (yêu cầu Admin)
 * - CrashDumps, Windows Error Reporting (WER)
 * - CryptnetUrlCache, D3DSCache (Direct3D Shader)
 * - Thumbcache (Explorer Thumbnail), Recent files
 * - Thùng rác (Recycle Bin) và DNS Cache
 */
class TempCleaner {
public:
    static CleanStats clean(bool dryRun = false);
};

#endif // TEMP_CLEANER_H
