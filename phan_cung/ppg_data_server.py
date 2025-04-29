import socket
import csv
import os
import threading
import time
from datetime import datetime
from flask import Flask, request, redirect, url_for, render_template_string, jsonify

# ================= Configuration =================
SERVER_IP   = '0.0.0.0'   # Listen on all interfaces
SERVER_PORT = 5005        # UDP port
data_dir    = './udp_data'  # Directory for CSV files
os.makedirs(data_dir, exist_ok=True)

# Flask app
app = Flask(__name__)

# ================ Global state ================
collecting_data = False
start_time = 0.0
collection_duration = 0  # in seconds
label = 'awake'
csv_file = None
csv_writer = None
file_lock = threading.Lock()

# ================ HTML Template ================
HTML = '''
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Data Collection Interface</title>
    <script src="https://cdn.tailwindcss.com"></script>
    <style>
        body {
            font-family: 'Inter', sans-serif;
        }
        .fade-in {
            animation: fadeIn 0.5s ease-in;
        }
        @keyframes fadeIn {
            from { opacity: 0; }
            to { opacity: 1; }
        }
    </style>
</head>
<body class="bg-gray-100 min-h-screen flex items-center justify-center p-4">
    <div class="bg-white rounded-lg shadow-lg p-6 max-w-md w-full fade-in">
        <h1 class="text-2xl font-bold text-gray-800 mb-6 text-center">Data Collection</h1>

        <form action="/start" method="post" class="mb-6">
            <div class="flex items-center space-x-4">
                <label for="minutes" class="text-gray-700 font-medium">Duration (minutes):</label>
                <input
                    type="number"
                    id="minutes"
                    name="minutes"
                    min="1"
                    required
                    class="w-full p-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
                    placeholder="Enter duration"
                >
                <button
                    type="submit"
                    {{ 'disabled' if collecting else '' }}
                    class="bg-blue-600 text-white px-4 py-2 rounded-lg hover:bg-blue-700 transition-colors disabled:bg-gray-400 disabled:cursor-not-allowed"
                >
                    Start
                </button>
            </div>
        </form>

        <form action="/set_label" method="post" class="mb-6">
            <label for="label" class="text-gray-700 font-medium">Label:</label>
            <select
                id="label"
                name="label"
                onchange="this.form.submit()"
                class="w-full p-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
            >
                <option value="awake" {{ 'selected' if label=='awake' else '' }}>Awake</option>
                <option value="sleep" {{ 'selected' if label=='sleep' else '' }}>Sleep</option>
            </select>
        </form>

        <p class="text-gray-700">
            Remaining Time: <span id="remaining" class="font-mono text-blue-600">{{ remaining }}</span>
        </p>
    </div>

    <script>
        function updateRemainingTime() {
            fetch('/remaining')
                .then(response => {
                    if (!response.ok) {
                        throw new Error('Network response was not ok');
                    }
                    return response.text();
                })
                .then(time => {
                    document.getElementById('remaining').innerText = time;
                })
                .catch(error => {
                    console.error('Error fetching remaining time:', error);
                    document.getElementById('remaining').innerText = 'Error';
                });
        }

        // Update every second
        setInterval(updateRemainingTime, 1000);

        // Initial update
        updateRemainingTime();
    </script>
</body>
</html>
'''

# ================ Helper Functions ================
def calculate_remaining():
    global collecting_data, start_time, collection_duration
    if not collecting_data:
        return '0m 0s'
    elapsed = time.time() - start_time
    if elapsed >= collection_duration:
        stop_collection()
        return '0m 0s'
    rem = collection_duration - elapsed
    m, s = divmod(int(rem), 60)
    return f"{m}m {s}s"


def stop_collection():
    global collecting_data, csv_file, csv_writer
    with file_lock:
        collecting_data = False
        if csv_file:
            csv_file.close()
            print('Collection finished, file closed.')
            csv_file = None
            csv_writer = None

# ================ UDP Listener Thread ================
def udp_listener():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((SERVER_IP, SERVER_PORT))
    print(f"UDP listener started on {SERVER_IP}:{SERVER_PORT}")
    global collecting_data, start_time, collection_duration, csv_writer
    while True:
        data, addr = sock.recvfrom(1024)
        if not collecting_data or not csv_writer:
            continue
        # Check expiration
        if time.time() - start_time > collection_duration:
            stop_collection()
            continue
        try:
            text = data.decode().strip()
            parts = text.split(',')  # [ESP_ms,accX,...,ir]
            server_ts = datetime.now().strftime('%Y-%m-%d %H:%M:%S.%f')
            row = [server_ts] + parts + [label]
            with file_lock:
                csv_writer.writerow(row)
            # flush immediately
            if csv_file:
                csv_file.flush()
        except Exception as e:
            print('UDP parse error:', e)

# Start UDP listener
t = threading.Thread(target=udp_listener, daemon=True)
t.start()

# ================ Flask Routes ================
@app.route('/')
def index():
    rem = calculate_remaining()
    return render_template_string(HTML,
                                  collecting=collecting_data,
                                  remaining=rem,
                                  label=label)

@app.route('/start', methods=['POST'])
def start():
    global collecting_data, start_time, collection_duration, csv_file, csv_writer
    if not collecting_data:
        minutes = int(request.form['minutes'])
        collection_duration = minutes * 60
        start_time = time.time()
        # Prepare CSV file
        fname = f"data_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv"
        path = os.path.join(data_dir, fname)
        csv_file = open(path, 'w', newline='')
        csv_writer = csv.writer(csv_file)
        csv_writer.writerow([
            'ServerTime', 'ESP_ms',
            'AccelX','AccelY','AccelZ',
            'GyroX','GyroY','GyroZ',
            'IR', 'Label'
        ])
        collecting_data = True
        print(f"Started collection: {fname}")
    return redirect(url_for('index'))

@app.route('/set_label', methods=['POST'])
def set_label():
    global label
    label = request.form.get('label', 'awake')
    return redirect(url_for('index'))

@app.route('/remaining')
def remaining():
    return calculate_remaining()

# ================ Run App ================
if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
