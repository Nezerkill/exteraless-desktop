add_executable(exteraless-plugins-scan EXCLUDE_FROM_ALL scan_cli.cpp)

target_link_libraries(exteraless-plugins-scan
PRIVATE
    exteraless
)

add_executable(exteraless-plugin-host-cli EXCLUDE_FROM_ALL
    plugin_host_cli.cpp
    ${CMAKE_SOURCE_DIR}/Telegram/SourceFiles/exteraless/plugin_runtime.cpp
    ${CMAKE_SOURCE_DIR}/Telegram/SourceFiles/exteraless/plugin_runtime.h
)

target_include_directories(exteraless-plugin-host-cli
PRIVATE
    ${CMAKE_SOURCE_DIR}/Telegram/SourceFiles
)

target_link_libraries(exteraless-plugin-host-cli
PRIVATE
    exteraless
)
