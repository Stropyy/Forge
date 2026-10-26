#include "forge/diff.h"

#include <sstream>
#include <vector>
#include <algorithm>

namespace forge {

/// Simple line-by-line diff using Longest Common Subsequence (LCS).
/// Produces unified-diff-style output.

static std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

std::string unified_diff(const std::string& old_content,
                         const std::string& new_content,
                         const std::string& old_label,
                         const std::string& new_label) {
    auto old_lines = split_lines(old_content);
    auto new_lines = split_lines(new_content);

    // Build LCS table
    size_t m = old_lines.size();
    size_t n = new_lines.size();
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));

    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            if (old_lines[i - 1] == new_lines[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
            } else {
                dp[i][j] = std::max(dp[i - 1][j], dp[i][j - 1]);
            }
        }
    }

    // Backtrack to produce diff
    std::vector<std::string> diff_lines;
    size_t i = m, j = n;

    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && old_lines[i - 1] == new_lines[j - 1]) {
            diff_lines.push_back(" " + old_lines[i - 1]);
            --i; --j;
        } else if (j > 0 && (i == 0 || dp[i][j - 1] >= dp[i - 1][j])) {
            diff_lines.push_back("+" + new_lines[j - 1]);
            --j;
        } else {
            diff_lines.push_back("-" + old_lines[i - 1]);
            --i;
        }
    }

    // Reverse since we built it backwards
    std::reverse(diff_lines.begin(), diff_lines.end());

    // Assemble output
    std::ostringstream out;
    out << "--- " << old_label << "\n";
    out << "+++ " << new_label << "\n";
    for (const auto& line : diff_lines) {
        out << line << "\n";
    }
    return out.str();
}

} // namespace forge
