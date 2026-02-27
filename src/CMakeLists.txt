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
    nfs/commit.cpp
    nfs/rename.cpp
    nfs/access.cpp
    nfs/fsinfo.cpp
    nfs/readdirplus.cpp
    nfs/symlink.cpp
    nfs/mknod.cpp
    nfs_client.cpp
)

target_include_directories(nfsclient_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(nfs4)

add_library(nfsclient_nfs4_facade STATIC nfs4_client.cpp)
target_include_directories(nfsclient_nfs4_facade PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nfsclient_nfs4_facade PUBLIC nfsclient_nfs4_lib)

add_library(nfsclient_nfs41_facade STATIC nfs41_client.cpp)
target_include_directories(nfsclient_nfs41_facade PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nfsclient_nfs41_facade PUBLIC nfsclient_nfs4_lib)

add_executable(nfsclient main.cpp)
target_link_libraries(nfsclient PRIVATE nfsclient_lib)
