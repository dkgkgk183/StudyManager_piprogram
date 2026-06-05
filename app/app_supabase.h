#ifndef APP_SUPABASE_H
#define APP_SUPABASE_H

#include <stdbool.h>
#include "app_sessions.h"

// ── 디바이스 관리 ──────────────────────────────────────────

bool supabase_find_device(const char *nfc_id, char *device_number_out, int buf_size);
bool supabase_get_all_devices(char devices[][32], int max_count, int *count_out);
bool supabase_register_device(const char *device_number, const char *nfc_id);

// ── 체크리스트 (과목별) ────────────────────────────────────

#define MAX_CHECKLIST_ITEMS_PER_SUBJECT  16
#define MAX_CHECKLIST_SUBJECTS           16

typedef struct {
    char id[40];
    char text[128];
    bool is_checked;
} checklist_item_t;

typedef struct {
    char subject_id[40];
    char subject_name[64];
    char color_hex[8];
    int  item_count;
    int  checked_count;
    checklist_item_t items[MAX_CHECKLIST_ITEMS_PER_SUBJECT];
} checklist_subject_group_t;

/* 오늘 체크리스트 가져오기 (06:00 ~ 익일 05:59 KST, 과목별 그룹핑) */
bool supabase_get_today_checklists(checklist_subject_group_t *group_out,
                                    int max_groups, int *group_count_out);

/* 체크리스트 항목 토글 (PATCH) */
bool supabase_toggle_checklist_item(const char *item_id, bool new_checked);

// ── 공부 세션 업로드 ───────────────────────────────────────

/* user_id (기기 식별자) 등록/조회.
 *  - intro 화면에서 NFC 매칭 성공 시, 또는 device_select 에서 등록 성공 시
 *    set_user_id() 로 기기번호를 등록해두면 이후 업로드에서 user_id 로 사용. */
void     supabase_set_user_id(const char *user_id);
const char *supabase_get_user_id(void);

/* user_id 전환 시 호출: 캐시된 subjects 등 로컬 캐시 무효화 */
void     supabase_clear_local_caches(void);

/* 단일 세션을 study_sessions 테이블에 POST.
 *  - 테이블: study_sessions (supabaseinfo.md 기준)
 *  - 전송 필드:
 *      id                (TEXT PK, 로컬 time_t 문자열)
 *      user_id           (TEXT NOT NULL, supabase_get_user_id() 또는 "default")
 *      subject_id        (TEXT NOT NULL)
 *      start_time        (TIMESTAMP, 로컬 시간 그대로)
 *      end_time          (TIMESTAMP, 로컬 시간 그대로)
 *      duration_seconds  (INT NOT NULL)
 *      self_score        (INT, 0)
 *      penalty_count     (INT, 0)
 *      tray_open_count   (INT, 0)
 *  - 2xx 응답 시 true 반환. 호출자는 sessions_mark_pushed() 로 로컬 마킹. */
bool supabase_upload_session(const local_session_t *session);

#endif
