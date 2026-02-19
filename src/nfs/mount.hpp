#pragma once

#include "nfs3_types.hpp"

#include <string>

namespace nfs3 {

// Connect to the MOUNT daemon on `host`, send MNT3 for `export_path`, and
// return the root file handle for that export.
Fh3 mnt(const std::string& host, const std::string& export_path);

}  // namespace nfs3
