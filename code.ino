#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

WiFiUDP Udp;
Adafruit_MCP4725 dac;

const char* ssid = "WiFi_SSID";          //Enter your WiFi SSID
const char* password = "PASSWORD";    //Enter your WiFi password
int maxUploadSpeed = 1;               //Max upload speed (in Mbps) on the scale
int maxDownloadSpeed = 7;             //Max download speed (in Mbps) on the scale

double netSpeed = 0;
int selectSwitch = 14;
int dacValue = 0;
int lengthRead;
int const incomingBufferSize = 135;
char incomingPacket[incomingBufferSize];

int const requestSize = 42;
byte downloadRequestPacekt[requestSize] = { 0x30, // SNMP Message type = Sequence
                                         0x28, // SNMP Message Length = 40
                                         0x02, // Version Type = Integer
                                         0x01, // Version Length = 1
                                         0x00, // Version Number = 0
                                         0x04, // SNMP Community String Type = Octet String
                                         0x06, // SNMP Community String Length = 6
                                         0x70, // p
                                         0x75, // u
                                         0x62, // b
                                         0x6C, // l
                                         0x69, // i
                                         0x63, // c
                                         0xA0, // SNMP PDU Type = GetRequest
                                         0x1B, // SNMP PDU Length = 27
                                         0x02, // Request ID Type = Integer
                                         0x01, // Request ID Length = 1
                                         0x01, // Request ID Value = 1 
                                         0x02, // Error Tyne = Integer
                                         0x01, // Error Length = 1
                                         0x00, // Error Value = 0
                                         0x02, // Error Index Type = Integer
                                         0x01, // Error Index Length  = 1
                                         0x00, // Error Index Value = 0
                                         0x30, // Varbind List Type = Sequence
                                         0x10, // Varbind List Length = 16
                                         0x30, // Varbind type = Sequence
                                         0x0E, // Varbind Length = 14
                                         0x06, // Object Identifier Type = Object Identifier
                                         0x0A, // Object Identifier Length = 10
                                         0x2B, // ISO.3
                                         0x06, // 6
                                         0x01, // 1
                                         0x02, // 2
                                         0x01, // 1
                                         0x02, // 2
                                         0x02, // 2
                                         0x01, // 1
                                         0x0A, // 10 (Upload)
                                         0x0A, // 10 
                                         0x05, // Value Type = Null
                                         0x00 }; // Value Length = 0

byte uploadRequestPacekt[requestSize] = { 0x30, // SNMP Message type = Sequence
                                         0x28, // SNMP Message Length = 40
                                         0x02, // Version Type = Integer
                                         0x01, // Version Length = 1
                                         0x00, // Version Number = 0
                                         0x04, // SNMP Community String Type = Octet String
                                         0x06, // SNMP Community String Length = 6
                                         0x70, // p
                                         0x75, // u
                                         0x62, // b
                                         0x6C, // l
                                         0x69, // i
                                         0x63, // c
                                         0xA0, // SNMP PDU Type = GetRequest
                                         0x1B, // SNMP PDU Length = 27
                                         0x02, // Request ID Type = Integer
                                         0x01, // Request ID Length = 1
                                         0x02, // Request ID Value = 2 (Should be different for different request message)
                                         0x02, // Error Tyne = Integer
                                         0x01, // Error Length = 1
                                         0x00, // Error Value = 0
                                         0x02, // Error Index Type = Integer
                                         0x01, // Error Index Length = 1
                                         0x00, // Error Index Value = 0
                                         0x30, // Varbind List Type = Sequence
                                         0x10, // Varbind List Length = 16
                                         0x30, // Varbind Type = Sequence
                                         0x0E, // Varbind Length = 14
                                         0x06, // Object Identifier Type = Object Identifier
                                         0x0A, // Object Identifier Length = 10
                                         0x2B, // ISO.3
                                         0x06, // 6
                                         0x01, // 1
                                         0x02, // 2
                                         0x01, // 1
                                         0x02, // 2
                                         0x02, // 2
                                         0x01, // 1
                                         0x10, // 16
                                         0x0A, // 10
                                         0x05, // Value Type = Null
                                         0x00 }; // Value Length = 0

unsigned long uploadValueLast = 0;
unsigned long downloadValueLast = 0;
unsigned long uploadTimeLast = 0;
unsigned long downloadTimeLast = 0;
unsigned long millisLast = 0;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.print("Connecting..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    }
  Serial.println("Connected!");
  pinMode(selectSwitch, INPUT_PULLUP);
  Udp.begin(161);
  dac.begin(0x60);
}

void loop() {
  unsigned long millisNow = millis();
  checkForReply(millisNow);    
  if((millisNow - millisLast) > 500){
    if(digitalRead(selectSwitch) == LOW){
      sendUploadRequest();
      sendUploadRequest();
    }
    else{
      sendDownloadRequest();
      sendDownloadRequest();
    }
    millisLast = millisNow;
  }
  updateMeter();
  delay(40);
}

void sendUploadRequest() {
  Udp.beginPacket(IPAddress(192, 168, 1, 1), 161);
  Udp.write(uploadRequestPacekt, requestSize);
  //Serial.println("Download Request Sent");
  Udp.endPacket();
}

void sendDownloadRequest() {
  Udp.beginPacket(IPAddress(192, 168, 1, 1), 161);
  Udp.write(downloadRequestPacekt, requestSize);
  //Serial.println("Upload Request Sent");
  Udp.endPacket();
}

int checkForReply(unsigned long inNow) {
  if (Udp.parsePacket()>0) {
    Serial.println(Udp.parsePacket());
    lengthRead = Udp.read(incomingPacket, incomingBufferSize);
    Udp.read(incomingPacket, incomingBufferSize);
    //printResponse();                            //Uncomment to see the response
    int pointer = 49;                             //49th byte tells the length of data 
    int dataLength = incomingPacket[pointer];
    pointer += 1;
    unsigned long value = 0;
    
    for (int i = 0; i < dataLength; i++) {
      value = (value<<8) | incomingPacket[pointer + i];
    }
    netSpeed = (double) 8/1000 * (value-uploadValueLast) / (inNow-uploadTimeLast);
    uploadValueLast = value;
    uploadTimeLast = inNow;
    Serial.println(netSpeed);
  }
  return false; 
}

void updateMeter(){
  double rate = netSpeed * 100;
  int netSpeedInt = (int)rate;
  if(digitalRead(selectSwitch) == LOW){
    dacValue = map(netSpeedInt, 0, maxUploadSpeed*100, 100, 4095);
  }
  else{
    dacValue = map(netSpeedInt, 0, maxDownloadSpeed*100, 100, 4095);
  }
  dac.setVoltage(dacValue, false);
  delay(30);
}

void printResponse(){
  for(int i = 0; i < lengthRead; i++){
    Serial.print(incomingPacket[i], HEX);
    Serial.print(" "); 
  }
}
