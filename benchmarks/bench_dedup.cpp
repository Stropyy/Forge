#include <iostream>
#include <iomanip>
#include <filesystem>
#include <string>
#include <vector>
#include <chrono>

#include "forge/forge.h"
#include "forge/fs_utils.h"
#include "forge/index.h"
#include "forge/commit.h"
#include "forge/object_store.h"

namespace stdfs = std::filesystem;

static uintmax_t directory_size(const stdfs::path& dir) {
    uintmax_t size = 0;
    if (!stdfs::exists(dir)) return 0;
    for (const auto& entry : stdfs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file()) {
            size += entry.file_size();
        }
    }
    return size;
}

int main() {
    std::string bench_dir = (stdfs::temp_directory_path() / "forge_benchmark").string();
    stdfs::remove_all(bench_dir);
    forge::fs::init_repo(bench_dir);

    std::cout << "========================================================\n";
    std::cout << " Forge VCS Deduplication Benchmark (1,000+ files)\n";
    std::cout << "========================================================\n";

    constexpr size_t NUM_DIRS = 20;
    constexpr size_t FILES_PER_DIR = 60; // 20 * 60 = 1,200 files
    constexpr size_t NUM_COMMITS = 5;

    // 5 templates (~1 KB each) reused across directories
    std::vector<std::string> templates;
    for (int t = 0; t < 5; ++t) {
        std::string s;
        s.reserve(1024);
        for (int i = 0; i < 32; ++i) {
            s += "// Boilerplate module header template #" + std::to_string(t) + "\n";
            s += "namespace mod" + std::to_string(t) + " { int value = " + std::to_string(i * t) + "; }\n";
        }
        templates.push_back(s);
    }

    uintmax_t total_raw_size_all_commits = 0;
    auto start_time = std::chrono::high_resolution_clock::now();

    for (size_t c = 1; c <= NUM_COMMITS; ++c) {
        uintmax_t commit_raw_size = 0;
        forge::Index index(bench_dir);
        index.load();

        for (size_t d = 0; d < NUM_DIRS; ++d) {
            std::string dir_path = "module_" + std::to_string(d);
            for (size_t f = 0; f < FILES_PER_DIR; ++f) {
                std::string file_path = dir_path + "/file_" + std::to_string(f) + ".cpp";
                std::string full_path = bench_dir + "/" + file_path;

                std::string content = templates[f % templates.size()];
                if (c > 1 && (f % 10 == (c % 10))) {
                    content += "\n// Commit " + std::to_string(c) + " modification\n";
                }

                forge::fs::write_file(full_path, content);
                index.add(full_path);
                commit_raw_size += content.size();
            }
        }

        index.save();
        std::string commit_hash = forge::create_commit(
            bench_dir, "Commit #" + std::to_string(c), "Benchmark", "bench@forge.local");

        total_raw_size_all_commits += commit_raw_size;

        uintmax_t objects_size = directory_size(stdfs::path(bench_dir) / forge::REPO_DIR / "objects");
        double savings = (1.0 - static_cast<double>(objects_size) / total_raw_size_all_commits) * 100.0;

        std::cout << "Commit #" << c << " [" << commit_hash.substr(0, 8) << "] "
                  << "Files: " << (NUM_DIRS * FILES_PER_DIR) << " | "
                  << "Cumulative Raw: " << std::fixed << std::setprecision(1)
                  << (total_raw_size_all_commits / 1024.0) << " KB | "
                  << "Store on Disk: " << (objects_size / 1024.0) << " KB | "
                  << "Deduplication Savings: " << std::setprecision(2) << savings << "%\n";
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

    uintmax_t final_store_size = directory_size(stdfs::path(bench_dir) / forge::REPO_DIR / "objects");
    double total_savings = (1.0 - static_cast<double>(final_store_size) / total_raw_size_all_commits) * 100.0;

    std::cout << "\n--------------------------------------------------------\n";
    std::cout << "Summary:\n";
    std::cout << "  Total Files per Commit : " << (NUM_DIRS * FILES_PER_DIR) << "\n";
    std::cout << "  Total Commits Evaluated: " << NUM_COMMITS << "\n";
    std::cout << "  Total Raw Uncompressed : " << (total_raw_size_all_commits / 1024.0) << " KB\n";
    std::cout << "  Total Store on Disk    : " << (final_store_size / 1024.0) << " KB\n";
    std::cout << "  Disk Usage Reduction   : " << std::fixed << std::setprecision(2)
              << total_savings << "%\n";
    std::cout << "  Elapsed Execution Time : " << elapsed << " ms\n";
    std::cout << "========================================================\n";

    stdfs::remove_all(bench_dir);
    return 0;
}
