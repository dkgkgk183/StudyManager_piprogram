cmake_minimum_required(VERSION 3.10)
project(lvgl_demo C)
set(CMAKE_C_STANDARD 99)

set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)

add_subdirectory(lvgl)

add_executable(demo
    main.c
    ili9341.c
    ui/ui_tab_nfc.c
    ui/ui_tab_weight.c
    ui/ui_tab_camera.c
    ui/font_korean_16.c
    ui/lv_font_montserrat_32.c
    app/app_nfc.c
    app/app_touch.c
    app/app_hx711.c
)

target_link_libraries(demo PRIVATE lvgl gpiod m pthread)
target_include_directories(demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})