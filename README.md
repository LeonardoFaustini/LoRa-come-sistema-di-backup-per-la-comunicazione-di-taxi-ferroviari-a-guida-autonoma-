# Guida alla repository GitHub

In questa sezione è riportata una breve guida a questa repository GitHub, la quale contiene tutto il materiale relativo al lavoro svolto nel tiroinio e nella tesi, tra cui i codici sorgente, i file STL, l'analisi delle antenne tramite nanoVNA e i risultati della parte di test. 

L'organizzazione delle cartelle del progetto è spiegata qui di seguito:

### Firmware
Nella cartella `File_di_configurazione_ESP32-LoRa` sono stati inseriti i firmware dei moduli trasmettitore e ricevitori usati per le topologie P2P e Star.

### Risultati test
In questa cartella sono stati inseriti tutti i dati acquisiti nelle fasi di test. Essi sono suddivisi in base alla topologia e alla configurazione usata, in base alla frequenza, duty cycle e potenza di trasmissione. All'interno di ogni configurazione sono presenti i relativi file csv ottenuti da seriale e da ThingSpeak e i relativi grafici (RSSI-distanza, SNR-distanza, RSSI-SNR, distribuzione dell’RSSI e una mappa interattiva che rappresenta i diversi pacchetti ricevuti mostrandone ID, distanza dal ricevitore e relativo RSSI).

### Case prototipo 3D
All'interno di questa cartella sono inseriti i file STL del case del prototipo e dell'antenna creata, già orientati e settati correttamente per la stampa FDM.

### Codici Python, lettura seriale e grafici
In questa cartella è presente il codice python usato per leggere e salvare i dati dalla seriale in fase di test e un notebook usato per passare dai dati csv ai grafici.


Inoltre, sono contenuti l'analisi tramite VNA delle diverse antenne usate in fase di test, lo schematico KiCad per il collegamento tra il modulo GPS e la Heltec, i codici MATLAB usati su ThingSpeak per spacchettare i buffer di dati ricevuti per poterne plottare i grafici.
