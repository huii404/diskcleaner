#include "DiskCleaner.h"
#include <iostream>
#include <string>

void printHelp(const char* exeName) {
    std::cout << "Cách sử dụng:\n"
              << "  " << exeName << "                Mở giao diện menu tương tác ANSI\n"
              << "  " << exeName << " --scan         Quét và ước tính dung lượng rác (không xóa)\n"
              << "  " << exeName << " --all          Thực thi dọn dẹp toàn bộ các phân hệ\n"
              << "  " << exeName << " --temp         Chỉ dọn file tạm và cache người dùng\n"
              << "  " << exeName << " --browser      Chỉ dọn cache trình duyệt & chat apps\n"
              << "  " << exeName << " --system       Chỉ dọn hệ thống chuyên sâu & Windows Update\n"
              << "  " << exeName << " --dev          Chỉ dọn cache môi trường dev (Python, Node...)\n"
              << "  " << exeName << " --downloads    Chỉ dọn bộ cài & file trùng trong Downloads\n"
              << "  " << exeName << " --help         Hiển thị trợ giúp này\n";
}

int main(int argc, char* argv[]) {
    CleanerCore::initConsole();

    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--help" || arg == "-h" || arg == "/?") {
            DiskCleaner::printBanner();
            printHelp(argv[0]);
            return 0;
        } else if (arg == "--scan") {
            DiskCleaner::runScanAnalysis();
            return 0;
        } else if (arg == "--all") {
            DiskCleaner::printBanner();
            CleanStats s = DiskCleaner::runAll(false);
            std::cout << "\nHoàn tất dọn dẹp toàn bộ! Đã giải phóng: " << CleanerCore::formatSize(s.bytesFreed) << "\n";
            return 0;
        } else if (arg == "--temp") {
            CleanStats s = DiskCleaner::runScope(CleanScope::SurfaceAndTemp, false);
            std::cout << "\nHoàn tất! Đã giải phóng: " << CleanerCore::formatSize(s.bytesFreed) << "\n";
            return 0;
        } else if (arg == "--browser") {
            CleanStats s = DiskCleaner::runScope(CleanScope::BrowserAndApps, false);
            std::cout << "\nHoàn tất! Đã giải phóng: " << CleanerCore::formatSize(s.bytesFreed) << "\n";
            return 0;
        } else if (arg == "--system") {
            CleanStats s = DiskCleaner::runScope(CleanScope::DeepSystem, false);
            std::cout << "\nHoàn tất! Đã giải phóng: " << CleanerCore::formatSize(s.bytesFreed) << "\n";
            return 0;
        } else if (arg == "--dev") {
            CleanStats s = DiskCleaner::runScope(CleanScope::DevEnvironments, false);
            std::cout << "\nHoàn tất! Đã giải phóng: " << CleanerCore::formatSize(s.bytesFreed) << "\n";
            return 0;
        } else if (arg == "--downloads") {
            CleanStats s = DiskCleaner::runScope(CleanScope::DownloadsSmart, false);
            std::cout << "\nHoàn tất! Đã chuyển vào Thùng rác: " << CleanerCore::formatSize(s.bytesRecycled) << "\n";
            return 0;
        } else {
            std::cout << "Tham số không hợp lệ: " << arg << "\n";
            printHelp(argv[0]);
            return 1;
        }
    }

    DiskCleaner::runInteractiveMenu();
    return 0;
}
