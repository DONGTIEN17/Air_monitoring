import paho.mqtt.client as mqtt
from tensorflow.keras.models import load_model
import tensorflow as tf  #
import joblib
import json
import numpy as np
import time
import ssl
import sys


#        MODEL PATH
MODEL_PATH = 'lstm_aqi_model_window_1.h5'
SCALER_PATH = 'scaler_window_1.pkl'

#       MQTT CONFIG

BROKER_ADDRESS = "your_brokeraddress.com"
PORT = 8883
CLIENT_ID = "Laptop_AI_Predictor_AQI"
USERNAME = "uer_name"
PASSWORD = "password"

TOPIC_SUBSCRIBE_SENSOR = "airmonitoring/air_data"        
TOPIC_PUBLISH_AQI = "aqi/predicted"

#       CA CERT

CA_CERT_CONTENT = """
-----BEGIN CERTIFICATE-----
MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBG
MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP
BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV
5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY
1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4
NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG
Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91
8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe
pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl
MrY=
-----END CERTIFICATE-----
"""

#       LOAD MODEL + SCALER

try:
    
    custom_objects = {
        'mse': tf.keras.metrics.MeanSquaredError()
    }
    
    model = load_model(MODEL_PATH, custom_objects=custom_objects)
    
    scaler = joblib.load(SCALER_PATH)
    print("Mô hình và Scaler đã tải thành công!")
except Exception as e:
    print(" Lỗi tải mô hình:", e)
    sys.exit(1)

#    AQI predict function

def predict_aqi(pm25, pm10, pressure, temp, humid):

    raw = np.array([[0, pm25, pm10, pressure, temp, humid]])

    # Scale
    scaled = scaler.transform(raw)

    # LSTM input shape (samples, time_steps, features)
    lstm_input = scaled.reshape(1, 1, 6)

    # Predict
    scaled_pred = model.predict(lstm_input, verbose=0)

    tmp = np.zeros((1, 6))
    tmp[0, 0] = scaled_pred[0, 0]

    aqi_pred = scaler.inverse_transform(tmp)[0, 0]
    aqi_pred += 100

    return round(aqi_pred, 2)


#           MQTT CALLBACK

def on_connect(client, userdata, flags, reason, props):
    if reason == 0:
        print("MQTT Connected")
        client.subscribe(TOPIC_SUBSCRIBE_SENSOR, qos=1)
        print("Subscribed:", TOPIC_SUBSCRIBE_SENSOR)
    else:
        print(" MQTT fail:", reason)


def on_message(client, userdata, msg):
    try:
        data = json.loads(msg.payload.decode())

        pm25 = data["PM25"]
        pm10 = data["PM10"]
        pressure = data["Pressure"]
        temp = data["Temperature"]
        humid = data["Humidity"]

        aqi_pred = predict_aqi(pm25, pm10, pressure, temp, humid)

        payload = {
            "aqi_predicted": aqi_pred,
            "pm25": pm25,
            "pm10": pm10,
            "pressure": pressure,
            "temp": temp,
            "humidity": humid,
            "timestamp": int(time.time()),
            "source": CLIENT_ID
        }

        client.publish(TOPIC_PUBLISH_AQI, json.dumps(payload), qos=1)

        print("\n===== AI PREDICT =====")
        print(json.dumps(payload, indent=2))

    except Exception as e:
        print(" Error:", e)


#       MQTT CLIENT CONFIG

client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=CLIENT_ID)
client.username_pw_set(USERNAME, PASSWORD)

client.on_connect = on_connect
client.on_message = on_message

# TLS
context = ssl.create_default_context(ssl.Purpose.SERVER_AUTH, cadata=CA_CERT_CONTENT)
client.tls_set_context(context)

client.connect(BROKER_ADDRESS, PORT, 60)

print(" AI Prediction Engine Started...")
client.loop_forever()