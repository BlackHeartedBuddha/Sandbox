import network
import socket
import time
import machine

# Optional: define temperature sensor if needed
sensor = machine.ADC(4)
conversion_factor = 3.3 / 65535

# Set up Access Point
ap = network.WLAN(network.AP_IF)
ap.active(True)

# Optional: set custom IP address
ap.ifconfig(('192.168.4.1', '255.255.255.0', '192.168.4.1', '8.8.8.8'))

# Configure SSID and password (min 8 chars)
ap.config(essid='MyESP-AP', password='12345678')

# Wait for AP to activate
while not ap.active():
    time.sleep(1)

print("Access Point is active.")
print("AP IP config:", ap.ifconfig())

# Optional TCP server — listens for ESP8266 messages
addr = socket.getaddrinfo('0.0.0.0', 1234)[0][-1]
s = socket.socket()
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(addr)
s.listen(1)
print("Waiting for ESP8266 connection on port 1234...")

while True:
    try:
        conn, client_addr = s.accept()
        print("ESP8266 connected from", client_addr)

        data = conn.recv(1024)
        if data:
            print("Received from ESP8266:", data.decode())

            # Optional: send PicoW temp back
            reading = sensor.read_u16() * conversion_factor
            temp = 27 - (reading - 0.706)/0.001721
            conn.send(str(temp).encode())
            print("Sent temperature:", temp)

        conn.close()

    except Exception as e:
        print("Error:", e)
