#include <Arduino.h>
#include <WiFi.h>
//#include "WiFiMulti.h"
//#include <ESPmDNS.h>    //OTA
#include <WebServer.h>  //OTA
#include <Update.h>     //OTA

#include "data.h"       //OTA

WebServer server(80);         //OTA

void PaginaSimple() {           //OTA
  server.sendHeader("Connection", "close");
  server.send(200, "text/html", Pagina);
}

void ActualizarPaso1() {  
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", (Update.hasError()) ? "FALLA AL ACTUALIZAR, INTENTE NUEVAMENTE O CONTACTE CON SERVICIO TECNICO" : "ACTUALIZADO CON EXITO, YA PUEDE CERRAR ESTA PAGINA WEB");
  ESP.restart();
}

void ActualizarPaso2() {        //OTA
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.setDebugOutput(true);
    Serial.printf("Actualizando: %s\n", upload.filename.c_str());    
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("Actualización Exitosa: %u\nReiniciando...\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
    Serial.setDebugOutput(false);
  } else {
    Serial.printf("Problema con la Actualización (Problema con la conexión); Estado=%d\n", upload.status);
  }
  yield();
}

void initwebserver() {            //OTA

  if (WiFi.waitForConnectResult() == WL_CONNECTED) {
    //MDNS.begin(nombre);
    server.on("/", HTTP_GET, PaginaSimple);
    server.on("/actualizar", HTTP_POST, ActualizarPaso1, ActualizarPaso2);
    server.begin();
    //MDNS.addService("http", "tcp", 80);
    //Serial.printf("Listo!\nAbre http://%s.local en navegador\n", nombre);
    Serial.print("Abre el navegador en la IP: ");
    Serial.println(WiFi.localIP());    
  } 
}

void handle_server(){
  server.handleClient();
}

void endwebserver(){
  server.stop();
}