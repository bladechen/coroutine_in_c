set_property(SOURCE arch_ctx.S PROPERTY LANGUAGE C)
add_library(coro arch_ctx.S  coro.c list.c)
target_include_directories(coro PUBLIC ./)
