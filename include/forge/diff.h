#pragma once

#include <string>

namespace forge {

/// Diff two strings (old vs new) and produce a unified-diff-style output.
/// Each line is prefixed with ' ' (unchanged), '+' (added), or '-' (removed).
std::string unified_diff(const std::string& old_content,
                         const std::string& new_content,
                         const std::string& old_label = "a",
                         const std::string& new_label = "b");

} // namespace forge
