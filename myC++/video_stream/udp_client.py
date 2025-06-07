import socket
import struct
import cv2
import numpy as np
from collections import defaultdict

MAX_PACKET_SIZE = 1500
PACKET_HEADER_SIZE = 8  # 4 bytes frame_id, 2 bytes total_parts, 2 bytes part_index
DATA_SIZE = MAX_PACKET_SIZE - PACKET_HEADER_SIZE

def main():
    host = '0.0.0.0'
    port = 8080

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((host, port))
    sock.settimeout(0.5)  # optional timeout for smoother quitting

    print(f"Listening for UDP packets on {host}:{port}")

    frame_buffer = defaultdict(dict)
    expected_parts = {}
    last_displayed = -1

    try:
        while True:
            try:
                packet, _ = sock.recvfrom(MAX_PACKET_SIZE)
            except socket.timeout:
                continue

            if len(packet) < PACKET_HEADER_SIZE:
                continue  # ignore invalid packets

            # Parse header
            frame_id = struct.unpack('!I', packet[:4])[0]
            total_parts = struct.unpack('!H', packet[4:6])[0]
            part_index = struct.unpack('!H', packet[6:8])[0]
            payload = packet[8:]

            # Store chunk
            frame_buffer[frame_id][part_index] = payload
            expected_parts[frame_id] = total_parts

            # Check if all parts of the frame are received
            if len(frame_buffer[frame_id]) == expected_parts[frame_id]:
                # Reassemble JPEG
                parts = [frame_buffer[frame_id][i] for i in range(total_parts)]
                jpeg_data = b''.join(parts)

                # Decode and display
                np_arr = np.frombuffer(jpeg_data, dtype=np.uint8)
                frame = cv2.imdecode(np_arr, cv2.IMREAD_COLOR)
                if frame is not None:
                    cv2.imshow("UDP Video", frame)
                    if cv2.waitKey(1) & 0xFF == ord('q'):
                        break

                # Clean up
                del frame_buffer[frame_id]
                del expected_parts[frame_id]
                last_displayed = frame_id

                # Optional: clean old frames
                keys_to_delete = [fid for fid in frame_buffer if fid < frame_id]
                for k in keys_to_delete:
                    del frame_buffer[k]
                    if k in expected_parts:
                        del expected_parts[k]

    finally:
        sock.close()
        cv2.destroyAllWindows()

if __name__ == '__main__':
    main()

