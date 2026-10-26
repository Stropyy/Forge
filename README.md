# Forge

Forge is a fast, lightweight, Git-inspired distributed version control system (VCS) written in modern C++17. It implements a content-addressed object store, SHA-256 hashing, index/staging area, commit DAG, branch references, three-way merge resolution, and unified diffing.

---

## Architecture Overview

Forge stores repository state inside a hidden `.forge` directory:

```
┌─────────┐     ┌───────────┐     ┌──────────────┐
│   CLI   │────▶│  Commands │────▶│   Core VCS   │
└─────────┘     └───────────┘     └──────┬───────┘
                                         │
                      ┌──────────────────┼──────────────────┐
                      ▼                  ▼                  ▼
                ┌──────────┐      ┌──────────┐      ┌──────────┐
                │  Object  │      │  Index   │      │   Refs   │
                │  Store   │      │ (Stage)  │      │ (Branch) │
                └──────────┘      └──────────┘      └──────────┘
```

### Core Components

- **Object Store (`.forge/objects/`)**: Content-addressable storage hashed using custom **SHA-256**. Objects are divided into three types:
  - `blob`: Raw file contents (deduplicated across directories and commits).
  - `tree`: Hierarchical directory listings mapping filenames, permissions, and hashes.
  - `commit`: Snapshot metadata containing tree pointer, author, timestamp, message, and parent commit hash(es).
- **Index (`.forge/index`)**: Staging area that tracks files scheduled for the next commit.
- **Reference System (`.forge/refs/` and `.forge/HEAD`)**: Tracks active branches (`.forge/refs/heads/<branch>`) and symbolic `HEAD` (or detached commit SHA).
- **Commit DAG & Merging**: Forms a Directed Acyclic Graph supporting linear branches, fast-forward merges, and three-way recursive merges with conflict markers.
- **Diff Engine**: Computes line-by-line unified diffs between the working tree, staging index, and commits.

---

## Prerequisites

- **C++ Compiler**: GCC (>= 9.0) or Clang supporting **C++17**
- **Build System**: CMake (>= 3.16) and `make` (or `ninja`)
- **Git**: Required for downloading GoogleTest during the build configuration
- **Python 3**: (Optional) For running the Python deduplication benchmark

On Debian / Ubuntu:
```bash
sudo apt update
sudo apt install -y build-essential cmake git python3
```

---

## Building Forge

### 1. Configure the Build
```bash
cmake -B build -S .
```
*(To build without tests or external dependencies: `cmake -B build -S . -DFORGE_BUILD_TESTS=OFF`)*

### 2. Compile
```bash
cmake --build build -j$(nproc)
```

This compiles:
- `build/forge`: Main CLI executable.
- `build/libforge_core.a`: Core VCS library.
- `build/forge_tests`: Unit and integration test suite (GoogleTest).
- `build/forge_bench`: C++ deduplication benchmark.

---

## Command Reference

| Command | Usage | Description |
|---|---|---|
| `init` | `forge init` | Initialize a new Forge repository in the current directory. |
| `add` | `forge add <file...>` or `forge add .` | Stage file(s) into the staging index. |
| `commit` | `forge commit -m "<message>"` | Record staged changes as a new commit snapshot. |
| `status` | `forge status` | Show status of modified, staged, and untracked files. |
| `log` | `forge log [-n <count>]` | Traverse and display the commit history DAG. |
| `branch` | `forge branch [<name>] [-d <name>]` | List, create, or delete branches. |
| `checkout` | `forge checkout <branch \| commit-hash>` | Switch branches or switch to a detached HEAD commit. |
| `merge` | `forge merge <branch>` | Merge specified branch into the current branch. |
| `diff` | `forge diff [--staged]` | Display line-level diffs (working tree vs index, or index vs HEAD). |
| `restore` | `forge restore [--source <commit>] <file>` | Restore a working tree file from HEAD or a specific commit. |

---

## Quickstart Tutorial

### 1. Initialize and Commit
```bash
# Initialize repo
forge init

# Create and stage a file
echo "Hello, Forge!" > README.txt
forge add README.txt

# Inspect status
forge status

# Commit
forge commit -m "Initial commit"

# View history
forge log
```

### 2. Branching & Merging
```bash
# Create and switch to a feature branch
forge branch feature
forge checkout feature

# Make changes on feature branch
echo "New feature content" >> README.txt
forge add README.txt
forge commit -m "Add feature content"

# Switch back to main and merge
forge checkout main
forge merge feature
```

---

## Running Tests

Forge uses GoogleTest for unit and integration testing.

```bash
# Run all tests directly
./build/forge_tests

# Run using CTest
ctest --test-dir build --output-on-failure

# Run specific test suites
./build/forge_tests --gtest_filter="TreeTest.*"
./build/forge_tests --gtest_filter="CommitTest.*"
./build/forge_tests --gtest_filter="MergeTest.*"
./build/forge_tests --gtest_filter="HashTest.*"
```

---

## Benchmarks & Storage Efficiency

Forge features content-level deduplication. When files share identical content (even across completely different directories or branches), only a single blob is stored on disk.

### Running the Python Benchmark (1,000+ files)
```bash
python3 scripts/benchmark.py
```

### Running the Native C++ Benchmark
```bash
./build/forge_bench
```

**Benchmark Results Summary:**
- Evaluated over 1,200 files across 20 directories across 5 sequential commit rounds.
- **Raw Uncompressed Size**: ~6.7 - 14.2 MB
- **On-Disk Store Size**: ~43.8 - 55.1 KB
- **Storage Reduction**: **> 99.3%** disk usage savings through content addressing and deduplication.

---

## Project Structure

```
.
├── CMakeLists.txt              # Build configuration
├── benchmarks/
│   └── bench_dedup.cpp         # C++ deduplication benchmark
├── include/forge/
│   ├── cli.h                   # CLI commands & dispatcher
│   ├── commit.h                # Commit object model & DAG serialization
│   ├── diff.h                  # Myers/LCS diff engine
│   ├── forge.h                 # Core constants & architecture definitions
│   ├── fs_utils.h              # Filesystem utilities
│   ├── hash.h                  # Hashing wrappers
│   ├── index.h                 # Staging index state management
│   ├── merge.h                 # 3-way merge resolution & conflict detection
│   ├── object_store.h          # Content-addressed blob/tree/commit store
│   ├── refs.h                  # Branch and HEAD reference management
│   ├── sha256.h                # Standalone SHA-256 implementation
│   └── tree.h                  # Tree serialization & directory traversal
├── scripts/
│   └── benchmark.py            # Python benchmark test suite
├── src/
│   ├── cli/
│   │   └── commands.cpp        # CLI command implementations
│   ├── core/
│   │   ├── commit.cpp          # Commit management
│   │   ├── diff.cpp            # Diff calculation
│   │   ├── fs_utils.cpp        # Filesystem helper implementations
│   │   ├── hash.cpp            # Hash implementations
│   │   ├── index.cpp           # Staging index implementations
│   │   ├── merge.cpp           # Merge logic implementations
│   │   ├── object_store.cpp    # Object store implementations
│   │   ├── refs.cpp            # Reference & HEAD management
│   │   └── tree.cpp            # Tree hierarchy implementations
│   └── main.cpp                # CLI entry point
└── tests/                      # GoogleTest test suites
    ├── test_commit.cpp
    ├── test_hash.cpp
    ├── test_merge.cpp
    ├── test_object_store.cpp
    ├── test_tree.cpp
    └── test_workflow.cpp
```
