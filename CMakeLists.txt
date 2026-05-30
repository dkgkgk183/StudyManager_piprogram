cmake_minimum_required(VERSION 3.10)
project(lvgl_demo C)
set(CMAKE_C_STANDARD 99)

set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)

add_subdirectory(lvgl)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(CJSON REQUIRED libcjson)

add_executable(demo
    main.c
    ili9341.c
    ui/ui_screen_intro.c
    ui/ui_screen_device_select.c
    ui/ui_screen_idle.c
    ui/ui_screen_study_plan.c
    ui/ui_screen_beforestudy.c
    ui/ui_screen_calibrate.c
    ui/font_korean_16.c
    ui/font_korean_20.c
    ui/lv_font_montserrat_32.c
    app/app_nfc.c
    app/app_touch.c
    app/app_hx711.c
    app/app_supabase.c
    app/app_motor.c
)

target_include_directories(demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
)

target_link_libraries(demo PRIVATE
    lvgl gpiod m pthread
    ${CURL_LIBRARIES}
    ${CJSON_LIBRARIES}
)
