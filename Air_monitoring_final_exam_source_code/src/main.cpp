#include <Arduino.h>
#include "secrets/wifi.h"
#include "wifi_connect.h"

#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "MQTT.h"

#include "secrets/mqtt.h"
#include "ca_cert_emqx.h"

// ===== PMS ====================
#include <Adafruit_PM25AQI.h>

// ===== BME680 + BSEC ==========
#include <Wire.h>
#include "bsec.h"

// GPS
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

// OLED
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "config/pins.h"
#include "config/topics.h"

// ======================================

WiFiClientSecure tlsClient;
PubSubClient mqttClient(tlsClient);
// PMS5003
Adafruit_PM25AQI aqi;
PM25_AQI_Data pmsData;

TwoWire I2CBUS = TwoWire(0);   // I2C bus 1 cho BME680

// BME680
Bsec iaqSensor;
// OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2CBUS, -1);

HardwareSerial SerialGPS(2); // Sử dụng UART 2
TinyGPSPlus gps;
// Create a mutex
SemaphoreHandle_t mqttMutex;

// =========================================================
//                     MQTT CALLBACK
// =========================================================
void mqttCallback(char* topic, byte* payload, unsigned int length)
{
    if (strcmp(topic, air_filter_topic) == 0)
    {
        char buf[length + 1];
        memcpy(buf, payload, length);
        buf[length] = '\0'; // chuyển payload thành chuỗi

        Serial.printf("Received on topic %s: %s\n", topic, buf);

        // ------------------------
        // XỬ LÝ BẬT TẮT LED
        // ------------------------
        if (strcasecmp(buf, "ON") == 0)       // Không phân biệt hoa/thường
        {
            digitalWrite(LED_PIN, HIGH);
            Serial.println("LED TURNED ON");
        }
        else if (strcasecmp(buf, "OFF") == 0)
        {
            digitalWrite(LED_PIN, LOW);
            Serial.println("LED TURNED OFF");
        }
        else
        {
            Serial.println("Unknown command!");
        }
    }
}



// =========================================================
//                 GỬI JSON CHO MÔ HÌNH AI
// =========================================================
void publishAIData(int pm25, int pm10, float pressure, float temp, float humid)
{
    StaticJsonDocument<200> doc;
    doc["PM25"]        = pm25;
    doc["PM10"]        = pm10;
    doc["Pressure"]    = pressure;
    doc["Temperature"] = temp;
    doc["Humidity"]    = humid;

    char buffer[200];
    serializeJson(doc, buffer);

    mqttClient.publish(publish_ai_topic, buffer);

    Serial.println("[AI JSON SENT]");
    Serial.println(buffer);
}


// =========================================================
//              TÍNH AQI CHUẨN VIỆT NAM (TCVN)
// =========================================================

typedef struct {
    int Il;
    float BP;
} AQITable;

// PM2.5 BP table
AQITable pm25Table[] = {
    {0,   0},
    {50,  25},
    {100, 50},
    {150, 80},
    {200, 150},
    {300, 250},
    {400, 350},
    {500, 500}
};

// PM10 BP table
AQITable pm10Table[] = {
    {0,   0},
    {50,  50},
    {100, 150},
    {150, 250},
    {200, 350},
    {300, 420},
    {400, 520},
    {500, 600}
};

int calcAQI(float C, AQITable* table, int size)
{
    for (int i = 0; i < size - 1; i++)
    {
        if (C >= table[i].BP && C <= table[i+1].BP)
        {
            float Il = table[i].Il;
            float Ih = table[i+1].Il;
            float BPl = table[i].BP;
            float BPh = table[i+1].BP;

            float aqi = ((Ih - Il) / (BPh - BPl)) * (C - BPl) + Il;
            return (int)round(aqi);
        }
    }

    return 500; // vượt max
}


// =========================================================
//                       PMS TASK
// =========================================================
// Global veriable
int lastPM25 = 0;
int lastPM10 = 0;
int aqi_final = 0;
int iaq = 0;
unsigned long start_time = 0;
const unsigned long duration = 300;

void pmsTask(void *param)
{
    for (;;)
    {
        if (aqi.read(&pmsData))
        {
            int pm1  = pmsData.pm10_standard;
            int pm25 = pmsData.pm25_standard;
            int pm10 = pmsData.pm100_standard;

            lastPM25 = pm25;
            lastPM10 = pm10;

            Serial.printf("[PMS] PM1=%d  PM2.5=%d  PM10=%d\n", pm1, pm25, pm10);
            
            xSemaphoreTake(mqttMutex, portMAX_DELAY);
            mqttClient.publish(pm1_topic,  String(pm1).c_str());
            mqttClient.publish(pm25_topic, String(pm25).c_str());
            mqttClient.publish(pm10_topic, String(pm10).c_str());
            xSemaphoreGive(mqttMutex);
            // ====================== TÍNH AQI ======================
            int aqi_pm25 = calcAQI(lastPM25, pm25Table, sizeof(pm25Table)/sizeof(AQITable));
            int aqi_pm10 = calcAQI(lastPM10, pm10Table, sizeof(pm10Table)/sizeof(AQITable));

            aqi_final = max(aqi_pm25, aqi_pm10);

            // Đã đổi topic thành aqi_cal_topic để dễ quản lý
            xSemaphoreTake(mqttMutex, portMAX_DELAY);
             mqttClient.publish(aqi_cal_topic, String(aqi_final).c_str()); 
            xSemaphoreGive(mqttMutex);
            Serial.printf("[AQI] PM2.5_AQI=%d  PM10_AQI=%d  =>  AQI=%d\n",aqi_pm25, aqi_pm10, aqi_final);

        // ======================================================
        }

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}


// =========================================================
//                       BME TASK
// =========================================================
void bmeTask(void *param)
{
    for (;;)
    {
        if (!iaqSensor.run())
        {
            Serial.println("[BME] Error reading sensor");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        iaq   = iaqSensor.iaq;
        float temp  = iaqSensor.temperature;
        float humid = iaqSensor.humidity;
        float press = iaqSensor.pressure / 100.0;

        Serial.printf("[BME] IAQ=%.1f  T=%.2f°C  H=%.2f%%  P=%.2f hPa\n",
                      iaq, temp, humid, press);
        // 
        xSemaphoreTake(mqttMutex, portMAX_DELAY);
        mqttClient.publish(iaq_topic,      String(iaq).c_str());
        mqttClient.publish(temp_topic,     String(temp).c_str());
        mqttClient.publish(humid_topic,    String(humid).c_str());
        mqttClient.publish(pressure_topic, String(press).c_str());
        xSemaphoreGive(mqttMutex);

        // JSON gửi AI
        publishAIData(lastPM25, lastPM10, press, temp, humid);

        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
void oledTask(void *param){
    for(;;){
            display.clearDisplay();
            display.setTextSize(2);
            display.setTextColor(WHITE);
            display.setCursor(0,10);
            display.printf("AQI: %d", aqi_final);
            display.setCursor(0,30);
            display.printf("IAQ: %d", iaq);
            display.display();
            vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}
void ledTask(void *param){
    for(;;){
            unsigned long current_time = millis();
            static bool ledState = false;
            if(aqi_final > 150){
                if(current_time - start_time >= duration){
                    ledState = !ledState;
                    digitalWrite(LED_Warning, ledState);
                    start_time = current_time;
                }
            }
            else{
                ledState = false;
                digitalWrite(LED_Warning, ledState);
            }
    }
            vTaskDelay(200/portTICK_PERIOD_MS);
}

// =========================================================
//                       GPS TASK
// =========================================================
void gpsTask(void *param)
{
    Serial.println("[GPS] Task started.");
    for (;;)
    {
        // Đọc dữ liệu từ Serial2 và decode bằng TinyGPSPlus
        while (SerialGPS.available() > 0)
        {
            gps.encode(SerialGPS.read());
        }

        // Kiểm tra xem vị trí có hợp lệ và đã cập nhật chưa
        if (gps.location.isUpdated() && gps.location.isValid())
        {
            float lat = gps.location.lat();
            float lon = gps.location.lng();

            Serial.printf("[GPS] Lat=%.6f, Lon=%.6f\n", lat, lon);
            
            // Gửi dữ liệu GPS lên MQTT dưới dạng JSON
            StaticJsonDocument<100> doc;
            doc["lat"] = lat;
            doc["lon"] = lon;

            char buffer[100];
            serializeJson(doc, buffer);
            xSemaphoreTake(mqttMutex, portMAX_DELAY);
            mqttClient.publish(gps_topic, buffer);
            xSemaphoreGive(mqttMutex);
            Serial.println("[GPS JSON SENT]");
        }
        else
        {
            Serial.println("[GPS] Waiting for fix...");
        }

        vTaskDelay(2000 / portTICK_PERIOD_MS); // Cập nhật vị trí mỗi 2 giây
    }
}
// =========================================================
//                          SETUP (Cập nhật)
// =========================================================
void setup()
{
    Serial.begin(115200);
    delay(200);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);
    pinMode(LED_Warning, OUTPUT);
    digitalWrite(LED_Warning, LOW);

    mqttMutex = xSemaphoreCreateMutex();

    setup_wifi(WiFiSecrets::ssid, WiFiSecrets::pass);
    tlsClient.setCACert(ca_cert);

    mqttClient.setServer(EMQX::broker, EMQX::port);
    mqttClient.setCallback(mqttCallback);

    // BME680 BSEC init
    I2CBUS.begin(BME_SDA, BME_SCL);
    iaqSensor.begin(0x77, I2CBUS);

    // OLED init
    I2CBUS.begin(OLED_SDA, OLED_SCL);

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("OLED init failed!");
        while (1);
    }
    Serial.println("OLED start to use !");
    display.clearDisplay();


    bsec_virtual_sensor_t sensorList[] = {
    BSEC_OUTPUT_IAQ,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_TEMPERATURE,
    BSEC_OUTPUT_SENSOR_HEAT_COMPENSATED_HUMIDITY,
    BSEC_OUTPUT_RAW_PRESSURE
    };

    iaqSensor.updateSubscription(sensorList, 4, BSEC_SAMPLE_RATE_LP);

    
    // PMS init (Sử dụng Serial1 - PMS_RX: 32, PMS_TX: 33)
    Serial1.begin(9600, SERIAL_8N1, PMS_RX, PMS_TX);
    if (!aqi.begin_UART(&Serial1))
    {
        Serial.println("PMS5003 NOT FOUND!");
        while (1);
    }
    Serial.println("PMS5003 OK!");
    
    // GPS init (Sử dụng Serial2 - GPS_RX: 16, GPS_TX: 17)
    SerialGPS.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
    Serial.println("GPS UART2 OK!");


    // Khởi tạo các Tasks
    xTaskCreatePinnedToCore(pmsTask, "PMSTask", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(bmeTask, "BMETask", 4096, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(gpsTask, "GPSTask", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(oledTask, "OLEDTask",4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(ledTask, "LEDTask", 2048, NULL, 1, NULL, 1);
}
// =========================================================
//                          LOOP
// =========================================================
void loop()
{
    MQTT::reconnect(
        mqttClient,
        "esp32-client",
        EMQX::username,
        EMQX::password,
        mqtt_subscribe_list,
        1
    );

    mqttClient.loop();
}
