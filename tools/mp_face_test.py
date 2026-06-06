"""
MediaPipe Face Detection basic test
- 카메라에서 얼굴을 감지하고 네모 박스 + 신뢰도를 그려 화면에 표시
- 종료: q  또는  ESC
- 의존성: pip install mediapipe opencv-python
"""

import cv2
import mediapipe as mp

mp_face_detection = mp.solutions.face_detection
mp_drawing = mp.solutions.drawing_utils


def draw_box(frame, det):
    h, w, _ = frame.shape
    bbox = det.location_data.relative_bounding_box
    x = max(int(bbox.xmin * w), 0)
    y = max(int(bbox.ymin * h), 0)
    bw = min(int(bbox.width * w), w - x)
    bh = min(int(bbox.height * h), h - y)

    cv2.rectangle(frame, (x, y), (x + bw, y + bh), (0, 255, 0), 2)
    score = det.score[0] if det.score else 0.0
    label = f"Face {score:.2f}"
    cv2.putText(
        frame, label, (x, max(y - 10, 20)),
        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2,
    )


def open_camera(index=0):
    # Windows: 내장 캠 0, USB 캠 1/2
    cap = cv2.VideoCapture(index, cv2.CAP_DSHOW)
    if not cap.isOpened():
        cap = cv2.VideoCapture(index)
    if not cap.isOpened():
        raise RuntimeError(
            f"카메라(index={index})를 열 수 없습니다. "
            "다른 번호(0/1/2)나 카메라 점유 여부를 확인하세요."
        )
    return cap


def main():
    cap = open_camera(0)

    with mp_face_detection.FaceDetection(
        model_selection=0,
        min_detection_confidence=0.5,
    ) as fd:
        win = "MediaPipe Face Detection (q: quit)"
        cv2.namedWindow(win, cv2.WINDOW_NORMAL)

        while cap.isOpened():
            ok, frame = cap.read()
            if not ok:
                print("프레임을 읽지 못했습니다.")
                break

            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            result = fd.process(rgb)

            n = 0
            if result.detections:
                for det in result.detections:
                    draw_box(frame, det)
                    n += 1

            status = f"Faces: {n}" if n else "No face detected"
            color = (0, 255, 0) if n else (0, 0, 255)
            cv2.putText(
                frame, status, (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 0.8, color, 2,
            )

            cv2.imshow(win, frame)
            key = cv2.waitKey(1) & 0xFF
            if key in (ord('q'), 27):
                break

    cap.release()
    cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
