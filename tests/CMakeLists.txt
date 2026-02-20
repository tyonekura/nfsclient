find_package(GTest REQUIRED)

add_executable(nfsclient_tests
    test_xdr.cpp
    test_rpc.cpp
    test_nfs_ops.cpp
    test_nfs_phase1.cpp
    test_nfs_p2a.cpp
)

target_link_libraries(nfsclient_tests
    PRIVATE
    nfsclient_lib
    GTest::gtest_main
)

target_include_directories(nfsclient_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

include(GoogleTest)
gtest_discover_tests(nfsclient_tests)

# Integration tests — require a live NFS server; not included in the default test run.
add_executable(nfsclient_integration_tests
    test_integration.cpp
)

target_link_libraries(nfsclient_integration_tests
    PRIVATE
    nfsclient_lib
    GTest::gtest_main
)

target_include_directories(nfsclient_integration_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
