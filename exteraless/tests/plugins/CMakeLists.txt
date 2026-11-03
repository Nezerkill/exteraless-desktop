add_executable(exteraless-plugins-scan EXCLUDE_FROM_ALL scan_cli.cpp)

target_link_libraries(exteraless-plugins-scan
PRIVATE
    exteraless
)
