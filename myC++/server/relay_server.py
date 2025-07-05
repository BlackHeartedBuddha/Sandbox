import socket
import struct
import cv2
import numpy as np
from threading import Thread, Lock
from ultralytics import YOLO
import time
import torch

# Load YOLOv8 model
device = "cuda" if torch.cuda.is_available() else "cpu"
print(f"Using device: {device}")
model = YOLO("yolo11s.pt").to(device)

clients = []
clients_lock = Lock()
encoded_frame = None

def recv_all(sock, size):
    """Receive exactly 'size' bytes from socket."""
    buf = b''
    while len(buf) < size:
        data = sock.recv(size - len(buf))
        if not data:
            return None
        buf += data
    return buf

def handle_stream_to_clients():
    """Send processed JPEG frames to all connected clients."""
    global encoded_frame
    while True:
        if encoded_frame is None:
            time.sleep(0.01)  # Avoid busy wait
            continue
        with clients_lock:
            for c in clients[:]:
                try:
                    c.sendall(struct.pack('!I', len(encoded_frame)))
                    c.sendall(encoded_frame)
                    print(f"Sent frame ({len(encoded_frame)} bytes) to client {c.getpeername()}")
                except Exception as e:
                    print(f"Client {c.getpeername()} disconnected or error: {e}")
                    clients.remove(c)
                    c.close()
        time.sleep(0.01)  # Control sending rate

def start_relay_server(host="0.0.0.0", port=9090):
    """Accept incoming clients who want the processed stream."""
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.bind((host, port))
    server.listen(5)
    print(f"🟢 YOLO relay server running on {host}:{port}")
    while True:
        client, addr = server.accept()
        print(f"🔌 Client connected from {addr}")
        with clients_lock:
            clients.append(client)

def main():
    global encoded_frame
    encoded_frame = None

    # Connect to the upstream video server (publisher)
    upstream_host = '192.168.0.107'  # Change to your upstream server IP
    upstream_port = 8080
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((upstream_host, upstream_port))
    print("🛰️  Connected to upstream video server")

    # Start threads for relay server and streaming frames to clients
    Thread(target=start_relay_server, daemon=True).start()
    Thread(target=handle_stream_to_clients, daemon=True).start()

    try:
        while True:
            # Receive frame size
            size_bytes = recv_all(sock, 4)
            if size_bytes is None:
                print("⚠️  Upstream server closed connection")
                break
            (jpeg_size,) = struct.unpack('!I', size_bytes)

            # Receive JPEG frame data
            jpeg_data = recv_all(sock, jpeg_size)
            if jpeg_data is None:
                print("⚠️  Upstream server closed connection")
                break

            # Decode JPEG frame to OpenCV image
            np_arr = np.frombuffer(jpeg_data, dtype=np.uint8)
            frame = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)
            if frame is None:
                print("❌ Failed to decode frame")
                continue

            # Run YOLO inference
            results = model(frame)[0]
            for box in results.boxes:
                x1, y1, x2, y2 = map(int, box.xyxy[0])
                conf = box.conf[0]
                cls = int(box.cls[0])
                label = model.names[cls]
                cv2.rectangle(frame, (x1, y1), (x2, y2), (0, 255, 0), 2)
                cv2.putText(frame, f"{label} {conf:.2f}", (x1, y1 - 5),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)

            # Encode processed frame back to JPEG
            success, buffer = cv2.imencode(".jpg", frame)
            if success:
                encoded_frame = buffer.tobytes()

            # Optionally display locally
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

    except Exception as e:
        print(f"Exception: {e}")

    finally:
        print("Cleaning up...")
        sock.close()
        with clients_lock:
            for c in clients:
                c.close()
            clients.clear()
        cv2.destroyAllWindows()

if __name__ == '__main__':
    main()

