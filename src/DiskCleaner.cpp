#include "DiskCleaner.h"
#include <iostream>
#include <iomanip>

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

void DiskCleaner::printScopePreview(CleanScope scope, bool dryRun) {
    std::cout << CleanerCore::C_YELLOW << "\n== XEM TRƯỚC PHẠM VI DỌN DẸP (" << (dryRun ? "QUÉT PHÂN TÍCH" : "THỰC THI") << ") ==\n" << CleanerCore::C_RESET;
    switch (scope) {
        case CleanScope::SurfaceAndTemp:
            std::cout << " • Mục tiêu: Temp người dùng, System Temp, Prefetch, WER Crash dumps, Recent, DNS, Thùng rác.\n";
            break;
        case CleanScope::BrowserAndApps:
            std::cout << " • Mục tiêu: Cache đa profile Chrome, Edge, Brave, Cốc Cốc, Firefox, Discord, Telegram.\n";
            std::cout << " • Bảo vệ an toàn: Tuyệt đối không can thiệp cache driver đồ họa NVIDIA.\n";
            break;
        case CleanScope::DeepSystem:
            std::cout << " • Mục tiêu: Windows Update download staging, Windows.old, $WINDOWS.~BT, CBS/DISM logs, Minidump.\n";
            break;
        case CleanScope::DevEnvironments:
            std::cout << " • Mục tiêu: Python (__pycache__, pip), Node (npm, pnpm, yarn), Gradle, Go, Rust, VS Code caches.\n";
            break;
        case CleanScope::DownloadsSmart:
            std::cout << " • Mục tiêu: Bộ cài (.exe/.msi), file ảnh, file video bị trùng lặp, file tải hỏng (.crdownload).\n";
            std::cout << " • Quy tắc giữ file: Chỉ giữ file gốc (file.ext) và file có chỉ số cao nhất (file (N).ext).\n";
            std::cout << " • Cơ chế an toàn: Chỉ quét đúng nhóm exe, ảnh, video; chuyển vào Thùng rác (Recycle Bin).\n";
            break;
        case CleanScope::All:
            std::cout << " • Mục tiêu: TOÀN BỘ 5 PHẠM VI TRÊN (Dọn dẹp triệt để toàn diện).\n";
            break;
    }
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
        std::cout << CleanerCore::C_CYAN << " [*] Đang xử lý: Cache trình duyệt (Chromium/Gecko) & Chat apps...\n" << CleanerCore::C_RESET;
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

    // NGUYÊN TẮC THỨ TỰ AN TOÀN:
    // Làm sạch toàn bộ Thùng rác hệ thống TRƯỚC KHI lệnh dọn Downloads chạy.
    // Nhờ đó, file trùng lặp đưa vào sọt rác ở bước cuối sẽ KHÔNG bị lệnh làm rỗng thùng rác xóa mất!
    if (scope == CleanScope::All) {
        std::cout << CleanerCore::C_CYAN << " [*] Đang dọn sạch Thùng rác hệ thống (trước khi xử lý Downloads)...\n" << CleanerCore::C_RESET;
        CleanStats sRb;
        CleanerCore::emptyRecycleBin(dryRun, sRb);
        total.add(sRb);
        std::cout << "     └── Đã dọn sạch Thùng rác: " << CleanerCore::C_GREEN << CleanerCore::formatSize(sRb.bytesFreed) << CleanerCore::C_RESET
                  << " (" << sRb.filesDeleted << " files cũ đã giải phóng)\n\n";
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

void DiskCleaner::runScanAnalysis() {
    CleanerCore::cls();
    printBanner();
    std::cout << CleanerCore::C_YELLOW << "=== BẮT ĐẦU QUÉT PHÂN TÍCH RÁC TOÀN HỆ THỐNG (DRY-RUN) ===\n"
              << "Chế độ này chỉ tính toán dung lượng và liệt kê, TUYỆT ĐỐI KHÔNG XÓA bất kỳ file nào.\n\n" << CleanerCore::C_RESET;

    CleanStats total = runAll(true);

    std::cout << CleanerCore::C_GREEN << CleanerCore::C_BOLD
              << "╔══════════════════════════════════════════════════════════════╗\n"
              << "║                    KẾT QUẢ QUÉT TOÀN BỘ                      ║\n"
              << "╚══════════════════════════════════════════════════════════════╝\n"
              << CleanerCore::C_RESET;
    std::cout << " [✓] Tổng dung lượng rác có thể GIẢI PHÓNG TRỰC TIẾP: " << CleanerCore::C_GREEN << CleanerCore::C_BOLD << CleanerCore::formatSize(total.bytesFreed) << CleanerCore::C_RESET << "\n";
    std::cout << " [✓] Tổng file cài / trùng lặp có thể DỌN VÀO THÙNG RÁC: " << CleanerCore::C_YELLOW << CleanerCore::C_BOLD << CleanerCore::formatSize(total.bytesRecycled) << CleanerCore::C_RESET << "\n";
    std::cout << " [✓] Tổng số files: " << (total.filesDeleted + total.filesRecycled) << " | Thư mục rác: " << total.dirsDeleted << "\n\n";

    if (total.bytesFreed > 0 || total.bytesRecycled > 0) {
        if (CleanerCore::confirm("Bạn có muốn thực thi DỌN DẸP NGAY BÂY GIỜ không? (y/N): ")) {
            std::cout << "\n";
            CleanStats realStats = runAll(false);
            std::cout << CleanerCore::C_GREEN << "\n[✓] HOÀN TẤT DỌN DẸP! Đã giải phóng: " << CleanerCore::formatSize(realStats.bytesFreed)
                      << " (và " << CleanerCore::formatSize(realStats.bytesRecycled) << " trong Thùng rác).\n" << CleanerCore::C_RESET;
        }
    }

    CleanerCore::waitEnter();
}

void DiskCleaner::runInteractiveMenu() {
    while (true) {
        CleanerCore::cls();
        printBanner();

        std::cout << "  [1] Quét & Phân tích rác toàn hệ thống (Scan Only / Dry-run)\n"
                  << "  [2] Dọn rác nhanh: Temp, Prefetch & Log người dùng\n"
                  << "  [3] Dọn Cache toàn diện: Trình duyệt & Chat Apps\n"
                  << "  [4] Dọn hệ thống chuyên sâu & Windows Update (Cần Admin)\n"
                  << "  [5] Dọn rác môi trường lập trình (Python, Node, Java, IDEs...)\n"
                  << "  [6] Dọn thông minh Downloads: Bộ cài ứng dụng đã cài & File trùng\n"
                  << "  [7] Dọn dẹp TOÀN DIỆN (Thực thi toàn bộ các mục trên)\n"
                  << "  [8] Tự khởi động lại chương trình với quyền Administrator\n"
                  << "  [0] Thoát chương trình\n\n"
                  << "Lựa chọn của bạn (0-8): ";

        std::string choice;
        std::getline(std::cin, choice);
        choice = CleanerCore::trim(choice);

        if (choice == "0") {
            std::cout << "\nTạm biệt!\n";
            break;
        } else if (choice == "1") {
            runScanAnalysis();
        } else if (choice == "2") {
            printScopePreview(CleanScope::SurfaceAndTemp, false);
            if (CleanerCore::confirm("Xác nhận dọn dẹp? (y/N): ")) {
                CleanStats s = runScope(CleanScope::SurfaceAndTemp, false);
                std::cout << CleanerCore::C_GREEN << "[✓] Đã giải phóng: " << CleanerCore::formatSize(s.bytesFreed) << CleanerCore::C_RESET << "\n";
                CleanerCore::waitEnter();
            }
        } else if (choice == "3") {
            printScopePreview(CleanScope::BrowserAndApps, false);
            if (CleanerCore::confirm("Xác nhận dọn dẹp? (y/N): ")) {
                CleanStats s = runScope(CleanScope::BrowserAndApps, false);
                std::cout << CleanerCore::C_GREEN << "[✓] Đã giải phóng: " << CleanerCore::formatSize(s.bytesFreed) << CleanerCore::C_RESET << "\n";
                CleanerCore::waitEnter();
            }
        } else if (choice == "4") {
            if (!CleanerCore::isElevated()) {
                std::cout << CleanerCore::C_RED << "\n[!] Tác vụ này cần quyền Administrator! Hãy chọn mục [8] để nâng quyền.\n" << CleanerCore::C_RESET;
                CleanerCore::waitEnter();
                continue;
            }
            printScopePreview(CleanScope::DeepSystem, false);
            if (CleanerCore::confirm("Xác nhận dọn dẹp? (y/N): ")) {
                CleanStats s = runScope(CleanScope::DeepSystem, false);
                std::cout << CleanerCore::C_GREEN << "[✓] Đã giải phóng: " << CleanerCore::formatSize(s.bytesFreed) << CleanerCore::C_RESET << "\n";
                CleanerCore::waitEnter();
            }
        } else if (choice == "5") {
            printScopePreview(CleanScope::DevEnvironments, false);
            if (CleanerCore::confirm("Xác nhận dọn dẹp? (y/N): ")) {
                CleanStats s = runScope(CleanScope::DevEnvironments, false);
                std::cout << CleanerCore::C_GREEN << "[✓] Đã giải phóng: " << CleanerCore::formatSize(s.bytesFreed) << CleanerCore::C_RESET << "\n";
                CleanerCore::waitEnter();
            }
        } else if (choice == "6") {
            printScopePreview(CleanScope::DownloadsSmart, false);
            if (CleanerCore::confirm("Xác nhận chuyển file vào Thùng rác? (y/N): ")) {
                CleanStats s = runScope(CleanScope::DownloadsSmart, false);
                std::cout << CleanerCore::C_YELLOW << "[✓] Đã chuyển vào Thùng rác: " << CleanerCore::formatSize(s.bytesRecycled) << CleanerCore::C_RESET << "\n";
                CleanerCore::waitEnter();
            }
        } else if (choice == "7") {
            printScopePreview(CleanScope::All, false);
            if (CleanerCore::confirm("Xác nhận thực thi DỌN DẸP TOÀN BỘ? (y/N): ")) {
                CleanStats s = runAll(false);
                std::cout << CleanerCore::C_GREEN << "\n[✓] TỔNG DUNG LƯỢNG ĐÃ GIẢI PHÓNG: " << CleanerCore::formatSize(s.bytesFreed) << CleanerCore::C_RESET << "\n";
                if (s.bytesRecycled > 0) {
                    std::cout << CleanerCore::C_YELLOW << "[✓] Tổng dung lượng chuyển vào Thùng rác: " << CleanerCore::formatSize(s.bytesRecycled) << CleanerCore::C_RESET << "\n";
                }
                CleanerCore::waitEnter();
            }
        } else if (choice == "8") {
            if (CleanerCore::isElevated()) {
                std::cout << CleanerCore::C_GREEN << "\nỨng dụng đã đang chạy dưới quyền Administrator!\n" << CleanerCore::C_RESET;
                CleanerCore::waitEnter();
            } else {
                std::cout << CleanerCore::C_YELLOW << "\nĐang yêu cầu nâng quyền Administrator qua UAC...\n" << CleanerCore::C_RESET;
                if (CleanerCore::restartAsAdmin()) {
                    return;
                } else {
                    std::cout << CleanerCore::C_RED << "Người dùng đã từ chối nâng quyền UAC.\n" << CleanerCore::C_RESET;
                    CleanerCore::waitEnter();
                }
            }
        }
    }
}
