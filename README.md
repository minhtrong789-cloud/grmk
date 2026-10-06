# grmk

A lightweight Git-compatible version control CLI and storage engine implemented in C. Implemented from scratch in pure C11, parsing binary Git index files, tree traversal, and SHA-1 hashing directly.

`grmk` provides two-way Git compatibility: both `git` and `grmk` can freely checkout each other's branches and inspect shared objects seamlessly.

- **Git-Compatible Objects**: Generates standard zlib-compressed objects and SHA-1 hashes directly readable by Git.
- **Safe Branch Isolation**: Protects original Git data by managing commits under isolated `grmk-` branches.
- **Fast Mirroring**: Quickly synchronizes Git index and HEAD state into `grmk`.

### Workflow & State Sync

> **Warning:** Use grmk on git might have some branch issues. Switching branches without syncing will desynchronize Git and grmk staging states. To avoid this YOU HAVE TO:

- **Keep in mind:** Must commit changes before start using grmk. 
- **Before grmk:** `git` ──► `grmk sync` ──► [work in grmk]
- **After grmk:** `git reset` ──► [work in git]

*Why this is needed:* `grmk` maintains its own independent staging file and `HEAD` reference. Resetting or syncing ensures Git's index stays in sync with the working tree state produced by `grmk`.

---

### Command Reference

### Core Actions
```text
grmk init [<dir>]                                  Create empty repository
grmk add <file>... [-A] [-u] [-v]                  Stage changes to index
grmk rm <file>... [-f] [-v]                        Remove files from tree & index
grmk commit [-a] [--amend] -m <msg>                Record changes to repository
grmk branch [-cr] [-sw] [-rn] [-d]                 Manage branches
grmk checkout <target>                             Switch branches or restore tree
grmk reset [<target>] [--soft|--mixed|--hard]      Reset current HEAD
grmk sync [-idx] [-H] [-b] [-a] [-f] [-v]          Sync state with native Git
```

### Inspection & Plumbing
```text
grmk status [-s]                                   Show working tree status
grmk diff [<file>]                                 Show changes vs stage
grmk log [-s] [-n <count>]                         Show commit logs
grmk ls-files [-stg] [-idx]                        List tracked files
grmk ls-tree [-r] <tree>                           List tree contents
grmk cat-file [-p|-t|-s|-e] <hash>                 Inspect repository objects
grmk hash-object [--blob|--tree|--commit] [-w]     Compute or write object ID
```


## Quickstart (Windows)

### Requirements
- MinGW-w64 / MSYS2 with GCC (C11 support) & `zlib`
- CMake >= 3.20

### Build & Setup PATH
```powershell
# 1. Build portable binary (static linked)
cmake -B build -G "MinGW Makefiles"
cmake --build build
# 2. Run unit tests
cd build
ctest --output-on-failure
cd ..
# 3. Add grmk to current terminal session
. .\envSet.ps1
```
## Storage & Internals
`grmk` operates directly on Git filesystem internals:
- **Loose Objects (`.git/objects/xx/` or `.grmk/objects/xx/`)**: Blobs, Trees, and Commits are zlib-deflated with standard Git headers (`<type> <size>\0<data>`).
- **Binary Staging Index**: Direct reading and writing of Git index format v2 with big-endian integer parsing and binary-search entry lookups.
- **Hashing**: SHA-1 computation powered by Windows Cryptography API: Next Generation (`bcrypt.dll`).
- **Zero Runtime Dependencies**: Fully statically linked executable with zero external runtime DLL requirements.
---
## License
MIT License
