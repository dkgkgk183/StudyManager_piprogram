cmake_minimum_required(VERSION 3.10)
project(lvgl_demo C)
set(CMAKE_C_STANDARD 99)

set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)

add_subdirectory(lvgl)

find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(CJSON REQUIRED libcjson)

# UI/공통 소스 (하드웨어 비종속)
set(UI_COMMON_SOURCES
    ui/ui_screen_intro.c
    ui/ui_screen_device_select.c
    ui/ui_screen_idle.c
    ui/ui_screen_study_plan.c
    ui/ui_screen_beforestudy.c
    ui/ui_screen_calibrate.c
    ui/ui_screen_studying.c
    ui/ui_screen_records.c
    ui/ui_screen_done.c
    ui/ui_screen_nfc_test.c
    ui/ui_screen_weight_test.c
    ui/font_korean_16.c
    ui/font_korean_20.c
    ui/font_korean_20_bold.c
    ui/lv_font_montserrat_32.c
    app/app_supabase.c
    app/app_sessions.c
)

# ────────────────────────────────────────────────────────────
# 타겟 1) lvgl_app : 라즈베리파이 + 실제 하드웨어 (ILI9341 SPI,
#                   XPT2046 터치, PN532 NFC, HX711, DC 모터)
#   빌드:   cmake .. && make lvgl_app
#   실행:   sudo ./lvgl_app
# ────────────────────────────────────────────────────────────
add_executable(lvgl_app
    main.c
    ili9341.c
    ${UI_COMMON_SOURCES}
    app/app_nfc.c
    app/app_touch.c
    app/app_hx711.c
    app/app_motor.c
)

target_include_directories(lvgl_app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
)

target_link_libraries(lvgl_app PRIVATE
    lvgl gpiod m pthread
    ${CURL_LIBRARIES}
    ${CJSON_LIBRARIES}
)

# ────────────────────────────────────────────────────────────
# 타겟 2) demo : PC/SDL2 데모 (하드웨어 불필요)
#   빌드:   cmake .. && make demo
#   실행:   ./demo
#
#   라즈베리파이/ILI9341 없이 PC 데스크탑에서 동일 UI를 띄워
#   개발/스크린샷/플로우 검증 용도로 사용한다. NFC/HX711/모터/
#   터치는 모두 *_stub.c 의 no-op 으로 대체된다.
# ────────────────────────────────────────────────────────────
find_package(SDL2 REQUIRED)

add_executable(demo
    demo_pc.c
    ${UI_COMMON_SOURCES}
    app/app_nfc_stub.c
    app/app_touch_stub.c
    app/app_hx711_stub.c
    app/app_motor_stub.c
)

target_include_directories(demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
)

target_link_libraries(demo PRIVATE
    lvgl m pthread
    SDL2::SDL2
    ${CURL_LIBRARIES}
    ${CJSON_LIBRARIES}
)

# ────────────────────────────────────────────────────────────
# 타겟 3) demo_weight : PC/SDL2 무게 센서 단독 테스트
#   빌드:   cd build && make demo_weight
#   실행:   ./demo_weight
# ────────────────────────────────────────────────────────────
add_executable(demo_weight
    demo_weight.c
    ${UI_COMMON_SOURCES}
    app/app_nfc_stub.c
    app/app_touch_stub.c
    app/app_hx711_stub.c
    app/app_motor_stub.c
)

target_include_directories(demo_weight PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${CJSON_INCLUDE_DIRS}
)

target_link_libraries(demo_weight PRIVATE
    lvgl m pthread
    SDL2::SDL2
    ${CURL_LIBRARIES}
    ${CJSON_LIBRARIES}
)
