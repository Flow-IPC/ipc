Vendored Conan recipe: jemalloc 5.4.0

Purpose: the GitHub CI pipeline's "early-warning" cells (see `deps` matrix axis in .github/workflows/main.yml) build
against the newest available versions of key dependencies.  For jemalloc that is 5.4.0 (released 9/2026), but as
of this writing conan-center has no recipe for it (newest there: 5.3.1).  So the pipeline exports this recipe into
the local Conan cache (`conan export conan/jemalloc --version 5.4.0`) in those cells only, before `conan install`.

Contents: `conanfile.py` is a verbatim copy of conan-center-index's recipes/jemalloc/all/conanfile.py (9/22/2026).
`conandata.yml` lists only 5.4.0 (release tarball URL + sha256).  conan-center's per-version patches are omitted: for
5.2.1 through 5.3.1 they are macOS/MSVC-only backports; none applies to Linux/gcc/clang, our only target.

Remove this directory, and the `conan export` step in the workflow, once conan-center publishes jemalloc/5.4.0 (check
recipes/jemalloc/config.yml there); then `[replace_requires]` alone suffices.
