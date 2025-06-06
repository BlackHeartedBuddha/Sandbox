import socket
import struct
import cv2
import numpy as np

def recv_all(sock, size):
    """Helper function to receive exactly `size` bytes from socket."""
    buf = b''
    while len(buf) < size:
        data = sock.recv(size - len(buf))
        if not data:
            return None
        buf += data
    return buf

def main():
    host = '127.0.0.1'  # Replace with your server IP or hostname
    port = 8080

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    print("Connected to server")

    try:
        while True:
            # First receive 4 bytes (uint32) indicating JPEG size
            size_bytes = recv_all(sock, 4)
            if size_bytes is None:
                print("Server closed connection")
                break

            # Unpack size from network byte order
            (jpeg_size,) = struct.unpack('!I', size_bytes)

            # Receive JPEG image bytes
            jpeg_data = recv_all(sock, jpeg_size)
            if jpeg_data is None:
                print("Server closed connection")
                break

            # Decode JPEG to OpenCV image (BGR)
            np_arr = np.frombuffer(jpeg_data, dtype=np.uint8)
            frame = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)
            if frame is None:
                print("Failed to decode frame")
                continue

            # Show frame
            cv2.imshow('Video Stream', frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

    finally:
        sock.close()
        cv2.destroyAllWindows()

if __name__ == '__main__':
    main()

