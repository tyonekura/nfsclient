add_executable(nfsclient_compliance4
    main.cpp
    runner4.cpp
    test_basic4.cpp
    test_attrs4.cpp
    test_stateid4.cpp
    test_stale4.cpp
    test_rename4.cpp
)

target_include_directories(nfsclient_compliance4
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(nfsclient_compliance4
    PRIVATE
    nfsclient_nfs4_facade
    nfsclient_nfs4_lib
    nfsclient_lib
)
