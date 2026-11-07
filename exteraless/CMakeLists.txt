find_package(PkgConfig QUIET)
set(SHERPA_ONNX_ROOT "" CACHE PATH
    "sherpa-onnx prefix, used when pkg-config cannot locate sherpa-onnx")

if (PkgConfig_FOUND)
    pkg_check_modules(SHERPA_ONNX QUIET sherpa-onnx)
endif()

if (SHERPA_ONNX_FOUND)
    set(exteraless_sherpa_include_dirs ${SHERPA_ONNX_INCLUDE_DIRS})
    set(exteraless_sherpa_libraries ${SHERPA_ONNX_LINK_LIBRARIES})
elseif (SHERPA_ONNX_ROOT)
    find_library(SHERPA_ONNX_C_API_LIBRARY
        NAMES sherpa-onnx-c-api
        HINTS "${SHERPA_ONNX_ROOT}/lib"
        REQUIRED)
    set(exteraless_sherpa_include_dirs "${SHERPA_ONNX_ROOT}/include")
    set(exteraless_sherpa_libraries "${SHERPA_ONNX_C_API_LIBRARY}")
else()
    message(FATAL_ERROR
        "sherpa-onnx not found: install its pkg-config file or pass "
        "-DSHERPA_ONNX_ROOT=<prefix>")
endif()

add_library(exteraless STATIC
    ai_client.cpp
    ai_client.h
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
    extra_pills.cpp
    extra_pills.h
    format.cpp
    format.h
    ghost_schedule.cpp
    ghost_schedule.h
    ghost_status.cpp
    ghost_status.h
    links.cpp
    links.h
    lrclib.cpp
    lrclib.h
    lyrics.cpp
    lyrics.h
    math.cpp
    math.h
    message_menu.h
    network.cpp
    network.h
    pangu.cpp
    pangu.h
    plugins/installer.cpp
    plugins/installer.h
    plugins/scanner.cpp
    proxy_rules_state.cpp
    proxy_rules_state.h
    plugins/scanner.h
    regex_filters.cpp
    regex_filters.h
    save_path.cpp
    save_path.h
    stt.cpp
    stt.h
    subtitles.cpp
    subtitles.h
    update_check.cpp
    update_check.h
    zalgo.cpp
    zalgo.h
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

target_include_directories(exteraless
PRIVATE
    ${exteraless_sherpa_include_dirs}
)

target_link_libraries(exteraless
PRIVATE
    ${exteraless_sherpa_libraries}
)

add_subdirectory(tests/plugins)
add_subdirectory(tests/chats)
