"""
PlantWatch - Backend Flask
==========================
Executa em Python 3. Instale as dependências:
  pip install flask flask-cors pyserial

Como rodar:
  python3 app.py
"""

import json
import os
import threading
import time

import serial
import serial.tools.list_ports
from flask import Flask, jsonify, request
from flask_cors import CORS

# ── Configuração ──────────────────────────────────────────────────────────────

SERIAL_PORT  = None    # None = detecta automaticamente
BAUD_RATE    = 115200  # Receptor usa 115200
DATA_FILE    = "plants.json"

app = Flask(__name__, static_folder=".", static_url_path="")
CORS(app)

# ── Estado global ─────────────────────────────────────────────────────────────

latest_reading = {"temp": None, "air": None, "soil": None, "ts": None}
serial_status  = {"connected": False, "port": None, "error": None}

# ── Utilitários de arquivo ────────────────────────────────────────────────────

def load_plants():
    if not os.path.exists(DATA_FILE):
        return []
    with open(DATA_FILE, "r", encoding="utf-8") as f:
        return json.load(f)

def save_plants(plants):
    with open(DATA_FILE, "w", encoding="utf-8") as f:
        json.dump(plants, f, ensure_ascii=False, indent=2)

# ── Detecção automática de porta Arduino ──────────────────────────────────────

def find_arduino_port():
    ports = serial.tools.list_ports.comports()
    keywords = ["arduino", "ch340", "cp210", "ftdi", "usb serial", "acm", "usbmodem"]
    for p in ports:
        desc = (p.description or "").lower()
        if any(k in desc for k in keywords):
            return p.device
    for p in ports:
        if "ttyACM" in p.device or "ttyUSB" in p.device:
            return p.device
    return None

# ── Thread de leitura serial ──────────────────────────────────────────────────

def serial_reader():
    global latest_reading, serial_status

    while True:
        port = SERIAL_PORT or find_arduino_port()

        if not port:
            serial_status = {"connected": False, "port": None,
                             "error": "Arduino não encontrado. Verifique o cabo USB."}
            time.sleep(5)
            continue

        try:
            with serial.Serial(port, BAUD_RATE, timeout=3) as ser:
                serial_status = {"connected": True, "port": port, "error": None}
                print(f"[Serial] Conectado em {port}")

                # O receptor envia JSON ao final de cada relatório:
                # {"temp":25.4,"air":60.2,"soil":45.0}
                # As outras linhas (RTS, ACK, RELATORIO, etc.) são ignoradas.
                while True:
                    raw = ser.readline().decode("utf-8", errors="ignore").strip()
                    if not raw:
                        continue

                    print(f"[Serial] {raw}")

                    if raw.startswith("{"):
                        try:
                            data = json.loads(raw)
                            if "temp" in data and "air" in data and "soil" in data:
                                latest_reading = {
                                    "temp": float(data["temp"]),
                                    "air":  float(data["air"]),
                                    "soil": float(data["soil"]),
                                    "ts":   time.time(),
                                }
                                print(f"[Leitura] Temp={latest_reading['temp']}C  "
                                      f"Ar={latest_reading['air']}%  Solo={latest_reading['soil']}%")
                        except (json.JSONDecodeError, ValueError) as e:
                            print(f"[Serial] JSON invalido ({e}): {raw}")

        except serial.SerialException as e:
            serial_status = {"connected": False, "port": port,
                             "error": f"Erro de serial: {e}"}
            print(f"[Serial] Desconectado: {e}")
            time.sleep(5)

# ── Rotas da API ──────────────────────────────────────────────────────────────

@app.route("/")
def index():
    return app.send_static_file("index.html")

@app.route("/api/reading")
def get_reading():
    return jsonify({**latest_reading, "serial": serial_status})

@app.route("/api/plants", methods=["GET"])
def get_plants():
    return jsonify(load_plants())

@app.route("/api/plants", methods=["POST"])
def add_plant():
    data = request.get_json()
    required = ["name", "temp_min", "temp_max", "air_min", "air_max", "soil_min", "soil_max"]
    if not all(k in data for k in required):
        return jsonify({"error": "Campos obrigatórios faltando"}), 400

    plants = load_plants()
    plant = {
        "id":       int(time.time() * 1000),
        "name":     data["name"],
        "species":  data.get("species", ""),
        "notes":    data.get("notes", ""),
        "temp_min": float(data["temp_min"]),
        "temp_max": float(data["temp_max"]),
        "air_min":  float(data["air_min"]),
        "air_max":  float(data["air_max"]),
        "soil_min": float(data["soil_min"]),
        "soil_max": float(data["soil_max"]),
    }
    plants.append(plant)
    save_plants(plants)
    return jsonify(plant), 201

@app.route("/api/plants/<int:plant_id>", methods=["DELETE"])
def delete_plant(plant_id):
    plants = load_plants()
    plants = [p for p in plants if p["id"] != plant_id]
    save_plants(plants)
    return jsonify({"ok": True})

@app.route("/api/plants/<int:plant_id>", methods=["PUT"])
def update_plant(plant_id):
    data   = request.get_json()
    plants = load_plants()
    for p in plants:
        if p["id"] == plant_id:
            for key in ["name","species","notes","temp_min","temp_max",
                        "air_min","air_max","soil_min","soil_max"]:
                if key in data:
                    p[key] = data[key]
            save_plants(plants)
            return jsonify(p)
    return jsonify({"error": "Planta não encontrada"}), 404

@app.route("/api/plants/<int:plant_id>/check")
def check_plant(plant_id):
    plants = load_plants()
    plant  = next((p for p in plants if p["id"] == plant_id), None)
    if not plant:
        return jsonify({"error": "Planta não encontrada"}), 404

    r = latest_reading
    if r["temp"] is None:
        return jsonify({"error": "Sem leitura dos sensores"}), 503

    alerts = []
    status = "ok"

    def eval_param(value, mn, mx, label_low, label_high, unit):
        nonlocal status
        if value < mn:
            alerts.append({"type": "low",  "msg": label_low,  "value": value, "unit": unit})
            status = "alert"
        elif value > mx:
            alerts.append({"type": "high", "msg": label_high, "value": value, "unit": unit})
            status = "alert"

    eval_param(r["temp"], plant["temp_min"], plant["temp_max"],
               "Temperatura muito baixa — aqueça o ambiente.",
               "Temperatura muito alta — resfrie o ambiente.", "°C")

    eval_param(r["air"], plant["air_min"], plant["air_max"],
               "Umidade do ar muito baixa — use um umidificador.",
               "Umidade do ar muito alta — melhore a ventilação.", "%")

    eval_param(r["soil"], plant["soil_min"], plant["soil_max"],
               "Solo muito seco — regue a planta.",
               "Solo muito úmido — pare de regar e melhore a drenagem.", "%")

    return jsonify({
        "plant":   plant,
        "reading": r,
        "status":  status,
        "alerts":  alerts,
    })

# Inicialização 

if __name__ == "__main__":
    t = threading.Thread(target=serial_reader, daemon=True)
    t.start()
    print("PlantWatch rodando em http://localhost:5000")
    app.run(debug=False, host="0.0.0.0", port=5000)