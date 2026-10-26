#pragma once

#include <string>
#include <optional>

namespace forge {

/// The three object types stored in the content-addressable store.
enum class ObjectType : uint8_t {
    Blob,    ///< Raw file content
    Tree,    ///< Directory listing (name → hash mappings)
    Commit   ///< Snapshot metadata + parent pointer(s)
};

/// Convert an ObjectType to its string representation.
std::string object_type_to_string(ObjectType type);

/// Parse a string into an ObjectType (returns nullopt on failure).
std::optional<ObjectType> string_to_object_type(const std::string& str);

/// Content-addressable object store.
///
/// Objects are stored as:
///   .forge/objects/<first-2-chars-of-hash>/<remaining-hash-chars>
///
/// File format on disk:
///   <type>\0<size>\0<content>
///
/// Blob deduplication is automatic: identical content produces the same
/// SHA-256 hash and writes are skipped if the object already exists.
class ObjectStore {
public:
    /// Construct a store rooted at the given repository path.
    /// @param repo_root  Path to the working directory containing .forge/
    explicit ObjectStore(const std::string& repo_root);

    /// Write an object to the store.
    /// @return The SHA-256 hash (hex) of the stored object.
    /// If an object with the same hash already exists, this is a no-op
    /// (deduplication).
    std::string write_object(ObjectType type, const std::string& content);

    /// Read an object's raw content from the store.
    /// @param hash  The full SHA-256 hex hash.
    /// @return The deserialized content (without type/size header).
    /// Throws std::runtime_error if the object does not exist.
    std::string read_object(const std::string& hash) const;

    /// Read an object and also retrieve its type.
    std::string read_object(const std::string& hash, ObjectType& out_type) const;

    /// Check whether an object exists in the store.
    bool exists(const std::string& hash) const;

    /// Hash content without writing it (dry-run).
    std::string hash_object(ObjectType type, const std::string& content) const;

private:
    std::string objects_dir_;  ///< Absolute path to .forge/objects/

    /// Build the filesystem path for an object given its hash.
    std::string object_path(const std::string& hash) const;

    /// Serialize an object into the on-disk format.
    std::string serialize(ObjectType type, const std::string& content) const;

    /// Deserialize an on-disk object, returning content and setting out_type.
    std::string deserialize(const std::string& raw, ObjectType& out_type) const;
};

} // namespace forge
