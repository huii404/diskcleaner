#include "CleanerCore.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <iomanip>
#include <cwctype>
#include <cstring>
#include <shellapi.h>
#include <shlobj.h>

const char* CleanerCore::C_RESET   = "\033[0m";
const char* CleanerCore::C_BOLD    = "\033[1m";
const char* CleanerCore::C_RED     = "\033[91m";
const char* CleanerCore::C_GREEN   = "\033[92m";
const char* CleanerCore::C_YELLOW  = "\033[93m";
const char* CleanerCore::C_CYAN    = "\033[96m";

namespace {
bool isExpectedCleanupFailure(const std::error_code& ec) {
    return ec == std::errc::permission_denied ||
           ec.value() == ERROR_ACCESS_DENIED ||
           ec.value() == ERROR_SHARING_VIOLATION ||
           ec.value() == ERROR_LOCK_VIOLATION ||
           ec.value() == ERROR_CURRENT_DIRECTORY;
}

void recordCleanupFailure(const std::error_code& ec, CleanStats& stats) {
    if (isExpectedCleanupFailure(ec)) stats.itemsSkipped++;
    else stats.errorsCount++;
}
}

void CleanerCore::initConsole() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
}

void CleanerCore::cls() {
    std::cout << "\033[2J\033[1;1H" << std::flush;
}

void CleanerCore::waitEnter() {
    std::cout << C_YELLOW << "\nNhấn [Enter] để tiếp tục..." << C_RESET;
    std::string dummy;
    std::getline(std::cin, dummy);
}

std::string CleanerCore::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string CleanerCore::toLower(const std::string& str) {
    std::string res = str;
    std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return res;
}

std::string CleanerCore::formatSize(long long bytes) {
    if (bytes <= 0) return "0 B";
    static const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    double sz = static_cast<double>(bytes);
    int unitIdx = 0;
    while (sz >= 1024.0 && unitIdx < 5) {
        sz /= 1024.0;
        unitIdx++;
    }
    std::ostringstream ss;
    if (unitIdx == 0) {
        ss << static_cast<long long>(sz) << " " << units[unitIdx];
    } else {
        ss << std::fixed << std::setprecision(2) << sz << " " << units[unitIdx];
    }
    return ss.str();
}

std::string CleanerCore::getSystemDriveRoot() {
    char sysDrive[MAX_PATH] = {0};
    if (GetEnvironmentVariableA("SystemDrive", sysDrive, sizeof(sysDrive)) > 0) {
        std::string drive = sysDrive;
        if (!drive.empty() && drive.back() != '\\') {
            drive += "\\";
        }
        return drive;
    }
    return "C:\\";
}

long long CleanerCore::getAvailableDiskSpace(const std::string& drivePath) {
    std::string path = drivePath.empty() ? getSystemDriveRoot() : drivePath;
    std::error_code ec;
    auto spaceInfo = fs::space(path, ec);
    if (ec) return 0;
    return static_cast<long long>(spaceInfo.available);
}

bool CleanerCore::isCriticalPath(const fs::path& p) {
    std::error_code ec;
    fs::path normalized = fs::weakly_canonical(p, ec);
    if (ec) {
        ec.clear();
        normalized = fs::absolute(p, ec).lexically_normal();
    }
    if (normalized.empty() || normalized == normalized.root_path()) return true;

    // Cache danh sách các đường dẫn hệ thống quan trọng để tránh gọi GetLogicalDrives() và tính toán lại
    static const std::vector<fs::path> criticalList = []() {
        std::vector<fs::path> list;
        std::string sysDriveStr = getSystemDriveRoot();
        fs::path sysDrive(sysDriveStr);

        static const std::vector<std::wstring> blocked = {
            L"Windows", L"Windows\\System32", L"Windows\\SysWOW64",
            L"Users", L"Program Files", L"Program Files (x86)",
            L"ProgramData", L"Recovery", L"System Volume Information"
        };
        for (const auto& b : blocked) {
            list.push_back(sysDrive / b);
        }

        DWORD driveMask = GetLogicalDrives();
        for (char c = 'A'; c <= 'Z'; ++c) {
            if (driveMask & (1 << (c - 'A'))) {
                list.push_back(fs::path(std::string(1, c) + ":\\"));
            }
        }
        return list;
    }();

    for (const auto& critical : criticalList) {
        if (fs::equivalent(normalized, critical, ec)) return true;
        if (ec) ec.clear();

        std::wstring lhs = normalized.lexically_normal().wstring();
        std::wstring rhs = critical.lexically_normal().wstring();
        std::transform(lhs.begin(), lhs.end(), lhs.begin(), ::towlower);
        std::transform(rhs.begin(), rhs.end(), rhs.begin(), ::towlower);
        if (lhs == rhs) return true;
    }

    return false;
}

bool CleanerCore::isElevated() {
    bool elevated = false;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            elevated = (elevation.TokenIsElevated != 0);
        }
        CloseHandle(hToken);
    }
    return elevated;
}

bool CleanerCore::restartAsAdmin(const std::string& args) {
    char exePath[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    SHELLEXECUTEINFOA sei{};
    sei.cbSize = sizeof(sei);
    sei.lpVerb = "runas";
    sei.lpFile = exePath;
    sei.lpParameters = args.c_str();
    sei.nShow = SW_NORMAL;

    if (ShellExecuteExA(&sei)) {
        return true;
    }
    return false;
}

bool CleanerCore::runCommand(const std::string& cmd, bool hideWindow) {
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (hideWindow) {
        si.dwFlags |= STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
    }
    std::string fullCmd = "cmd.exe /c " + cmd;
    std::vector<char> cmdVec(fullCmd.begin(), fullCmd.end());
    cmdVec.push_back('\0');

    DWORD flags = hideWindow ? CREATE_NO_WINDOW : 0;
    if (CreateProcessA(NULL, cmdVec.data(), NULL, NULL, FALSE, flags, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return (exitCode == 0);
    }
    return false;
}

bool CleanerCore::wipeFolderContents(const fs::path& dirPath, bool dryRun, CleanStats& stats) {
    std::error_code ec;
    if (!fs::exists(dirPath, ec)) return true;

    // BẢO VỆ AN TOÀN: Tuyệt đối không xóa thư mục gốc hệ thống
    if (isCriticalPath(dirPath)) {
        std::cout << C_RED << " [CHẶN] Từ chối xóa đường dẫn nguy hiểm: " << dirPath.string() << C_RESET << "\n";
        stats.errorsCount++;
        return false;
    }

    try {
        for (const auto& entry : fs::directory_iterator(dirPath, fs::directory_options::skip_permission_denied, ec)) {
            if (ec) { ec.clear(); continue; }
            try {
                if (entry.is_regular_file(ec)) {
                    uintmax_t sz = entry.file_size(ec);
                    if (ec) { sz = 0; ec.clear(); }
                    if (dryRun) {
                        stats.bytesFreed += sz;
                        stats.filesDeleted++;
                    } else {
                        SetFileAttributesW(entry.path().c_str(), FILE_ATTRIBUTE_NORMAL);
                        if (fs::remove(entry.path(), ec)) {
                            stats.bytesFreed += sz;
                            stats.filesDeleted++;
                        } else {
                            recordCleanupFailure(ec, stats);
                        }
                    }
                } else if (entry.is_directory(ec)) {
                    // Gọi forceDeleteFolder — hàm này tự tính size và cập nhật stats
                    forceDeleteFolder(entry.path(), dryRun, stats);
                }
            } catch (...) {
                stats.errorsCount++;
            }
        }
    } catch (...) {
        stats.errorsCount++;
        return false;
    }
    return true;
}

bool CleanerCore::forceDeleteFolder(const fs::path& dirPath, bool dryRun, CleanStats& stats) {
    std::error_code ec;
    if (!fs::exists(dirPath, ec)) return true;

    // BẢO VỆ AN TOÀN: Tuyệt đối không xóa thư mục gốc hệ thống
    if (isCriticalPath(dirPath)) {
        std::cout << C_RED << " [CHẶN] Từ chối xóa đường dẫn nguy hiểm: " << dirPath.string() << C_RESET << "\n";
        stats.errorsCount++;
        return false;
    }

    if (dryRun) {
        // Dry-run cần duyệt metadata để đưa ra con số chính xác nhưng không đọc nội dung file.
        uintmax_t dirSz = 0;
        try {
            for (auto it = fs::recursive_directory_iterator(
                     dirPath, fs::directory_options::skip_permission_denied, ec);
                 it != fs::recursive_directory_iterator();) {
                if (ec) { ec.clear(); it.increment(ec); continue; }
                if (it->is_regular_file(ec)) {
                    uintmax_t sz = it->file_size(ec);
                    if (!ec) dirSz += sz;
                    else ec.clear();
                }
                it.increment(ec);
            }
        } catch (...) {
            stats.errorsCount++;
        }
        stats.bytesFreed += dirSz;
        stats.dirsDeleted++;
        return true;
    }

    // Chạy thật: không pre-scan toàn cây. Đo dung lượng trống trước/sau và xóa ngay,
    // nhờ đó cây thư mục thông thường chỉ bị duyệt một lần bởi remove_all().
    long long freeBefore = getAvailableDiskSpace(dirPath.root_path().string());
    fs::remove_all(dirPath, ec);
    if (!ec && !fs::exists(dirPath, ec)) {
        long long freeAfter = getAvailableDiskSpace(dirPath.root_path().string());
        if (freeAfter > freeBefore) stats.bytesFreed += freeAfter - freeBefore;
        stats.dirsDeleted++;
        return true;
    }

    // Fallback chỉ chạy khi lần xóa nhanh thất bại: gỡ thuộc tính read-only rồi thử lại.
    ec.clear();
    try {
        for (auto it = fs::recursive_directory_iterator(
                 dirPath, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator();) {
            if (ec) { ec.clear(); it.increment(ec); continue; }
            SetFileAttributesW(it->path().c_str(), FILE_ATTRIBUTE_NORMAL);
            it.increment(ec);
        }
    } catch (...) {}
    SetFileAttributesW(dirPath.c_str(), FILE_ATTRIBUTE_NORMAL);
    ec.clear();
    fs::remove_all(dirPath, ec);

    // Fallback: Re-check isCriticalPath trước khi gọi shell command rd /s /q
    if (fs::exists(dirPath, ec) && !isCriticalPath(dirPath)) {
        std::string cmd = "rd /s /q \"" + dirPath.string() + "\" >nul 2>&1";
        runCommand(cmd, true);
    }

    if (!fs::exists(dirPath, ec)) {
        long long freeAfter = getAvailableDiskSpace(dirPath.root_path().string());
        if (freeAfter > freeBefore) stats.bytesFreed += freeAfter - freeBefore;
        stats.dirsDeleted++;
        return true;
    }

    // File/thư mục cache đang được tiến trình khác giữ là tình huống bình thường.
    stats.itemsSkipped++;
    return false;
}

bool CleanerCore::safeDeleteFile(const fs::path& filePath, bool dryRun, CleanStats& stats) {
    std::error_code ec;
    if (!fs::exists(filePath, ec)) return true;
    uintmax_t sz = fs::file_size(filePath, ec);
    if (ec) { sz = 0; ec.clear(); }
    if (dryRun) {
        stats.bytesFreed += sz;
        stats.filesDeleted++;
        return true;
    }

    SetFileAttributesW(filePath.c_str(), FILE_ATTRIBUTE_NORMAL);
    if (fs::remove(filePath, ec)) {
        stats.bytesFreed += sz;
        stats.filesDeleted++;
        return true;
    }
    recordCleanupFailure(ec, stats);
    return false;
}

bool CleanerCore::moveToRecycleBin(const fs::path& filePath, bool dryRun, CleanStats& stats) {
    std::error_code ec;
    if (!fs::exists(filePath, ec)) return true;
    uintmax_t sz = fs::file_size(filePath, ec);
    if (ec) { sz = 0; ec.clear(); }

    if (dryRun) {
        stats.bytesRecycled += sz;
        stats.filesRecycled++;
        return true;
    }

    std::wstring pathStr = filePath.wstring();
    pathStr.push_back(L'\0'); // Double null-terminated cho SHFILEOPSTRUCTW

    SHFILEOPSTRUCTW fileOp{};
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = pathStr.c_str();
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

    int res = SHFileOperationW(&fileOp);
    if (res == 0 && !fileOp.fAnyOperationsAborted) {
        stats.bytesRecycled += sz;
        stats.filesRecycled++;
        return true;
    }
    if (res == ERROR_ACCESS_DENIED || res == ERROR_SHARING_VIOLATION ||
        res == ERROR_LOCK_VIOLATION) {
        stats.itemsSkipped++;
    } else {
        stats.errorsCount++;
    }
    return false;
}

bool CleanerCore::emptyRecycleBin(bool dryRun, CleanStats& stats) {
    SHQUERYRBINFO rbi{};
    rbi.cbSize = sizeof(rbi);
    if (SUCCEEDED(SHQueryRecycleBinW(NULL, &rbi))) {
        if (dryRun) {
            stats.bytesFreed += rbi.i64Size;
            stats.filesDeleted += static_cast<int>(rbi.i64NumItems);
            return true;
        }
        HRESULT hr = SHEmptyRecycleBinW(NULL, NULL, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
        if (SUCCEEDED(hr)) {
            stats.bytesFreed += rbi.i64Size;
            stats.filesDeleted += static_cast<int>(rbi.i64NumItems);
            return true;
        }
    }
    // Fallback qua powershell nếu API lỗi
    if (!dryRun && runCommand("powershell -NoProfile -Command \"Clear-RecycleBin -Force -ErrorAction Stop\"", true)) {
        return true;
    }
    stats.errorsCount++;
    return false;
}

bool CleanerCore::flushDns() {
    HMODULE dnsApi = LoadLibraryW(L"dnsapi.dll");
    if (dnsApi) {
        using FlushResolverCacheFn = BOOL (WINAPI*)();
        FARPROC rawFunction = GetProcAddress(dnsApi, "DnsFlushResolverCache");
        FlushResolverCacheFn flushResolverCache = nullptr;
        static_assert(sizeof(flushResolverCache) == sizeof(rawFunction));
        std::memcpy(&flushResolverCache, &rawFunction, sizeof(flushResolverCache));
        if (flushResolverCache && flushResolverCache()) {
            FreeLibrary(dnsApi);
            return true;
        }
        FreeLibrary(dnsApi);
    }
    return runCommand("ipconfig /flushdns >nul 2>&1", true);
}

bool CleanerCore::takeOwnershipAndGrantAdmin(const fs::path& targetPath) {
    // Whitelist bảo vệ an toàn: chỉ cho phép chiếm quyền trên các thư mục nâng cấp hệ thống đã biết
    std::string pathStr = targetPath.string();
    std::string sysDrive = getSystemDriveRoot();
    std::vector<std::string> allowedPrefixes = {
        sysDrive + "$WINDOWS.~BT",
        sysDrive + "$WINDOWS.~WS",
        sysDrive + "Windows.old"
    };
    bool allowed = false;
    std::string normalizedTarget = toLower(fs::path(pathStr).lexically_normal().string());
    for (const auto& prefix : allowedPrefixes) {
        if (normalizedTarget == toLower(fs::path(prefix).lexically_normal().string())) {
            allowed = true;
            break;
        }
    }
    if (!allowed) {
        std::cout << C_RED << " [CHẶN] takeOwnership từ chối thực thi trên đường dẫn không thuộc whitelist an toàn: " << pathStr << C_RESET << "\n";
        return false;
    }

    std::string cmdTake = "takeown /F \"" + pathStr + "\" /A /R /D Y >nul 2>&1";
    std::string cmdAcl  = "icacls \"" + pathStr + "\" /grant *S-1-5-32-544:F /T /C /Q >nul 2>&1";
    bool ok1 = runCommand(cmdTake, true);
    bool ok2 = runCommand(cmdAcl, true);
    return ok1 || ok2; // Ít nhất 1 lệnh thành công mới coi là thành công
}

bool CleanerCore::filesHaveSameContent(const fs::path& first, const fs::path& second) {
    std::error_code ec;
    if (fs::file_size(first, ec) != fs::file_size(second, ec) || ec) return false;
    std::ifstream a(first, std::ios::binary);
    std::ifstream b(second, std::ios::binary);
    if (!a || !b) return false;
    std::vector<char> aBuf(64 * 1024), bBuf(64 * 1024);
    while (a && b) {
        a.read(aBuf.data(), static_cast<std::streamsize>(aBuf.size()));
        b.read(bBuf.data(), static_cast<std::streamsize>(bBuf.size()));
        if (a.gcount() != b.gcount() ||
            !std::equal(aBuf.begin(), aBuf.begin() + a.gcount(), bBuf.begin())) return false;
    }
    return a.eof() && b.eof();
}
