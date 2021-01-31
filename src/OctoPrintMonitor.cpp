#include <WiFiClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include "OctoPrintMonitor.h"

const int JOB_DECODE_SIZE   = 1024;   // TODO
const int PRINT_DECODE_SIZE = 2048;   // TODO

void OctoPrintMonitor::setCurrentPrinter(String server, int port, String apiKey, String userName, String password, String type)
{
    this->apiKey = apiKey;
    this->server = server;
    this->userName = userName;
    this->password = password;
    this->port = port;
    this->type = type;
}

void OctoPrintMonitor::update()
{
    updateJobStatus();
    updatePrinterStatus();
}

void OctoPrintMonitor::updateJobStatus()
{
    String result;
    int httpCode;


    if(this->type == "moonraker")
    {
      httpCode = performAPIGet(MOONRAKER_INFO, result);
    }
    else
    {
      httpCode = performAPIGet(OCTOPRINT_JOB, result);
    }

    if(httpCode == 200)
    {
        data.validJobData = true;
        deserialiseJob(result);
    }
    else
    {
        data.validJobData = false;
    }
}

void OctoPrintMonitor::updatePrinterStatus()
{
    String result;
    int httpCode;


    if(this->type == "moonraker")
    {
      httpCode = performAPIGet(MOONRAKER_PRINTERINFO, result);
    }
    else {
      httpCode = performAPIGet(OCTOPRINT_PRINTER, result);
    }

    if(httpCode == 200)
    {
        data.validPrintData = true;
        deserialisePrint(result);
    }
    else
    {
        data.validPrintData = false;
    }
}

int OctoPrintMonitor::performAPIGet(String apiCall, String& payload)
{
    // must be in this order
    WiFiClient client;
    HTTPClient http;

    http.begin(client, this->server, this->port, apiCall);
    http.setTimeout(2000);
    http.addHeader("X-Api-Key", this->apiKey);

    if(this->userName != "")
    {
        http.setAuthorization(this->userName.c_str(), this->password.c_str());
    }

    int httpCode = http.GET();

    // Serial.print("server: ");
    // Serial.println(this->server);
    // Serial.print("port: ");
    // Serial.println(this->port);
    // Serial.print("apiCall: ");
    // Serial.println(apiCall);
    // Serial.print("HTTP CODE: ");
    // Serial.println(httpCode);

    if (httpCode > 0)
    {
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
        {
            payload = http.getString();
        }
    }

    return httpCode;
}

void OctoPrintMonitor::deserialiseJob(String payload)
{
    DynamicJsonDocument doc(JOB_DECODE_SIZE);
    deserializeJson(doc, payload);

    //Serial.print("deserialiseJob: ");
    //Serial.println(payload);

    data.jobState = (const char*)doc["state"];

    if(this->type == "moonraker")
    {

      if(doc["result"]["status"]["virtual_sdcard"]["is_active"] != false)
      {
        data.jobLoaded = true;
        data.estimatedPrintTime = doc["result"]["status"]["toolhead"]["estimated_print_time"];
        data.filamentLength = doc["result"]["status"]["print_stats"]["filament_used"];
        data.fileName = (const char*)doc["result"]["status"]["print_stats"]["filename"];

        data.percentComplete = doc["result"]["status"]["virtual_sdcard"]["progress"];
        data.printTimeElapsed = doc["result"]["status"]["print_stats"]["print_duration"];
        data.printTimeRemaining = doc["result"]["status"]["toolhead"]["estimated_print_time"];

        float fileProgress = doc["result"]["status"]["virtual_sdcard"]["progress"].as<float>();
        float progressPrintTime = doc["result"]["status"]["print_stats"]["print_duration"].as<float>();

        data.estimatedPrintTime = progressPrintTime / fileProgress;
        data.printTimeRemaining = progressPrintTime / fileProgress - data.printTimeElapsed;

      }
      else
      {
          data.jobLoaded = false;
      }

    }
    else {

      if(doc["job"]["file"]["display"] != nullptr)
      {
          data.jobLoaded = true;
          data.estimatedPrintTime = doc["job"]["estimatedPrintTime"];
          data.filamentLength = doc["job"]["filament"]["tool0"]["length"];
          data.fileName = (const char*)doc["job"]["file"]["display"];

          data.percentComplete = doc["result"]["status"]["virtual_sdcard"]["progress"];
          data.printTimeElapsed = doc["progress"]["printTime"];
          data.printTimeRemaining = doc["progress"]["printTimeLeft"];
      }
      else
      {
          data.jobLoaded = false;
      }

    }
}

void OctoPrintMonitor::deserialisePrint(String payload)
{
    DynamicJsonDocument doc(PRINT_DECODE_SIZE);
    deserializeJson(doc, payload);

    //Serial.print("deserialisePrint: ");
    //Serial.println(payload);

    if(this->type == "moonraker")
    {
      data.tool0Temp = doc["result"]["status"]["extruder"]["temperature"];
      data.tool0Target= doc["result"]["status"]["extruder"]["target"];

      data.bedTemp = doc["result"]["status"]["heater_bed"]["temperature"];
      data.bedTarget = doc["result"]["status"]["heater_bed"]["target"];

      data.printState = (const char*)doc["result"]["status"]["print_stats"]["state"];
      data.printerFlags = 0;

    }
    else
    {
      data.tool0Temp = doc["temperature"]["tool0"]["actual"];
      data.tool0Target= doc["temperature"]["tool0"]["target"];

      data.bedTemp = doc["temperature"]["bed"]["actual"];
      data.bedTarget = doc["temperature"]["bed"]["target"];

      data.printState = (const char*)doc["state"]["text"];
      data.printerFlags = 0;

      if(doc["state"]["flags"]["cancelling"])
      {
          data.printerFlags |= PRINT_STATE_CANCELLING;
      }
      if(doc["state"]["flags"]["closedOrError"])
      {
          data.printerFlags |= PRINT_STATE_CLOSED_OR_ERROR;
      }
      if(doc["state"]["flags"]["error"])
      {
          data.printerFlags |= PRINT_STATE_ERROR;
      }
      if(doc["state"]["flags"]["finishing"])
      {
          data.printerFlags |= PRINT_STATE_FINISHING;
      }
      if(doc["state"]["flags"]["operational"])
      {
          data.printerFlags |= PRINT_STATE_OPERATIONAL;
      }
      if(doc["state"]["flags"]["paused"])
      {
          data.printerFlags |= PRINT_STATE_PAUSED;
      }
      if(doc["state"]["flags"]["pausing"])
      {
          data.printerFlags |= PRINT_STATE_PAUSING;
      }
      if(doc["state"]["flags"]["printing"])
      {
          data.printerFlags |= PRINT_STATE_PRINTING;
      }
      if(doc["state"]["flags"]["ready"])
      {
          data.printerFlags |= PRINT_STATE_READY;
      }
      if(doc["state"]["flags"]["resuming"])
      {
          data.printerFlags |= PRINT_STATE_RESUMING;
      }
      if(doc["state"]["flags"]["sdReady"])
      {
          data.printerFlags |= PRINT_STATE_SD_READY;
      }
    }

}

