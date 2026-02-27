add_executable(nfsclient_compliance41
    main.cpp
    runner41.cpp
    test_session41.cpp
    test_basic41.cpp
    test_attrs41.cpp
    test_stateid41.cpp
    test_rename41.cpp
)

target_include_directories(nfsclient_compliance41
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(nfsclient_compliance41
    PRIVATE
    nfsclient_nfs41_facade
    nfsclient_nfs4_lib
    nfsclient_lib
)
