import cv2
import mediapipe as mp
import numpy as np

# Load transparent PNG image to overlay
overlay_img = cv2.imread('witch.png', cv2.IMREAD_UNCHANGED)
overlay_img2 = cv2.imread('bg.jpg', cv2.IMREAD_UNCHANGED)

# Mediapipe setup
mp_face_mesh = mp.solutions.face_mesh
face_mesh = mp_face_mesh.FaceMesh(refine_landmarks=True)

# Capture from webcam
cap = cv2.VideoCapture(0)

def overlay_transparent(background, overlay, x, y, overlay_size=None):
    bg = background.copy()
    if overlay_size:
        overlay = cv2.resize(overlay, overlay_size, interpolation=cv2.INTER_AREA)

    h, w, _ = overlay.shape

    # Clip overlay to stay in bounds
    if x < 0:
        overlay = overlay[:, -x:]
        w -= -x
        x = 0
    if y < 0:
        overlay = overlay[-y:, :]
        h -= -y
        y = 0
    if x + w > bg.shape[1]:
        overlay = overlay[:, :bg.shape[1] - x]
        w = bg.shape[1] - x
    if y + h > bg.shape[0]:
        overlay = overlay[:bg.shape[0] - y, :]
        h = bg.shape[0] - y

    if overlay.shape[2] < 4:
    # No alpha channel, just paste it
        bg[y:y+h, x:x+w] = overlay
        return bg

    alpha = overlay[:, :, 3] / 255.0
    for c in range(3):
        bg[y:y+h, x:x+w, c] = alpha * overlay[:, :, c] + (1 - alpha) * bg[y:y+h, x:x+w, c]

    return bg

while True:
    ret, frame = cap.read()
    if not ret:
        break

    h, w, _ = frame.shape
    frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    results = face_mesh.process(frame_rgb)

    # Start with black background
    frame = np.zeros((h, w, 3), dtype=np.uint8)

    if results.multi_face_landmarks:
        for landmarks in results.multi_face_landmarks:
            lm = landmarks.landmark
            top_lip = lm[13]
            bottom_lip = lm[14]
            eye_left = lm[159]
            eye_right = lm[145]

            mouth_open = abs(top_lip.y - bottom_lip.y) > 0.04
            eyes_closed = abs(eye_left.y - eye_right.y) < 0.005

            nose = lm[1]
            cx, cy = int(nose.x * w), int(nose.y * h)

            if mouth_open or eyes_closed:
                frame = overlay_transparent(frame, overlay_img, cx - 100, cy - 150, overlay_size=(200, 200))
            else:
                frame = overlay_transparent(frame, overlay_img2, cx - 250, cy - 250, overlay_size=(500, 500))
    cv2.imshow('VTuber Overlay', frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()
