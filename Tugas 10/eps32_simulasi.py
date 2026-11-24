import time
import random
import paho.mqtt.client as mqtt

# Konfigurasi Broker & Topik (NIM: 2410010179)
BROKER = "localhost"  # Ubah ke "broker.hivemq.com" jika menggunakan broker publik
PORT = 1883
NIM = "2410010179"

TOPIC_SUHU = f"iot/{NIM}/sensor/suhu"
TOPIC_RELAY = f"iot/{NIM}/aktuator/relay"
TOPIC_PWM = f"iot/{NIM}/aktuator/pwm"

# Status simulasi perangkat keras ESP32
gpio_26_relay = "OFF"
gpio_2_pwm = 0

def on_connect(client, userdata, flags, rc, properties=None):
    if rc == 0:
        print(f"[TERHUBUNG] Berhasil konek ke MQTT Broker ({BROKER})")
        client.subscribe(TOPIC_RELAY)
        client.subscribe(TOPIC_PWM)
        print(f"[SUBSCRIBE] Mendengarkan topik:\n  - {TOPIC_RELAY}\n  - {TOPIC_PWM}\n")
    else:
        print(f"[GAGAL] Koneksi gagal dengan kode: {rc}")

def on_message(client, userdata, msg):
    global gpio_26_relay, gpio_2_pwm
    topic = msg.topic
    payload = msg.payload.decode("utf-8").strip()

    if topic == TOPIC_RELAY:
        if payload in ["ON", "OFF"]:
            gpio_26_relay = payload
            status_pin = "HIGH (1)" if payload == "ON" else "LOW (0)"
            print(f"[AKTUATOR RELAY] Perintah: {payload} -> Mengatur GPIO 26 ke {status_pin}")
        else:
            print(f"[PERINGATAN] Perintah relay tidak valid: {payload}")

    elif topic == TOPIC_PWM:
        try:
            pwm_val = int(float(payload))
            if 0 <= pwm_val <= 255:
                gpio_2_pwm = pwm_val
                persentase = round((pwm_val / 255) * 100, 1)
                print(f"[AKTUATOR PWM]   Nilai: {pwm_val}/255 ({persentase}%) -> Mengatur kecerahan LED GPIO 2")
            else:
                print(f"[PERINGATAN] Nilai PWM di luar rentang (0-255): {pwm_val}")
        except ValueError:
            print(f"[ERROR] Payload PWM bukan angka: {payload}")

# Inisialisasi MQTT Client
client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"ESP32_Sim_{NIM}")
client.on_connect = on_connect
client.on_message = on_message

client.connect(BROKER, PORT, 60)
client.loop_start()

try:
    while True:
        # Simulasi pembacaan sensor suhu DHT22 (rentang 24.0°C - 34.0°C)
        suhu_dht22 = round(random.uniform(24.0, 34.0), 1)
        client.publish(TOPIC_SUHU, str(suhu_dht22))
        print(f"[SENSOR DHT22]   Publish ke {TOPIC_SUHU} -> Suhu: {suhu_dht22}°C | Status: Relay={gpio_26_relay}, PWM={gpio_2_pwm}")
        
        # Jeda pengiriman setiap 3 detik sesuai instruksi
        time.sleep(3)

except KeyboardInterrupt:
    print("\n[BERHENTI] Simulasi ESP32 dihentikan oleh pengguna.")
    client.loop_stop()
    client.disconnect()