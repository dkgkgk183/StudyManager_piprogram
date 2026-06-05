#ifndef APP_SESSIONS_H
#define APP_SESSIONS_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define MAX_LOCAL_SESSIONS 100

typedef struct {
    char id[40];
    char subject_name[64];
    char subject_id[40];
    char color_hex[8];      /* "#FF5722" 형태 */
    char start_time[24];   /* "YYYY-MM-DDTHH:MM:SS" */
    char end_time[24];
    int  duration_seconds;
    bool pushed;           /* Supabase 전송 완료 여부 */
} local_session_t;

/* 초기화: 파일에서 로드 */
void sessions_init(void);

/* 세션 저장 (파일에 즉시 기록) */
bool sessions_save(const local_session_t *session);

/* 전체 세션 로드 (리턴값: 세션 수) */
int sessions_load_all(local_session_t *out, int max_count);

/* 전송 안 된 세션 수 */
int sessions_get_unpushed_count(void);

/* 전체 세션 수 */
int sessions_get_count(void);

/* 전송 완료 표시 (전체 마크) */
void sessions_mark_all_pushed(void);

/* 세션 삭제 (인덱스 기반) */
bool sessions_delete(int index);

/* index 위치의 세션을 pushed=true 로 마크, 파일 저장 */
bool sessions_mark_pushed(int index);

/* cutoff 이전에 끝난 세션 삭제 (end_time < cutoff). 삭제된 개수 반환 */
int sessions_prune_before(time_t cutoff);

#endif
