#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <U8g2lib.h>
#include <TinyGPS++.h>

// IDENTITÀ 
#define NODE_ID     101 
#define LORA_SYNC_WORD 0x12   // Sync Word per LoRa (deve essere lo stesso sia su TX che su RX)

// PIN_MAP UFFICIALE HELTEC V3 
// LoRa SX1262 
#define PIN_LORA_NSS      8   // CS
#define PIN_LORA_SCK      9
#define PIN_LORA_MISO     11
#define PIN_LORA_MOSI     10
#define PIN_LORA_NRST     12  // Reset
#define PIN_LORA_BUSY     13
#define PIN_LORA_DIO1     14

// OLED SSD1306 (128x64, HW I2C) 
#define OLED_SDA    17
#define OLED_SCL    18
#define OLED_RST    21

// Controllo Alimentazione 
#define PIN_VEXT    36 

// PIN BATTERIA HELTEC V3
#define PIN_BATT 1
#define ADC_CTRL 37

// PIN GPS
#define GPS_RX_PIN 4  // Da collegare al TX del GPS
#define GPS_TX_PIN 5  // Da collegare all'RX del GPS

// PARAMETRI RADIO (863-870 MHz Europa)
#define FREQ  [inserisci la frequenza in MHz]f
#define BW    [inserisci la banda in kHz]f
#define SF    [inserisci SF]
#define CR      5
#define TX_PWR  14       // dBm (Max 22dBm su V3)
#define TX_DC   1.0f     // Duty Cycle 1%

//                                               COSTRUTTORI:
// Display OLED
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RST, OLED_SCL, OLED_SDA);

// Modulo Radio SX1262 
SPIClass radioSPI(HSPI); 
SPISettings spiSettings(2000000, MSBFIRST, SPI_MODE0);
Module* mod = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_NRST, PIN_LORA_BUSY, radioSPI, spiSettings);
SX1262 radio = mod;

// Modulo GPS 
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);

// STRUTTURA DATI 
struct __attribute__((packed)) GPSPacket {
  uint16_t id_TX;
  uint16_t id_pacchetto;
  float lat;
  float lon;
  uint8_t batt;
};
GPSPacket gpsData;


// Gestione Duty Cycle
float psConst = (100.0f / TX_DC) - 1.0f; 
uint32_t txNext = 0;

// Funzione lettura batteria 0-100%
uint8_t getBatteryLevel() {
  pinMode(ADC_CTRL, OUTPUT);
  digitalWrite(ADC_CTRL, LOW); 
  delay(10);
  int raw = analogRead(PIN_BATT);
  digitalWrite(ADC_CTRL, HIGH);
  float voltage = (raw / 4095.0) * 3.3 * 2.0; 
  int pct = (voltage - 3.2) * 100 / (4.2 - 3.2); 
  return (uint8_t)constrain(pct, 0, 100);
}


void updateGPS() {
  // Leggiamo i dati dal modulo GPS
  while (gpsSerial.available() > 0) {               // se sono disponibili dati sul seriale del GPS
    gps.encode(gpsSerial.read());                   // li passiamo al parser di TinyGPS++ per aggiornare i dati di posizione    
  }

  // Aggiorniamo la struttura dati solo se abbiamo una posizione valida
  if (gps.location.isValid()) {
    gpsData.lat = (float)gps.location.lat();
    gpsData.lon = (float)gps.location.lng();
    // Se vuoi puoi aggiungere anche l'altitudine o il numero di satelliti
    Serial.printf("[GPS] Fix OK! Satelliti: %d\n", gps.satellites.value());
  } else {
    // Se non c'è il fix, i dati restano quelli precedenti o 0
    // Serial.println(F("[GPS] In attesa di segnale..."));
  }

  // Aggiorno la struttura dati da trasmettere
  gpsData.id_TX = NODE_ID;
  gpsData.batt = getBatteryLevel();
}

void setup() {
  Serial.begin(115200); 
  gpsData.id_pacchetto = 0; // Inizializzo il contatore dei pacchetti

  //1. Inizializzazione Serial GPS
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println(F("Seriale GPS RX avviata..."));
  
  // 2. Attivazione Alimentazione Vext (Necessaria per OLED e sensori) 
  pinMode(PIN_VEXT, OUTPUT);
  digitalWrite(PIN_VEXT, LOW); // LOW attiva l'alimentazione sulla V3
  delay(100);

  // 3. Inizializzazione Display 
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 12, "HELTEC V3 TX READY");
  u8g2.sendBuffer();

  // 4. Inizializzazione SPI dedicata per LoRa 
  radioSPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);

  // 5. Setup Radio SX1262 
  Serial.print(F("[SX1262] Inizializzazione... "));
  int state = radio.begin(FREQ, BW, SF, CR, LORA_SYNC_WORD, TX_PWR);    

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Successo!"));
  } else {
    Serial.print(F("Errore: "));
    Serial.println(state);
    u8g2.drawStr(0, 24, "LORA ERROR!");
    u8g2.sendBuffer();
    while (true);
  }
}

void loop() {
  if ((int32_t)(millis() - txNext) >= 0) {
    updateGPS();
    
    // Controlliamo che le coordinate non siano 0.0 prima di procedere - così aspetto fix GPS prima di trasmettere!
    if (gpsData.lat != 0.0 && gpsData.lon != 0.0) {
        
        Serial.printf("[TX] Inviando dati Nodo %d...\n", gpsData.id_TX);
        
        uint32_t toa_us = radio.getTimeOnAir(sizeof(gpsData));    
        int state = radio.transmit((uint8_t*)&gpsData, sizeof(gpsData));    

        u8g2.clearBuffer();
        if (state == RADIOLIB_ERR_NONE) {
          uint32_t toa_ms = toa_us / 1000;
          uint32_t quiet_time = ceil(toa_ms * psConst);
          txNext = millis() + quiet_time;

          Serial.printf("[OK] ToA: %u ms. Attesa: %u ms\n", toa_ms, quiet_time);
          
          u8g2.setFont(u8g2_font_6x10_tf);
          char buf[32];

          sprintf(buf, "TX OK #%d", gpsData.id_pacchetto);
          u8g2.drawStr(0, 12, buf);

          sprintf(buf, "Nodo ID: %d", gpsData.id_TX);
          u8g2.drawStr(0, 26, buf);

          sprintf(buf, "Lat: %.6f", gpsData.lat);
          u8g2.drawStr(0, 42, buf);

          sprintf(buf, "Lon: %.6f", gpsData.lon);
          u8g2.drawStr(0, 58, buf);

          gpsData.id_pacchetto++; 

        } else {
          Serial.printf("[ERR] Errore TX: %d\n", state);

          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_6x10_tf);
          u8g2.drawStr(0, 12, "LORA TX FAIL!");
          
          char errBuf[32];
          sprintf(errBuf, "Codice: %d", state);
          u8g2.drawStr(0, 26, errBuf);
          
          if (state == RADIOLIB_ERR_TX_TIMEOUT) {
              u8g2.drawStr(0, 45, "TIMEOUT - BUSY?");
          } else {
              u8g2.drawStr(0, 45, "RETRYING...");
          }
          
          txNext = millis() + 5000; 
        }
        u8g2.sendBuffer();

    } else {
        //  GESTIONE SE IL GPS NON HA ANCORA IL FIX 
        Serial.println("[GPS] In attesa di fix (0.0, 0.0)...");
        
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawStr(0, 12, "WAITING GPS FIX");
        u8g2.drawStr(0, 26, "Searching...");
        u8g2.sendBuffer();
        
        // Riprova tra 2 secondi senza incrementare id_pacchetto
        txNext = millis() + 2000; 
    }
    
  }
}
