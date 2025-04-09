//EX-1 yaha arduino jo hai ek server ki tarah kaam karta hai. jaise pahle esd read kiya aur rfid backened se data send karta hai status valid or invalid. if status is valid then open the gate otherwise don't open the gate. 
// it read the esd first then wait for the rfid for three minutes if in between the rfid is not come then it reset the sequence. and if rfid scans first then it ignores it and reset the sequence
#include <SPI.h>
#include <Ethernet.h>
#include <ArduinoJson.h>

#define ESD_PIN 4
#define RELAY_PIN 8

unsigned long detectionTimestamp = 0;
unsigned long timeout = 180000;
bool esdDetected = false;

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
IPAddress ip(192, 168, 1, 158);
EthernetServer server(5000);

void setup() {
    Ethernet.begin(mac, ip);
    server.begin();
    Serial.begin(9600);
    Serial.print("Server running at: ");
    Serial.println(Ethernet.localIP());

    pinMode(ESD_PIN, INPUT_PULLUP);
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, HIGH);
}

void resetSequence() {
    esdDetected = false;
    detectionTimestamp = 0;
    Serial.println("Sequence reset.");
}

void triggerRelay() {
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("Gate Opened");
    delay(1000);
    digitalWrite(RELAY_PIN, HIGH);
}

void checkEsdSignal() {
    if (digitalRead(ESD_PIN) == HIGH && !esdDetected) {
        esdDetected = true;
        detectionTimestamp = millis();
        Serial.println("ESD signal detected, waiting for validation...");
    }

    if (esdDetected && millis() - detectionTimestamp > timeout) {
        Serial.println("Timeout: No validation received, resetting...");
        resetSequence();
    }
}

void handleClientRequest() {
    EthernetClient client = server.available();
    if (client) {
        Serial.println("Client connected");

        String request = "";
        while (client.available()) {
            char c = client.read();
            request += c;
        }

        Serial.println("Received request: " + request);

        if (request.indexOf("POST /rfid") != -1) {
            int jsonStart = request.indexOf("{");
            int jsonEnd = request.lastIndexOf("}");
            if (jsonStart != -1 && jsonEnd != -1) {
                String jsonString = request.substring(jsonStart, jsonEnd + 1);
                StaticJsonDocument<200> doc;
                DeserializationError error = deserializeJson(doc, jsonString);

                if (!error) {
                    String status = doc["status"];
                    if (status == "valid" && esdDetected) {
                        Serial.println("Valid entry received. Opening gate...");
                        triggerRelay();
                        resetSequence();
                    } else if (status == "invalid") {
                        Serial.println("Invalid entry received. Not opening gate.");
                    } else {
                        Serial.println("Valid request but no ESD signal detected. Ignoring.");
                    }
                    client.println("HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nResponse received");
                } else {
                    Serial.println("JSON Parsing failed!");
                    client.println("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nInvalid JSON");
                }
            } else {
                Serial.println("Invalid Request Format!");
                client.println("HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nInvalid Request");
            }
        } else {
            client.println("HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nInvalid Endpoint");
        }
        
        client.stop();
        Serial.println("Client disconnected");
    }
}

void loop() {
    checkEsdSignal();
    handleClientRequest();
}
