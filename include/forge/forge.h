#pragma once

/// Forge — A Git-inspired version control system
///
/// Architecture Overview
/// ─────────────────────
///
/// ┌─────────┐     ┌───────────┐     ┌──────────────┐
/// │   CLI   │────▶│  Commands  │────▶│   Core VCS   │
/// └─────────┘     └───────────┘     └──────┬───────┘
///                                          │
///                       ┌──────────────────┼──────────────────┐
///                       ▼                  ▼                  ▼
///                 ┌──────────┐      ┌──────────┐      ┌──────────┐
///                 │  Object  │      │  Index   │      │   Refs   │
///                 │  Store   │      │ (Stage)  │      │ (Branch) │
///                 └──────────┘      └──────────┘      └──────────┘
///
/// Object types:  Blob  |  Tree  |  Commit
/// Storage:       .forge/objects/<sha256-prefix>/<sha256-rest>
/// Refs:          .forge/refs/heads/<branch-name>
/// HEAD:          .forge/HEAD  (ref: refs/heads/main  OR  <sha256>)
/// Index:         .forge/index  (staged file list)
///
/// The DAG is formed by Commit objects pointing to parent commit(s).
/// Merges produce commits with two parents.

#include <string>

namespace forge {

/// Semantic version of the Forge VCS
inline constexpr const char* VERSION = "0.1.0";

/// Name of the hidden repository directory (analogous to .git)
inline constexpr const char* REPO_DIR = ".forge";

} // namespace forge
