# Windows Disk Cleaner (C++17)

Công cụ dòng lệnh dọn dẹp rác hệ thống Windows chuyên sâu, tối ưu hiệu năng và an toàn dữ liệu.

---

## 🚀 Tính Năng Chính

* **Temp & Cache**: Dọn dẹp `%TEMP%`, System Temp, Prefetch, Crash Dumps, Shader Cache, Thumbnails và làm rỗng Thùng rác.
* **Trình duyệt & Zalo PC**: Hỗ trợ dọn cache Chromium (Chrome, Edge, Cốc Cốc, Brave...), Firefox và dọn rác chuyên sâu **Zalo PC** (xóa các bản cài cũ > 500MB sau update, zip update thừa 260MB, installer updater, temp media và Electron cache). Bảo tồn tuyệt đối dữ liệu tin nhắn, tài khoản và NVIDIA GLCache. Loại bỏ quét các app không dùng khác để tiết kiệm tài nguyên.
* **Hệ thống & Windows Update**: Dọn `SoftwareDistribution\Download`, `Windows.old`, `$WINDOWS.~BT`, Dump BSOD, log hệ thống và dọn dẹp DISM Component Store an toàn.
* **Môi trường Lập trình & AI Tools**: Quét và dọn cache package/build của Python (`pip`, `__pycache__`), Node.js (`npm`, `yarn`, `pnpm`), Java/Android (`gradle`), Go, Rust, VS Code... và các công cụ AI (**Antigravity**, **OpenAI/Codex**, **Claude**, **Cursor AI**). Tuyệt đối bảo vệ mã nguồn git, cấu hình, API keys/auth tokens và lịch sử chat.
* **Downloads Thông minh**:
  * Xóa vĩnh viễn file tải dang dở (`.crdownload`, `.part`, `.tmp` > 24h).
  * Chuyển vào Thùng rác (Recycle Bin) bộ cài (`.exe`, `.msi`) của ứng dụng đã cài đặt và các bản sao file ảnh/video/exe trùng lặp.

---

## 🛠️ Biên Dịch (Build)

**Yêu cầu:** Windows 10/11 (64-bit), `g++` hỗ trợ C++17 (MinGW-w64 / MSYS2).

### Cách 1: Dùng script tự động
Chạy file script:
```cmd
build.bat
```
File thực thi sẽ được xuất ra tại `bin\cleaner.exe`.

### Cách 2: Lệnh g++ thủ công
```bash
g++ -std=c++17 -O2 -Iinclude src\*.cpp -lshell32 -lole32 -ladvapi32 -lversion -luuid -static -s -o bin\cleaner.exe
```

---

## 📖 Hướng Dẫn Sử Dụng

> **Khuyến nghị**: Chạy chương trình dưới quyền **Administrator** để dọn sạch rác hệ thống và Windows Update.

### 1. Giao diện Menu tương tác
```cmd
bin\cleaner.exe
```
* **[1] CHẠY DỌN DẸP TỰ ĐỘNG**: Quét và dọn dẹp tuần tự toàn bộ các phân hệ, hiển thị dashboard dung lượng giải phóng trực tiếp.
* **[2] THOÁT**: Đóng công cụ.

### 2. Dòng lệnh (CLI / Silent Mode)

| Lệnh | Mô tả |
| :--- | :--- |
| `bin\cleaner.exe --all` | Dọn dẹp toàn bộ tất cả các phân hệ |
| `bin\cleaner.exe --temp` | Chỉ dọn file tạm và cache hệ thống |
| `bin\cleaner.exe --browser` | Chỉ dọn cache trình duyệt & chat |
| `bin\cleaner.exe --system` | Chỉ dọn hệ thống chuyên sâu & Windows Update |
| `bin\cleaner.exe --dev` | Chỉ dọn cache môi trường lập trình |
| `bin\cleaner.exe --downloads` | Chỉ dọn bộ cài đã dùng & file trùng trong Downloads |
| `bin\cleaner.exe --help` | Hiển thị bảng trợ giúp |

---

## 🛡️ Cơ Chế An Toàn

1. **Bảo tồn dữ liệu quan trọng**: Tuyệt đối không xóa repo git (`.git`), file nguồn dev, hay cache đồ họa game nhạy cảm.
2. **Xóa mềm Downloads**: Các tệp nghi vấn (bản sao trùng, bộ cài cũ) được chuyển vào Thùng rác thay vì xóa vĩnh viễn.
3. **Thứ tự dọn rác chuẩn**: Thùng rác được dọn trước khi xử lý Downloads, đảm bảo file xóa mềm từ Downloads được giữ lại trong Thùng rác để khôi phục nếu cần.

