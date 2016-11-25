/* 
  Cardboard BattleBot Control Firmware
  Copyright (c) 2016 Jeff Malins. All rights reserved.
  
  Adapted from FSBrowser - Example WebServer with SPIFFS backend for esp8266
  Copyright (c) 2015 Hristo Gochkov.
 
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*
  upload the contents of the data folder with MkSPIFFS Tool ("ESP8266 Sketch Data Upload" in Tools menu in Arduino IDE)
  or you can upload the contents of a folder if you CD in that folder and run the following command:
  for file in `ls -A1`; do curl -F "file=@$PWD/$file" esp8266fs.local/edit; done
  
  access the sample web page at http://esp8266fs.local
  edit the page by going to http://esp8266fs.local/edit
*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <stdlib.h>

#define DBG_OUTPUT_PORT Serial

/********************************************************************************
 * Network Configuration                                                        *
 ********************************************************************************/
 
#define SERVER_PORT 80
#define HOST_NAME   "battlebot"

/********************************************************************************
 * WiFi Setup                                                                   *
 *  Implement flexible WiFi setup. The default is create an access point for    *
 *  the controller device to connect to. This is inconvenient for development,  *
 *  however. If a file called `wifi.config` is present in the file system,      *
 *  then the controller will instead connect to an existing WiFi network.       *
 *                                                                              *
 *  The format of `wifi.config` is one line:                                    *
 *  SSID:password                                                               *
 ********************************************************************************/

#define CONFIG_FILE "/wifi.config"

// configure and connect to wifi //
void setupWiFi() {
  // check for config file //
  String ssid, password;
  if(SPIFFS.exists(CONFIG_FILE)) {
    DBG_OUTPUT_PORT.println("WiFi configuration found");
 
    File file = SPIFFS.open(CONFIG_FILE, "r");
    String contents = file.readString();
    int index = contents.indexOf(":");
    ssid     = contents.substring(0, index);
    password = contents.substring(index + 1);
    file.close();
  }

  if(ssid) {
    // connect to WiFi network //
    DBG_OUTPUT_PORT.printf("Connecting to \'%s\'...\n", ssid.c_str());
    if (String(WiFi.SSID()) != ssid) {
      WiFi.begin(ssid.c_str(), password.c_str());
    }
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      DBG_OUTPUT_PORT.print(".");
    }
  } else {
    DBG_OUTPUT_PORT.println("FIXME: access point stuff here");
  }
  
  DBG_OUTPUT_PORT.println("");
  DBG_OUTPUT_PORT.print("Connected! IP address: ");
  DBG_OUTPUT_PORT.println(WiFi.localIP());

  // start mDNS //
  MDNS.begin(HOST_NAME);
  DBG_OUTPUT_PORT.print("Open http://");
  DBG_OUTPUT_PORT.print(HOST_NAME);
  DBG_OUTPUT_PORT.println(".local/ to access user interface");
}

/********************************************************************************
 * Web Server                                                                   *
 *  Implementation of web server methods to server up the UI web resources and  *
 *  allow updating of modifications during development.                         *
 ********************************************************************************/

// web server instance //
ESP8266WebServer server(SERVER_PORT);

// file handle for the current upload //
File fsUploadFile;

// format file size as a human-readable string //
String formatFileSize(size_t bytes){
  if (bytes < 1024){
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)){
    return String(bytes/1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)){
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
}

// infer the content type from the file extension //
String getContentType(String filename){
  if (server.hasArg("download"))       return "application/octet-stream";
  else if (filename.endsWith(".htm"))  return "text/html";
  else if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css"))  return "text/css";
  else if (filename.endsWith(".js"))   return "application/javascript";
  else if (filename.endsWith(".png"))  return "image/png";
  else if (filename.endsWith(".gif"))  return "image/gif";
  else if (filename.endsWith(".jpg"))  return "image/jpeg";
  else if (filename.endsWith(".ico"))  return "image/x-icon";
  else if (filename.endsWith(".xml"))  return "text/xml";
  else if (filename.endsWith(".pdf"))  return "application/x-pdf";
  else if (filename.endsWith(".zip"))  return "application/x-zip";
  else if (filename.endsWith(".gz"))   return "application/x-gzip";
  return "text/plain";
}

// serve the contents of a file back to client //
bool handleFileRead(String path){
  DBG_OUTPUT_PORT.println("handleFileRead: " + path);
  if (path.endsWith("/")) path += "index.html";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if (SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if (SPIFFS.exists(pathWithGz))
      path += ".gz";
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

// update the contents of a file in the file system //
void handleFileUpload(){
  if (server.uri() != "/edit") return;
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) filename = "/"+filename;
    DBG_OUTPUT_PORT.print("handleFileUpload Name: "); DBG_OUTPUT_PORT.println(filename);
    fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    DBG_OUTPUT_PORT.print("handleFileUpload Data: "); DBG_OUTPUT_PORT.println(upload.currentSize);
    if (fsUploadFile)
      fsUploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (fsUploadFile) 
      fsUploadFile.close();
    DBG_OUTPUT_PORT.print("handleFileUpload Size: "); DBG_OUTPUT_PORT.println(upload.totalSize);
  }
}

// delete a file from the file system //
void handleFileDelete(){
  if (server.args() == 0) return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  DBG_OUTPUT_PORT.println("handleFileDelete: " + path);
  if (path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if (!SPIFFS.exists(path))
    return server.send(404, "text/plain", "FileNotFound");
  SPIFFS.remove(path);
  server.send(200, "text/plain", "");
  path = String();
}

// return the contents of a directory in the file system as JSON //
void handleFileList() {
  if (!server.hasArg("dir")) {server.send(500, "text/plain", "BAD ARGS"); return;}
    
  String path = server.arg("dir");
  DBG_OUTPUT_PORT.println("handleFileList: " + path);
  Dir dir = SPIFFS.openDir(path);
  path = String();

  String output = "[";
  while (dir.next()){
    File entry = dir.openFile("r");
    if (output != "[") output += ',';
    bool isDir = false;
    output += "{\"type\":\"";
    output += (isDir)?"dir":"file";
    output += "\",\"name\":\"";
    output += String(entry.name()).substring(1);
    output += "\"}";
    entry.close();
  }
  output += "]";
  server.send(200, "text/json", output);
}

/*
void handleFileCreate(){
  if (server.args() == 0)
    return server.send(500, "text/plain", "BAD ARGS");
  String path = server.arg(0);
  DBG_OUTPUT_PORT.println("handleFileCreate: " + path);
  if (path == "/")
    return server.send(500, "text/plain", "BAD PATH");
  if (SPIFFS.exists(path))
    return server.send(500, "text/plain", "FILE EXISTS");
  File file = SPIFFS.open(path, "w");
  if (file)
    file.close();
  else
    return server.send(500, "text/plain", "CREATE FAILED");
  server.send(200, "text/plain", "");
  path = String();
}
*/

/********************************************************************************
 * Hardware Control                                                             *
 *  Handle control of robot hardware based on calls to the web API.             *
 ********************************************************************************/

// hardware definitions //
#define PIN_R_PWM   5   // 1,2EN aka D1 pwm A
#define PIN_R_DIR   0   // 1A,2A aka D3 dir A
#define PIN_L_PWM   4   // 1,2EN aka D1 pwm B
#define PIN_L_DIR   2   // 1A,2A aka D3 dir B

#define PIN_LED1    2   // note: conflicts with dir B
#define PIN_LED2    16

// configure the hardware //
void setupHardware() {
  // motor control pins to output //
  pinMode(PIN_L_PWM, OUTPUT);
  pinMode(PIN_L_DIR, OUTPUT);
  pinMode(PIN_R_PWM, OUTPUT);
  pinMode(PIN_R_DIR, OUTPUT);

  // LED to output //
  pinMode(PIN_LED2, OUTPUT);
  setLED(false);
}

// set the debugging LED //
void setLED(bool active) {
  digitalWrite(PIN_LED2, !active);
}

// set power to the wheels //
void setWheelPower(int left, int right) {
  left  = constrain(left,  -1023, 1023);
  right = constrain(right, -1023, 1023);

  digitalWrite(PIN_L_DIR, left >= 0);
  digitalWrite(PIN_R_DIR, left >= 0);
  
  analogWrite(PIN_L_PWM, abs(left));
  analogWrite(PIN_R_PWM, abs(right));
}

// interpret data PUT to the control endpoint //
//  format: "${leftPower}:${rightPower}"
//
//  where:  leftPower  - int [-1023, 1023]
//          rightPower - int [-1023, 1023]
//
//  positive values are forward 
void handleControlPut(){
  String body = server.arg("plain"); // "plain" is the PUT body //
  DBG_OUTPUT_PORT.println("handleControlPut: " + body);

  int index = body.indexOf(":");
  int i = body.substring(0, index).toInt();
  int j = body.substring(index + 1).toInt();  
  
  DBG_OUTPUT_PORT.print("i: "); DBG_OUTPUT_PORT.print(i); 
  DBG_OUTPUT_PORT.print(", j: "); DBG_OUTPUT_PORT.println(j); 

  setWheelPower(i, j);
  
  server.send(200, "text/plain", "");
  body = String();
}

/********************************************************************************
 * Main Entry Points                                                            *
 *  Configure the hardware, connect to WiFi, start mDNS and setup web server    *
 *  route handling.                                                             *
 ********************************************************************************/

void setup(void){
  setupHardware();

  // configure debug serial port //
  DBG_OUTPUT_PORT.begin(115200);
  DBG_OUTPUT_PORT.print("\n");
  DBG_OUTPUT_PORT.setDebugOutput(true);

  // start file system and debug contents //
  SPIFFS.begin();
  {
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {    
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      DBG_OUTPUT_PORT.printf("FS File: %s, size: %s\n", fileName.c_str(), formatFileSize(fileSize).c_str());
    }
    DBG_OUTPUT_PORT.printf("\n");
  }
  
  setupWiFi();
  
  // web server control, file browser routes //
  server.on("/list", HTTP_GET, handleFileList);
  server.on("/edit", HTTP_GET, [](){
    if(!handleFileRead("/edit.htm")) server.send(404, "text/plain", "FileNotFound");
  });
  //server.on("/edit", HTTP_PUT, handleFileCreate);
  server.on("/edit", HTTP_DELETE, handleFileDelete);
  server.on("/edit", HTTP_POST, 
    [](){ server.send(200, "text/plain", ""); }, 
    handleFileUpload
  );

  // hardware control routes //
  server.on("/control", HTTP_PUT, handleControlPut);
  
  // use the not-found route to server up arbitrary files //
  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });

  // we're ready //
  server.begin();
  DBG_OUTPUT_PORT.println("HTTP server started");
}
 
void loop(void){
  server.handleClient();
}