add_executable(nfsclient_compliance
    main.cpp
    runner.cpp
    test_basic.cpp
    test_wcc.cpp
    test_commit.cpp
    test_attributes.cpp
    test_permissions.cpp
    test_stale.cpp
    test_edge_cases.cpp
)

target_include_directories(nfsclient_compliance
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(nfsclient_compliance
    PRIVATE
    nfsclient_lib
)
