https://wowjdjvjhpnbirptpffb.supabase.co

eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Indvd2pkanZqaHBuYmlycHRwZmZiIiwicm9sZSI6ImFub24iLCJpYXQiOjE3NzYwMTM3NDEsImV4cCI6MjA5MTU4OTc0MX0.FMbj1kB9skHM7tMtMlpdoWHvAcI-AFEaTfiZHFwGo9Q

------------------sql code---------------------
-- ============================================================
-- Supabase Migration: Study Manager App
-- 실행 방법: Supabase Dashboard > SQL Editor 에 붙여넣고 실행
-- ============================================================

-- ── 1. 테이블 생성 ─────────────────────────────────────────

CREATE TABLE IF NOT EXISTS categories (
id          TEXT PRIMARY KEY,
user_id     TEXT NOT NULL,
name        TEXT NOT NULL,
sort_order  INT  NOT NULL DEFAULT 0,
updated_at  TIMESTAMP NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS subjects (
id          TEXT PRIMARY KEY,
user_id     TEXT NOT NULL,
category_id TEXT,
name        TEXT NOT NULL,
color_hex   TEXT NOT NULL DEFAULT '#4CAF50',
updated_at  TIMESTAMP NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS study_sessions (
id               TEXT PRIMARY KEY,
user_id          TEXT NOT NULL,
subject_id       TEXT NOT NULL,
plan_id          TEXT,
start_time       TIMESTAMP NOT NULL,
end_time         TIMESTAMP,
duration_seconds INT NOT NULL DEFAULT 0,
self_score       INT NOT NULL DEFAULT 0,
penalty_count    INT NOT NULL DEFAULT 0,
updated_at       TIMESTAMP NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS checklist_items (
id          TEXT PRIMARY KEY,
user_id     TEXT NOT NULL,
subject_id  TEXT NOT NULL,
date        TEXT NOT NULL,
text        TEXT NOT NULL,
is_checked  BOOLEAN NOT NULL DEFAULT false,
sort_order  INT NOT NULL DEFAULT 0,
created_at  TIMESTAMP NOT NULL,
updated_at  TIMESTAMP NOT NULL DEFAULT now()
);

DROP TABLE IF EXISTS device_registrations CASCADE;
CREATE TABLE device_registrations (
device_number TEXT PRIMARY KEY,
nfc_id        TEXT,
registered_at TIMESTAMP NOT NULL DEFAULT now()
);

-- ── 2. 인덱스 ─────────────────────────────────────────────

CREATE INDEX IF NOT EXISTS idx_categories_user      ON categories      (user_id);
CREATE INDEX IF NOT EXISTS idx_subjects_user        ON subjects        (user_id);
CREATE INDEX IF NOT EXISTS idx_sessions_user_date   ON study_sessions  (user_id, start_time);
CREATE INDEX IF NOT EXISTS idx_checklist_user_date  ON checklist_items (user_id, date);

-- ── 3. RLS 활성화 ─────────────────────────────────────────

ALTER TABLE categories            ENABLE ROW LEVEL SECURITY;
ALTER TABLE subjects              ENABLE ROW LEVEL SECURITY;
ALTER TABLE study_sessions        ENABLE ROW LEVEL SECURITY;
ALTER TABLE checklist_items       ENABLE ROW LEVEL SECURITY;
ALTER TABLE device_registrations  DISABLE ROW LEVEL SECURITY;

DROP POLICY IF EXISTS "device_isolation_categories" ON categories;
CREATE POLICY "device_isolation_categories" ON categories
USING (user_id = current_setting('app.user_id', true));

DROP POLICY IF EXISTS "device_isolation_subjects" ON subjects;
CREATE POLICY "device_isolation_subjects" ON subjects
USING (user_id = current_setting('app.user_id', true));

DROP POLICY IF EXISTS "device_isolation_sessions" ON study_sessions;
CREATE POLICY "device_isolation_sessions" ON study_sessions
USING (user_id = current_setting('app.user_id', true));

DROP POLICY IF EXISTS "device_isolation_checklist" ON checklist_items;
CREATE POLICY "device_isolation_checklist" ON checklist_items
USING (user_id = current_setting('app.user_id', true));

-- ── 4. updated_at 자동 갱신 트리거 ────────────────────────

CREATE OR REPLACE FUNCTION update_updated_at()
RETURNS TRIGGER AS $$
BEGIN
NEW.updated_at = LOCALTIMESTAMP;
RETURN NEW;
END;
$$ LANGUAGE plpgsql;

DROP TRIGGER IF EXISTS trg_categories_updated_at ON categories;
CREATE TRIGGER trg_categories_updated_at
BEFORE UPDATE ON categories
FOR EACH ROW EXECUTE FUNCTION update_updated_at();

DROP TRIGGER IF EXISTS trg_subjects_updated_at ON subjects;
CREATE TRIGGER trg_subjects_updated_at
BEFORE UPDATE ON subjects
FOR EACH ROW EXECUTE FUNCTION update_updated_at();

DROP TRIGGER IF EXISTS trg_sessions_updated_at ON study_sessions;
CREATE TRIGGER trg_sessions_updated_at
BEFORE UPDATE ON study_sessions
FOR EACH ROW EXECUTE FUNCTION update_updated_at();

DROP TRIGGER IF EXISTS trg_checklist_updated_at ON checklist_items;
CREATE TRIGGER trg_checklist_updated_at
BEFORE UPDATE ON checklist_items
FOR EACH ROW EXECUTE FUNCTION update_updated_at();

-- ── 5. 마이그레이션 ────────────────────────────────────────

-- study_sessions에 penalty_count 컬럼 추가 (이미 있으면 무시)
ALTER TABLE study_sessions ADD COLUMN IF NOT EXISTS penalty_count INT NOT NULL DEFAULT 0;
-- study_sessions에 tray_open_count 컬럼 추가 (폰 들어올림 카운트, RPi 호환)
ALTER TABLE study_sessions ADD COLUMN IF NOT EXISTS tray_open_count INT NOT NULL DEFAULT 0;

-- ── 6. Realtime 활성화 (NFC만) ─────────────────────────────

DO $$
BEGIN
ALTER PUBLICATION supabase_realtime ADD TABLE study_sessions;
EXCEPTION WHEN duplicate_object THEN NULL;
END $$;

DO $$
BEGIN
ALTER PUBLICATION supabase_realtime ADD TABLE device_registrations;
EXCEPTION WHEN duplicate_object THEN NULL;
END $$;
