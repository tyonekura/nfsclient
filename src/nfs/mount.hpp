#pragma once

#include "nfs3_types.hpp"

#include <string>
#include <vector>

namespace nfs3 {

// An entry from the server's export list.
struct ExportEntry {
    std::string              path;    // exported directory path
    std::vector<std::string> groups;  // allowed netgroups/hostnames; empty = world-accessible
};

// MOUNTPROC3_MNT (proc 1): mount an export and return the root file handle.
Fh3 mnt(const std::string& host, const std::string& export_path);

// MOUNTPROC3_UMNT (proc 3): notify the server of an unmount.
// This is advisory — the server may ignore it, but it is good practice.
void umnt(const std::string& host, const std::string& export_path);

// MOUNTPROC3_EXPORT (proc 5): retrieve the server's export list.
std::vector<ExportEntry> export_list(const std::string& host);

}  // namespace nfs3
