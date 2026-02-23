add_executable(nfsclient_bench
    main.cpp
    workload_seqread.cpp
    workload_seqwrite.cpp
    workload_randread.cpp
    workload_randwrite.cpp
    workload_meta.cpp
    workload_mixed.cpp
)

target_include_directories(nfsclient_bench
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(nfsclient_bench
    PRIVATE
    nfsclient_lib
)
