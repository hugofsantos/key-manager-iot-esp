#include <SPIFFS.h>
#include <FS.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define TOUCH_PIN 4
#define BUZZER_PIN 26
// RFID configuration
#define SS_PIN 5   // ESP32 pin GIOP5
#define RST_PIN 27 // ESP32 pin GIOP27

MFRC522 rfid(SS_PIN, RST_PIN);

// Wi-Fi and MQTT configuration
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

const char *ssid = "wifi_ssid";
const char *password = "wifi_password";
const char *mqttBroker = "mqtt_broker";
const int mqttPort = 1883;
const char *mqttUsername = "mqtt_username";
const char *mqttPassword = "mqtt_password";

const char *rfidTopic = "/rfid";
const char *cadastrarTopic = "/cadastrar";
const char *retiradaTopic = "/retirada";
const char *devolucaoTopic = "/devolucao";
const char *solicitarRetiradaTopic = "/solicitar_retirada";
const char *solicitarDevolucaoTopic = "/solicitar_devolucao";
const char *erroTopic = "/erro";

String uidString = ""; // Último RFID lido

// LCD configuration
LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C address 0x27, 16 columns and 2 rows

// SPIFFS Configuration
const char *logFileName = "/eventLogs";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "a.st1.ntp.br", -3 * 3600, 60000);

void giveRoom(std::string room)
{
  lcd.clear();
  lcd.print("Sala ");
  lcd.print(room.c_str());
  lcd.print(" liberada");

  digitalWrite(BUZZER_PIN, HIGH);
  delay(250);
  digitalWrite(BUZZER_PIN, LOW);
  delay(250);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(250);
  digitalWrite(BUZZER_PIN, LOW);

  logEvent("Sala " + String(room.c_str()) + " liberada para " + uidString);
}

void giveBackRoom(std::string room)
{
  lcd.clear();
  lcd.print("Sala ");
  lcd.print(room.c_str());
  lcd.print(" devolvida");

  digitalWrite(BUZZER_PIN, HIGH);
  delay(250);
  digitalWrite(BUZZER_PIN, LOW);
  delay(250);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(250);
  digitalWrite(BUZZER_PIN, LOW);

  logEvent("Sala " + String(room.c_str()) + " devolvida por " + uidString);
}

void requestRoom(std::string room)
{
  lcd.clear();
  lcd.print("Sala ");
  lcd.print(room.c_str());
  lcd.print(" solicita-");
  lcd.setCursor(0, 1);
  lcd.print("da ");

  digitalWrite(BUZZER_PIN, HIGH);
  delay(250);
  digitalWrite(BUZZER_PIN, LOW);

  logEvent("Sala " + String(room.c_str()) + " solicitada por " + uidString);
}

void requestGiveBackRoom(std::string room)
{
  lcd.clear();
  lcd.print("Devolucao da sa-");
  lcd.setCursor(0, 1);
  lcd.print("la ");
  lcd.print(room.c_str());
  lcd.print(" solicitada");

  digitalWrite(BUZZER_PIN, HIGH);
  delay(250);
  digitalWrite(BUZZER_PIN, LOW);
  logEvent("Solicitação da sala " + String(room.c_str()) + " feita por " + uidString);
}

void processError(std::string error_type)
{
  lcd.clear();
  if (error_type == "UNKNOWN_RFID")
  {
    lcd.print("RFID nao cadas-");
    lcd.setCursor(0, 1);
    lcd.print("trado!");

    logEvent("RFID " + uidString + " não cadastrado.");
  }
  if (error_type == "NO_RESERVATION")
  {
    lcd.print("Sem reserva nes-");
    lcd.setCursor(0, 1);
    lcd.print("se horario!");

    logEvent("RFID " + uidString + " não tem reserva nesse horário.");
  }

  digitalWrite(BUZZER_PIN, HIGH);
  delay(1000);
  digitalWrite(BUZZER_PIN, LOW);
}

void readAndPrintLogs()
{
  File logFile = SPIFFS.open(logFileName, "r");

  if (logFile)
  {
    String logEntry;

    while (logFile.available())
    {
      logEntry = logFile.readStringUntil('\n');
      Serial.println(logEntry);
    }

    logFile.close();
  }
}

void logEvent(const String &logMessage)
{
  File logFile = SPIFFS.open(logFileName, "a");

  if (logFile)
  {
    timeClient.forceUpdate();
    String logEntry = logMessage + "(" + timeClient.getFormattedTime() + ")";
    Serial.print("LOG SALVO: ");
    Serial.println(logEntry);

    logFile.println(logEntry);
    logFile.close();
  }
}

void formatFile()
{
  Serial.println("Formantando SPIFFS");
  SPIFFS.format();
  Serial.println("Formatou SPIFFS");
}

void setupWiFi()
{
  delay(10);
  Serial.println();
  Serial.print("Conectando-se à rede Wi-Fi ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Conexão Wi-Fi estabelecida");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());
}

void setupMQTT()
{
  mqttClient.setServer(mqttBroker, mqttPort);
  mqttClient.setCallback(callback);

  while (!mqttClient.connected())
  {
    Serial.println("Conectando ao broker MQTT...");

    if (mqttClient.connect("ESP32Client", mqttUsername, mqttPassword))
    {
      Serial.println("Conexão MQTT estabelecida");
      mqttClient.subscribe(retiradaTopic);
      mqttClient.subscribe(devolucaoTopic);
      mqttClient.subscribe(solicitarRetiradaTopic);
      mqttClient.subscribe(solicitarDevolucaoTopic);
      mqttClient.subscribe(erroTopic);
    }
    else
    {
      Serial.print("Falha na conexão MQTT, erro: ");
      Serial.print(mqttClient.state());
      Serial.println(" Tentando novamente em 5 segundos...");
      delay(5000);
    }
  }
}

void setupLCD()
{
  lcd.begin();
  lcd.backlight();
}

void setupSpiffs()
{
  // formatFile();
  if (SPIFFS.begin())
  { // Tentar abrir o arquivo de logs
    timeClient.begin();
    readAndPrintLogs();
  }
  else
    Serial.println("Falha ao montar o sistema de arquivos SPIFFS");
}

void setup()
{
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Configuração RFID
  Serial.begin(9600);
  SPI.begin();     // init SPI bus
  rfid.PCD_Init(); // init MFRC522

  setupLCD(); // Configuração do LCD
  setupWiFi();
  setupMQTT();
  setupSpiffs();

  Serial.println("Tap an RFID/NFC tag on the RFID-RC522 reader");
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.print("Mensagem recebida no tópico: ");
  Serial.println(topic);

  Serial.print("Conteúdo da mensagem: ");

  std::string msg = "";
  for (int i = 0; i < length; i++)
  {
    msg.push_back((char)payload[i]);
    Serial.print((char)payload[i]);
  }
  Serial.println();
  Serial.println(msg.c_str());

  if (strcmp(topic, retiradaTopic) == 0)
    giveRoom(msg); // RETIRADA
  if (strcmp(topic, devolucaoTopic) == 0)
    giveBackRoom(msg); // DEVOLUCAO
  if (strcmp(topic, solicitarRetiradaTopic) == 0)
    requestRoom(msg); // SOLICITAR RETIRADA
  if (strcmp(topic, solicitarDevolucaoTopic) == 0)
    requestGiveBackRoom(msg); // SOLICITAR DEVOLUCAO
  if (strcmp(topic, erroTopic) == 0)
    processError(msg); // ERRO
}

void loop()
{
  if (rfid.PICC_IsNewCardPresent())
  { // new tag is available
    if (rfid.PICC_ReadCardSerial())
    { // NUID has been read
      MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
      Serial.print("RFID/NFC Tag Type: ");
      Serial.println(rfid.PICC_GetTypeName(piccType));

      // print UID in Serial Monitor in the hex format
      Serial.print("UID:");
      uidString = "";
      for (int i = 0; i < rfid.uid.size; i++)
      {
        uidString += String(rfid.uid.uidByte[i], HEX);
      }
      Serial.println(uidString);

      // Publish UID to MQTT topic
      const boolean isTouched = touchRead(TOUCH_PIN) < 30;
      Serial.println(touchRead(TOUCH_PIN));
      mqttClient.publish(isTouched ? cadastrarTopic : rfidTopic, uidString.c_str());

      rfid.PICC_HaltA();      // halt PICC
      rfid.PCD_StopCrypto1(); // stop encryption on PCD
    }
  }
  mqttClient.loop();
}