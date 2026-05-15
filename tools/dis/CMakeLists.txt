file(GLOB_RECURSE DISASM_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_executable(disasm ${DISASM_SOURCES})

target_link_libraries(disasm PRIVATE cbc_lib)
