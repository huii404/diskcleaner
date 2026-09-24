#ifndef BROWSER_CLEANER_H
#define BROWSER_CLEANER_H

#include "CleanerCore.h"

/**
 * @brief Module dọn dẹp Cache trình duyệt và ứng dụng mạng
 * - Chromium based: Google Chrome, Microsoft Edge, Cốc Cốc, Brave, Vivaldi, Opera, Opera GX
 *   (Hỗ trợ đa profile: Default, Profile 1, Profile 2, Guest, System...)
 *   (Các loại cache: Cache, Code Cache, GPUCache, DawnCache, ShaderCache, Service Worker)
 * - Gecko based: Mozilla Firefox (Profiles Roaming & Local: cache2, startupCache, jumpListCache)
 * - App Cache: Discord, Telegram Desktop
 * (Đã loại trừ hoàn toàn cache driver card đồ họa NVIDIA GLCache để tránh lỗi)
 */
class BrowserCleaner {
public:
    static CleanStats clean(bool dryRun = false);
};

#endif // BROWSER_CLEANER_H
