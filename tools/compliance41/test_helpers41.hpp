#pragma once

#include "nfs4/nfs4_error.hpp"
#include "runner41.hpp"

#include <stdexcept>
#include <string>

// Assert that `expr` evaluates to true; throw ComplianceFailure41 otherwise.
#define CHECK41(expr) \
    do { \
        if (!(expr)) { \
            throw compliance41::ComplianceFailure41( \
                "CHECK41 failed: " #expr " at " __FILE__ ":" + std::to_string(__LINE__)); \
        } \
    } while (0)

// Assert that `expr` throws Nfs4Error with the given Nfsstat4 `code`.
// If no exception is thrown, or a different NFS status is returned, the test FAILs.
// If a non-Nfs4Error exception propagates, the runner records SKIP.
#define EXPECT_NFS41_ERR(expr, code) \
    do { \
        try { \
            (expr); \
            throw compliance41::ComplianceFailure41( \
                "EXPECT_NFS41_ERR(" #code "): no exception thrown"); \
        } catch (const Nfs4Error& _nfs4_e) { \
            if (!_nfs4_e.is(code)) { \
                throw compliance41::ComplianceFailure41( \
                    std::string("EXPECT_NFS41_ERR(" #code "): got nfsstat4=") \
                    + std::to_string(_nfs4_e.status)); \
            } \
        } \
    } while (0)
