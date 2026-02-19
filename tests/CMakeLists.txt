find_package(GTest REQUIRED)

add_executable(nfsclient_tests
    test_xdr.cpp
    test_rpc.cpp
    test_nfs_ops.cpp
)

target_link_libraries(nfsclient_tests
    PRIVATE
    nfsclient_lib
    GTest::gtest_main
)

target_include_directories(nfsclient_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

include(GoogleTest)
gtest_discover_tests(nfsclient_tests)
