add_library(exteraless STATIC
    backup.cpp
    backup.h
    chats.cpp
    chats.h
    config.cpp
    config.h
    deleted_entry.cpp
    deleted_entry.h
    doubletap.cpp
    doubletap.h
    exteraless.cpp
    exteraless.h
    format.cpp
    format.h
    ghost_status.cpp
    ghost_status.h
    links.cpp
    links.h
    message_menu.h
    network.cpp
    network.h
    plugins/scanner.cpp
    proxy_rules_state.cpp
    proxy_rules_state.h
    plugins/scanner.h
)

target_link_libraries(exteraless
PUBLIC
    desktop-app::common_options
    Qt::Core
    Qt::Network
)

target_include_directories(exteraless
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

add_subdirectory(tests/plugins)
add_subdirectory(tests/chats)
