add_library(nfsclient_lib STATIC
    xdr/xdr.cpp
    rpc/rpc_client.cpp
    nfs/portmap.cpp
    nfs/mount.cpp
    nfs/getattr.cpp
    nfs/lookup.cpp
    nfs/read.cpp
    nfs/write.cpp
    nfs/create.cpp
    nfs/dirop.cpp
    nfs/setattr.cpp
    nfs/readdir.cpp
    nfs_client.cpp
)

target_include_directories(nfsclient_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(nfsclient main.cpp)
target_link_libraries(nfsclient PRIVATE nfsclient_lib)
