#include <Arduino.h>
#include <SPI.h>
#include <RadioLib.h>
#include <U8g2lib.h>
//#include <TinyGPS++.h>        Utile per il prototipo ma in questo test i TX saranno fermi


// IDENTITÀ 
#define NODE_ID     102       // Da cambiare per ogni TX
#define SYNC_WORD 0X12

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

// Pin per lettura batteria
#define PIN_BATT 1
#define ADC_CTRL 37 // Pin per attivare la lettura batteria sulla V3

// PARAMETRI RADIO (863-870 MHz Europa)
#define FREQ    869.525f   // MHz
#define BW      125.0f   // kHz
#define SF      7
#define CR      5
#define TX_PWR  22       // dBm (Max 22dBm su V3) anche se per legge fino a 27dBm
#define TX_DC   10.0f     // Duty Cycle 10%

// PIN GPS per Heltec V3 (UART1)
#define GPS_RX_PIN 47  // Va al TX del modulo GPS
#define GPS_TX_PIN 48  // Va al RX del modulo GPS
#define GPS_BAUD   9600 // Velocità standard dei moduli NEO-6M/M8N


//                                               COSTRUTTORI:
// Display OLED
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, OLED_RST, OLED_SCL, OLED_SDA);

// Modulo Radio SX1262 
SPIClass radioSPI(HSPI); 
SPISettings spiSettings(2000000, MSBFIRST, SPI_MODE0);
Module* mod = new Module(PIN_LORA_NSS, PIN_LORA_DIO1, PIN_LORA_NRST, PIN_LORA_BUSY, radioSPI, spiSettings);
SX1262 radio = mod;

// Modulo GPS                 Utile per il prototipo ma in questo test i TX saranno fermi
// TinyGPSPlus gps;
// HardwareSerial gpsSerial(1); // Usiamo la UART1 dell'ESP32

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

// Funzione per leggere il livello della batteria
uint8_t getBatteryLevel() {
  pinMode(ADC_CTRL, OUTPUT);
  digitalWrite(ADC_CTRL, LOW); // Attiva il partitore di tensione
  delay(10);
  
  int raw = analogRead(PIN_BATT);
  digitalWrite(ADC_CTRL, HIGH); // Spegne il partitore per risparmiare energia

  // L'ESP32 V3 ha una risoluzione di 12 bit (0-4095)
  // 3.3V corrispondono a circa 4.2V della batteria carica tramite il partitore
  float voltage = (raw / 4095.0) * 3.3 * 2; // Moltiplichi per 2 per il partitore
  
  // Mappa il voltaggio (3.2V - 4.2V) in percentuale (0-100)
  int pct = (voltage - 3.2) * 100;
  if (pct > 100) pct = 100;
  if (pct < 0) pct = 0;
  return (uint8_t)pct;
}

// Generazione dati GPS casuali (simulazione)
void updateGPS(){
  gpsData.batt = getBatteryLevel();

  // Coordinate centro Rovereto
  float centerLat = 45.890926f;
  float centerLon = 11.043538f;

  // Aggiungo un offset casuale tra -0.001000 e +0.001000 (circa +/- 110 metri)
  float latOffset = (random(-1000, 1001) / 1000000.0f);
  float lonOffset = (random(-1000, 1001) / 1000000.0f);

  gpsData.lat = centerLat + latOffset;
  gpsData.lon = centerLon + lonOffset;
}
// void updateGPS() {                                 Utile per il prototipo ma in questo test i TX saranno fermi
//   // Leggiamo i dati dal modulo GPS
//   while (gpsSerial.available() > 0) {
//     gps.encode(gpsSerial.read());
//   }

//   // Aggiorniamo la struttura dati solo se abbiamo una posizione valida
//   if (gps.location.isValid()) {
//     gpsData.lat = (float)gps.location.lat();
//     gpsData.lon = (float)gps.location.lng();
//     // Se vuoi puoi aggiungere anche l'altitudine o il numero di satelliti
//     Serial.printf("[GPS] Fix OK! Satelliti: %d\n", gps.satellites.value());
//   } else {
//     // Se non c'è il fix, i dati restano quelli precedenti o 0
//     // Serial.println(F("[GPS] In attesa di segnale..."));
//   }

//   gpsData.id_TX = NODE_ID;
//   gpsData.id_pacchetto++;
//   gpsData.batt = getBatteryLevel();
// }

void setup() {
  Serial.begin(115200); 
  randomSeed(analogRead(1) + NODE_ID);    // Così ogni TX lo ha diverso, usa 1 NON 0!!!
  gpsData.id_pacchetto = 0; // Inizializzo il contatore dei pacchetti
  gpsData.id_TX = NODE_ID;

  // 1. Attivazione Alimentazione Vext (Necessaria per OLED e sensori) 
  pinMode(PIN_VEXT, OUTPUT);
  digitalWrite(PIN_VEXT, LOW); // LOW attiva l'alimentazione sulla V3
  delay(100);

  // 2. Inizializzazione Display 
  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.clearBuffer();
  u8g2.drawStr(0, 12, "HELTEC V3 TX READY");
  u8g2.sendBuffer();

  // 3. Inizializzazione SPI dedicata per LoRa 
  radioSPI.begin(PIN_LORA_SCK, PIN_LORA_MISO, PIN_LORA_MOSI, PIN_LORA_NSS);

  // 4. Setup Radio SX1262 
  Serial.print(F("[SX1262] Inizializzazione... "));
  int state = radio.begin(FREQ, BW, SF, CR, SYNC_WORD, TX_PWR);    

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("Successo!"));
  } else {
    Serial.print(F("Errore: "));
    Serial.println(state);
    u8g2.drawStr(0, 24, "LORA ERROR!");
    u8g2.sendBuffer();
    while (true);
  }

  // Inizializza la seriale per il GPS                          Utile per il prototipo ma in questo test i TX saranno fermi
  // gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  // Serial.println(F("Seriale GPS avviata..."));
}

void loop() {
  if ((int32_t)(millis() - txNext) >= 0) {
    updateGPS();  
    
    // LOGICA LBT(Listen Before Talk) tramite CAD (Channel Activity Detection)
    Serial.print(F("[CAD] Controllo canale... "));
    int cadState = radio.scanChannel();

    if (cadState == RADIOLIB_LORA_DETECTED) {
      // Canale occupato! Aspettiamo un tempo casuale (backoff) e riproviamo nel prossimo loop
      uint32_t backoff = random(200, 1000); 
      Serial.printf("Occupato! Riprovo tra %u ms\n", backoff);
      txNext = millis() + backoff; 
    } 
    else {
      // Canale libero! Trasmettiamo
      Serial.print(F("[TX] Canale libero. Invio... "));
      
      uint32_t toa_us = radio.getTimeOnAir(sizeof(gpsData));
      int state = radio.transmit((uint8_t*)&gpsData, sizeof(gpsData));

      if (state == RADIOLIB_ERR_NONE) {
        gpsData.id_pacchetto++;
        uint32_t toa_ms = toa_us / 1000;
        uint32_t quiet_time = ceil(toa_ms * psConst);
        // Aggiungo un jitter casuale tra 0 e 500 ms per evitare collisioni ricorrenti con altri nodi sincronizzati!!
        uint32_t jitter = random(0, 501); 
        txNext = millis() + quiet_time + jitter;
        

        Serial.printf("[OK] ToA: %u ms. Attesa Totale: %u ms (di cui Jitter: %u)\n", toa_ms, quiet_time + jitter, jitter);
        
        u8g2.clearBuffer();

        // 1. Riga superiore: Identità del Nodo
        char buf[32];
        sprintf(buf, "NODO ID: %d", NODE_ID); 
        u8g2.drawStr(0, 10, buf); // Posizionato in alto (y=10)

        // 2. Stato della trasmissione
        u8g2.drawStr(0, 25, "LBT TX OK");

        // 3. ID del Pacchetto
        sprintf(buf, "Pkt ID: %d", gpsData.id_pacchetto);
        u8g2.drawStr(0, 40, buf);

        // 4. Coordinate (Latitudine)
        sprintf(buf, "Lat: %.6f", gpsData.lat);
        u8g2.drawStr(0, 55, buf);

        u8g2.sendBuffer();
      } else {
        Serial.printf("Errore: %d\n", state);
        txNext = millis() + 1000; // In caso di errore HW, riprova dopo 1s
      }
    }
  }
}