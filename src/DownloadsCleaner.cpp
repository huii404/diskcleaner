#include "DownloadsCleaner.h"
#include <iostream>
#include <vector>
#include <map>
#include <regex>
#include <algorithm>
#include <chrono>
#include <shlobj.h>

bool DownloadsCleaner::isCorruptDownload(const std::string& ext) {
    return (ext == ".crdownload" || ext == ".part" || ext == ".tmp");
}

bool DownloadsCleaner::isSupportedExecutable(const std::string& ext) {
    return (ext == ".exe" || ext == ".msi");
}

bool DownloadsCleaner::isSupportedImage(const std::string& ext) {
    static const std::unordered_set<std::string> imgExts = {
        ".jpg", ".jpeg", ".png", ".gif", ".webp", ".bmp", ".svg", ".ico", ".tiff"
    };
    return (imgExts.find(ext) != imgExts.end());
}

bool DownloadsCleaner::isSupportedVideo(const std::string& ext) {
    static const std::unordered_set<std::string> vidExts = {
        ".mp4", ".mkv", ".avi", ".mov", ".wmv", ".flv", ".webm", ".m4v"
    };
    return (vidExts.find(ext) != vidExts.end());
}

std::string DownloadsCleaner::getDownloadsPath() {
    PWSTR path = NULL;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, NULL, &path))) {
        char buf[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, path, -1, buf, MAX_PATH, NULL, NULL);
        CoTaskMemFree(path);
        return std::string(buf);
    }
    const char *userProf = getenv("USERPROFILE");
    if (userProf) return std::string(userProf) + "\\Downloads";
    return "";
}

std::string DownloadsCleaner::cleanAppName(const std::string& raw) {
    std::string s = CleanerCore::toLower(raw);
    static const std::vector<std::string> junkPatterns = {
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
        while ((pos = s.find(jp, pos)) != std::string::npos) {
            s.replace(pos, jp.length(), " ");
            pos += 1;
        }
    }
    return CleanerCore::trim(s);
}

std::unordered_set<std::string> DownloadsCleaner::getInstalledAppNames() {
    std::unordered_set<std::string> installed;

    auto scanRegistryKey = [&](HKEY root, const char *subKey) {
        HKEY hKey;
        if (RegOpenKeyExA(root, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char keyName[256];
            DWORD keyNameSize = sizeof(keyName);
            DWORD keyIndex = 0;

            while (RegEnumKeyExA(hKey, keyIndex++, keyName, &keyNameSize, NULL, NULL, NULL, NULL) == ERROR_SUCCESS) {
                HKEY hSubKey;
                if (RegOpenKeyExA(hKey, keyName, 0, KEY_READ, &hSubKey) == ERROR_SUCCESS) {
                    char displayName[512] = {0};
                    DWORD dataSize = sizeof(displayName);
                    if (RegQueryValueExA(hSubKey, "DisplayName", NULL, NULL, (LPBYTE)displayName, &dataSize) == ERROR_SUCCESS) {
                        std::string name = CleanerCore::trim(CleanerCore::toLower(displayName));
                        if (!name.empty()) {
                            installed.insert(name);
                            std::string cleaned = cleanAppName(name);
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

    scanRegistryKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
    scanRegistryKey(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
    scanRegistryKey(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall");

    return installed;
}

std::string DownloadsCleaner::getExeProductName(const std::string& exePath) {
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

    std::vector<BYTE> data(size);
    if (!pGetInfo(exePath.c_str(), 0, size, data.data())) {
        FreeLibrary(hVer);
        return "";
    }

    struct LANGANDCODEPAGE {
        WORD wLanguage;
        WORD wCodePage;
    } *lpTranslate = NULL;
    UINT cbTranslate = 0;

    std::string productName = "";
    std::string fileDescription = "";

    auto queryField = [&](const char *blockFormat, WORD lang, WORD cp, const char *field) -> std::string {
        char subBlock[128];
        sprintf_s(subBlock, sizeof(subBlock), blockFormat, lang, cp, field);
        LPVOID lpBuffer = NULL;
        UINT sizeStr = 0;
        if (pQueryVal(data.data(), subBlock, &lpBuffer, &sizeStr) && lpBuffer && sizeStr > 0) {
            return std::string((char*)lpBuffer);
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

    auto isGenericEngine = [](const std::string &val) -> bool {
        std::string lower = CleanerCore::toLower(val);
        static const std::vector<std::string> genericEngines = {
            "inno setup", "nullsoft", "nsis", "installshield", "advanced installer",
            "wise installation", "wix toolset", "7-zip self-extracting", "winrar sfx",
            "bootstrap", "setup application", "installer"
        };
        for (const auto &ge : genericEngines) {
            if (lower.find(ge) != std::string::npos) return true;
        }
        return false;
    };

    if (!productName.empty() && !isGenericEngine(productName)) {
        return CleanerCore::trim(productName);
    }
    if (!fileDescription.empty() && !isGenericEngine(fileDescription)) {
        return CleanerCore::trim(fileDescription);
    }

    return "";
}

CleanStats DownloadsCleaner::clean(bool dryRun) {
    CleanStats stats;

    std::string dlPath = getDownloadsPath();
    if (dlPath.empty()) return stats;

    std::error_code ec;
    if (!fs::exists(dlPath, ec) || !fs::is_directory(dlPath, ec)) return stats;

    // 1. DỌN FILE TẢI DỞ DANG / LỖI (.crdownload, .part, .tmp) QUÁ 24 GIỜ
    FILETIME nowFt;
    GetSystemTimeAsFileTime(&nowFt);
    ULARGE_INTEGER nowTime;
    nowTime.LowPart = nowFt.dwLowDateTime;
    nowTime.HighPart = nowFt.dwHighDateTime;

    for (const auto &entry : fs::directory_iterator(dlPath, fs::directory_options::skip_permission_denied, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        std::string ext = CleanerCore::toLower(entry.path().extension().string());

        if (isCorruptDownload(ext)) {
            WIN32_FILE_ATTRIBUTE_DATA fad;
            if (GetFileAttributesExW(entry.path().c_str(), GetFileExInfoStandard, &fad)) {
                ULARGE_INTEGER fileTime;
                fileTime.LowPart = fad.ftLastWriteTime.dwLowDateTime;
                fileTime.HighPart = fad.ftLastWriteTime.dwHighDateTime;
                if (nowTime.QuadPart > fileTime.QuadPart) {
                    ULONGLONG diffSeconds = (nowTime.QuadPart - fileTime.QuadPart) / 10000000ULL;
                    if (diffSeconds >= 24 * 3600) { // Cũ hơn 24 giờ
                        // XÓA CỨNG TRIỆT ĐỂ: File lỗi/dở dang không có giá trị phục hồi
                        CleanerCore::safeDeleteFile(entry.path(), dryRun, stats);
                    }
                }
            }
        }
    }

    // 2. CƠ CHẾ AN TOÀN TUYỆT ĐỐI: CHỈ LỌC .EXE, .MSI, FILE ẢNH VÀ VIDEO
    struct DownloadFileItem {
        fs::path fullPath;
        std::string filename;
        std::string ext;
        std::string baseStem;
        int copyIndex;       // 0: bản gốc (file.ext), >=1: bản sao Windows đánh số file (1).ext
        uintmax_t size;
    };

    // Regex phát hiện bản sao Windows: "filename (1).ext", "filename (2).ext"
    std::regex dupRegex(R"(^(.+?)\s*\(([0-9]+)\)\.([a-zA-Z0-9]+)$)", std::regex::icase);
    std::map<std::string, std::vector<DownloadFileItem>> groups;

    for (const auto &entry : fs::directory_iterator(dlPath, fs::directory_options::skip_permission_denied, ec)) {
        if (!entry.is_regular_file(ec)) continue;

        std::string ext = CleanerCore::toLower(entry.path().extension().string());

        // LỌC CHẶT CHẼ THEO WHITELIST: Chỉ chấp nhận exe/msi, ảnh hoặc video!
        // Bỏ qua tuyệt đối: .zip, .rar, .pdf, .docx, .xlsx, .txt, code, v.v...
        bool isExe = isSupportedExecutable(ext);
        bool isImg = isSupportedImage(ext);
        bool isVid = isSupportedVideo(ext);

        if (!isExe && !isImg && !isVid) {
            continue;
        }

        std::string filename = entry.path().filename().string();
        std::string stem = entry.path().stem().string();
        uintmax_t sz = entry.file_size(ec);

        std::string baseStem = stem;
        int copyIndex = 0;
        std::smatch match;
        if (std::regex_match(filename, match, dupRegex)) {
            baseStem = match[1].str();
            try { copyIndex = std::stoi(match[2].str()); } catch (...) { copyIndex = 1; }
        }

        std::string lowerBaseStem = CleanerCore::toLower(baseStem);
        std::string groupKey = lowerBaseStem + ext;
        if (groupKey.empty()) groupKey = filename;

        groups[groupKey].push_back({entry.path(), filename, ext, baseStem, copyIndex, sz});
    }

    auto installed = getInstalledAppNames();

    // 3. XỬ LÝ DỌN DẸP THEO NHÓM
    for (auto &pair : groups) {
        auto &fileList = pair.second;
        if (fileList.empty()) continue;

        std::string ext = fileList[0].ext;
        bool isExe = isSupportedExecutable(ext);

        // A. NẾU LÀ FILE BỘ CÀI (.EXE / .MSI) -> KIỂM TRA ĐÃ CÀI ĐẶT CHƯA
        if (isExe) {
            std::string prodName = "";
            for (const auto &item : fileList) {
                if (item.ext == ".exe") {
                    std::string pName = getExeProductName(item.fullPath.string());
                    if (!pName.empty()) {
                        prodName = pName;
                        break;
                    }
                }
            }

            std::string cleanedProd = cleanAppName(prodName);
            std::string cleanedBase = cleanAppName(fileList[0].baseStem);

            static const std::unordered_set<std::string> genericBlacklist = {
                "setup", "installer", "install", "update", "updater", "app", "application",
                "win", "windows", "tool", "tools", "patch", "package", "download", "temp",
                "x64", "x86", "64bit", "32bit", "full", "free", "beta", "portable"
            };

            bool groupInstalled = false;

            auto matchCandidate = [&](const std::string &candidate) -> bool {
                if (candidate.empty() || candidate.length() < 3) return false;
                if (genericBlacklist.find(candidate) != genericBlacklist.end()) return false;

                for (const auto &inst : installed) {
                    if (inst == candidate) return true;
                    if (candidate.length() < 5) continue;

                    size_t pos = inst.find(candidate);
                    if (pos != std::string::npos) {
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

            // Tuyệt đối không xóa ứng dụng portable hoặc standalone
            std::string lowerFile = CleanerCore::toLower(fileList[0].filename);
            if (lowerFile.find("portable") != std::string::npos || lowerFile.find("standalone") != std::string::npos) {
                groupInstalled = false;
            }

            // Nếu ứng dụng ĐÃ ĐƯỢC CÀI ĐẶT -> Chuyển toàn bộ file cài đặt vào Thùng rác
            if (groupInstalled) {
                for (const auto &item : fileList) {
                    CleanerCore::moveToRecycleBin(item.fullPath, dryRun, stats);
                }
                continue; // Đã xử lý xong nhóm này
            }
        }

        // B. NẾU ỨNG DỤNG CHƯA CÀI ĐẶT, HOẶC LÀ FILE ẢNH, HOẶC LÀ FILE VIDEO:
        //    ÁP DỤNG QUY TẮC DỌN TRÙNG LẶP:
        //    "CHỈ GIỮ TÊN GỐC (file.ext) VÀ TÊN CÓ CHỈ SỐ CAO NHẤT (file (N).ext)"
        //    CÁC BẢN SAO Ở GIỮA ĐỀU ĐƯỢC CHUYỂN VÀO THÙNG RÁC.
        if (fileList.size() > 1) {
            // Sắp xếp tăng dần theo chỉ số copyIndex (0, 1, 2, ..., N)
            std::sort(fileList.begin(), fileList.end(), [](const DownloadFileItem &a, const DownloadFileItem &b) {
                return a.copyIndex < b.copyIndex;
            });

            // Tìm file có copyIndex cao nhất (maxCopyIndex > 0)
            int maxCopyIndex = -1;
            size_t highestIdx = 0;
            for (size_t i = 0; i < fileList.size(); ++i) {
                if (fileList[i].copyIndex > maxCopyIndex) {
                    maxCopyIndex = fileList[i].copyIndex;
                    highestIdx = i;
                }
            }

            // Kiểm tra có file gốc (copyIndex == 0) không
            bool hasBase = (fileList[0].copyIndex == 0);

            for (size_t i = 0; i < fileList.size(); ++i) {
                // Giữ lại bản gốc (copyIndex == 0)
                if (hasBase && i == 0) {
                    continue;
                }

                // Giữ lại bản sao có chỉ số cao nhất (nếu có bản sao)
                if (maxCopyIndex > 0 && i == highestIdx) {
                    continue;
                }

                // Nếu không có bản gốc (chỉ có các bản sao (1), (2), (5)...):
                // Giữ lại bản sao đầu tiên (thay thế cho bản gốc) và bản sao cao nhất
                if (!hasBase && i == 0) {
                    continue;
                }

                // Các file còn lại (file (1), file (2)... nằm giữa gốc và cao nhất)
                // được đưa vào Thùng rác để giải phóng dung lượng!
                CleanerCore::moveToRecycleBin(fileList[i].fullPath, dryRun, stats);
            }
        }
    }

    return stats;
}
