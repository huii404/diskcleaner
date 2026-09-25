#include "DiskCleaner.h"
#include <iostream>
#include <functional>
#include <vector>
#include <exception>

// ─────────────────────────── helpers nội bộ ────────────────────────────────
namespace {

// Đếm số ký tự hiển thị (code points UTF-8) — tiếng Việt mỗi ký tự = 1 cột
static int displayLen(const std::string& s) {
    int n = 0;
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if      (c < 0x80) i += 1;
        else if (c < 0xE0) i += 2;
        else if (c < 0xF0) i += 3;
        else               i += 4;
        ++n;
    }
    return n;
}

static const char* SEP =
    " ──────────────────────────────────────────────────────────────";

// ─── Struct trạng thái giai đoạn ───────────────────────────────────────────
struct AutoStageResult {
    std::string name;
    CleanStats  stats;
    bool done    = false;
    bool skipped = false;
};

// In một dòng giai đoạn có dot-leader:
//   "  [✓] <name>.........<pad>  <result>"
static void printStageRow(const char* icon, const char* iconColor,
                           const std::string& name,
                           const std::string& result, const char* resultColor) {
    constexpr int TOTAL       = 64;
    constexpr int PREFIX_COLS = 6;   // "  [X] "
    constexpr int RESULT_COLS = 12;

    int dots = TOTAL - PREFIX_COLS - displayLen(name) - RESULT_COLS;
    if (dots < 2) dots = 2;
    int pad = RESULT_COLS - displayLen(result);
    if (pad < 1) pad = 1;

    std::cout << "  "
              << iconColor << "[" << icon << "]" << CleanerCore::C_RESET
              << " " << name
              << CleanerCore::C_DIM;
    for (int i = 0; i < dots; ++i) std::cout << '.';
    std::cout << CleanerCore::C_RESET;
    for (int i = 0; i < pad; ++i) std::cout << ' ';
    std::cout << resultColor << result << CleanerCore::C_RESET << "\n";
}

// ─── Render dashboard tự động ──────────────────────────────────────────────
// currentIdx: stage đang chạy (-1 = chưa bắt đầu, >= size = tất cả xong)
static void renderAutoDashboard(const std::vector<AutoStageResult>& stages,
                                int currentIdx, bool finished) {
    CleanerCore::cls();
    DiskCleaner::printBanner();

    CleanStats total;
    for (const auto& s : stages) total.add(s.stats);

    std::cout << CleanerCore::C_BOLD << " DỌN DẸP TỰ ĐỘNG\n" << CleanerCore::C_RESET;
    if (!finished && currentIdx >= 0 && currentIdx < static_cast<int>(stages.size())) {
        std::cout << " Đang xử lý: "
                  << CleanerCore::C_CYAN << stages[currentIdx].name
                  << CleanerCore::C_RESET << "...\n";
    }
    std::cout << "\n" << SEP << "\n";

    for (int i = 0; i < static_cast<int>(stages.size()); ++i) {
        const auto& st = stages[i];
        const char* icon; const char* iconColor;
        std::string result; const char* resultColor;

        if (st.skipped) {
            icon = "-";  iconColor = CleanerCore::C_YELLOW;
            result = "bỏ qua";      resultColor = CleanerCore::C_YELLOW;
        } else if (st.done) {
            icon = "✓";  iconColor = CleanerCore::C_GREEN;
            long long freed = st.stats.bytesFreed + st.stats.bytesRecycled;
            result = CleanerCore::formatSize(freed);
            resultColor = CleanerCore::C_GREEN;
        } else if (i == currentIdx) {
            icon = "»";  iconColor = CleanerCore::C_CYAN;
            result = "đang xử lý";  resultColor = CleanerCore::C_CYAN;
        } else {
            icon = " ";  iconColor = CleanerCore::C_DIM;
            result = "---";          resultColor = CleanerCore::C_DIM;
        }

        printStageRow(icon, iconColor, st.name, result, resultColor);
    }

    long long totalBytes = total.bytesFreed + total.bytesRecycled;
    std::cout << SEP << "\n"
              << "  " << CleanerCore::C_BOLD << "TỔNG ĐÃ GIẢI PHÓNG: "
              << CleanerCore::C_GREEN  << CleanerCore::formatSize(totalBytes)
              << CleanerCore::C_RESET
              << "  (" << total.filesDeleted << " files, "
              << total.dirsDeleted   << " dirs)\n"
              << SEP << "\n";

    if (finished) {
        std::cout << "\n";
        if (total.errorsCount > 0) {
            std::cout << "  " << CleanerCore::C_RED
                      << "⚠  " << total.errorsCount
                      << " tác vụ hệ thống không thể hoàn tất.\n"
                      << CleanerCore::C_RESET;
        }
        if (total.itemsSkipped > 0) {
            std::cout << "  " << CleanerCore::C_YELLOW
                      << "○  " << total.itemsSkipped
                      << " mục đang được dùng bởi tiến trình khác, đã bỏ qua.\n"
                      << CleanerCore::C_RESET;
        }
        std::cout << "\n  " << CleanerCore::C_GREEN << CleanerCore::C_BOLD
                  << "✓  HOÀN TẤT DỌN DẸP.\n" << CleanerCore::C_RESET << "\n";
    }
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════════
//  PUBLIC API
// ═══════════════════════════════════════════════════════════════════════════

void DiskCleaner::printBanner() {
    std::cout << CleanerCore::C_CYAN << CleanerCore::C_BOLD
              << "╔══════════════════════════════════════════════════════════════╗\n"
              << "║       CHƯƠNG TRÌNH DỌN DẸP RÁC CHUYÊN SÂU WINDOWS (C++)     ║\n"
              << "║          Tối ưu bộ nhớ đệm, ổ đĩa và rác hệ thống           ║\n"
              << "╚══════════════════════════════════════════════════════════════╝\n"
              << CleanerCore::C_RESET;

    std::string drive     = CleanerCore::getSystemDriveRoot();
    long long   freeBytes = CleanerCore::getAvailableDiskSpace(drive);
    bool        admin     = CleanerCore::isElevated();

    std::cout << " Ổ đĩa: " << CleanerCore::C_YELLOW << drive << CleanerCore::C_RESET
              << " | Trống: " << CleanerCore::C_GREEN << CleanerCore::formatSize(freeBytes) << CleanerCore::C_RESET
              << " | Quyền: "
              << (admin
                    ? std::string(CleanerCore::C_GREEN) + "Administrator"
                    : std::string(CleanerCore::C_RED)   + "User thường (Cần Admin để dọn sâu)")
              << CleanerCore::C_RESET << "\n\n";
}

CleanStats DiskCleaner::runScope(CleanScope scope, bool dryRun) {
    CleanStats total;

    if (scope == CleanScope::SurfaceAndTemp || scope == CleanScope::All) {
        std::cout << CleanerCore::C_CYAN << " [*] Đang xử lý: Temp & Cache hệ thống cơ bản...\n" << CleanerCore::C_RESET;
        CleanStats s = TempCleaner::clean(dryRun);
        total.add(s);
        std::cout << "     └── Giải phóng: " << CleanerCore::C_GREEN
                  << CleanerCore::formatSize(s.bytesFreed) << CleanerCore::C_RESET
                  << " (" << s.filesDeleted << " files)\n\n";
    }

    if (scope == CleanScope::BrowserAndApps || scope == CleanScope::All) {
        std::cout << CleanerCore::C_CYAN << " [*] Đang xử lý: Cache trình duyệt & Zalo PC...\n" << CleanerCore::C_RESET;
        CleanStats s = BrowserCleaner::clean(dryRun);
        total.add(s);
        std::cout << "     └── Giải phóng: " << CleanerCore::C_GREEN
                  << CleanerCore::formatSize(s.bytesFreed) << CleanerCore::C_RESET
                  << " (" << s.filesDeleted << " files, " << s.dirsDeleted << " dirs)\n\n";
    }

    if (scope == CleanScope::DeepSystem || scope == CleanScope::All) {
        std::cout << CleanerCore::C_CYAN << " [*] Đang xử lý: Hệ thống sâu & Windows Update...\n" << CleanerCore::C_RESET;
        CleanStats s = SystemDeepCleaner::clean(dryRun, false);
        total.add(s);
        std::cout << "     └── Giải phóng: " << CleanerCore::C_GREEN
                  << CleanerCore::formatSize(s.bytesFreed) << CleanerCore::C_RESET
                  << " (" << s.filesDeleted << " files, " << s.dirsDeleted << " dirs)\n\n";
    }

    if (scope == CleanScope::DevEnvironments || scope == CleanScope::All) {
        std::cout << CleanerCore::C_CYAN << " [*] Đang xử lý: Môi trường Dev...\n" << CleanerCore::C_RESET;
        CleanStats s = DevCleaner::clean(dryRun);
        total.add(s);
        std::cout << "     └── Giải phóng: " << CleanerCore::C_GREEN
                  << CleanerCore::formatSize(s.bytesFreed) << CleanerCore::C_RESET
                  << " (" << s.filesDeleted << " files, " << s.dirsDeleted << " dirs)\n\n";
    }

    // Downloads luôn cuối — file vừa vào Recycle Bin không bị dọn tiếp.
    if (scope == CleanScope::DownloadsSmart || scope == CleanScope::All) {
        std::cout << CleanerCore::C_CYAN << " [*] Đang xử lý: Downloads thông minh (CHẠY CUỐI)...\n" << CleanerCore::C_RESET;
        CleanStats s = DownloadsCleaner::clean(dryRun);
        total.add(s);
        if (s.filesDeleted > 0) {
            std::cout << "     ├── [XÓA CỨNG] File tải dở/lỗi: "
                      << CleanerCore::C_GREEN << CleanerCore::formatSize(s.bytesFreed)
                      << CleanerCore::C_RESET << " (" << s.filesDeleted << " files)\n";
        }
        std::cout << "     └── [RECYCLE]  File trùng & bộ cài đã cài: "
                  << CleanerCore::C_YELLOW << CleanerCore::formatSize(s.bytesRecycled)
                  << CleanerCore::C_RESET
                  << " (" << s.filesRecycled << " files)\n\n";
    }

    return total;
}

CleanStats DiskCleaner::runAll(bool dryRun) {
    return runScope(CleanScope::All, dryRun);
}

void DiskCleaner::runAutomaticCleanup(bool waitAtEnd) {
    std::vector<AutoStageResult> stages = {
        {"Temp & Cache cơ bản",        {}},
        {"Trình duyệt & Zalo PC",      {}},
        {"Hệ thống & Windows Update",  {}},
        {"Môi trường Dev",             {}},
        {"Downloads thông minh",       {}}
    };

    auto executeStage = [&](int idx, const std::function<CleanStats()>& cleaner) {
        renderAutoDashboard(stages, idx, false);
        try {
            stages[idx].stats = cleaner();
        } catch (const std::exception&) {
            stages[idx].stats.errorsCount++;
        } catch (...) {
            stages[idx].stats.errorsCount++;
        }
        stages[idx].done = true;
        renderAutoDashboard(stages, idx + 1, false);
    };

    renderAutoDashboard(stages, -1, false);

    executeStage(0, [] { return TempCleaner::clean(false); });
    executeStage(1, [] { return BrowserCleaner::clean(false); });

    if (CleanerCore::isElevated()) {
        executeStage(2, [] { return SystemDeepCleaner::clean(false, true); });
    } else {
        stages[2].skipped = true;
        stages[2].done    = true;
        renderAutoDashboard(stages, 3, false);
    }

    executeStage(3, [] { return DevCleaner::clean(false, true); });
    // Downloads luôn cuối để file vừa vào Recycle Bin không bị dọn tiếp.
    executeStage(4, [] { return DownloadsCleaner::clean(false); });

    renderAutoDashboard(stages, static_cast<int>(stages.size()), true);
    if (waitAtEnd) CleanerCore::waitEnter();
}

// runInteractiveMenu không còn được dùng — giữ lại để tránh linker error
// nếu có code bên ngoài vẫn gọi (forward-compat).
void DiskCleaner::runInteractiveMenu() {
    runAutomaticCleanup(true);
}
