find_package(cpplibs)
add_executable(server main.cpp server.h flow.cpp server.cpp)
target_link_libraries(server coro ev)
