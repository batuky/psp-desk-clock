cmake_minimum_required(VERSION 3.12)
project(psp-desk-clock C)

# Build with the PSP toolchain: tools/dev.sh make eboot

# Same wildcard as the Makefile, so a new core file needs no edit here.
# CONFIGURE_DEPENDS re-globs on every build and picks up added or removed files.
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS src/core/*.c)

add_executable(psp-desk-clock
    src/psp/main.c
    ${CORE_SOURCES}
)
target_include_directories(psp-desk-clock PRIVATE src/core)
target_compile_options(psp-desk-clock PRIVATE -Wall -Wextra)
# The pure core gets the warning flags core-psp-check uses (minus -std=c11).
# main.c talks to the PSP SDK, so it only gets the warnings above.
set_source_files_properties(${CORE_SOURCES} PROPERTIES COMPILE_OPTIONS "-Werror;-pedantic")
target_link_libraries(psp-desk-clock PRIVATE pspctrl pspdisplay pspge psppower psprtc)

create_pbp_file(
    TARGET psp-desk-clock
    TITLE "PSP Desk Clock"
)
