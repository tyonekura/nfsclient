#pragma once

#include "nfs/nfs_error.hpp"

#include <stdexcept>
#include <string>

namespace compliance {

// Thrown by CHECK / EXPECT_NFS_ERR macros when an assertion fails.
// The runner catches this and records the test as FAIL.
struct ComplianceFailure : std::runtime_error {
    using std::runtime_error::runtime_error;
};

}  // namespace compliance

// Assert that `expr` evaluates to true; throw ComplianceFailure otherwise.
#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            throw compliance::ComplianceFailure( \
                "CHECK failed: " #expr " at " __FILE__ ":" + std::to_string(__LINE__)); \
        } \
    } while (0)

// Assert that `expr` throws NfsError with the given Nfsstat3 `code`.
// If no exception is thrown, or a different NFS status is returned, the test FAILs.
// If a non-NfsError exception propagates, the runner records SKIP (infrastructure error).
#define EXPECT_NFS_ERR(expr, code) \
    do { \
        try { \
            (expr); \
            throw compliance::ComplianceFailure( \
                "EXPECT_NFS_ERR(" #code "): no exception thrown"); \
        } catch (const NfsError& _nfs_e) { \
            if (!_nfs_e.is(code)) { \
                throw compliance::ComplianceFailure( \
                    std::string("EXPECT_NFS_ERR(" #code "): got nfsstat3=") \
                    + std::to_string(_nfs_e.status)); \
            } \
        } \
    } while (0)
