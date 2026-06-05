#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <U8g2lib.h>

// IDENTITÀ 
#define NODE_ID     100
#define SYNC_WORD   0x12

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


#define ADC_CTRL    37

// PARAMETRI RADIO (863-870 MHz Europa) - uguali al TX perchè monocanale!
#define FREQ    869.525f   // MHz
#define BW      125.0f   // kHz
#define SF      7
#define CR      5


//                                               COSTRUTTORI:
// Display OLED
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RST, OLED_SCL, OLED_SDA);

// Modulo Radio SX1262 
SPIClass radioSPI(HSPI); 
SPISettings spiSettings(2000000, MSBFIRST, SPI_MODE0);
Module* mod = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_NRST, PIN_LORA_BUSY, radioSPI, spiSettings);
SX1262 radio = mod;

// STRUTTURA DATI 
struct __attribute__((packed)) GPSPacket {
  uint16_t id_TX;
  uint16_t id_pacchetto;
  float lat;
  float lon;
  uint8_t batt;
};
GPSPacket gpsData;


void setup() {
  Serial.begin(115200); 
  
  // ACCENSIONE FISICA SCHERMO
  pinMode(PIN_VEXT, OUTPUT);
  digitalWrite(PIN_VEXT, LOW); 
  delay(100);

  // INIZIALIZZAZIONE DISPLAY
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 12, "RX AVVIATO...");
  u8g2.sendBuffer();

  // 3. Inizializzazione SPI dedicata per LoRa 
  radioSPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);

  // 4. Setup Radio SX1262 
  Serial.print(F("[SX1262] Inizializzazione... "));
  int state = radio.begin(FREQ, BW, SF, CR, SYNC_WORD, 0); 

  u8g2.clearBuffer();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Successo!"));
    u8g2.drawStr(0, 12, "LORA RX READY");
    u8g2.drawStr(0, 26, "Frequenza: 869.525");
  } else {
    Serial.print(F("Errore: "));
    Serial.println(state);
    char errBuf[32];
    sprintf(errBuf, "LORA ERR: %d", state);
    u8g2.drawStr(0, 12, errBuf); 
    u8g2.sendBuffer();
    while (true);
  }
  u8g2.sendBuffer();
}

void loop() {
  int state = radio.receive((uint8_t*)&gpsData, sizeof(gpsData));

  if(state == RADIOLIB_ERR_NONE) {
    float rssi = radio.getRSSI();
    float snr  = radio.getSNR();

    // FONDAMENTALE PER LO SCRIPT PYTHON CHE LEGGE I DATI DALLA SERIALE
    // Formato: DATA:ID_TX,ID_PKT,LAT,LON,RSSI,SNR
    Serial.printf("DATA:%d,%d,%.6f,%.6f,%.1f,%.1f\n", 
                  gpsData.id_TX, gpsData.id_pacchetto, gpsData.lat, gpsData.lon, rssi, snr);

    // Aggiorna Display per monitoraggio rapido
    u8g2.clearBuffer();
    char buf[64];
    snprintf(buf, sizeof(buf), "RX NODO: %u", gpsData.id_TX);
    u8g2.drawStr(0, 12, buf);
    snprintf(buf, sizeof(buf), "PKT ID: %u", gpsData.id_pacchetto);
    u8g2.drawStr(0, 26, buf);
    snprintf(buf, sizeof(buf), "RSSI: %.1f SNR: %.1f", rssi, snr);
    u8g2.drawStr(0, 40, buf);
    snprintf(buf, sizeof(buf), "BATT: %u%%", gpsData.batt);
    u8g2.drawStr(0, 54, buf);
    u8g2.sendBuffer();
    
  } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
    // Silenzioso nel log per non sporcare la seriale
  } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    Serial.println(F("[RX] Errore CRC!"));
  }
}