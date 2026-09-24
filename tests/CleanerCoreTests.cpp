#include "CleanerCore.h"

#include <fstream>
#include <iostream>

namespace {
int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << '\n';
        ++failures;
    } else {
        std::cout << "[PASS] " << message << '\n';
    }
}

void writeFile(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << content;
}
}

int main() {
    const fs::path root = fs::temp_directory_path() /
        ("diskcleaner-qa-" + std::to_string(GetCurrentProcessId()));
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    expect(!ec, "create isolated test directory");

    const fs::path first = root / "same-a.bin";
    const fs::path second = root / "same-b.bin";
    const fs::path different = root / "different.bin";
    writeFile(first, "identical-content");
    writeFile(second, "identical-content");
    writeFile(different, "different-content");
    expect(CleanerCore::filesHaveSameContent(first, second),
           "binary comparison accepts identical files");
    expect(!CleanerCore::filesHaveSameContent(first, different),
           "binary comparison rejects different files");

    const fs::path dryRunDir = root / "dry-run";
    writeFile(dryRunDir / "nested" / "sample.tmp", "1234567890");
    CleanStats dryStats;
    expect(CleanerCore::forceDeleteFolder(dryRunDir, true, dryStats),
           "dry-run reports deletable directory");
    expect(fs::exists(dryRunDir), "dry-run keeps directory intact");
    expect(dryStats.bytesFreed == 10, "dry-run computes metadata size");

    const fs::path deleteDir = root / "delete-real";
    writeFile(deleteDir / "nested" / "sample.tmp", "delete-me");
    CleanStats deleteStats;
    expect(CleanerCore::forceDeleteFolder(deleteDir, false, deleteStats),
           "real cleanup deletes isolated directory tree");
    expect(!fs::exists(deleteDir), "deleted directory no longer exists");
    expect(deleteStats.dirsDeleted == 1, "real cleanup updates directory stats");

    const fs::path singleFile = root / "single.tmp";
    writeFile(singleFile, "single-file");
    CleanStats fileStats;
    expect(CleanerCore::safeDeleteFile(singleFile, true, fileStats),
           "file dry-run succeeds");
    expect(fs::exists(singleFile), "file dry-run keeps file intact");
    expect(CleanerCore::safeDeleteFile(singleFile, false, fileStats),
           "real file cleanup succeeds");
    expect(!fs::exists(singleFile), "real file cleanup removes file");

    const fs::path lockedFile = root / "locked.tmp";
    writeFile(lockedFile, "locked-file");
    HANDLE lock = CreateFileW(lockedFile.c_str(), GENERIC_READ, 0, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    expect(lock != INVALID_HANDLE_VALUE, "open file with exclusive lock");
    CleanStats lockedStats;
    expect(!CleanerCore::safeDeleteFile(lockedFile, false, lockedStats),
           "locked cache file is not deleted");
    expect(lockedStats.itemsSkipped == 1 && lockedStats.errorsCount == 0,
           "locked cache file is skipped instead of reported as an error");
    if (lock != INVALID_HANDLE_VALUE) CloseHandle(lock);
    CleanerCore::safeDeleteFile(lockedFile, false, lockedStats);

    expect(CleanerCore::isCriticalPath(CleanerCore::getSystemDriveRoot()),
           "system drive root is protected");

    fs::remove_all(root, ec);
    expect(!fs::exists(root), "test directory cleanup succeeds");
    if (failures == 0) {
        std::cout << "All CleanerCore tests passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
