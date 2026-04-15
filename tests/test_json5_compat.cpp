//
// test_json5_compat.cpp — JSON5 conformance tests against the official test suite
//
// Reads every file under tests/testdata/ and validates that:
//   .json  files → parse succeeds  (valid JSON, also valid JSON5)
//   .json5 files → parse succeeds  (valid JSON5)
//   .txt   files → parse fails     (invalid JSON5)
//   .js    files → parse fails     (valid ES5 but disallowed by JSON5)
//
// Skips the "todo/" subdirectory (known unimplemented features).
//

#include <json5/json5.hpp>

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ── Helpers ──────────────────────────────────────────────────────

static std::string read_entire_file(const fs::path& p) {
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs) {
        std::cerr << "  ERROR: cannot open " << p << "\n";
        std::abort();
    }
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

enum class Expect { pass, fail, skip };

static Expect classify(const fs::path& p) {
    auto ext = p.extension().string();
    if (ext == ".json" || ext == ".json5") return Expect::pass;
    if (ext == ".txt"  || ext == ".js")    return Expect::fail;
    // .errorSpec, .md, .editorconfig, .gitattributes, etc.
    return Expect::skip;
}

// ── Result counters ──────────────────────────────────────────────

struct Stats {
    int total   = 0;
    int passed  = 0;
    int failed  = 0;
    int skipped = 0;

    struct Failure {
        std::string file;
        std::string expected;   // "pass" or "fail"
        std::string detail;
    };
    std::vector<Failure> failures;
};

// ── Core test runner ─────────────────────────────────────────────

static void run_one(const fs::path& relpath, const fs::path& abspath, Stats& stats) {
    auto expect = classify(abspath);
    if (expect == Expect::skip) {
        ++stats.skipped;
        return;
    }

    ++stats.total;
    std::string content = read_entire_file(abspath);
    std::string error_msg;
    auto result = json5::try_parse(content, &error_msg);

    // dump the result


    if (expect == Expect::pass) {
        if (result.has_value()) {
            ++stats.passed;
            std::cout << "  PASS  " << relpath.string() << " (expected: pass), result: " << json5::stringify(result.value(), {}) << "\n";
        } else {
            ++stats.failed;
            std::cout << "  FAIL  " << relpath.string()
                      << "  (expected parse success, got error: " << error_msg << ")\n";
            stats.failures.push_back({relpath.string(), "pass", error_msg});
        }
    } else {  // Expect::fail
        if (!result.has_value()) {
            ++stats.passed;
            std::cout << "  PASS  " << relpath.string() << " (expected: fail), result: " << error_msg << "\n";
        } else {
            ++stats.failed;
            std::cout << "  FAIL  " << relpath.string()
                      << "  (expected parse failure, but parse succeeded)\n";
            stats.failures.push_back({relpath.string(), "fail", "parse succeeded unexpectedly"});
        }
    }
}

// ── Entry point ──────────────────────────────────────────────────

int main() {
#ifdef TESTDATA_DIR
    const fs::path testdata_dir = TESTDATA_DIR;
#else
    const fs::path testdata_dir = fs::path(__FILE__).parent_path() / "testdata";
#endif

    if (!fs::exists(testdata_dir) || fs::is_empty(testdata_dir)) {
        std::cout << "json5 compat: testdata directory is empty or missing, skipping.\n";
        std::cout << "\ncompat tests: 0 passed, 0 failed (testdata not available)\n";
        return 0;
    }

    std::cout << "json5 compat: scanning " << testdata_dir << "\n\n";

    Stats stats;

    // Collect all files sorted for deterministic output
    std::vector<fs::path> files;
    for (auto& entry : fs::recursive_directory_iterator(testdata_dir)) {
        if (!entry.is_regular_file()) continue;

        // Skip the "todo" subdirectory (unimplemented features)
        auto rel = fs::relative(entry.path(), testdata_dir);
        if (rel.string().find("todo") == 0) {
            ++stats.skipped;
            continue;
        }

        files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());

    for (auto& f : files) {
        auto rel = fs::relative(f, testdata_dir);
        run_one(rel, f, stats);
    }

    // ── Summary ──────────────────────────────────────────────────
    std::cout << "\n";
    if (!stats.failures.empty()) {
        std::cout << "── Failures ────────────────────────────────────────────\n";
        for (auto& f : stats.failures) {
            std::cout << "  " << f.file
                      << "  [expected " << f.expected << "]"
                      << "  " << f.detail << "\n";
        }
        std::cout << "\n";
    }

    std::cout << "compat tests: "
              << stats.passed << " passed, "
              << stats.failed << " failed, "
              << stats.skipped << " skipped"
              << " (total cases: " << stats.total << ")\n";

    return stats.failed > 0 ? 1 : 0;
}
