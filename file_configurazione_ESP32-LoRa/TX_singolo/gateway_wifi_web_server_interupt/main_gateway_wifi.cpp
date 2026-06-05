// RX Gateway — Heltec V3
// Strategia: RadioLib in modalità ASINCRONA con interrupt su DIO1
// La radio rimane SEMPRE in ascolto; un flag volatile segnala al loop() quando è arrivato un pacchetto.
// Così ThingSpeak non causa mai packet loss.

#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <U8g2lib.h>
#include <WiFiManager.h>
#include <ThingSpeak.h>
#include <WiFi.h>

//  IDENTITÀ 
#define NODE_ID 100
#define LORA_SYNC_WORD 0x12   // Sync Word per LoRa (deve essere lo stesso sia su TX che su RX)

// PIN MAP HELTEC V3
#define PIN_LORA_NSS  8
#define PIN_LORA_SCK  9
#define PIN_LORA_MISO 11
#define PIN_LORA_MOSI 10
#define PIN_LORA_NRST 12
#define PIN_LORA_BUSY 13
#define PIN_LORA_DIO1 14

#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_RST 21
#define PIN_VEXT 36

//  PARAMETRI RADIO 
#define FREQ  [inserisci la frequenza in MHz]f
#define BW    [inserisci la banda in kHz]f
#define SF    [inserisci SF]
#define CR    5

//  THINGSPEAK
unsigned long myChannelNumber = [inserisci ID canale];
const char*   myWriteAPIKey   = "[inserisci chiave di scrittura canale]";
#define CLOUD_INTERVAL_MS 20000UL   // Intervallo minimo tra invii cloud (20s)

//  COSTRUTTORI 
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RST, OLED_SCL, OLED_SDA);

SPIClass    radioSPI(HSPI);
SPISettings spiSettings(2000000, MSBFIRST, SPI_MODE0);
Module*     mod   = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_NRST, PIN_LORA_BUSY, radioSPI, spiSettings);
SX1262      radio = mod;

WiFiClient client;

//  STRUTTURA DATI 
struct __attribute__((packed)) GPSPacket {
  uint16_t id_TX;
  uint16_t id_pacchetto;
  float    lat;
  float    lon;
  uint8_t  batt;
};
GPSPacket gpsData;

//  FLAG INTERRUPT 
volatile bool packetReceived = false;

// Callback chiamata dall'interrupt hardware su DIO1 (in ~microsecondi).
// IRAM_ATTR: forza la funzione in RAM interna così è sempre raggiungibile,
// anche durante operazioni flash (es. WiFi).
IRAM_ATTR void onPacketReceived() {
  packetReceived = true;
}

//  BUFFER CLOUD 
int    p_count         = 0;
String str_id_TX       = "";
String str_id_pacchetto= "";
String str_lat         = "";
String str_lon         = "";
String str_rssi        = "";
String str_snr         = "";

unsigned long lastCloudUpdate = 0;

//  DISPLAY 
void showOLED(const char* line1, const char* line2 = nullptr, const char* line3 = nullptr) {
  u8g2.clearBuffer();
  if (line1) u8g2.drawStr(0, 12, line1);
  if (line2) u8g2.drawStr(0, 24, line2);
  if (line3) u8g2.drawStr(0, 36, line3);
  u8g2.sendBuffer();
}

void resetBuffer() {    // Altrimenti continua a chiedere di aumentare il buffer e creava problemi
  p_count = 0;
  str_id_TX = "";          str_id_TX.reserve(100);
  str_id_pacchetto = "";   str_id_pacchetto.reserve(100);
  str_lat = "";            str_lat.reserve(300); // 300 byte sono circa 25 pacchetti di sola latitudine
  str_lon = "";            str_lon.reserve(300);
  str_rssi = "";           str_rssi.reserve(100);
  str_snr = "";            str_snr.reserve(100);
}


void setup() {
  Serial.begin(115200);

  // 1. Alimentazione Vext (OLED + sensori)
  pinMode(PIN_VEXT, OUTPUT);
  digitalWrite(PIN_VEXT, LOW);
  delay(100);

  // 2. Display
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);
  showOLED("HELTEC V3 RX BOOT");

  // 3. WiFiManager
  WiFiManager wm;
  if (!wm.autoConnect("Heltec_Gateway_AP")) {
    Serial.println("WiFi fallito — riavvio");
    showOLED("WIFI FALLITO!", "Riavvio...");
    delay(3000);
    ESP.restart();
  }

  Serial.println("WiFi connesso: " + WiFi.localIP().toString());
  showOLED("WIFI OK!", WiFi.localIP().toString().c_str());
  delay(2000);

  // 4. ThingSpeak
  ThingSpeak.begin(client);

  // 5. SPI LoRa
  radioSPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);

  // 6. Inizializzazione radio
  Serial.print(F("[SX1262] Init... "));
  int state = radio.begin(FREQ, BW, SF, CR, LORA_SYNC_WORD, 0);        
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("ERRORE: %d\n", state);
    showOLED("LORA ERROR!", String(state).c_str());
    while (true);   // Blocca solo qui, prima di startReceive
  }
  Serial.println(F("OK"));

  // 7. Avvia ricezione ASINCRONA con callback interrupt
  //    Da questo momento la radio è sempre in ascolto.
  //    onPacketReceived() verrà chiamata automaticamente all'arrivo di ogni pacchetto.
  state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[SX1262] startReceive errore: %d\n", state);
    showOLED("RX START ERR");
    while (true);
  }
  radio.setPacketReceivedAction(onPacketReceived);

  showOLED("RX PRONTO", "In ascolto...");
  Serial.println(F("[RX] In ascolto (modalita asincrona)"));
}


void loop() {

  // BLOCCO 1: LETTURA PACCHETTO (non bloccante) 
  if (packetReceived) {
    packetReceived = false; 

    int state = radio.readData((uint8_t*)&gpsData, sizeof(gpsData));

    if (state == RADIOLIB_ERR_NONE) {
      float rssi = radio.getRSSI();
      float snr  = radio.getSNR();

      // 1. Log seriale
      Serial.printf("DATA:%d,%d,%.6f,%.6f,%.1f,%.1f\n",
                    gpsData.id_TX, gpsData.id_pacchetto,
                    gpsData.lat, gpsData.lon, rssi, snr);

      // 2. Accumulo nel buffer per il Cloud
      str_id_TX        += String(gpsData.id_TX)        + ",";
      str_id_pacchetto += String(gpsData.id_pacchetto) + ",";
      str_lat          += String(gpsData.lat, 6)       + ",";
      str_lon          += String(gpsData.lon, 6)       + ",";
      str_rssi         += String(rssi, 1)              + ",";
      str_snr          += String(snr,  1)              + ",";
      p_count++;

      // 3. Feedback OLED 
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_6x10_tf);
      
      char riga1[32], riga2[32], riga3[32], riga4[32];
      sprintf(riga1, "RX OK #%d (Nodo %d)", p_count, gpsData.id_TX);
      sprintf(riga2, "PKT ID: %d", gpsData.id_pacchetto);
      sprintf(riga3, "LAT: %.6f", gpsData.lat);
      sprintf(riga4, "LON: %.6f", gpsData.lon);
      
      u8g2.drawStr(0, 12, riga1);
      u8g2.drawStr(0, 26, riga2);
      u8g2.drawStr(0, 40, riga3);
      u8g2.drawStr(0, 54, riga4);
      u8g2.sendBuffer();

    } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
      Serial.println(F("[RX] CRC error — pacchetto scartato"));
    } else {
      Serial.printf("[RX] Errore lettura: %d\n", state);
    }

    // 4. Riavvia subito la ricezione
    radio.startReceive();
  }

  // INVIO CLOUD 
  if (p_count > 0 && (millis() - lastCloudUpdate >= CLOUD_INTERVAL_MS)) {

    Serial.printf("[CLOUD] Invio %d pacchetti...\n", p_count);

    // Rimuovi l'ultima virgola da ogni stringa
    str_id_TX.remove(str_id_TX.length() - 1);
    str_id_pacchetto.remove(str_id_pacchetto.length() - 1);
    str_lat.remove(str_lat.length() - 1);
    str_lon.remove(str_lon.length() - 1);
    str_rssi.remove(str_rssi.length() - 1);
    str_snr.remove(str_snr.length() - 1);

    ThingSpeak.setField(1, str_id_TX);
    ThingSpeak.setField(2, str_id_pacchetto);
    ThingSpeak.setField(3, str_lat);
    ThingSpeak.setField(4, str_lon);
    ThingSpeak.setField(5, str_rssi);
    ThingSpeak.setField(6, str_snr);

    int response = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

    if (response == 200) {
      Serial.println(F("[CLOUD] OK — buffer resettato"));
      resetBuffer();
      lastCloudUpdate = millis();
    } else {
      // Non resettare il buffer: riprova al prossimo giro
      // Aggiorna il timer per non inviare troppi messaggi ThingSpeak in caso di errore
      Serial.printf("[CLOUD] Errore HTTP %d — riprovo tra %lu s\n",
                    response, CLOUD_INTERVAL_MS / 1000);
      lastCloudUpdate = millis();
    }
  }
}