#define _GNU_SOURCE
#include "app_sessions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../lvgl/lvgl.h"
#include <cjson/cJSON.h>

#define SESSIONS_FILE "sessions_local.json"

static local_session_t sessions[MAX_LOCAL_SESSIONS];
static int session_count = 0;
static int id_counter = 0;

void sessions_init(void)
{
    FILE *f = fopen(SESSIONS_FILE, "r");
    if (!f) { session_count = 0; id_counter = 0; return; }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) { fclose(f); session_count = 0; id_counter = 0; return; }

    char *buf = malloc(size + 1);
    if (!buf) { fclose(f); session_count = 0; id_counter = 0; return; }

    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    cJSON *json = cJSON_Parse(buf);
    free(buf);

    if (!json || !cJSON_IsArray(json)) {
        cJSON_Delete(json);
        session_count = 0;
        id_counter = 0;
        return;
    }

    session_count = cJSON_GetArraySize(json);
    if (session_count > MAX_LOCAL_SESSIONS) session_count = MAX_LOCAL_SESSIONS;

    for (int i = 0; i < session_count; i++) {
        cJSON *item = cJSON_GetArrayItem(json, i);
        local_session_t *s = &sessions[i];
        cJSON *j;

        j = cJSON_GetObjectItem(item, "id");
        strncpy(s->id, cJSON_IsString(j) ? j->valuestring : "", sizeof(s->id) - 1);

        j = cJSON_GetObjectItem(item, "subject_name");
        strncpy(s->subject_name, cJSON_IsString(j) ? j->valuestring : "", sizeof(s->subject_name) - 1);

        j = cJSON_GetObjectItem(item, "subject_id");
        strncpy(s->subject_id, cJSON_IsString(j) ? j->valuestring : "", sizeof(s->subject_id) - 1);

        j = cJSON_GetObjectItem(item, "color_hex");
        strncpy(s->color_hex, cJSON_IsString(j) ? j->valuestring : "#4CAF50", sizeof(s->color_hex) - 1);

        j = cJSON_GetObjectItem(item, "start_time");
        strncpy(s->start_time, cJSON_IsString(j) ? j->valuestring : "", sizeof(s->start_time) - 1);

        j = cJSON_GetObjectItem(item, "end_time");
        strncpy(s->end_time, cJSON_IsString(j) ? j->valuestring : "", sizeof(s->end_time) - 1);

        j = cJSON_GetObjectItem(item, "duration_seconds");
        s->duration_seconds = cJSON_IsNumber(j) ? j->valueint : 0;

        j = cJSON_GetObjectItem(item, "pushed");
        s->pushed = cJSON_IsTrue(j);
    }

    cJSON_Delete(json);

    /* id_counter 갱신: 기존 id 중 최대값 + 1 */
    id_counter = 0;
    for (int i = 0; i < session_count; i++) {
        int num = atoi(sessions[i].id);
        if (num >= id_counter) id_counter = num + 1;
    }
}

static void save_to_file(void)
{
    cJSON *json = cJSON_CreateArray();

    for (int i = 0; i < session_count; i++) {
        cJSON *item = cJSON_CreateObject();
        cJSON_AddStringToObject(item, "id", sessions[i].id);
        cJSON_AddStringToObject(item, "subject_name", sessions[i].subject_name);
        cJSON_AddStringToObject(item, "subject_id", sessions[i].subject_id);
        cJSON_AddStringToObject(item, "color_hex", sessions[i].color_hex);
        cJSON_AddStringToObject(item, "start_time", sessions[i].start_time);
        cJSON_AddStringToObject(item, "end_time", sessions[i].end_time);
        cJSON_AddNumberToObject(item, "duration_seconds", sessions[i].duration_seconds);
        cJSON_AddBoolToObject(item, "pushed", sessions[i].pushed);
        cJSON_AddItemToArray(json, item);
    }

    char *str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    FILE *f = fopen(SESSIONS_FILE, "w");
    if (f) {
        fprintf(f, "%s", str);
        fclose(f);
    }
    free(str);
}

bool sessions_save(const local_session_t *session)
{
    if (session_count >= MAX_LOCAL_SESSIONS) return false;

    sessions[session_count] = *session;
    session_count++;
    save_to_file();
    return true;
}

int sessions_load_all(local_session_t *out, int max_count)
{
    int count = session_count < max_count ? session_count : max_count;
    for (int i = 0; i < count; i++) {
        out[i] = sessions[i];
    }
    return count;
}

int sessions_get_unpushed_count(void)
{
    int count = 0;
    for (int i = 0; i < session_count; i++) {
        if (!sessions[i].pushed) count++;
    }
    return count;
}

int sessions_get_count(void)
{
    return session_count;
}

void sessions_mark_all_pushed(void)
{
    for (int i = 0; i < session_count; i++) {
        sessions[i].pushed = true;
    }
    save_to_file();
}

bool sessions_delete(int index)
{
    if (index < 0 || index >= session_count) return false;

    for (int i = index; i < session_count - 1; i++) {
        sessions[i] = sessions[i + 1];
    }
    session_count--;
    save_to_file();
    return true;
}

bool sessions_mark_pushed(int index)
{
    if (index < 0 || index >= session_count) return false;
    if (sessions[index].pushed) return true;
    sessions[index].pushed = true;
    save_to_file();
    return true;
}

/* "YYYY-MM-DDTHH:MM:SS" → time_t (로컬 TZ). 파싱 실패 시 0 */
static time_t parse_iso_local(const char *iso)
{
    if (!iso || !iso[0]) return 0;
    struct tm tm = {0};
    if (strptime(iso, "%Y-%m-%dT%H:%M:%S", &tm) == NULL) return 0;
    return mktime(&tm);
}

int sessions_prune_before(time_t cutoff)
{
    int removed = 0;
    int i = 0;
    while (i < session_count) {
        time_t end_t = parse_iso_local(sessions[i].end_time);
        if (end_t > 0 && end_t < cutoff) {
            for (int j = i; j < session_count - 1; j++) {
                sessions[j] = sessions[j + 1];
            }
            session_count--;
            removed++;
        } else {
            i++;
        }
    }
    if (removed > 0) save_to_file();
    return removed;
}
