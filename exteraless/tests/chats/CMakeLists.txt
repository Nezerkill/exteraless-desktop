add_executable(exteraless-chats-cli EXCLUDE_FROM_ALL chats_cli.cpp)

target_link_libraries(exteraless-chats-cli
PRIVATE
    exteraless
)
