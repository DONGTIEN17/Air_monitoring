# Air Monitoring System

## Problem Statement
### Real-World Demand

In the face of rapid urbanization, air quality has become a significant concern impacting public health. Traditional methods of air monitoring are often insufficient, leading to delayed responses to pollution and hazardous conditions. This highlights the urgent need for an intelligent air monitoring system capable of:

_ Multi-Parameter Monitoring: Continuously tracking crucial environmental indicators such as PM2.5, Temperature, Humidity, Pressure, and VOC levels.

_ Enhanced Safety Alerts: Automatically detecting and warning of potential risks like high pollution levels, toxic gas leaks, or abnormal temperature changes.

_ Automation and Accessibility: Developing a cost-effective, easy-to-deploy system that makes advanced environmental monitoring accessible to a broader range of users.

### Inspiration for the Project

Toxic Gas Risks: Incidents of gas leaks and poor air quality in urban areas underscore the importance of automated detection and alerts.

Data Stability: Many existing systems struggle with accuracy and real-time data display, making reliable sensors and user-friendly interfaces crucial for better decision-making.

## Main Content
### Hardware Components
#### Hardware List
Component	Quantity	Function :

_ESP32 DevKit	Central processing, control, and network connectivity.

_BME680 Environmental Sensor	1	Measures Temperature, Humidity, Pressure, and VOC/Gas Index.

_ PMS5003 Dust Sensor	1	Monitors PM1, PM2.5, PM10 concentrations.

_GPS Module (NEO-6M / ATGM336H)	1	Provides GPS coordinates (Latitude, Longitude, Satellite count).

_OLED Display (I2C)	1	Displays real-time environmental data.

_LED Warning Indicator	1	Alerts when air quality thresholds are exceeded.

_Cables + Power Source	—	For wiring and powering the system.

### Software Overview

#### Node-RED Dashboard

Visualization: Displays real-time data for PM1, PM2.5, PM10, Temperature, Humidity, Pressure, location of monitor, AQI, IAQ .

Management: Set alert thresholds, control system status, and monitor historical data.

Safety Monitoring: Receive alerts for high pollution levels, toxic gases, and abnormal temperature or humidity.

#### MQTT Broker

EMQX used for real-time data transmission.

ESP32 publishes sensor data to the broker.

Node-RED subscribes to this data and updates the dashboard accordingly.

Virtual machine subcribes and publishes value

#### Development Environment

PlatformIO with VS Code.

Virtual Machine (VMWare or Install on your window)


#### System Concept

Data Collection:

PMS5003 measures particulate matter (PM2.5, PM10).

BME680 captures temperature, humidity, pressure, and VOC levels.

GPS module provides location coordinates.

Local Processing:

The ESP32 processes sensor data, checks against thresholds, and updates the OLED display.

The LED indicator provides a visual warning when air quality falls below safe levels.

Cloud Communication:

ESP32 sends data to the MQTT broker.

Node-RED retrieves and visualizes this data on the dashboard.

Virtual Machine subcribes and publishes to predict the AQI

Visualization & Alerts:

The dashboard shows real-time data and alerts for any abnormal conditions.

Users can set thresholds and receive notifications for environmental hazards.

## AI model

In this project I used LSTM model to predict the AQI in next hours.
![Here is the compare table between real index and predicted index](images/chart_real_pred.png)
![Evaluation](images/Evaluate_index.png)

## Block Diagram 

![1. Block Diagram](images/Block_Diagram.png)
![2. Algorithm Flow Chart](Images/Algorithm_Flow_Chart.png)
## Pin Diagram
![3. Pin Diagram](images/PIN_Diagram.png)

## Result

