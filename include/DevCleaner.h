#ifndef DEV_CLEANER_H
#define DEV_CLEANER_H

#include "CleanerCore.h"
#include <vector>

/**
 * @brief Module dọn dẹp môi trường lập trình và build artifacts
 * Tuyệt đối bảo vệ: .git, .github, .gitignore, thư viện dùng chung (.m2/repository, nuget packages)
 * - Python: pip cache, __pycache__, .pytest_cache, .mypy_cache, .ruff_cache, .tox, *.pyc, *.pyo
 * - Node / Web: npm-cache, Yarn Cache, pnpm store/cache, electron cache, deno deps, .turbo, .parcel-cache
 * - Java & Android: gradle caches & daemons, android cache
 * - IDEs: VS Code Cache/CachedData, Cursor Cache/CachedData
 * - Compilers / Package Managers: Go build cache (go-build), Rust cargo registry cache & rustup downloads, NuGet v3-cache
 */
class DevCleaner {
private:
    static std::vector<fs::path> detectDevScanRoots();
    static int cleanDirectoryArtifacts(const fs::path& rootPath,
                                       const std::vector<std::string>& targetDirNames,
                                       const std::vector<std::string>& targetExtensions,
                                       bool dryRun,
                                       CleanStats& stats);
public:
    static CleanStats clean(bool dryRun = false);
};

#endif // DEV_CLEANER_H
