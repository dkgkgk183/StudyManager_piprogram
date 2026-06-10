# 탁상 AI 스터디 매니저 (Raspberry Pi)

> 라즈베리파이 + LVGL 기반 데스크탑 학습 보조 장치. NFC 태그로 기기를 인식하고, 무게 센서와 모터로 스마트폰 보관함을 제어하며, Supabase 클라우드와 연동하여 학습 데이터를 관리합니다.

---

## 주요 기능

- **NFC 기기 인식**: PN532 모듈로 NFC 태그를 읽어 기기 번호 자동 매칭/등록
- **무게 센서**: HX711로 스마트폰 올림/내림 감지 (학습 모드 전환)
- **DC 모터 제어**: 스마트폰 보관함 서랍 자동 잠금/해제
- **Supabase 클라우드 동기화**: 학습 세션 데이터를 클라우드에 업로드하고 스마트폰 앱과 연동
- **체크리스트 표시**: 오늘의 학습 체크리스트를 디스플레이에 표시
- **학습 기록 관리**: 로컬에 세션 저장, 미전송 세션 관리

---

## 화면 흐름

```
대기 화면 (STUDY MANAGER)
    ↓ NFC 태그 접촉
기기 선택/등록 화면
    ↓ 등록 완료
과목 선택 화면 (beforestudy)
    ↓ 과목 선택 + 공부 시작
학습 진행 화면 (studying)
    ↓ 공부 종료
완료 화면 (done)
    ↓ 기록 저장
대기 화면으로 복귀
```

---

## 지원 하드웨어

| 부품 | 역할 |
|------|------|
| Raspberry Pi | 메인 보드 |
| ILI9341 (SPI) | 240x320 TFT 디스플레이 |
| XPT2046 | 저항식 터치스크린 |
| PN532 | NFC/RFID 리더 |
| HX711 | 무게 센서 (스마트폰 감지) |
| DC 모터 + 드라이버 | 보관함 서랍 제어 |

---

## 프로젝트 구조

```
lvgl_project/
├── main.c                    # 메인 엔트리포인트 (RPi 하드웨어 초기화)
├── CMakeLists.txt            # 빌드 설정
├── lv_conf.h                 # LVGL 설정 (v9.1.0, RGB565)
├── ili9341.c/h               # ILI9341 디스플레이 드라이버
├── ui/                       # 화면 (UI) 소스
│   ├── ui_screen_idle.c/h        # 대기 화면
│   ├── ui_screen_intro.c/h       # NFC 매칭 화면
│   ├── ui_screen_device_select.c/h  # 기기 선택/등록
│   ├── ui_screen_study_plan.c/h  # 학습 계획
│   ├── ui_screen_beforestudy.c/h # 과목 선택 (공부 시작 전)
│   ├── ui_screen_studying.c/h    # 학습 진행
│   ├── ui_screen_done.c/h        # 학습 완료
│   ├── ui_screen_records.c/h     # 기록 조회
│   ├── ui_screen_calibrate.c/h   # 무게 센서 캘리브레이션
│   ├── ui_screen_nfc_test.c/h    # NFC 테스트
│   ├── ui_screen_weight_test.c/h # 무게 테스트
│   └── font_korean_*.c           # 한글 폰트 파일
├── app/                      # 애플리케이션 로직
│   ├── app_nfc.c/h              # NFC 제어 (PN532)
│   ├── app_touch.c/h            # 터치 입력 (XPT2046)
│   ├── app_hx711.c/h            # 무게 센서 (HX711)
│   ├── app_motor.c/h            # DC 모터 제어
│   ├── app_sessions.c/h         # 세션 로컬 저장 관리
│   └── app_supabase.c/h         # Supabase 클라우드 API
├── lvgl/                     # LVGL 라이브러리 (submodule)
└── tools/                    # 유틸리티 스크립트
```

---

## Supabase 클라우드 동기화

Raspberry Pi에서 학습 세션 종료 시 Supabase에 자동 업로드됩니다.

| 테이블 | 용도 |
|--------|------|
| `categories` | 과목 카테고리 |
| `subjects` | 과목 정보 |
| `study_sessions` | 학습 세션 기록 |
| `checklist_items` | 오늘의 체크리스트 |
| `device_registrations` | NFC 기기 등록 정보 |

- **기기 격리**: `user_id` (3자리 기기 번호) 기반 RLS 정책으로 데이터 격리

---

## 스마트폰 앱 연동

이 프로젝트는 **Flutter 기반 스마트폰 앱**과 함께 사용됩니다. 스마트폰 앱은 Supabase를 통해 라즈베리파이와 동일한 데이터를 관리합니다.

주요 기능:
- AI 플래너 (OpenRouter API 기반 할 일 정리)
- 체크리스트 관리 및 공부 모드 (스마트폰 뒤집기 감지)
- 학습 통계 시각화 (도넛 차트, 집중 점수)
- Supabase 클라우드 수동 동기화

---

## 기술 스택

| 구분 | 기술 |
|------|------|
| UI 프레임워크 | LVGL v9.1.0 |
| 언어 | C99 |
| 빌드 시스템 | CMake |
| 디스플레이 | ILI9341 (SPI, RGB565) |
| 터치 | XPT2046 (저항식) |
| NFC | PN532 (I2C/SPI) |
| 무게 센서 | HX711 |
| 클라우드 | Supabase (REST API, Realtime) |
| HTTP | libcurl |
| JSON | libcjson |
| PC 데모 | SDL2 |
