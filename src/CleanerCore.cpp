#include "CleanerCore.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <iomanip>
#include <shellapi.h>
#include <shlobj.h>

const char* CleanerCore::C_RESET   = "\033[0m";
const char* CleanerCore::C_BOLD    = "\033[1m";
const char* CleanerCore::C_RED     = "\033[91m";
const char* CleanerCore::C_GREEN   = "\033[92m";
const char* CleanerCore::C_YELLOW  = "\033[93m";
const char* CleanerCore::C_BLUE    = "\033[94m";
const char* CleanerCore::C_CYAN    = "\033[96m";
const char* CleanerCore::C_MAGENTA = "\033[95m";
const char* CleanerCore::C_WHITE   = "\033[97m";

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

bool CleanerCore::confirm(const std::string& prompt, bool defaultYes) {
    std::cout << prompt;
    std::string ans;
    std::getline(std::cin, ans);
    ans = trim(ans);
    if (ans.empty()) return defaultYes;
    return (ans == "y" || ans == "Y" || ans == "yes" || ans == "YES");
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

    SHELLEXECUTEINFOA sei = { sizeof(sei) };
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
    STARTUPINFOA si = { sizeof(si) };
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

bool CleanerCore::runAdminCommand(const std::string& cmd, bool silent) {
    if (isElevated()) {
        return runCommand(cmd, true);
    }
    if (!silent) {
        std::cout << C_YELLOW << "[!] Tác vụ cần quyền Administrator: " << cmd << C_RESET << "\n";
    }
    return false;
}

uintmax_t CleanerCore::calculateDirectorySize(const fs::path& dirPath) {
    std::error_code ec;
    if (!fs::exists(dirPath, ec) || !fs::is_directory(dirPath, ec)) return 0;
    uintmax_t total = 0;
    try {
        for (auto it = fs::recursive_directory_iterator(dirPath, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator();) {
            if (ec) { ec.clear(); try { it++; } catch (...) { break; } continue; }
            if (it->is_regular_file(ec)) {
                total += it->file_size(ec);
            }
            it.increment(ec);
        }
    } catch (...) {}
    return total;
}

bool CleanerCore::wipeFolderContents(const fs::path& dirPath, bool dryRun, CleanStats& stats) {
    std::error_code ec;
    if (!fs::exists(dirPath, ec)) return true;

    try {
        for (const auto& entry : fs::directory_iterator(dirPath, fs::directory_options::skip_permission_denied, ec)) {
            if (ec) { ec.clear(); continue; }
            try {
                if (entry.is_regular_file(ec)) {
                    uintmax_t sz = entry.file_size(ec);
                    if (dryRun) {
                        stats.bytesFreed += sz;
                        stats.filesDeleted++;
                    } else {
                        SetFileAttributesW(entry.path().c_str(), FILE_ATTRIBUTE_NORMAL);
                        if (fs::remove(entry.path(), ec)) {
                            stats.bytesFreed += sz;
                            stats.filesDeleted++;
                        } else {
                            stats.errorsCount++;
                        }
                    }
                } else if (entry.is_directory(ec)) {
                    uintmax_t dirSz = calculateDirectorySize(entry.path());
                    if (dryRun) {
                        stats.bytesFreed += dirSz;
                        stats.dirsDeleted++;
                    } else {
                        if (forceDeleteFolder(entry.path(), false, stats)) {
                            // stats updated in forceDeleteFolder
                        }
                    }
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

    uintmax_t dirSz = calculateDirectorySize(dirPath);
    if (dryRun) {
        stats.bytesFreed += dirSz;
        stats.dirsDeleted++;
        return true;
    }

    try {
        for (auto it = fs::recursive_directory_iterator(dirPath, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator();) {
            if (ec) { ec.clear(); try { it++; } catch (...) { break; } continue; }
            SetFileAttributesW(it->path().c_str(), FILE_ATTRIBUTE_NORMAL);
            it.increment(ec);
        }
    } catch (...) {}

    SetFileAttributesW(dirPath.c_str(), FILE_ATTRIBUTE_NORMAL);
    fs::remove_all(dirPath, ec);
    if (!ec && !fs::exists(dirPath, ec)) {
        stats.bytesFreed += dirSz;
        stats.dirsDeleted++;
        return true;
    }

    // Fallback: lệnh cmd rd /s /q
    std::string cmd = "cmd.exe /d /c \"rd /s /q \"" + dirPath.string() + "\"\" >nul 2>&1";
    runCommand(cmd, true);

    if (!fs::exists(dirPath, ec)) {
        stats.bytesFreed += dirSz;
        stats.dirsDeleted++;
        return true;
    }

    stats.errorsCount++;
    return false;
}

bool CleanerCore::safeDeleteFile(const fs::path& filePath, bool dryRun, CleanStats& stats) {
    std::error_code ec;
    if (!fs::exists(filePath, ec)) return true;
    uintmax_t sz = fs::file_size(filePath, ec);
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
    stats.errorsCount++;
    return false;
}

bool CleanerCore::moveToRecycleBin(const fs::path& filePath, bool dryRun, CleanStats& stats) {
    std::error_code ec;
    if (!fs::exists(filePath, ec)) return true;
    uintmax_t sz = fs::file_size(filePath, ec);

    if (dryRun) {
        stats.bytesRecycled += sz;
        stats.filesRecycled++;
        return true;
    }

    std::wstring pathStr = filePath.wstring();
    pathStr.push_back(L'\0'); // Double null-terminated cho SHFILEOPSTRUCTW

    SHFILEOPSTRUCTW fileOp = {0};
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = pathStr.c_str();
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

    int res = SHFileOperationW(&fileOp);
    if (res == 0 && !fileOp.fAnyOperationsAborted) {
        stats.bytesRecycled += sz;
        stats.filesRecycled++;
        return true;
    }
    return false;
}

bool CleanerCore::emptyRecycleBin(bool dryRun, CleanStats& stats) {
    SHQUERYRBINFO rbi = { sizeof(rbi) };
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
    if (!dryRun) {
        runCommand("powershell -NoProfile -Command \"Clear-RecycleBin -Force -ErrorAction SilentlyContinue\"", true);
    }
    return true;
}

bool CleanerCore::flushDns() {
    HMODULE hDns = LoadLibraryA("dnsapi.dll");
    if (hDns) {
        typedef BOOL (WINAPI *DnsFlushResolverCacheFn)();
        auto pfn = (DnsFlushResolverCacheFn)GetProcAddress(hDns, "DnsFlushResolverCache");
        if (pfn && pfn()) {
            FreeLibrary(hDns);
            return true;
        }
        FreeLibrary(hDns);
    }
    return runCommand("ipconfig /flushdns >nul 2>&1", true);
}

bool CleanerCore::takeOwnershipAndGrantAdmin(const fs::path& targetPath) {
    std::string pathStr = targetPath.string();
    std::string cmdTake = "takeown /F \"" + pathStr + "\" /A /R /D Y >nul 2>&1";
    std::string cmdAcl  = "icacls \"" + pathStr + "\" /grant *S-1-5-32-544:F /T /C /Q >nul 2>&1";
    runCommand(cmdTake, true);
    runCommand(cmdAcl, true);
    return true;
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
