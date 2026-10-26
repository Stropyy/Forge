#include "forge/hash.h"

#include <iomanip>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "forge/sha256.h"

namespace forge {

std::string sha256(const std::string& data) {
    return SHA256::hash(data);
}

std::string sha256_file(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file for hashing: " + filepath);
    }

    std::ostringstream contents;
    contents << file.rdbuf();
    return sha256(contents.str());
}

} // namespace forge
