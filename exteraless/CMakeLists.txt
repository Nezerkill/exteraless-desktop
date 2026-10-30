add_library(exteraless STATIC
    backup.cpp
    backup.h
    config.cpp
    config.h
    exteraless.cpp
    exteraless.h
    network.cpp
    network.h
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
