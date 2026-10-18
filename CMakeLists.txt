cmake_minimum_required(VERSION 3.5)
project(psp-desk-clock C)

# Build with the PSP toolchain: tools/dev.sh make eboot
add_executable(psp-desk-clock
    src/psp/main.c
    src/core/bitmap_font.c
    src/core/calendar.c
    src/core/clock_app.c
    src/core/clock_face.c
    src/core/date_line.c
    src/core/month_calendar.c
    src/core/settings.c
    src/core/time_display.c
)
target_include_directories(psp-desk-clock PRIVATE src/core)
target_compile_options(psp-desk-clock PRIVATE -Wall -Wextra)
target_link_libraries(psp-desk-clock PRIVATE pspctrl pspdisplay pspge psppower psprtc)

create_pbp_file(
    TARGET psp-desk-clock
    TITLE "PSP Desk Clock"
)
