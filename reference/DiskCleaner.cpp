#include "DiskCleaner.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <filesystem>
#include <vector>
#include <string>
#include <regex>
#include <iomanip>
#include <thread>
#include <chrono>
#include <unordered_set>
#include <map>

using namespace std;
namespace fs = std::filesystem;

DiskCleaner::DiskCleaner(SystemCore &core) : sc(core) {}

// ----------------------------------------------------------------------------------
// CÁC HÀM TIỆN ÍCH XÓA DỮ LIỆU CẤP THẤP
// ----------------------------------------------------------------------------------

void DiskCleaner::wipeFolderContents(const fs::path &dirPath) {
    std::error_code ec;
    if (!fs::exists(dirPath, ec)) return;
    try {
        for (const auto &entry : fs::directory_iterator(dirPath, fs::directory_options::skip_permission_denied, ec)) {
            try {
                SetFileAttributesA(entry.path().string().c_str(), FILE_ATTRIBUTE_NORMAL);
                fs::remove_all(entry.path(), ec);
            } catch (...) {}
        }
    } catch (...) {}
}

bool DiskCleaner::forceDeleteFolder(const fs::path &path) {
    std::error_code ec;
    if (!fs::exists(path, ec)) return true;

    // Gỡ thuộc tính Read-Only toàn bộ cây thư mục
    try {
        for (auto it = fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator();) {
            if (ec) { ec.clear(); try { it++; } catch (...) { break; } continue; }
            try {
                SetFileAttributesA(it->path().string().c_str(), FILE_ATTRIBUTE_NORMAL);
            } catch (...) {}
            it.increment(ec);
        }
    } catch (...) {}

    SetFileAttributesA(path.string().c_str(), FILE_ATTRIBUTE_NORMAL);
    fs::remove_all(path, ec);
    if (!ec && !fs::exists(path, ec)) return true;

    // Fallback lệnh rd /s /q
    string cmd = "cmd.exe /d /c \"rd /s /q \"" + path.string() + "\"\" >nul 2>&1";
    system(cmd.c_str());
    return !fs::exists(path, ec);
}

string DiskCleaner::getSystemDriveRoot() {
    char sysDrive[MAX_PATH] = {0};
    if (GetEnvironmentVariableA("SystemDrive", sysDrive, sizeof(sysDrive)) > 0) {
        string drive = sysDrive;
        if (!drive.empty() && drive.back() != '\\') {
            drive += "\\";
        }
        return drive;
    }
    char winDir[MAX_PATH] = {0};
    if (GetWindowsDirectoryA(winDir, sizeof(winDir)) > 0) {
        if (strlen(winDir) >= 2 && winDir[1] == ':') {
            return string(winDir, 2) + "\\";
        }
    }
    return "C:\\";
}

bool DiskCleaner::moveToRecycleBin(const fs::path &filePath) {
    std::string pathStr = filePath.string();
    pathStr.push_back('\0'); // double null-terminated for SHFILEOPSTRUCT

    SHFILEOPSTRUCTA fileOp = {0};
    fileOp.wFunc = FO_DELETE;
    fileOp.pFrom = pathStr.c_str();
    fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

    int res = SHFileOperationA(&fileOp);
    if (res == 0 && !fileOp.fAnyOperationsAborted) {
        return true;
    }
    // Không xóa vĩnh viễn nếu Recycle Bin không khả dụng.
    return false;
}

static bool filesHaveSameContent(const fs::path& first, const fs::path& second) {
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

int DiskCleaner::cleanDirectoryArtifacts(const fs::path &rootPath, 
                                        const vector<string> &targetDirNames, 
                                        const vector<string> &targetExtensions, 
                                        long long &freedBytes) {
    std::error_code ec;
    if (!fs::exists(rootPath, ec) || !fs::is_directory(rootPath, ec)) return 0;

    int deletedCount = 0;
    vector<fs::path> dirsToDelete;
    vector<fs::path> filesToDelete;

    try {
        for (auto it = fs::recursive_directory_iterator(rootPath, 
                 fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator();) {
            if (ec) { ec.clear(); try { it++; } catch (...) { break; } continue; }

            try {
                const auto &entry = *it;
                string filename = entry.path().filename().string();

                // TUYỆT ĐỐI BẢO VỆ .git, .github, .gitignore
                if (_stricmp(filename.c_str(), ".git") == 0 || 
                    _stricmp(filename.c_str(), ".github") == 0 ||
                    _stricmp(filename.c_str(), ".gitignore") == 0 ||
                    _stricmp(filename.c_str(), ".gitattributes") == 0) {
                    if (entry.is_directory(ec)) {
                        it.disable_recursion_pending();
                    }
                    it.increment(ec);
                    continue;
                }

                if (entry.is_directory(ec)) {
                    bool matchDir = false;
                    for (const auto &td : targetDirNames) {
                        if (_stricmp(filename.c_str(), td.c_str()) == 0) {
                            matchDir = true;
                            break;
                        }
                    }
                    if (matchDir) {
                        dirsToDelete.push_back(entry.path());
                        it.disable_recursion_pending();
                    }
                } else if (entry.is_regular_file(ec)) {
                    string ext = entry.path().extension().string();
                    for (const auto &te : targetExtensions) {
                        if (_stricmp(ext.c_str(), te.c_str()) == 0) {
                            filesToDelete.push_back(entry.path());
                            break;
                        }
                    }
                }
            } catch (...) {}
            it.increment(ec);
        }
    } catch (...) {}

    for (const auto &f : filesToDelete) {
        try {
            uintmax_t sz = fs::file_size(f, ec);
            SetFileAttributesA(f.string().c_str(), FILE_ATTRIBUTE_NORMAL);
            if (fs::remove(f, ec)) {
                freedBytes += sz;
                deletedCount++;
            }
        } catch (...) {}
    }

    for (const auto &d : dirsToDelete) {
        try {
            uintmax_t dirSize = 0;
            try {
                for (const auto &sub : fs::recursive_directory_iterator(d, fs::directory_options::skip_permission_denied, ec)) {
                    if (!ec && sub.is_regular_file(ec)) dirSize += sub.file_size(ec);
                }
            } catch (...) {}

            if (forceDeleteFolder(d)) {
                freedBytes += dirSize;
                deletedCount++;
            }
        } catch (...) {}
    }

    return deletedCount;
}

// ----------------------------------------------------------------------------------
// HELPERS THÔNG MINH CHO DỌN DOWNLOADS (EXE & DUPLICATES)
// ----------------------------------------------------------------------------------

string DiskCleaner::getDownloadsPath() {
    PWSTR path = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &path))) {
        char buf[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, path, -1, buf, MAX_PATH, NULL, NULL);
        CoTaskMemFree(path);
        return string(buf);
    }
    const char *userProf = getenv("USERPROFILE");
    if (userProf) return string(userProf) + "\\Downloads";
    return "";
}

// Chuẩn hóa tên ứng dụng thành các từ khóa có nghĩa
string DiskCleaner::cleanAppName(const string &raw) {
    string s = raw;
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    
    // Gỡ bỏ kiến trúc và hậu tố installer phổ biến
    static const vector<string> junkPatterns = {
        "-x64", "_x64", " x64", ".x64",
        "-x86", "_x86", " x86", ".x86",
        "-win64", "_win64", " win64",
        "-win32", "_win32", " win32",
        "-amd64", "_amd64", " amd64",
        "-arm64", "_arm64", " arm64",
        "64-bit", "32-bit", "64bit", "32bit",
        "-setup", "_setup", " setup",
        "-installer", "_installer", " installer",
        "-install", "_install", " install",
        "standalone", "portable", "full"
    };

    for (const auto &jp : junkPatterns) {
        size_t pos = 0;
        while ((pos = s.find(jp, pos)) != string::npos) {
            s.replace(pos, jp.length(), " ");
            pos += 1;
        }
    }

    // Giữ lại chữ và số, biến ký tự đặc biệt thành dấu cách
    string res = "";
    for (char c : s) {
        if (isalnum((unsigned char)c)) res += c;
        else if (res.empty() || res.back() != ' ') res += ' ';
    }
    return SystemCore::trim(res);
}

// Quét toàn bộ DisplayName của các phần mềm đã cài đặt trên hệ thống qua Registry
unordered_set<string> DiskCleaner::getInstalledAppNames() {
    unordered_set<string> installed;

    auto scanRegistryKey = [&](HKEY root, const char *subKey) {
        HKEY hKey;
        if (RegOpenKeyExA(root, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char keyName[256];
            DWORD keyIndex = 0;
            DWORD keyNameSize = sizeof(keyName);

            while (RegEnumKeyExA(hKey, keyIndex++, keyName, &keyNameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                HKEY hSubKey;
                if (RegOpenKeyExA(hKey, keyName, 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
                    char displayName[512] = {0};
                    DWORD dataSize = sizeof(displayName);
                    if (RegQueryValueExA(hSubKey, "DisplayName", NULL, NULL, (LPBYTE)displayName, &dataSize) == ERROR_SUCCESS) {
                        string name = string(displayName);
                        transform(name.begin(), name.end(), name.begin(), ::tolower);
                        name = SystemCore::trim(name);
                        if (!name.empty()) {
                            installed.insert(name);
                            string cleaned = cleanAppName(name);
                            if (!cleaned.empty()) installed.insert(cleaned);
                        }
                    }
                    RegCloseKey(hSubKey);
                }
                keyNameSize = sizeof(keyName);
            }
            RegCloseKey(hKey);
        }
    };

    // 1. Quét 64-bit & 32-bit System Uninstall
    scanRegistryKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
    scanRegistryKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall");

    // 2. Quét User-level Uninstall
    scanRegistryKey(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall");

    return installed;
}

// Lấy ProductName và FileDescription từ PE Header qua version.dll (LoadLibrary runtime)
string DiskCleaner::getExeProductName(const string &exePath) {
    HMODULE hVer = LoadLibraryA("version.dll");
    if (!hVer) return "";

    typedef DWORD (WINAPI *pfnGetFileVersionInfoSizeA)(LPCSTR, LPDWORD);
    typedef BOOL (WINAPI *pfnGetFileVersionInfoA)(LPCSTR, DWORD, DWORD, LPVOID);
    typedef BOOL (WINAPI *pfnVerQueryValueA)(LPCVOID, LPCSTR, LPVOID*, PUINT);

    auto pGetSize = (pfnGetFileVersionInfoSizeA)GetProcAddress(hVer, "GetFileVersionInfoSizeA");
    auto pGetInfo = (pfnGetFileVersionInfoA)GetProcAddress(hVer, "GetFileVersionInfoA");
    auto pQueryVal = (pfnVerQueryValueA)GetProcAddress(hVer, "VerQueryValueA");

    if (!pGetSize || !pGetInfo || !pQueryVal) {
        FreeLibrary(hVer);
        return "";
    }

    DWORD dummy = 0;
    DWORD size = pGetSize(exePath.c_str(), &dummy);
    if (size == 0) {
        FreeLibrary(hVer);
        return "";
    }

    vector<BYTE> data(size);
    if (!pGetInfo(exePath.c_str(), 0, size, data.data())) {
        FreeLibrary(hVer);
        return "";
    }

    struct LANGANDCODEPAGE {
        WORD wLanguage;
        WORD wCodePage;
    } *lpTranslate = NULL;
    UINT cbTranslate = 0;

    string productName = "";
    string fileDescription = "";

    auto queryField = [&](const char *blockFormat, WORD lang, WORD cp, const char *field) -> string {
        char subBlock[128];
        sprintf_s(subBlock, sizeof(subBlock), blockFormat, lang, cp, field);
        LPVOID lpBuffer = NULL;
        UINT sizeStr = 0;
        if (pQueryVal(data.data(), subBlock, &lpBuffer, &sizeStr) && lpBuffer && sizeStr > 0) {
            return string((char*)lpBuffer);
        }
        return "";
    };

    if (pQueryVal(data.data(), "\\VarFileInfo\\Translation", (LPVOID*)&lpTranslate, &cbTranslate) && 
        cbTranslate >= sizeof(struct LANGANDCODEPAGE)) {
        WORD lang = lpTranslate[0].wLanguage;
        WORD cp = lpTranslate[0].wCodePage;
        productName = queryField("\\StringFileInfo\\%04x%04x\\%s", lang, cp, "ProductName");
        fileDescription = queryField("\\StringFileInfo\\%04x%04x\\%s", lang, cp, "FileDescription");
    }

    // Fallbacks
    if (productName.empty()) {
        static const struct { WORD lang; WORD cp; } fbList[] = {
            {0x0409, 0x04b0}, {0x0409, 0x04e4}, {0x0000, 0x04b0}, {0x0400, 0x04b0}
        };
        for (const auto &fb : fbList) {
            if (productName.empty()) productName = queryField("\\StringFileInfo\\%04x%04x\\%s", fb.lang, fb.cp, "ProductName");
            if (fileDescription.empty()) fileDescription = queryField("\\StringFileInfo\\%04x%04x\\%s", fb.lang, fb.cp, "FileDescription");
            if (!productName.empty()) break;
        }
    }

    FreeLibrary(hVer);

    // Lọc bỏ tên các bộ đóng gói installer chung chung
    auto isGenericEngine = [](const string &val) -> bool {
        string lower = val;
        transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        static const vector<string> genericEngines = {
            "inno setup", "nullsoft", "nsis", "installshield", "advanced installer",
            "wise installation", "wix toolset", "7-zip self-extracting", "winrar sfx",
            "bootstrap", "setup application", "installer"
        };
        for (const auto &ge : genericEngines) {
            if (lower.find(ge) != string::npos) return true;
        }
        return false;
    };

    if (!productName.empty() && !isGenericEngine(productName)) {
        return SystemCore::trim(productName);
    }
    if (!fileDescription.empty() && !isGenericEngine(fileDescription)) {
        return SystemCore::trim(fileDescription);
    }

    return "";
}

// ----------------------------------------------------------------------------------
// CÁC NHIỆM VỤ DỌN RÁC CHÍNH
// ----------------------------------------------------------------------------------

// 1. Dọn rác tạm bề mặt & Cache người dùng
long long DiskCleaner::cleanSurfaceAndUserTemp() {
    string sysDrive = getSystemDriveRoot();
    long long before = 0, after = 0;
    try { before = fs::space(sysDrive).available; } catch (...) {}

    vector<thread> threads;
    threads.emplace_back([this]() { sc.runCMD("del /s /f /q \"%temp%\\*\" 2>nul & for /d %p in (\"%temp%\\*\") do rmdir /s /q \"%p\" 2>nul"); });
    if (SystemCore::isElevated()) {
        threads.emplace_back([this]() { sc.runCMD("del /f /s /q \"%systemroot%\\temp\\*\" 2>nul & for /d %p in (\"%systemroot%\\temp\\*\") do rmdir /s /q \"%p\" 2>nul"); });
    }
    threads.emplace_back([this]() { sc.runCMD("del /f /s /q \"%AppData%\\Microsoft\\Windows\\Recent\\*\" 2>nul"); });
    threads.emplace_back([this]() { sc.runCMD("del /f /s /q \"%LocalAppData%\\D3DSCache\\*\" 2>nul"); });
    threads.emplace_back([this]() { sc.runCMD("del /f /s /q \"%LocalAppData%\\Low\\Microsoft\\CryptnetUrlCache\\*\" 2>nul"); });
    threads.emplace_back([this]() { sc.runCMD("del /f /s /q \"%LocalAppData%\\CrashDumps\\*\" 2>nul"); });
    threads.emplace_back([this]() { sc.runCMD("del /f /s /q \"%LocalAppData%\\Microsoft\\Windows\\WER\\Temp\\*\" 2>nul"); });
    threads.emplace_back([this]() { sc.runCMD("del /f /s /q \"%LocalAppData%\\Microsoft\\Windows\\WER\\ReportArchive\\*\" 2>nul"); });
    threads.emplace_back([this]() { sc.runCMD("del /f /s /q \"%LocalAppData%\\Microsoft\\Windows\\WER\\ReportQueue\\*\" 2>nul"); });
    if (SystemCore::isElevated()) {
        threads.emplace_back([this]() { sc.runCMD("del /f /s /q \"%ProgramData%\\Microsoft\\Windows\\WER\\Temp\\*\" 2>nul"); });
    }
    threads.emplace_back([this]() { sc.runCMD("del /f /s /q \"%LocalAppData%\\Microsoft\\Windows\\INetCache\\*\" 2>nul"); });
    threads.emplace_back([this]() { sc.runCMD("powershell -NoProfile -Command \"Clear-RecycleBin -Force -ErrorAction SilentlyContinue\""); });
    threads.emplace_back([this]() { sc.runCMD("ipconfig /flushdns >nul 2>&1"); });

    for (auto &t : threads) t.join();

    try { after = fs::space(sysDrive).available; } catch (...) {}
    return (after > before) ? (after - before) : 0;
}

// 2. Dọn rác Trình duyệt & Ứng dụng
long long DiskCleaner::cleanBrowserAndAppCache() {
    string sysDrive = getSystemDriveRoot();
    long long before = 0, after = 0;
    try { before = fs::space(sysDrive).available; } catch (...) {}

    clearBrowserCache();

    vector<thread> threads;
    threads.emplace_back([this]() { sc.runCMD("del /f /s /q \"%LocalAppData%\\NVIDIA\\GLCache\\*\" 2>nul"); });
    threads.emplace_back([this]() { sc.runCMD("del /f /s /q \"%LocalAppData%\\Microsoft\\Windows\\Explorer\\thumbcache_*.db\" 2>nul"); });
    threads.emplace_back([this]() { sc.runCMD("del /f /s /q \"%AppData%\\discord\\Cache\\*\" 2>nul"); });
    threads.emplace_back([this]() { sc.runCMD("del /f /s /q \"%AppData%\\discord\\Code Cache\\*\" 2>nul"); });
    threads.emplace_back([this]() { sc.runCMD("del /f /s /q \"%AppData%\\Telegram Desktop\\tdata\\user_data\\cache\\*\" 2>nul"); });

    for (auto &t : threads) t.join();

    try { after = fs::space(sysDrive).available; } catch (...) {}
    return (after > before) ? (after - before) : 0;
}

// 3. Dọn dẹp Chuyên sâu & Tồn dư Cập nhật (DISM, WinSxS, Windows.old, Event Logs)
long long DiskCleaner::cleanDeepSystemAndUpdates() {
    string sysDrive = getSystemDriveRoot();
    long long before = 0, after = 0;
    try { before = fs::space(sysDrive).available; } catch (...) {}

    string batContent = "@echo off\nchcp 65001 >nul\n";
    batContent += "net stop wuauserv 2>nul\n";
    batContent += "net stop bits 2>nul\n";
    batContent += "del /f /s /q \"%systemroot%\\SoftwareDistribution\\Download\\*\" 2>nul\n";
    batContent += "net start bits 2>nul\n";
    batContent += "net start wuauserv 2>nul\n";

    batContent += "mkdir \"%SystemDrive%\\EmptyFolderTmp\" 2>nul\n";
    batContent += "robocopy \"%SystemDrive%\\EmptyFolderTmp\" \"%systemroot%\\temp\" /mir /w:0 /r:0 /log:nul >nul 2>&1\n";
    batContent += "robocopy \"%SystemDrive%\\EmptyFolderTmp\" \"%systemroot%\\Prefetch\" /mir /w:0 /r:0 /log:nul >nul 2>&1\n";

    // Tồn dư cập nhật bản lớn (Dùng SID *S-1-5-32-544:F để tương thích 100% mọi ngôn ngữ Windows)
    batContent += "if exist \"%SystemDrive%\\$WINDOWS.~BT\" (\n";
    batContent += "    takeown /F \"%SystemDrive%\\$WINDOWS.~BT\" /A /R /D Y >nul 2>&1\n";
    batContent += "    icacls \"%SystemDrive%\\$WINDOWS.~BT\" /grant *S-1-5-32-544:F /T /C /Q >nul 2>&1\n";
    batContent += "    rd /s /q \"%SystemDrive%\\$WINDOWS.~BT\" 2>nul\n";
    batContent += ")\n";

    batContent += "if exist \"%SystemDrive%\\$WINDOWS.~WS\" (\n";
    batContent += "    takeown /F \"%SystemDrive%\\$WINDOWS.~WS\" /A /R /D Y >nul 2>&1\n";
    batContent += "    icacls \"%SystemDrive%\\$WINDOWS.~WS\" /grant *S-1-5-32-544:F /T /C /Q >nul 2>&1\n";
    batContent += "    rd /s /q \"%SystemDrive%\\$WINDOWS.~WS\" 2>nul\n";
    batContent += ")\n";

    batContent += "if exist \"%SystemDrive%\\Windows.old\" (\n";
    batContent += "    takeown /F \"%SystemDrive%\\Windows.old\" /A /R /D Y >nul 2>&1\n";
    batContent += "    icacls \"%SystemDrive%\\Windows.old\" /grant *S-1-5-32-544:F /T /C /Q >nul 2>&1\n";
    batContent += "    rd /s /q \"%SystemDrive%\\Windows.old\" 2>nul\n";
    batContent += ")\n";

    batContent += "del /f /s /q \"%SystemRoot%\\Panther\\*.*\" 2>nul\n";
    batContent += "del /f /s /q \"%SystemRoot%\\LiveKernelReports\\*.*\" 2>nul\n";
    batContent += "del /f /s /q \"%SystemRoot%\\Minidump\\*.*\" 2>nul\n";
    batContent += "del /f /q \"%SystemRoot%\\MEMORY.DMP\" 2>nul\n";
    batContent += "del /f /s /q \"%SystemRoot%\\Logs\\CBS\\*.*\" 2>nul\n";
    batContent += "del /f /s /q \"%SystemRoot%\\Logs\\DISM\\*.*\" 2>nul\n";
    batContent += "del /f /q \"%SystemRoot%\\WindowsUpdate.log\" 2>nul\n";
    batContent += "del /f /s /q \"%ProgramData%\\Microsoft\\Windows\\WER\\ReportQueue\\*\" 2>nul\n";
    batContent += "del /f /s /q \"%ProgramData%\\Microsoft\\Windows\\WER\\ReportArchive\\*\" 2>nul\n";

    // Chuẩn dọn dẹp DISM WinSxS & Delivery Optimization
    batContent += "dism /online /cleanup-image /startcomponentcleanup /resetbase\n";
    batContent += "powershell -NoProfile -Command \"Get-DeliveryOptimizationStatus | Remove-DeliveryOptimizationCache -Confirm:$false\" 2>nul\n";
    batContent += "for /f \"tokens=*\" %%a in ('wevtutil el 2^>nul') do wevtutil cl \"%%a\" 2>nul\n";
    batContent += "powercfg -h off\n";
    // Thiết lập StateFlags0001 để cleanmgr /sagerun:1 dọn dẹp tất cả các mục VolumeCaches
    batContent += "for /f \"tokens=*\" %%k in ('reg query \"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VolumeCaches\" 2^>nul') do (\n";
    batContent += "    reg add \"%%k\" /v StateFlags0001 /t REG_DWORD /d 2 /f >nul 2>&1\n";
    batContent += ")\n";
    batContent += "cleanmgr /sagerun:1\n";
    batContent += "rmdir \"%SystemDrive%\\EmptyFolderTmp\" 2>nul\n";

    bool batchOk = SystemCore::runBatchAsAdmin(batContent, "Dọn dẹp hệ thống chuyên sâu & Tồn dư cập nhật");

    try { after = fs::space(sysDrive).available; } catch (...) {}
    if (!batchOk) return -1;
    return (after > before) ? (after - before) : 0;
}

// 4. Dọn rác Môi trường lập trình (Dev Artifacts & Caches)
long long DiskCleaner::cleanDevArtifactsAndCaches() {
    auto getTotalDrivesFreeSpace = []() -> long long {
        long long total = 0;
        DWORD mask = GetLogicalDrives();
        for (char c = 'C'; c <= 'Z'; ++c) {
            if (mask & (1 << (c - 'A'))) {
                string r = string(1, c) + ":\\";
                if (GetDriveTypeA(r.c_str()) == DRIVE_FIXED) {
                    try { total += fs::space(r).available; } catch (...) {}
                }
            }
        }
        return total;
    };

    long long before = getTotalDrivesFreeSpace();
    cleanDevCaches(false);
    long long after = getTotalDrivesFreeSpace();

    return (after > before) ? (after - before) : 0;
}

// 5. Dọn file cài đặt Exe & Rác tải về trong Downloads (KHÔNG RECYCLE BIN, CHỈ EXE/MSI, KIỂM TRA REGISTRY CHÍNH XÁC)
long long DiskCleaner::cleanDownloadsExesAndDuplicates() {
    string dlPath = getDownloadsPath();
    if (dlPath.empty() || !fs::exists(dlPath)) {
        cout << "     └── [!] Không tìm thấy thư mục Downloads!\n";
        return 0;
    }

    std::error_code ec;
    long long freedBytes = 0;
    int deletedExeCount = 0;
    int deletedDuplicateCount = 0;
    int deletedCorruptCount = 0;

    cout << "     ├── Đang quét ứng dụng đã cài...\n";
    unordered_set<string> installed = getInstalledAppNames();

    // A. DỌN FILE TẢI DỞ DANG (.crdownload, .part, .tmp cũ hơn 24 giờ qua Win32 API chính xác)
    ULARGE_INTEGER nowTime;
    GetSystemTimeAsFileTime((LPFILETIME)&nowTime);

    for (const auto &entry : fs::directory_iterator(dlPath, fs::directory_options::skip_permission_denied, ec)) {
        if (!entry.is_regular_file(ec)) continue;

        string ext = entry.path().extension().string();
        transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".crdownload" || ext == ".part" || ext == ".tmp") {
            WIN32_FILE_ATTRIBUTE_DATA fad;
            if (GetFileAttributesExA(entry.path().string().c_str(), GetFileExInfoStandard, &fad)) {
                ULARGE_INTEGER fileTime;
                fileTime.LowPart = fad.ftLastWriteTime.dwLowDateTime;
                fileTime.HighPart = fad.ftLastWriteTime.dwHighDateTime;
                if (nowTime.QuadPart > fileTime.QuadPart) {
                    ULONGLONG diffSeconds = (nowTime.QuadPart - fileTime.QuadPart) / 10000000ULL;
                    if (diffSeconds >= 24 * 3600) { // Cũ hơn 24 giờ
                        uintmax_t sz = entry.file_size(ec);
                        SetFileAttributesA(entry.path().string().c_str(), FILE_ATTRIBUTE_NORMAL);
                        if (fs::remove(entry.path(), ec)) {
                            freedBytes += sz;
                            deletedCorruptCount++;
                        }
                    }
                }
            }
        }
    }

    // B. CẤU TRÚC PHÂN TÍCH FILE CÀI ĐẶT (.EXE / .MSI)
    struct ExeItem {
        fs::path fullPath;
        string filename;
        string ext;
        string baseStem;
        int copyIndex;       // 0: bản gốc (setup.exe), >=1: bản trùng lặp (setup (1).exe)
        uintmax_t size;
        bool isInstalled;
    };

    // Regex phát hiện bản sao Windows: "app (1).exe", "app(2).msi"
    regex dupRegex(R"(^(.+?)\s*\(([0-9]+)\)\.(exe|msi)$)", regex::icase);

    // Thu thập và nhóm các file cài đặt theo tên gốc
    map<string, vector<ExeItem>> groups;

    for (const auto &entry : fs::directory_iterator(dlPath, fs::directory_options::skip_permission_denied, ec)) {
        if (!entry.is_regular_file(ec)) continue;

        string ext = entry.path().extension().string();
        transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".exe" && ext != ".msi") continue;

        string filename = entry.path().filename().string();
        string stem = entry.path().stem().string();
        uintmax_t sz = entry.file_size(ec);

        string baseStem = stem;
        int copyIndex = 0;
        smatch match;
        if (regex_match(filename, match, dupRegex)) {
            baseStem = match[1].str();
            try { copyIndex = stoi(match[2].str()); } catch (...) { copyIndex = 1; }
        }

        // Khóa định danh nhóm: chữ thường của baseStem + extension để gom các file trùng lặp (1), (2)
        string lowerBaseStem = baseStem;
        transform(lowerBaseStem.begin(), lowerBaseStem.end(), lowerBaseStem.begin(), ::tolower);
        string groupKey = lowerBaseStem + ext;
        if (groupKey.empty()) groupKey = filename;

        groups[groupKey].push_back({entry.path(), filename, ext, baseStem, copyIndex, sz, false});
    }

    // C. KIỂM TRA CÀI ĐẶT & XỬ LÝ TRÙNG LẶP CHO TỪNG NHÓM
    for (auto &pair : groups) {
        auto &fileList = pair.second;
        if (fileList.empty()) continue;

        // Trích xuất ứng viên tên phần mềm đại diện cho nhóm
        string prodName = "";
        for (const auto &item : fileList) {
            if (item.ext == ".exe") {
                string pName = getExeProductName(item.fullPath.string());
                if (!pName.empty()) {
                    prodName = pName;
                    break;
                }
            }
        }

        string cleanedProd = cleanAppName(prodName);
        string cleanedBase = cleanAppName(fileList[0].baseStem);

        // Danh sách từ khóa cấm so khớp (tránh false-positive)
        static const unordered_set<string> genericBlacklist = {
            "setup", "installer", "install", "update", "updater", "app", "application",
            "win", "windows", "tool", "tools", "patch", "package", "download", "temp",
            "x64", "x86", "64bit", "32bit", "full", "free", "beta", "portable"
        };

        bool groupInstalled = false;

        auto matchCandidate = [&](const string &candidate) -> bool {
            if (candidate.empty() || candidate.length() < 3) return false;
            if (genericBlacklist.find(candidate) != genericBlacklist.end()) return false;

            // 1. So khớp với DisplayName trong Registry
            for (const auto &inst : installed) {
                if (inst == candidate) return true;

                if (candidate.length() < 5) continue;

                size_t pos = inst.find(candidate);
                if (pos != string::npos) {
                    bool leftOk = (pos == 0 || !isalnum((unsigned char)inst[pos - 1]));
                    bool rightOk = (pos + candidate.length() == inst.length() || !isalnum((unsigned char)inst[pos + candidate.length()]));
                    if (leftOk && rightOk) {
                        if (candidate.length() * 10 >= inst.length() * 4) {
                            return true;
                        }
                    }
                }
            }
            return false;
        };

        if (matchCandidate(cleanedProd) || matchCandidate(cleanedBase)) {
            groupInstalled = true;
        }

        // Tuyệt đối không tự động xóa các file portable hoặc standalone
        string lowerFile = fileList[0].filename;
        transform(lowerFile.begin(), lowerFile.end(), lowerFile.begin(), ::tolower);
        if (lowerFile.find("portable") != string::npos || lowerFile.find("standalone") != string::npos) {
            groupInstalled = false;
        }

        // ÁP DỤNG QUY TẮC XỬ LÝ AN TOÀN (Chuyển vào Recycle Bin thay vì xóa cứng vĩnh viễn)
        if (groupInstalled) {
            // Phần mềm ĐÃ CÀI ĐẶT trên hệ thống -> Chuyển toàn bộ file cài đặt vào Thùng rác
            for (const auto &item : fileList) {
                SetFileAttributesA(item.fullPath.string().c_str(), FILE_ATTRIBUTE_NORMAL);
                if (moveToRecycleBin(item.fullPath)) {
                    deletedExeCount++;
                }
            }
        } else {
            // Phần mềm CHƯA CÀI ĐẶT -> Giữ lại bản gốc (copyIndex 0), chỉ chuyển các bản sao trùng lặp (1), (2)... vào Thùng rác
            if (fileList.size() > 1) {
                sort(fileList.begin(), fileList.end(), [](const ExeItem &a, const ExeItem &b) {
                    return a.copyIndex < b.copyIndex;
                });

                for (size_t i = 1; i < fileList.size(); ++i) {
                    if (fileList[i].copyIndex > 0 &&
                        filesHaveSameContent(fileList[0].fullPath, fileList[i].fullPath)) {
                        SetFileAttributesA(fileList[i].fullPath.string().c_str(), FILE_ATTRIBUTE_NORMAL);
                        if (moveToRecycleBin(fileList[i].fullPath)) {
                            deletedDuplicateCount++;
                        }
                    }
                }
            }
        }
    }

    cout << "     ├── [✓] Đã chuyển vào Thùng rác " << deletedExeCount << " file cài đặt (.exe/.msi) của ứng dụng đã cài đặt trên máy\n";
    cout << "     ├── [✓] Đã dọn vào Thùng rác " << deletedDuplicateCount << " file cài đặt tải trùng lặp (1), (2)\n";
    if (deletedCorruptCount > 0) {
        cout << "     ├── [✓] Đã dọn " << deletedCorruptCount << " file tải dở dang kẹt lại (.crdownload/.part)\n";
    }

    return freedBytes;
}

// ----------------------------------------------------------------------------------
// DỌN DẸP CACHE DEV CHI TIẾT
// ----------------------------------------------------------------------------------

void DiskCleaner::cleanDevCaches(bool interactive) {
    char *localAppData = std::getenv("LOCALAPPDATA");
    char *appData = std::getenv("APPDATA");
    char *userProfile = std::getenv("USERPROFILE");

    string baseLocal = localAppData ? string(localAppData) : "";
    string baseApp   = appData ? string(appData) : "";
    string baseUser  = userProfile ? string(userProfile) : "";

    std::vector<fs::path> scanRoots;
    fs::path currentRoot = fs::current_path();
    if (currentRoot.filename() == "bin" && fs::exists(currentRoot.parent_path())) {
        currentRoot = currentRoot.parent_path();
    }
    scanRoots.push_back(currentRoot);

    DWORD driveMask = GetLogicalDrives();
    for (char c = 'C'; c <= 'Z'; ++c) {
        if (driveMask & (1 << (c - 'A'))) {
            string rootDrive = string(1, c) + ":\\";
            if (GetDriveTypeA(rootDrive.c_str()) == DRIVE_FIXED) {
                static const vector<string> commonDevFolders = {
                    "Code", "Projects", "Source", "Repos", "Dev", "Web", "Workspace"
                };
                for (const auto &df : commonDevFolders) {
                    fs::path devPath = rootDrive + df;
                    std::error_code ec;
                    if (fs::exists(devPath, ec) && fs::is_directory(devPath, ec)) {
                        bool duplicate = false;
                        for (const auto &sr : scanRoots) {
                            if (fs::equivalent(sr, devPath, ec)) { duplicate = true; break; }
                        }
                        if (!duplicate) scanRoots.push_back(devPath);
                    }
                }
            }
        }
    }

    if (!baseUser.empty()) {
        static const vector<string> userDevFolders = {
            "Desktop", "Documents", "Projects", "source\\repos"
        };
        for (const auto &uf : userDevFolders) {
            fs::path uPath = fs::path(baseUser) / uf;
            std::error_code ec;
            if (fs::exists(uPath, ec) && fs::is_directory(uPath, ec)) {
                bool duplicate = false;
                for (const auto &sr : scanRoots) {
                    if (fs::equivalent(sr, uPath, ec)) { duplicate = true; break; }
                }
                if (!duplicate) scanRoots.push_back(uPath);
            }
        }
    }

    // 1. Python
    bool hasPython = SystemCore::runRawCommand("where python") || 
                     SystemCore::runRawCommand("where py") ||
                     (!baseLocal.empty() && fs::exists(baseLocal + "\\pip\\cache"));

    if (hasPython) {
        if (!baseLocal.empty()) wipeFolderContents(baseLocal + "\\pip\\cache");
        long long pyFreed = 0;
        for (const auto &sr : scanRoots) {
            cleanDirectoryArtifacts(sr, 
                {"__pycache__", ".pytest_cache", ".mypy_cache", ".ruff_cache", ".tox"}, 
                {".pyc", ".pyo"}, 
                pyFreed);
        }
    }

    // 2. Node.js / JavaScript
    if (!baseLocal.empty()) {
        wipeFolderContents(baseLocal + "\\npm-cache");
        wipeFolderContents(baseLocal + "\\Yarn\\Cache");
        wipeFolderContents(baseLocal + "\\pnpm\\store");
        wipeFolderContents(baseLocal + "\\pnpm\\cache");
        wipeFolderContents(baseLocal + "\\electron\\Cache");
        wipeFolderContents(baseLocal + "\\Microsoft\\TypeScript");
        wipeFolderContents(baseLocal + "\\deno\\deps");
    }
    if (!baseApp.empty()) {
        wipeFolderContents(baseApp + "\\npm-cache");
    }
    if (!baseUser.empty()) {
        wipeFolderContents(baseUser + "\\.turbo");
        wipeFolderContents(baseUser + "\\.npm");
        wipeFolderContents(baseUser + "\\.yarn");
        wipeFolderContents(baseUser + "\\.pnpm-store");
    }

    long long nodeFreed = 0;
    // Chỉ dọn các thư mục cache tạm, bảo vệ node_modules của các dự án
    for (const auto &sr : scanRoots) {
        cleanDirectoryArtifacts(sr, 
            {".turbo", ".parcel-cache", ".cache"}, 
            {}, 
            nodeFreed);
    }

    // 3. Java Gradle & Android (chỉ xóa cache/daemon, bảo vệ kho thư viện .m2/repository dùng chung)
    if (!baseUser.empty()) {
        wipeFolderContents(baseUser + "\\.gradle\\caches");
        wipeFolderContents(baseUser + "\\.gradle\\daemon");
        wipeFolderContents(baseUser + "\\.android\\cache");
    }

    // 4. VS Code, Cursor, NuGet, Rust, Go (chỉ xóa cache tạm, bảo vệ .nuget/packages dùng chung)
    if (!baseApp.empty()) {
        wipeFolderContents(baseApp + "\\Code\\Cache");
        wipeFolderContents(baseApp + "\\Code\\CachedData");
        wipeFolderContents(baseApp + "\\Code\\CachedExtensionVSIXs");
        wipeFolderContents(baseApp + "\\Cursor\\Cache");
        wipeFolderContents(baseApp + "\\Cursor\\CachedData");
    }
    if (!baseLocal.empty()) {
        wipeFolderContents(baseLocal + "\\NuGet\\v3-cache");
        wipeFolderContents(baseLocal + "\\go-build");
    }
    if (!baseUser.empty()) {
        wipeFolderContents(baseUser + "\\.cargo\\registry\\cache");
        wipeFolderContents(baseUser + "\\.rustup\\downloads");
    }

    if (interactive) {
        cout << "\n [✓] Hoàn tất dọn dẹp các môi trường phát triển (Dev)!\n";
        sc.waitEnter();
    }
}

// ----------------------------------------------------------------------------------
// DỌN DẸP TOÀN DIỆN TRÌNH DUYỆT (MULTI-PROFILE CHROMIUM & FIREFOX)
// ----------------------------------------------------------------------------------

void DiskCleaner::clearBrowserCache() {
    char *localAppData = std::getenv("LOCALAPPDATA");
    char *appData = std::getenv("APPDATA");
    if (!localAppData && !appData) return;

    string baseLocal = localAppData ? string(localAppData) : "";
    string baseApp   = appData ? string(appData) : "";

    vector<string> chromiumBases = {
        baseLocal + "\\Google\\Chrome\\User Data",
        baseLocal + "\\Microsoft\\Edge\\User Data",
        baseLocal + "\\CocCoc\\Browser\\User Data",
        baseLocal + "\\BraveSoftware\\Brave-Browser\\User Data",
        baseLocal + "\\Vivaldi\\User Data",
        baseLocal + "\\Opera Software\\Opera Stable",
        baseLocal + "\\Opera Software\\Opera GX Stable"
    };

    if (!baseApp.empty()) {
        chromiumBases.push_back(baseApp + "\\Opera Software\\Opera Stable");
        chromiumBases.push_back(baseApp + "\\Opera Software\\Opera GX Stable");
    }

    static const vector<string> cacheFolderNames = {
        "Cache", "Code Cache", "GPUCache", "DawnCache", "ShaderCache", 
        "GrShaderCache", "GraphiteDawnCache", "Service Worker\\CacheStorage", 
        "Service Worker\\ScriptCache"
    };

    // Duyệt đa profile Chromium
    for (const string &baseDir : chromiumBases) {
        std::error_code ec;
        if (!fs::exists(baseDir, ec)) continue;

        try {
            for (const auto &entry : fs::directory_iterator(baseDir, fs::directory_options::skip_permission_denied, ec)) {
                if (!entry.is_directory(ec)) continue;
                string dirName = entry.path().filename().string();
                
                bool isProfile = (dirName == "Default" || dirName.rfind("Profile", 0) == 0 || 
                                  dirName == "Guest Profile" || dirName == "System Profile");
                
                if (isProfile) {
                    for (const auto &cacheName : cacheFolderNames) {
                        fs::path targetCache = entry.path() / cacheName;
                        wipeFolderContents(targetCache);
                    }
                } else if (dirName == "ShaderCache" || dirName == "GrShaderCache" || dirName == "DawnCache") {
                    wipeFolderContents(entry.path());
                }
            }
        } catch (...) {}
    }

    // Mozilla Firefox (Roaming & Local)
    if (!baseApp.empty()) {
        string ffPath = baseApp + "\\Mozilla\\Firefox\\Profiles";
        std::error_code ec;
        if (fs::exists(ffPath, ec)) {
            try {
                for (const auto &profile : fs::directory_iterator(ffPath, fs::directory_options::skip_permission_denied, ec)) {
                    if (profile.is_directory(ec)) {
                        wipeFolderContents(profile.path() / "cache2");
                        wipeFolderContents(profile.path() / "startupCache");
                        wipeFolderContents(profile.path() / "jumpListCache");
                    }
                }
            } catch (...) {}
        }
    }

    if (!baseLocal.empty()) {
        string ffLocal = baseLocal + "\\Mozilla\\Firefox\\Profiles";
        std::error_code ec;
        if (fs::exists(ffLocal, ec)) {
            try {
                for (const auto &profile : fs::directory_iterator(ffLocal, fs::directory_options::skip_permission_denied, ec)) {
                    if (profile.is_directory(ec)) {
                        wipeFolderContents(profile.path() / "cache2");
                        wipeFolderContents(profile.path() / "startupCache");
                    }
                }
            } catch (...) {}
        }
    }
}

// ----------------------------------------------------------------------------------
// ĐIỀU PHỐI THỰC THI DỌN RÁC THEO LỰA CHỌN MENU
// ----------------------------------------------------------------------------------

void DiskCleaner::runCleanChoice(int choice) {
    if (choice < 1 || choice > 2) return;

    sc.cls();
    static const char* scopes[] = {
        "",
        "Plus: Temp, cache người dùng, trình duyệt, ứng dụng, hệ thống chuyên sâu và Downloads",
        "Pro: Toàn bộ Plus và cache công cụ lập trình"
    };
    cout << "\n== XEM TRƯỚC TÁC VỤ DỌN DẸP ==\n"
         << " Phạm vi : " << scopes[choice] << "\n"
         << " Chuyên sâu: Windows Update, log hệ thống, Windows.old và hibernation (cần Admin).\n"
         << " Khôi phục: Bộ cài Downloads được đưa vào Thùng rác; cache hệ thống không thể hoàn tác.\n\n";
    if (!sc.confirm(" Tiếp tục thực hiện? (y/N): ")) {
        cout << "\nĐã hủy, chưa có thay đổi nào được thực hiện.\n";
        Sleep(600);
        return;
    }
    cout << "\n";
    long long totalFreed = 0;

    {
        cout << "[*] Dọn temp & cache người dùng...\n";
        long long f1 = cleanSurfaceAndUserTemp();
        totalFreed += f1;
        cout << "     └── [✓] " << (f1 > 0 ? ("Giải phóng " + SystemCore::formatSize(f1)) : "Đã sạch sẽ từ trước") << "\n\n";
    }

    {
        cout << "[*] Dọn cache trình duyệt & ứng dụng...\n";
        long long f2 = cleanBrowserAndAppCache();
        totalFreed += f2;
        cout << "     └── [✓] " << (f2 > 0 ? ("Giải phóng " + SystemCore::formatSize(f2)) : "Đã sạch sẽ từ trước") << "\n\n";
    }

    {
        cout << "[*] Dọn hệ thống chuyên sâu...\n";
        long long f3 = cleanDeepSystemAndUpdates();
        if (f3 > 0) {
            totalFreed += f3;
            cout << "     └── [✓] Giải phóng " << SystemCore::formatSize(f3) << "\n\n";
        } else if (f3 == 0) {
            cout << "     └── [✓] Đã sạch sẽ từ trước\n\n";
        } else {
            cout << "     └── [!] Bị hủy hoặc cần quyền Administrator để dọn dẹp chuyên sâu\n\n";
        }
    }

    if (choice == 2) {
        cout << "[*] Dọn cache lập trình...\n";
        long long f4 = cleanDevArtifactsAndCaches();
        totalFreed += f4;
        cout << "     └── [✓] " << (f4 > 0 ? ("Giải phóng " + SystemCore::formatSize(f4)) : "Đã sạch sẽ từ trước") << "\n\n";
    }

    long long totalRecycled = 0;
    {
        cout << "[*] Dọn bộ cài Downloads...\n";
        long long f5 = cleanDownloadsExesAndDuplicates();
        totalRecycled += f5;
        cout << "     └── [✓] " << (f5 > 0 ? ("Đã chuyển vào Thùng rác " + SystemCore::formatSize(f5)) : "Đã sạch sẽ từ trước") << "\n\n";
    }

    cout << "\n\n";
    if (totalFreed > 0) {
        cout << " [✓] TỔNG DUNG LƯỢNG ĐÃ GIẢI PHÓNG TRỰC TIẾP: \x1b[92m" << SystemCore::formatSize(totalFreed) << "\x1b[0m\n";
    }
    if (totalRecycled > 0) {
        cout << " [ℹ] Tổng dung lượng đã chuyển vào Thùng rác: \x1b[93m" << SystemCore::formatSize(totalRecycled) << "\x1b[0m (Dọn sạch Thùng rác để giải phóng ổ đĩa)\n";
    }
    if (totalFreed <= 0 && totalRecycled <= 0) {
        cout << " [✓] Hệ thống đã rất sạch sẽ.\n";
    }
    cout << "\n";
    sc.waitEnter();
}
