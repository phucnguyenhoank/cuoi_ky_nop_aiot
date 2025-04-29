import socket
import csv
import os
from datetime import datetime

# Cấu hình
SERVER_IP   = '0.0.0.0'  # lắng nghe trên tất cả interface
SERVER_PORT = 5005
BUFFER_SIZE = 1024
OUT_DIR     = './udp_data'

os.makedirs(OUT_DIR, exist_ok=True)
csv_filename = os.path.join(
    OUT_DIR,
    f"data_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
)

# Mở file và ghi header
with open(csv_filename, 'w', newline='') as f:
    writer = csv.writer(f)
    writer.writerow([
        'ServerTime',
        'ESP_ms',
        'AccelX', 'AccelY', 'AccelZ',
        'GyroX',  'GyroY',  'GyroZ',
        'IR'
    ])

    # Khởi socket UDP
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((SERVER_IP, SERVER_PORT))
    print(f"Listening on UDP {SERVER_IP}:{SERVER_PORT}")
    
    while True:
        data, addr = sock.recvfrom(BUFFER_SIZE)
        server_time = datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')
        try:
            parts = data.decode('utf-8').strip().split(',')
            # parts = [ESP_ms, accX, accY, accZ, gyroX, gyroY, gyroZ, ir]
            row = [server_time] + parts
            writer.writerow(row)
            f.flush()  # ensure ghi ngay vào đĩa
            print(f"Got from {addr}: {row}")
        except Exception as e:
            print("Parse error:", e, "->", data)
