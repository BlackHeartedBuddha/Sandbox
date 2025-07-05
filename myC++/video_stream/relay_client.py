import socket
import struct
import cv2
import numpy as np

def recv_all(sock, size):
    buf = b''
    while len(buf) < size:
        data = sock.recv(size - len(buf))
        if not data:
            return None
        buf += data
    return buf

def main():
    relay_host = '192.168.0.115'  # Change to your relay server IP
    relay_port = 9090

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    print(f"Connecting to relay server {relay_host}:{relay_port}...")
    sock.connect((relay_host, relay_port))
    print("Connected!")

    try:
        while True:
            print("Waiting for frame size...")
            size_data = recv_all(sock, 4)
            if size_data is None:
                print("Connection closed by server.")
                break

            (frame_size,) = struct.unpack('!I', size_data)
            print(f"Expecting frame of size: {frame_size} bytes")

            frame_data = recv_all(sock, frame_size)
            if frame_data is None:
                print("Connection closed by server during frame receive.")
                break

            # Decode JPEG frame
            np_arr = np.frombuffer(frame_data, dtype=np.uint8)
            frame = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)
            if frame is None:
                print("Failed to decode frame.")
                continue

            print(f"Received frame: {frame.shape}")

            # Show frame
            cv2.imshow('Relay Stream', frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                print("Exiting...")
                break

    except Exception as e:
        print(f"Exception: {e}")

    finally:
        sock.close()
        cv2.destroyAllWindows()

if __name__ == '__main__':
    main()

