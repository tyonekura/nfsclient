add_library(nfsclient_nfs4_lib STATIC
    nfs4_attr.cpp
    compound.cpp
    fh_ops.cpp
    setclientid.cpp
    lookup.cpp
    getattr.cpp
    access.cpp
    open.cpp
    read.cpp
    write.cpp
    commit.cpp
    dirop.cpp
    setattr.cpp
    create.cpp
    readdir.cpp
    readlink.cpp
)

target_include_directories(nfsclient_nfs4_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(nfsclient_nfs4_lib PUBLIC nfsclient_lib)
