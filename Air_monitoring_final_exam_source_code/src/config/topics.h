#pragma once
// PMS topic
const char* pm1_topic       = "airmonitoring/pm1";
const char* pm25_topic      = "airmonitoring/pm25";
const char* pm10_topic      = "airmonitoring/pm10";
// BME topic
const char* iaq_topic       = "airmonitoring/iaq";
const char* temp_topic      = "airmonitoring/temperature";
const char* humid_topic     = "airmonitoring/humidity";
const char* pressure_topic  = "airmonitoring/pressure";
// Virt topic
const char* publish_ai_topic = "airmonitoring/air_data";
const char* aqi_cal_topic = "airmonitoring/aqi_cal";

// GPS topic
const char* gps_topic = "airmonitoring/gps_location";
// filter topic
const char* air_filter_topic = "airmonitoring/air_filter";
const char* mqtt_subscribe_list[] = { air_filter_topic };