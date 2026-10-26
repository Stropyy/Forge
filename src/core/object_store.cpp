#include "forge/object_store.h"
#include "forge/hash.h"
#include "forge/fs_utils.h"
#include "forge/forge.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace forge {

// ── Helpers ──────────────────────────────────────────────────────────────

std::string object_type_to_string(ObjectType type) {
    switch (type) {
        case ObjectType::Blob:   return "blob";
        case ObjectType::Tree:   return "tree";
        case ObjectType::Commit: return "commit";
    }
    return "unknown";
}

std::optional<ObjectType> string_to_object_type(const std::string& str) {
    if (str == "blob")   return ObjectType::Blob;
    if (str == "tree")   return ObjectType::Tree;
    if (str == "commit") return ObjectType::Commit;
    return std::nullopt;
}

// ── ObjectStore ──────────────────────────────────────────────────────────

ObjectStore::ObjectStore(const std::string& repo_root)
    : objects_dir_(repo_root + "/" + REPO_DIR + "/objects") {}

std::string ObjectStore::object_path(const std::string& hash) const {
    // Split hash into 2-char prefix directory and remaining chars.
    // e.g., "abcdef..." → "objects/ab/cdef..."
    if (hash.size() < 4) {
        throw std::runtime_error("Invalid hash (too short): " + hash);
    }
    return objects_dir_ + "/" + hash.substr(0, 2) + "/" + hash.substr(2);
}

std::string ObjectStore::serialize(ObjectType type, const std::string& content) const {
    // Format: "<type>\0<size>\0<content>"
    std::string header = object_type_to_string(type);
    header += '\0';
    header += std::to_string(content.size());
    header += '\0';
    header += content;
    return header;
}

std::string ObjectStore::deserialize(const std::string& raw, ObjectType& out_type) const {
    // Find first null byte → type string
    auto first_null = raw.find('\0');
    if (first_null == std::string::npos) {
        throw std::runtime_error("Corrupt object: missing type delimiter");
    }
    std::string type_str = raw.substr(0, first_null);

    auto parsed_type = string_to_object_type(type_str);
    if (!parsed_type) {
        throw std::runtime_error("Corrupt object: unknown type '" + type_str + "'");
    }
    out_type = *parsed_type;

    // Find second null byte → size string
    auto second_null = raw.find('\0', first_null + 1);
    if (second_null == std::string::npos) {
        throw std::runtime_error("Corrupt object: missing size delimiter");
    }

    // Everything after the second null is the content
    return raw.substr(second_null + 1);
}

std::string ObjectStore::hash_object(ObjectType type, const std::string& content) const {
    return sha256(serialize(type, content));
}

std::string ObjectStore::write_object(ObjectType type, const std::string& content) {
    std::string serialized = serialize(type, content);
    std::string hash = sha256(serialized);

    // Deduplication: skip write if the object already exists
    std::string path = object_path(hash);
    if (std::filesystem::exists(path)) {
        return hash;
    }

    // Ensure the prefix directory exists
    fs::ensure_directory(std::filesystem::path(path).parent_path().string());

    // Write atomically (write to temp, then rename — simplified here)
    fs::write_file(path, serialized);

    return hash;
}

std::string ObjectStore::read_object(const std::string& hash) const {
    ObjectType type;
    return read_object(hash, type);
}

std::string ObjectStore::read_object(const std::string& hash, ObjectType& out_type) const {
    std::string path = object_path(hash);
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Object not found: " + hash);
    }

    std::string raw = fs::read_file(path);
    return deserialize(raw, out_type);
}

bool ObjectStore::exists(const std::string& hash) const {
    return std::filesystem::exists(object_path(hash));
}

} // namespace forge
