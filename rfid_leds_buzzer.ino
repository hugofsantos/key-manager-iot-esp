#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <LedControl.h>
#include <map>
#include <string>

#define BUZZER_PIN 4
// RFID
#define SS_PIN  5  // ESP32 pin GIOP5 
#define RST_PIN 27 // ESP32 pin GIOP27 
MFRC522 rfid(SS_PIN, RST_PIN);

// Wi-Fi 
const char* ssid = "imd0902";
const char* password = "imd0902iot";

// MQTT
const char* mqttBroker = "broker";
const int mqttPort = 1883;
const char* mqttUsername = "user";
const char* mqttPassword = "password";
const char* rfidTopic = "/rfid";
const char* liberarTopic = "/liberar";
const char* trancarTopic = "/trancar";

const int DIN_PIN = 13;
const int CS_PIN = 26;
const int CLK_PIN = 25;

std::map<std::string, int> salas;
LedControl display = LedControl(DIN_PIN, CLK_PIN, CS_PIN);

byte SALAS[8] =
{
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000,
  0b00000000
};

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void turnOnLine(const int line) {
  if (line >= 0 && line < 8) {
    SALAS[line] = 0b11111111;
  }
}

void turnOffLine(const int line) {
  if (line >= 0 && line < 8) {
    SALAS[line] = 0b00000000;
  }
}

void displayImage(const byte* image) {
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      display.setLed(0, i, j, bitRead(image[i], 7 - j));
    }
  }
}

void setupWiFi() {
  delay(10);
  Serial.println();
  Serial.print("Conectando-se à rede Wi-Fi ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Conexão Wi-Fi estabelecida");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

void setupMQTT() {
  mqttClient.setServer(mqttBroker, mqttPort);
  mqttClient.setCallback(callback);

  while (!mqttClient.connected()) {
    Serial.println("Conectando ao broker MQTT...");

    if (mqttClient.connect("ESP32Client", mqttUsername, mqttPassword)) {
      Serial.println("Conexão MQTT estabelecida");
      mqttClient.subscribe(liberarTopic);
      mqttClient.subscribe(trancarTopic);
    } else {
      Serial.print("Falha na conexão MQTT, erro: ");
      Serial.print(mqttClient.state());
      Serial.println(" Tentando novamente em 5 segundos...");
      delay(5000);
    }
  }
}

void setupLed() {
  display.clearDisplay(0);
  display.shutdown(0, false);
  display.setIntensity(0, 5); 
  
  // Configuração das salas
  salas["1"] = 0;
  salas["2"] = 1;
  salas["3"] = 2;
  salas["4"] = 3;
  salas["5"] = 4;
  salas["6"] = 5;
  salas["7"] = 6;
  salas["8"] = 7;
}

int getMatrixLineBySala(std::string sala) {
  std::map<std::string, int>::iterator it = salas.find(sala);
  if (it != salas.end()) {
    return it->second;
  }

  return -1;
}

void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Configuração RFID
  Serial.begin(9600);
  SPI.begin(); // init SPI bus
  rfid.PCD_Init(); // init MFRC522

  setupLed();
  setupWiFi();
  setupMQTT();

  Serial.println("Tap an RFID/NFC tag on the RFID-RC522 reader");
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensagem recebida no tópico: ");
  Serial.println(topic);

  Serial.print("Conteúdo da mensagem: ");
  
  std::string salaRecebida = "";
  for (int i = 0; i < length; i++) {
    salaRecebida.push_back((char)payload[i]); 
    Serial.print((char)payload[i]);
  }
  Serial.println();
  Serial.println(salaRecebida.c_str());

  const int salaLine = getMatrixLineBySala(salaRecebida);
   
  if(strcmp(topic, liberarTopic) == 0){
    turnOnLine(salaLine);
    
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(BUZZER_PIN, LOW);    
  }
  if(strcmp(topic, trancarTopic) == 0){
    turnOffLine(salaLine);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(BUZZER_PIN, LOW);  
  }

  displayImage(SALAS);
  Serial.println();  
}

void loop() {
  if (rfid.PICC_IsNewCardPresent()) { // new tag is available
    if (rfid.PICC_ReadCardSerial()) { // NUID has been read
      MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
      Serial.print("RFID/NFC Tag Type: ");
      Serial.println(rfid.PICC_GetTypeName(piccType));

      // print UID in Serial Monitor in the hex format
      Serial.print("UID:");
      String uidString;
      for (int i = 0; i < rfid.uid.size; i++) {
        uidString += String(rfid.uid.uidByte[i], HEX);
      }
      Serial.println(uidString);

      // Publish UID to MQTT topic
      mqttClient.publish(rfidTopic, uidString.c_str());

      rfid.PICC_HaltA(); // halt PICC
      rfid.PCD_StopCrypto1(); // stop encryption on PCD
    }
  }
  mqttClient.loop();
}
