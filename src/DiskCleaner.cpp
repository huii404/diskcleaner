#include "DiskCleaner.h"
#include <iostream>
#include <functional>
#include <vector>
#include <exception>

namespace {
struct AutoStageResult {
    std::string name;
    CleanStats stats;
};

void renderAutoDashboard(const std::vector<AutoStageResult>& stages,
                         const std::string& currentStage,
                         bool finished) {
    CleanerCore::cls();
    DiskCleaner::printBanner();

    CleanStats total;
    for (const auto& stage : stages) total.add(stage.stats);

    std::cout << CleanerCore::C_BOLD << " DỌN DẸP TỰ ĐỘNG\n\n" << CleanerCore::C_RESET;
    if (!finished) {
        std::cout << " Đang chạy: " << CleanerCore::C_CYAN << currentStage
                  << CleanerCore::C_RESET << "\n\n";
    }

    std::cout << " ──────────────────────────────────────────\n"
              << " TỔNG DUNG LƯỢNG ĐÃ XÓA: " << CleanerCore::C_GREEN
              << CleanerCore::C_BOLD << CleanerCore::formatSize(total.bytesFreed)
              << CleanerCore::C_RESET << "\n"
              << " ──────────────────────────────────────────\n";

    if (finished) {
        std::cout << CleanerCore::C_GREEN << CleanerCore::C_BOLD
                  << "\n HOÀN TẤT DỌN DẸP.\n"
                  << CleanerCore::C_RESET;
        if (total.errorsCount > 0) {
            std::cout << CleanerCore::C_RED
                      << " Có " << total.errorsCount
                      << " tác vụ hệ thống không hoàn tất.\n"
                      << CleanerCore::C_RESET;
        }
    }
}
}

void DiskCleaner::printBanner() {
    std::cout << CleanerCore::C_CYAN << CleanerCore::C_BOLD
              << "╔══════════════════════════════════════════════════════════════╗\n"
              << "║       CHƯƠNG TRÌNH DỌN DẸP RÁC CHUYÊN SÂU WINDOWS (C++)      ║\n"
              << "║          Tối ưu bộ nhớ đệm, ổ đĩa và rác hệ thống            ║\n"
              << "╚══════════════════════════════════════════════════════════════╝\n"
              << CleanerCore::C_RESET;

    std::string drive = CleanerCore::getSystemDriveRoot();
    long long freeBytes = CleanerCore::getAvailableDiskSpace(drive);
    bool admin = CleanerCore::isElevated();

    std::cout << " Ổ đĩa hệ thống: " << CleanerCore::C_YELLOW << drive << CleanerCore::C_RESET
              << " | Dung lượng trống: " << CleanerCore::C_GREEN << CleanerCore::formatSize(freeBytes) << CleanerCore::C_RESET
              << " | Quyền: " << (admin ? (std::string(CleanerCore::C_GREEN) + "Administrator") : (std::string(CleanerCore::C_RED) + "User thường (Cần Admin cho dọn sâu)")) << CleanerCore::C_RESET
              << "\n\n";
}

CleanStats DiskCleaner::runScope(CleanScope scope, bool dryRun) {
    CleanStats total;

    if (scope == CleanScope::SurfaceAndTemp || scope == CleanScope::All) {
        std::cout << CleanerCore::C_CYAN << " [*] Đang xử lý: File tạm (Temp) & Cache hệ thống cơ bản...\n" << CleanerCore::C_RESET;
        CleanStats s = TempCleaner::clean(dryRun);
        total.add(s);
        std::cout << "     └── Đã tìm thấy/giải phóng: " << CleanerCore::C_GREEN << CleanerCore::formatSize(s.bytesFreed) << CleanerCore::C_RESET
                  << " (" << s.filesDeleted << " files)\n\n";
    }

    if (scope == CleanScope::BrowserAndApps || scope == CleanScope::All) {
        std::cout << CleanerCore::C_CYAN << " [*] Đang xử lý: Cache trình duyệt & Zalo PC...\n" << CleanerCore::C_RESET;
        CleanStats s = BrowserCleaner::clean(dryRun);
        total.add(s);
        std::cout << "     └── Đã tìm thấy/giải phóng: " << CleanerCore::C_GREEN << CleanerCore::formatSize(s.bytesFreed) << CleanerCore::C_RESET
                  << " (" << s.filesDeleted << " files, " << s.dirsDeleted << " dirs)\n\n";
    }

    if (scope == CleanScope::DeepSystem || scope == CleanScope::All) {
        std::cout << CleanerCore::C_CYAN << " [*] Đang xử lý: Hệ thống chuyên sâu & Windows Update...\n" << CleanerCore::C_RESET;
        CleanStats s = SystemDeepCleaner::clean(dryRun, false);
        total.add(s);
        std::cout << "     └── Đã tìm thấy/giải phóng: " << CleanerCore::C_GREEN << CleanerCore::formatSize(s.bytesFreed) << CleanerCore::C_RESET
                  << " (" << s.filesDeleted << " files, " << s.dirsDeleted << " dirs)\n\n";
    }

    if (scope == CleanScope::DevEnvironments || scope == CleanScope::All) {
        std::cout << CleanerCore::C_CYAN << " [*] Đang xử lý: Môi trường lập trình (Dev Caches & Build Artifacts)...\n" << CleanerCore::C_RESET;
        CleanStats s = DevCleaner::clean(dryRun);
        total.add(s);
        std::cout << "     └── Đã tìm thấy/giải phóng: " << CleanerCore::C_GREEN << CleanerCore::formatSize(s.bytesFreed) << CleanerCore::C_RESET
                  << " (" << s.filesDeleted << " files, " << s.dirsDeleted << " dirs)\n\n";
    }

    // LỆNH DỌN DOWNLOADS NẰM Ở CUỐI CÙNG:
    // - File tải dở dang / lỗi (.crdownload, .part, .tmp): XÓA CỨNG TRIỆT ĐỂ.
    // - File lặp/trùng chỉ số nhỏ (file (1), file (2)...): XÓA MỀM (đưa vào Thùng rác để có thể lấy lại).
    if (scope == CleanScope::DownloadsSmart || scope == CleanScope::All) {
        std::cout << CleanerCore::C_CYAN << " [*] Đang xử lý: Thư mục Downloads (CHẠY CUỐI CÙNG)...\n" << CleanerCore::C_RESET;
        CleanStats s = DownloadsCleaner::clean(dryRun);
        total.add(s);
        if (s.filesDeleted > 0) {
            std::cout << "     ├── [XÓA CỨNG] File tải lỗi/dở dang (.crdownload, .part): " << CleanerCore::C_GREEN << CleanerCore::formatSize(s.bytesFreed) << CleanerCore::C_RESET
                      << " (" << s.filesDeleted << " files)\n";
        }
        std::cout << "     └── [XÓA MỀM] Đã đưa vào Thùng rác (file trùng lặp & bộ cài): " << CleanerCore::C_YELLOW << CleanerCore::formatSize(s.bytesRecycled) << CleanerCore::C_RESET
                  << " (" << s.filesRecycled << " files - có thể khôi phục từ Recycle Bin)\n\n";
    }

    return total;
}

CleanStats DiskCleaner::runAll(bool dryRun) {
    return runScope(CleanScope::All, dryRun);
}

void DiskCleaner::runAutomaticCleanup(bool waitAtEnd) {
    std::vector<AutoStageResult> stages = {
        {"Temp, cache cơ bản & Thùng rác", {}},
        {"Cache trình duyệt & Zalo PC", {}},
        {"Rác hệ thống, Windows Update & Component Store", {}},
        {"Cache và build artifacts môi trường lập trình", {}},
        {"Downloads thông minh", {}}
    };

    auto executeStage = [&](size_t index, const std::function<CleanStats()>& cleaner) {
        renderAutoDashboard(stages, stages[index].name, false);

        try {
            stages[index].stats = cleaner();
        } catch (const std::exception&) {
            stages[index].stats.errorsCount++;
        } catch (...) {
            stages[index].stats.errorsCount++;
        }
        renderAutoDashboard(stages, stages[index].name, false);
    };

    executeStage(0, [] { return TempCleaner::clean(false); });
    executeStage(1, [] { return BrowserCleaner::clean(false); });

    if (CleanerCore::isElevated()) {
        executeStage(2, [] { return SystemDeepCleaner::clean(false, true); });
    } else {
        stages[2].stats.itemsSkipped++;
    }

    executeStage(3, [] { return DevCleaner::clean(false, true); });
    // Downloads luôn chạy cuối để file vừa đưa vào Recycle Bin không bị dọn tiếp.
    executeStage(4, [] { return DownloadsCleaner::clean(false); });

    renderAutoDashboard(stages, "", true);
    if (waitAtEnd) CleanerCore::waitEnter();
}

void DiskCleaner::runInteractiveMenu() {
    while (true) {
        CleanerCore::cls();
        printBanner();

        std::cout << "  [1] CHẠY DỌN DẸP TỰ ĐỘNG\n"
                  << "  [2] THOÁT\n\n"
                  << "Chương trình sẽ tự kiểm tra từng tiêu chí, chỉ dọn khi có rác\n"
                  << "và cập nhật dung lượng đã xóa thật lên dashboard.\n\n"
                  << "Lựa chọn của bạn (1-2): ";

        std::string choice;
        std::getline(std::cin, choice);
        choice = CleanerCore::trim(choice);

        if (choice == "2") {
            std::cout << "\nTạm biệt!\n";
            break;
        } else if (choice == "1") {
            if (!CleanerCore::isElevated()) {
                std::cout << CleanerCore::C_YELLOW
                          << "\nĐang yêu cầu quyền Administrator để dọn đủ mọi tiêu chí...\n"
                          << CleanerCore::C_RESET;
                if (CleanerCore::restartAsAdmin("--auto")) {
                    return;
                }
            }
            runAutomaticCleanup(true);
        }
    }
}
