#pragma once

#include <cstdint>
#include <string>

namespace nfs3 {

// Query the RPCBIND (portmap) daemon at port 111 for the TCP port of the
// given (prog, vers) pair. Returns 0 if the program is not registered.
uint16_t getport(const std::string& host, uint32_t prog, uint32_t vers);

}  // namespace nfs3
