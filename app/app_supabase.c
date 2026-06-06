#define _GNU_SOURCE
#include "app_supabase.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>

#define SUPABASE_URL "https://wowjdjvjhpnbirptpffb.supabase.co"
#define SUPABASE_KEY "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Indvd2pkanZqaHBuYmlycHRwZmZiIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzYwMTM3NDEsImV4cCI6MjA5MTU4OTc0MX0.FMbj1kB9skHM7tMtMlpdoWHvAcI-AFEaTfiZHFwGo9Q"

// curl 응답 버퍼
struct curl_buf {
    char *data;
    size_t size;
};

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    struct curl_buf *buf = (struct curl_buf *)userdata;
    char *tmp = realloc(buf->data, buf->size + total + 1);
    if (!tmp) return 0;
    buf->data = tmp;
    memcpy(buf->data + buf->size, ptr, total);
    buf->size += total;
    buf->data[buf->size] = '\0';
    return total;
}

// URL 인코딩 (공백 등 특수문자 변환)
static char *url_encode_str(CURL *curl, const char *str) {
    return curl_easy_escape(curl, str, 0);
}

// Supabase 요청 헤더 설정
static struct curl_slist *make_headers(void) {
    struct curl_slist *headers = NULL;
    char apikey_hdr[512];
    char auth_hdr[512];
    snprintf(apikey_hdr, sizeof(apikey_hdr), "apikey: %s", SUPABASE_KEY);
    snprintf(auth_hdr, sizeof(auth_hdr), "Authorization: Bearer %s", SUPABASE_KEY);
    headers = curl_slist_append(headers, apikey_hdr);
    headers = curl_slist_append(headers, auth_hdr);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    return headers;
}

// GET 요청 수행, 응답 문자열 반환 (호출자가 free)
static bool do_get(const char *url, char **response_out) {
    CURL *curl = curl_easy_init();
    if (!curl) return false;

    struct curl_buf buf = {0};
    struct curl_slist *headers = make_headers();

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(buf.data);
        fprintf(stderr, "Supabase GET 실패: %s\n", curl_easy_strerror(res));
        return false;
    }
    *response_out = buf.data;
    return true;
}

// POST 요청 수행
static bool do_post(const char *url, const char *json_body, char **response_out) {
    CURL *curl = curl_easy_init();
    if (!curl) return false;

    struct curl_buf buf = {0};
    struct curl_slist *headers = make_headers();
    // POST 전용 헤더 추가
    headers = curl_slist_append(headers, "Prefer: return=minimal");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(buf.data);
        fprintf(stderr, "Supabase POST 실패: %s\n", curl_easy_strerror(res));
        return false;
    }
    if (http_code >= 400) {
        fprintf(stderr, "Supabase POST HTTP %ld: %s\n", http_code, buf.data ? buf.data : "");
        free(buf.data);
        return false;
    }
    if (response_out) {
        *response_out = buf.data;
    } else {
        free(buf.data);
    }
    return true;
}

bool supabase_find_device(const char *nfc_id, char *device_number_out, int buf_size) {
    CURL *curl = curl_easy_init();
    if (!curl) return false;

    char *encoded = url_encode_str(curl, nfc_id);
    if (!encoded) { curl_easy_cleanup(curl); return false; }

    char url[512];
    snprintf(url, sizeof(url),
        "%s/rest/v1/device_registrations?nfc_id=eq.%s&select=device_number",
        SUPABASE_URL, encoded);
    curl_free(encoded);
    curl_easy_cleanup(curl);

    char *resp = NULL;
    if (!do_get(url, &resp)) return false;

    // 응답: [{"device_number":"1"}] 또는 []
    cJSON *arr = cJSON_Parse(resp);
    free(resp);
    if (!arr || !cJSON_IsArray(arr)) { cJSON_Delete(arr); return false; }

    int count = cJSON_GetArraySize(arr);
    if (count == 0) { cJSON_Delete(arr); return false; }

    cJSON *item = cJSON_GetArrayItem(arr, 0);
    cJSON *dn = cJSON_GetObjectItem(item, "device_number");
    if (dn && cJSON_IsString(dn)) {
        strncpy(device_number_out, dn->valuestring, buf_size - 1);
        device_number_out[buf_size - 1] = '\0';
        cJSON_Delete(arr);
        return true;
    }
    cJSON_Delete(arr);
    return false;
}

bool supabase_get_all_devices(char devices[][32], int max_count, int *count_out) {
    char url[512];
    snprintf(url, sizeof(url),
        "%s/rest/v1/device_registrations?select=device_number&order=device_number.asc",
        SUPABASE_URL);

    char *resp = NULL;
    if (!do_get(url, &resp)) return false;

    cJSON *arr = cJSON_Parse(resp);
    free(resp);
    if (!arr || !cJSON_IsArray(arr)) { cJSON_Delete(arr); return false; }

    int count = cJSON_GetArraySize(arr);
    if (count > max_count) count = max_count;

    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        cJSON *dn = cJSON_GetObjectItem(item, "device_number");
        if (dn && cJSON_IsString(dn)) {
            strncpy(devices[i], dn->valuestring, 31);
            devices[i][31] = '\0';
        }
    }
    *count_out = count;
    cJSON_Delete(arr);
    return true;
}

// PATCH 요청 수행 (기존 레코드 업데이트)
static bool do_patch(const char *url, const char *json_body) {
    CURL *curl = curl_easy_init();
    if (!curl) return false;

    struct curl_buf buf = {0};
    struct curl_slist *headers = make_headers();

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PATCH");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        fprintf(stderr, "Supabase PATCH 실패: %s\n", curl_easy_strerror(res));
        free(buf.data);
        return false;
    }
    if (http_code >= 400) {
        fprintf(stderr, "Supabase PATCH HTTP %ld: %s\n", http_code,
                buf.data && buf.data[0] ? buf.data : "(empty)");
        free(buf.data);
        return false;
    }
    free(buf.data);
    return true;
}

bool supabase_register_device(const char *device_number, const char *nfc_id) {
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "nfc_id", nfc_id);
    char *body = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);

    // PATCH로 먼저 시도 (기존 기기의 nfc_id 업데이트)
    CURL *curl = curl_easy_init();
    char *encoded = url_encode_str(curl, device_number);
    char url[512];
    snprintf(url, sizeof(url),
        "%s/rest/v1/device_registrations?device_number=eq.%s",
        SUPABASE_URL, encoded);
    curl_free(encoded);
    curl_easy_cleanup(curl);

    bool ok = do_patch(url, body);
    if (!ok) {
        // PATCH 실패 → 새 레코드 INSERT
        snprintf(url, sizeof(url), "%s/rest/v1/device_registrations", SUPABASE_URL);
        ok = do_post(url, body, NULL);
    }
    free(body);
    return ok;
}

/* ── subject_id 배열 관리 ── */
typedef struct {
    char id[40];
    char name[64];
    char color_hex[8];
} subject_info_t;

static subject_info_t g_subjects[32];
static int g_subject_count;

/* 기기 식별자(= device_number, DB의 user_id 컬럼 값).
 * intro 매칭/등록 시 set_user_id() 로 채워짐. 비어있으면 "default" 폴백.
 * forward declaration of function definition below. */
static char g_user_id[32] = {0};
static inline const char *current_user_id_or_default(void) {
    return g_user_id[0] ? g_user_id : "default";
}

static bool load_subjects(void) {
    char url[512];
    /* 현재 로그인된 user_id(=device_number) 의 subjects 만 로드 */
    snprintf(url, sizeof(url),
        "%s/rest/v1/subjects?select=id,name,color_hex&user_id=eq.%s",
        SUPABASE_URL, current_user_id_or_default());

    char *resp = NULL;
    if (!do_get(url, &resp)) return false;

    cJSON *arr = cJSON_Parse(resp);
    free(resp);
    if (!arr || !cJSON_IsArray(arr)) { cJSON_Delete(arr); return false; }

    int count = cJSON_GetArraySize(arr);
    if (count > 32) count = 32;
    g_subject_count = count;

    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        memset(&g_subjects[i], 0, sizeof(subject_info_t));

        cJSON *id = cJSON_GetObjectItem(item, "id");
        if (id && cJSON_IsString(id))
            strncpy(g_subjects[i].id, id->valuestring, sizeof(g_subjects[i].id) - 1);

        cJSON *nm = cJSON_GetObjectItem(item, "name");
        if (nm && cJSON_IsString(nm))
            strncpy(g_subjects[i].name, nm->valuestring, sizeof(g_subjects[i].name) - 1);

        cJSON *cl = cJSON_GetObjectItem(item, "color_hex");
        if (cl && cJSON_IsString(cl))
            strncpy(g_subjects[i].color_hex, cl->valuestring, sizeof(g_subjects[i].color_hex) - 1);
    }

    cJSON_Delete(arr);
    return true;
}

static subject_info_t *find_subject(const char *subject_id) {
    for (int i = 0; i < g_subject_count; i++) {
        if (strcmp(g_subjects[i].id, subject_id) == 0)
            return &g_subjects[i];
    }
    return NULL;
}

/* ── 공부일 UTC 범위 (KST 06:00 ~ 익일 06:00) ──
 *   KST = UTC+9
 *   KST 06:00 = UTC (전날) 21:00
 *   예) KST 2026-06-03 14:00 → UTC 2026-06-02T21:00:00Z ~ 2026-06-03T21:00:00Z
 */
static void get_study_day_utc_range(char *start_iso, int start_size,
                                     char *end_iso,   int end_size) {
    time_t now      = time(NULL);
    time_t kst_now  = now + 9 * 3600;
    struct tm kst;
    gmtime_r(&kst_now, &kst);

    int year = kst.tm_year + 1900;
    int mon  = kst.tm_mon + 1;
    int mday = kst.tm_mday;

    if (kst.tm_hour < 6) {
        mday -= 1;
        if (mday < 1) {
            mon -= 1;
            if (mon < 1) { mon = 12; year -= 1; }
            static const int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
            mday = dim[mon - 1];
            if (mon == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)))
                mday = 29;
        }
    }

    struct tm kst_start = {0};
    kst_start.tm_year = year - 1900;
    kst_start.tm_mon  = mon - 1;
    kst_start.tm_mday = mday;
    kst_start.tm_hour = 6;
    kst_start.tm_min  = 0;
    kst_start.tm_sec  = 0;

    time_t start_epoch = timegm(&kst_start) - 9 * 3600;
    time_t end_epoch   = start_epoch + 24 * 3600;

    struct tm utc;
    gmtime_r(&start_epoch, &utc);
    snprintf(start_iso, start_size, "%04d-%02d-%02dT%02d:%02d:%02dZ",
             utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
             utc.tm_hour, utc.tm_min, utc.tm_sec);

    gmtime_r(&end_epoch, &utc);
    snprintf(end_iso, end_size, "%04d-%02d-%02dT%02d:%02d:%02dZ",
             utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
             utc.tm_hour, utc.tm_min, utc.tm_sec);
}

bool supabase_get_today_checklists(checklist_subject_group_t *group_out,
                                    int max_groups, int *group_count_out) {
    char start_iso[32], end_iso[32];
    get_study_day_utc_range(start_iso, sizeof(start_iso),
                             end_iso,   sizeof(end_iso));

    if (!load_subjects()) return false;

    CURL *curl = curl_easy_init();
    char *enc_start = url_encode_str(curl, start_iso);
    char *enc_end   = url_encode_str(curl, end_iso);
    char *enc_uid   = url_encode_str(curl, current_user_id_or_default());
    curl_easy_cleanup(curl);
    if (!enc_start || !enc_end || !enc_uid) {
        free(enc_start); free(enc_end); free(enc_uid); return false;
    }

    char url[1024];
    snprintf(url, sizeof(url),
        "%s/rest/v1/checklist_items"
        "?select=id,text,is_checked,sort_order,subject_id"
        "&user_id=eq.%s"
        "&date=gte.%s&date=lt.%s"
        "&order=sort_order.asc",
        SUPABASE_URL, enc_uid, enc_start, enc_end);
    free(enc_start);
    free(enc_end);
    free(enc_uid);

    char *resp = NULL;
    if (!do_get(url, &resp)) return false;

    cJSON *arr = cJSON_Parse(resp);
    free(resp);
    if (!arr || !cJSON_IsArray(arr)) { cJSON_Delete(arr); return false; }

    int count = cJSON_GetArraySize(arr);
    int group_count = 0;

    /* 그룹 초기화 */
    memset(group_out, 0, sizeof(checklist_subject_group_t) * max_groups);

    for (int i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);

        /* subject_id 추출 */
        cJSON *sid = cJSON_GetObjectItem(item, "subject_id");
        if (!sid || !cJSON_IsString(sid)) continue;
        const char *subject_id = sid->valuestring;

        /* 기존 그룹 찾기 또는 새 그룹 생성 */
        int gi = -1;
        for (int g = 0; g < group_count; g++) {
            if (strcmp(group_out[g].subject_id, subject_id) == 0) {
                gi = g;
                break;
            }
        }
        if (gi < 0) {
            if (group_count >= max_groups) continue;
            gi = group_count++;
            strncpy(group_out[gi].subject_id, subject_id,
                    sizeof(group_out[gi].subject_id) - 1);

            /* 로컬 subjects 캐시에서 name/color_hex 조회 */
            subject_info_t *si = find_subject(subject_id);
            if (si) {
                strncpy(group_out[gi].subject_name, si->name,
                        sizeof(group_out[gi].subject_name) - 1);
                strncpy(group_out[gi].color_hex, si->color_hex,
                        sizeof(group_out[gi].color_hex) - 1);
            }
        }

        /* 아이템 추가 */
        checklist_item_t *ci = &group_out[gi].items[group_out[gi].item_count];
        if (group_out[gi].item_count >= MAX_CHECKLIST_ITEMS_PER_SUBJECT) continue;

        cJSON *id = cJSON_GetObjectItem(item, "id");
        if (id && cJSON_IsString(id))
            strncpy(ci->id, id->valuestring, sizeof(ci->id) - 1);

        cJSON *txt = cJSON_GetObjectItem(item, "text");
        if (txt && cJSON_IsString(txt))
            strncpy(ci->text, txt->valuestring, sizeof(ci->text) - 1);

        cJSON *chk = cJSON_GetObjectItem(item, "is_checked");
        ci->is_checked = (chk && cJSON_IsBool(chk)) ? cJSON_IsTrue(chk) : false;
        if (ci->is_checked) group_out[gi].checked_count++;

        group_out[gi].item_count++;
    }

    *group_count_out = group_count;
    cJSON_Delete(arr);
    return true;
}

bool supabase_toggle_checklist_item(const char *item_id, bool new_checked) {
    CURL *curl = curl_easy_init();
    if (!curl) return false;

    char *enc_id  = url_encode_str(curl, item_id);
    char *enc_uid = url_encode_str(curl, current_user_id_or_default());
    if (!enc_id || !enc_uid) {
        free(enc_id); free(enc_uid); curl_easy_cleanup(curl);
        return false;
    }

    /* user_id 도 같이 필터링 — RLS 가 anon key 에 대해
     * current_setting('app.user_id') 와 일치하는 행만 허용하는 경우
     * 자기 디바이스 행만 업데이트되도록 보장한다. */
    char url[512];
    snprintf(url, sizeof(url),
        "%s/rest/v1/checklist_items?id=eq.%s&user_id=eq.%s",
        SUPABASE_URL, enc_id, enc_uid);
    curl_free(enc_id);
    curl_free(enc_uid);
    curl_easy_cleanup(curl);

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(obj, "is_checked", new_checked);
    char *body = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);

    bool ok = do_patch(url, body);
    free(body);
    return ok;
}

/* ── 공부 세션 업로드 ───────────────────────────────────── */

void supabase_set_user_id(const char *user_id)
{
    if (user_id) {
        strncpy(g_user_id, user_id, sizeof(g_user_id) - 1);
        g_user_id[sizeof(g_user_id) - 1] = '\0';
    } else {
        g_user_id[0] = '\0';
    }
    /* user_id 가 바뀌면 다음 조회에서 재로딩하도록 로컬 캐시도 무효화 */
    g_subject_count = 0;
}

const char *supabase_get_user_id(void)
{
    return g_user_id;
}

/* user_id 전환/로그아웃 시 호출: 캐시된 subjects 등을 모두 무효화 */
void supabase_clear_local_caches(void)
{
    g_subject_count = 0;
    memset(g_subjects, 0, sizeof(g_subjects));
}

bool supabase_upload_session(const local_session_t *session)
{
    if (!session || !session->id[0] || !session->subject_id[0]) return false;

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "id",               session->id);
    cJSON_AddStringToObject(obj, "user_id",          g_user_id[0] ? g_user_id : "default");
    cJSON_AddStringToObject(obj, "subject_id",       session->subject_id);
    cJSON_AddStringToObject(obj, "start_time",       session->start_time);
    cJSON_AddStringToObject(obj, "end_time",         session->end_time);
    cJSON_AddNumberToObject(obj, "duration_seconds", session->duration_seconds);
    cJSON_AddNumberToObject(obj, "self_score",       0);
    cJSON_AddNumberToObject(obj, "penalty_count",    session->penalty_count);
    cJSON_AddNumberToObject(obj, "tray_open_count",  session->tray_open_count);

    char *body = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);

    char url[256];
    snprintf(url, sizeof(url), "%s/rest/v1/study_sessions", SUPABASE_URL);
    bool ok = do_post(url, body, NULL);
    free(body);
    return ok;
}
