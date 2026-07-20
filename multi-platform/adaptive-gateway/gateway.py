import asyncio
import paho.mqtt.client as mqtt
from aiocoap import *

# Konfigurasi
MQTT_BROKER = "broker.hivemq.com"
MQTT_TOPIC_SUBS = "sensor/jalur_hijau/#" # Topik untuk sensor PIR & MQ135
MQTT_TOPIC_PUB = "sensor/gateway/data"  # Topik untuk mengirim data gabungan

# 1. Fungsi Callback untuk Data Jalur Hijau
def on_message(client, userdata, msg):
    print(f"Data Jalur Hijau diterima dari {msg.topic}: {msg.payload.decode()}")
    # Anda bisa memproses data ini lebih lanjut di sini

# Setup MQTT Client
mqtt_client = mqtt.Client()
mqtt_client.on_message = on_message
mqtt_client.connect(MQTT_BROKER, 1883, 60)
mqtt_client.subscribe(MQTT_TOPIC_SUBS)
mqtt_client.loop_start() # Menjalankan listener di background

# 2. Fungsi untuk Mengambil Data Jalur Kuning (CoAP)
async def get_coap_data():
    protocol = await Context.create_client_context()
    # Ganti dengan IP ESP32 CoAP Anda
    request = Message(code=GET, uri='coap://192.168.74.20/suhu')
    try:
        response = await protocol.request(request).response
        return response.payload.decode('utf-8')
    except Exception as e:
        print(f"Gagal akses CoAP: {e}")
        return None

# 3. Main Loop
async def main():
    while True:
        # Ambil data CoAP
        coap_data = await get_coap_data()
        if coap_data:
            print(f"Data CoAP diterima: {coap_data}")
            # Publikasikan data CoAP ke broker agar bisa dibaca di dashboard
            mqtt_client.publish(MQTT_TOPIC_PUB, f"CoAP Data: {coap_data}")
            
        await asyncio.sleep(5) # Interval polling

if __name__ == "__main__":
    asyncio.run(main())
