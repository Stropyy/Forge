#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace forge {

/// Compute the SHA-256 hash of raw byte data.
/// Returns a 64-character lowercase hex string.
std::string sha256(const std::string& data);

/// SHA-256 hash a file's contents on disk.
/// Throws std::runtime_error if the file cannot be read.
std::string sha256_file(const std::string& filepath);

} // namespace forge
