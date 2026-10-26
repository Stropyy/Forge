#!/usr/bin/env python3
"""
Benchmark script for Forge VCS:
- Creates a repository with 1,000+ files across nested directories.
- Duplicates content across directories (tests deduplication).
- Performs repeated commits with incremental edits.
- Computes total object store size on disk vs. total cumulative raw file size.
"""

import os
import sys
import shutil
import tempfile
import subprocess
from pathlib import Path

def get_dir_size(path: Path) -> int:
    total = 0
    if not path.exists():
        return 0
    for p in path.rglob("*"):
        if p.is_file():
            total += p.stat().st_size
    return total

def run_cmd(cmd, cwd):
    res = subprocess.run(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if res.returncode != 0:
        raise RuntimeError(f"Command failed ({' '.join(cmd)}): {res.stderr}")
    return res.stdout.strip()

def main():
    forge_bin = Path(__file__).resolve().parent.parent / "build" / "forge"
    if not forge_bin.exists():
        forge_bin_alt = shutil.which("forge")
        if forge_bin_alt:
            forge_bin = Path(forge_bin_alt)
        else:
            print(f"Error: forge executable not found at {forge_bin}. Please build the project first.")
            sys.exit(1)

    temp_dir = Path(tempfile.mkdtemp(prefix="forge_bench_"))
    try:
        print("========================================================")
        print(" Forge VCS Deduplication Benchmark (1,000+ files)")
        print(f" Working Directory: {temp_dir}")
        print("========================================================")

        run_cmd([str(forge_bin), "init"], cwd=temp_dir)

        num_dirs = 20
        files_per_dir = 60  # 1,200 total files
        num_commits = 5

        # 5 distinct boilerplate templates (reused heavily across modules)
        templates = [
            f"// Boilerplate module header template #{t}\n" +
            "".join([f"namespace mod{t} {{ int value = {i * t}; }}\n" for i in range(32)])
            for t in range(5)
        ]

        total_raw_cumulative = 0

        for c in range(1, num_commits + 1):
            commit_raw = 0
            for d in range(num_dirs):
                dir_path = temp_dir / f"module_{d}"
                dir_path.mkdir(parents=True, exist_ok=True)

                for f in range(files_per_dir):
                    file_path = dir_path / f"file_{f}.cpp"
                    content = templates[f % len(templates)]
                    if c > 1 and (f % 10 == (c % 10)):
                        content += f"\n// Modification in commit {c}\n"

                    file_path.write_text(content)
                    commit_raw += len(content.encode("utf-8"))

            run_cmd([str(forge_bin), "add", "."], cwd=temp_dir)
            out = run_cmd([str(forge_bin), "commit", "-m", f"Commit #{c}"], cwd=temp_dir)

            total_raw_cumulative += commit_raw
            store_size = get_dir_size(temp_dir / ".forge" / "objects")
            savings = (1.0 - store_size / total_raw_cumulative) * 100.0

            print(f"Commit #{c} ({out.split()[0]}) | "
                  f"Files: {num_dirs * files_per_dir} | "
                  f"Cumulative Raw: {total_raw_cumulative / 1024:.1f} KB | "
                  f"Store on Disk: {store_size / 1024:.1f} KB | "
                  f"Deduplication Savings: {savings:.2f}%")

        final_store_size = get_dir_size(temp_dir / ".forge" / "objects")
        total_savings = (1.0 - final_store_size / total_raw_cumulative) * 100.0

        print("\n--------------------------------------------------------")
        print("Summary:")
        print(f"  Total Files per Commit : {num_dirs * files_per_dir}")
        print(f"  Total Commits Evaluated: {num_commits}")
        print(f"  Total Raw Uncompressed : {total_raw_cumulative / 1024:.1f} KB")
        print(f"  Total Store on Disk    : {final_store_size / 1024:.1f} KB")
        print(f"  Disk Usage Reduction   : {total_savings:.2f}%")
        print("========================================================")

    finally:
        shutil.rmtree(temp_dir, ignore_errors=True)

if __name__ == "__main__":
    main()
