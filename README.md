# HỆ THỐNG DỌN DẸP RÁC CHUYÊN SÂU WINDOWS (C++17)

Dự án C++ độc lập chuyên nghiên cứu, tối ưu và mở rộng các giải pháp dọn dẹp rác hệ thống, giải phóng dung lượng ổ đĩa một cách an toàn và triệt để trên hệ điều hành Windows.

---

## 📁 Cấu trúc Thư mục Dự án

```text
G:\Code\C++\project\Bin\
├── include/                     # Header files chứa định nghĩa module
│   ├── CleanerCore.h            # Tiện ích Win32 API lõi, quyền Admin, thao tác file an toàn
│   ├── TempCleaner.h            # Dọn file tạm & cache người dùng
│   ├── BrowserCleaner.h         # Dọn cache trình duyệt (Chromium/Gecko) & chat apps
│   ├── SystemDeepCleaner.h      # Dọn Windows Update, Windows.old, Kernel dumps, DISM
│   ├── DevCleaner.h             # Dọn cache môi trường dev (Python, Node, Gradle, IDEs...)
│   ├── DownloadsCleaner.h       # Dọn an toàn & thông minh thư mục Downloads (Ảnh, Video, Exe)
│   └── DiskCleaner.h            # Bộ điều phối trung tâm & giao diện Menu CLI
├── src/                         # Mã nguồn C++17 triển khai logic
│   ├── CleanerCore.cpp
│   ├── TempCleaner.cpp
│   ├── BrowserCleaner.cpp
│   ├── SystemDeepCleaner.cpp
│   ├── DevCleaner.cpp
│   ├── DownloadsCleaner.cpp
│   ├── DiskCleaner.cpp
│   └── main.cpp                 # Điểm khởi chạy CLI (hỗ trợ tham số và Menu tương tác)
├── reference/                   # Mã nguồn gốc đối chiếu trích xuất từ CMD
│   ├── DiskCleaner.h
│   ├── DiskCleaner.cpp
│   └── reset_windows_update.bat
├── bin/                         # Thư mục chứa file thực thi sau khi biên dịch
│   └── cleaner.exe
├── build.bat                    # Script tự động tìm g++ và biên dịch dự án
└── README.md                    # Tài liệu kiến trúc và hướng dẫn sử dụng
```

---

## 🧹 Các Thể Loại & Logic Xóa Rác Chi Tiết

Hệ thống được chia thành **5 nhóm module chuyên biệt**, mỗi nhóm xử lý một khu vực dữ liệu rác cụ thể:

### 1. Dọn File Tạm & Cache Bề Mặt (`TempCleaner`)
* **`%TEMP%` (User Temp)**: Các file tạm sinh ra trong phiên làm việc của các phần mềm người dùng.
* **`%SYSTEMROOT%\Temp` (System Temp)**: File tạm cấp hệ thống và dịch vụ nền Windows (yêu cầu Administrator).
* **`%SYSTEMROOT%\Prefetch`**: File đệm khởi động ứng dụng cũ, Windows lưu lại lịch sử thực thi của các file `.exe` (yêu cầu Administrator).
* **`CrashDumps` & `WER` (Windows Error Reporting)**: Báo cáo sự cố ứng dụng lưu tại `%LocalAppData%\CrashDumps`, `%LocalAppData%\Microsoft\Windows\WER\...` và `%ProgramData%\Microsoft\Windows\WER\Temp`.
* **DirectX Shader & URL Cache**:
  * `%LocalAppData%\D3DSCache`: Bộ đệm shader Direct3D cũ.
  * `%LocalAppData%Low\Microsoft\CryptnetUrlCache`: Chứng chỉ mạng đã hết hạn.
  * `%LocalAppData%\Microsoft\Windows\INetCache`: Bộ nhớ đệm WinInet.
* **Thumbcache & Recent Items**: Cache biểu tượng ảnh thu nhỏ trong Windows Explorer (`thumbcache_*.db`) và danh sách tệp mở gần đây.
* **Thùng rác (Recycle Bin)**: Dùng Win32 API `SHEmptyRecycleBinW` dọn rác trực tiếp siêu tốc mà không cần gọi tiến trình con.
* **DNS Cache**: Gọi hàm `DnsFlushResolverCache` từ thư viện `dnsapi.dll` để làm mới bảng phân giải tên miền.

---

### 2. Dọn Cache Trình Duyệt & Ứng Dụng Chat (`BrowserCleaner`)
* **Trình duyệt nhân Chromium (Đa profile)**:
  * Hỗ trợ tự động nhận diện: **Google Chrome, Microsoft Edge, Cốc Cốc, Brave, Vivaldi, Opera, Opera GX**.
  * Quét đệ quy qua mọi profile: `Default`, `Profile 1`, `Profile 2`, `Guest Profile`, `System Profile`.
  * Các loại cache chuyên biệt được dọn sạch:
    * `Cache`: Bộ đệm web HTTP/HTTPS tĩnh.
    * `Code Cache`: JavaScript V8 bytecode đã biên dịch.
    * `GPUCache`, `DawnCache`, `ShaderCache`, `GrShaderCache`, `GraphiteDawnCache`: Bộ đệm đồ họa của trình duyệt.
    * `Service Worker\CacheStorage` & `ScriptCache`: Bộ đệm ứng dụng web chạy nền.
* **Mozilla Firefox (Gecko)**:
  * Quét qua cả 2 thư mục `%APPDATA%\Mozilla\Firefox\Profiles` và `%LOCALAPPDATA%\Mozilla\Firefox\Profiles`.
  * Dọn sạch: `cache2`, `startupCache`, `jumpListCache`.
* **Ứng dụng chat**: Dọn cache hình ảnh và dữ liệu tạm của **Discord** (`Cache`, `Code Cache`) và **Telegram Desktop** (`tdata\user_data\cache`).
* **BẢO VỆ ĐỒ HỌA (Đã loại bỏ NVIDIA GLCache)**:
  * Theo yêu cầu an toàn, hệ thống **không can thiệp vào `%LocalAppData%\NVIDIA\GLCache`**, ngăn ngừa hoàn toàn nguy cơ giật lag hoặc lỗi hiển thị game/đồ họa 3D.

---

### 3. Dọn Hệ Thống Chuyên Sâu & Windows Update (`SystemDeepCleaner`)
*(Yêu cầu quyền Administrator)*
* **Windows Update Download Staging**:
  * Vị trí: `%SystemRoot%\SoftwareDistribution\Download`.
  * Logic an toàn: Tạm ngắt dịch vụ `wuauserv` (Windows Update) và `bits` (Background Intelligent Transfer Service), xóa sạch các gói cập nhật đã cài xong còn tồn đọng, sau đó khởi động lại dịch vụ ngay lập tức.
* **Delivery Optimization Cache**: Xóa bộ đệm phân phối bản cập nhật qua mạng nội bộ LAN (`%ProgramData%\Microsoft\Windows\DeliveryOptimization\Cache`).
* **Tồn dư bản nâng cấp lớn Windows**:
  * Các thư mục: `C:\Windows.old`, `C:\$WINDOWS.~BT`, `C:\$WINDOWS.~WS`.
  * Cơ chế cấp quyền đặc biệt: Tự động dùng lệnh `takeown` và `icacls *S-1-5-32-544:F` (SID quản trị viên chuẩn quốc tế, chạy được trên mọi phiên bản ngôn ngữ Windows) để cấp toàn quyền xóa trước khi loại bỏ thư mục.
* **Kernel Dumps & Log hệ thống**:
  * BSOD Crash Dumps: `MEMORY.DMP`, `%SystemRoot%\Minidump`.
  * Log lỗi và cài đặt: `%SystemRoot%\Panther`, `LiveKernelReports`, `Logs\CBS`, `Logs\DISM`, `WindowsUpdate.log`.
* **DISM WinSxS Component Store (Tùy chọn)**: Hỗ trợ dọn dẹp các phiên bản component cũ trong WinSxS bằng lệnh `dism.exe /online /cleanup-image /startcomponentcleanup /resetbase`.

---

### 4. Dọn Môi Trường Lập Trình & Build Artifacts (`DevCleaner`)
* **Nguyên tắc bảo vệ mã nguồn tuyệt đối**:
  * Bộ quét đệ quy luôn bỏ qua và bảo vệ 100%: `.git`, `.github`, `.gitignore`, `.gitattributes`.
  * Bảo vệ các kho thư viện chung của máy tính: `.m2/repository` (Maven), `.nuget/packages`.
* **Tự động dò tìm thư mục dự án**: Tự động duyệt qua toàn bộ các ổ cứng cố định (C:, D:, E:, ...) vào các thư mục dev phổ biến: `Code`, `Projects`, `Source`, `Repos`, `Dev`, `Web`, `Workspace`, cùng với `Desktop`, `Documents`.
* **Phân hệ dọn dẹp**:
  * **Python**: `%LocalAppData%\pip\cache`, các thư mục bytecode `__pycache__`, `.pytest_cache`, `.mypy_cache`, `.ruff_cache`, `.tox`, file `*.pyc`, `*.pyo`.
  * **Node.js / Web**: `npm-cache`, `Yarn\Cache`, `pnpm\store`, `pnpm\cache`, `electron\Cache`, `Microsoft\TypeScript`, `deno\deps`, `.turbo`, `.parcel-cache`.
  * **Java & Android**: `.gradle\caches`, `.gradle\daemon`, `.android\cache`.
  * **IDEs & Biên dịch**: VS Code (`Code\Cache`, `CachedData`, `CachedExtensionVSIXs`), Cursor (`Cursor\Cache`), `go-build` (Go), `cargo\registry\cache`, `rustup\downloads` (Rust), NuGet `v3-cache`.

---

### 5. Dọn Dẹp An Toàn & Thông Minh Thư Mục Downloads (`DownloadsCleaner`)

#### 🛡️ Cơ Chế Phân Biệt Xóa Cứng vs Xóa Mềm:
1. **XÓA CỨNG TRIỆT ĐỂ (Hard Delete)**:
   * Áp dụng riêng cho **File tải lỗi/dở dang**: `.crdownload`, `.part`, `.tmp` (cũ hơn 24 giờ).
   * Các file này bị ngắt kết nối tải, hỏng header hoặc đứt gãy giữa chừng, hoàn toàn không có giá trị khôi phục $\rightarrow$ Xóa vĩnh viễn trực tiếp bằng `CleanerCore::safeDeleteFile`, không đưa vào Thùng rác làm bẩn sọt rác.
2. **XÓA MỀM AN TOÀN (Soft Delete)**:
   * Áp dụng cho **File trùng lặp (Ảnh, Video, Exe)** và **Bộ cài đặt của app đã cài**.
   * Chuyển vào **Thùng rác (Recycle Bin)** bằng Windows Shell API (`SHFileOperationW` với cờ `FOF_ALLOWUNDO`) $\rightarrow$ Tạo điều kiện lấy lại file bất cứ khi nào bạn muốn.

#### ⚡ GIẢI QUYẾT XUNG ĐỘT THỨ TỰ (Execution Order Guarantee):
* **Vấn đề tiềm ẩn**: Nếu lệnh dọn Thùng rác (`emptyRecycleBin`) chạy sau hoặc chạy xen kẽ, nó sẽ vô hình cuốn sạch luôn cả các file trùng vừa mới xóa mềm vào sọt rác!
* **Cách khắc phục chuẩn**:
  * Trong chuỗi tự động (`runAll` hoặc `--all`), lệnh **Làm rỗng Thùng rác** được thực thi **TRƯỚC** để quét sạch sẽ toàn bộ rác tồn dư cũ của hệ thống.
  * Lệnh dọn dẹp thư mục **Downloads LUÔN NẰM Ở CUỐI CÙNG** của chuỗi tự động.
  * **KẾT QUẢ**: Khi chương trình kết thúc, Thùng rác của bạn **sạch bóng mọi rác cũ**, và **CHỈ CHỨA DUY NHẤT các file trùng lặp của Downloads** vừa ném vào, an toàn 100% để bạn kiểm tra hoặc khôi phục!

#### 📦 Logic Dọn File Cài Đặt (.exe, .msi):
* Trích xuất thông tin `ProductName` từ PE Header Version Resource (thông qua `version.dll`).
* Quét cơ sở dữ liệu Uninstall trong Windows Registry:
  * `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall` (64-bit)
  * `HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall` (32-bit)
  * `HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall` (User-level)
* Nếu phần mềm **đã được cài đặt** trên máy -> chuyển bộ cài đặt vào Thùng rác.
* Tự động bỏ qua các file có nhãn `portable` hoặc `standalone`.

#### 🔁 Logic Dọn Rác Trùng Lặp (Ảnh, Video, Exe):
* **Nhận diện bản sao**: Sử dụng Regex nhận diện quy ước đặt tên bản sao trùng lặp tự động của Windows:
  $$\text{filename (N).ext} \quad (N \ge 1)$$
* **Gom nhóm**: Tất cả file cùng tên gốc và cùng định dạng được nhóm lại với nhau:
  $$\text{Ví dụ: } \{\text{file.mp4}, \text{file (1).mp4}, \text{file (2).mp4}, \text{file (5).mp4}\}$$
* **QUY TẮC GIỮ FILE**:
  * ✅ **GIỮ LẠI:** File gốc (`file.ext` - chỉ số 0).
  * ✅ **GIỮ LẠI:** File có chỉ số cao nhất nếu có bản sao (`file (5).ext` - chỉ số cao nhất $N_{max}$).
  * 🗑️ **CHUYỂN VÀO THÙNG RÁC:** Toàn bộ các bản sao trung gian ở giữa (`file (1).ext`, `file (2).ext`...).
  * *(Nếu không có file gốc mà chỉ có các bản sao $(1), (2), (5)$... thì giữ bản sao đầu tiên $(1)$ và bản sao cao nhất $(5)$)*.

---

## 🚀 Hướng Dẫn Biên Dịch & Chạy

### Yêu Cầu Môi Trường
* Hệ điều hành: Windows 10 / 11 (64-bit).
* Trình biên dịch: `g++` hỗ trợ C++17 (MinGW-w64 hoặc MSYS2 UCRT64).

### 1. Biên Dịch Bằng Script Tự Động
Chỉ cần nhấp đúp hoặc chạy script:
```cmd
build.bat
```
Script sẽ tự động dò tìm `g++` trong PATH hoặc MSYS2, biên dịch toàn bộ source files trong `src/` và tạo ra `bin\cleaner.exe`.

### 2. Biên Dịch Thủ Công Bằng Dòng Lệnh
```bash
g++ -std=c++17 -O3 -Iinclude src\*.cpp -lshlwapi -lshell32 -lole32 -ladvapi32 -lversion -luuid -static-libgcc -static-libstdc++ -static -s -o bin\cleaner.exe
```

### 3. Cách Sử Dụng Chương Trình

#### Chạy giao diện Menu tương tác:
```cmd
bin\cleaner.exe
```
Menu gồm 8 chức năng trực quan:
* `[1]`: Quét & Phân tích rác toàn hệ thống (Dry-run, không xóa).
* `[2]`: Dọn rác nhanh (Temp, Prefetch & Log người dùng).
* `[3]`: Dọn Cache trình duyệt & Chat Apps.
* `[4]`: Dọn hệ thống chuyên sâu & Windows Update (cần Admin).
* `[5]`: Dọn rác môi trường lập trình (Python, Node, Gradle, IDEs...).
* `[6]`: Dọn thông minh Downloads (Ảnh, Video, Exe trùng lặp).
* `[7]`: Dọn dẹp TOÀN DIỆN (Thực thi toàn bộ các phân hệ).
* `[8]`: Tự khởi động lại chương trình với quyền Administrator qua UAC.

#### Chạy trực tiếp qua tham số dòng lệnh (Headless / Silent):
| Lệnh | Ý nghĩa |
| :--- | :--- |
| `bin\cleaner.exe --scan` | Quét phân tích tính toán dung lượng rác (không xóa) |
| `bin\cleaner.exe --all` | Thực thi dọn dẹp toàn bộ tất cả các phân hệ |
| `bin\cleaner.exe --temp` | Chỉ dọn file tạm và cache cơ bản |
| `bin\cleaner.exe --browser` | Chỉ dọn cache trình duyệt và chat apps |
| `bin\cleaner.exe --system` | Chỉ dọn hệ thống chuyên sâu & Windows Update |
| `bin\cleaner.exe --dev` | Chỉ dọn rác môi trường dev (Python, Node...) |
| `bin\cleaner.exe --downloads` | Chỉ dọn bộ cài, ảnh, video trùng trong Downloads |
| `bin\cleaner.exe --help` | Xem bảng trợ giúp tham số |

---

## 🔬 Ý Tưởng Nghiên Cứu Mở Rộng Tiếp Theo

1. **Volume Shadow Copies (VSS)**: Tích hợp `vssadmin resize shadowstorage` hoặc `vssadmin delete shadows /for=c: /oldest` để thu hồi dung lượng restore point cũ.
2. **CompactOS Compression**: Tích hợp lệnh `compact.exe /compactos:always` để nén các file nhị phân của Windows mà không ảnh hưởng hiệu năng.
3. **Hibernation File (`hiberfil.sys`)**: Tùy chọn thu nhỏ hoặc tắt chế độ ngủ đông (`powercfg -h off` hoặc `powercfg -h -type reduced`) để giải phóng ngay lập tức dung lượng bằng 50%-100% dung lượng RAM.
4. **Phân tích hình ảnh tương đồng (Perceptual Hashing)**: Mở rộng khả năng phát hiện ảnh trùng lặp dù đã bị đổi tên hoàn toàn bằng thuật toán pHash hoặc dHash.
