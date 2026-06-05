# Guida alla repository GitHub

In questa sezione è riportata una breve guida a questa repository GitHub, la quale contiene tutto il materiale relativo al lavoro svolto nel tiroinio e nella tesi, tra cui i codici sorgente, i file STL, l'analisi delle antenne tramite nanoVNA e i risultati della parte di test. 

L'organizzazione delle cartelle del progetto è spiegata qui di seguito:

### Firmware
Nella cartella `File_di_configurazione_ESP32-LoRa` sono stati inseriti i firmware dei moduli trasmettitore e ricevitori usati per le topologie P2P e Star.

### Risultati test
In questa cartella sono stati inseriti tutti i dati acquisiti nelle fasi di test. Sono stati suddivisi per topologie e per configurazioni usati, in base alla frequenza, duty cycle e potenza di trasmissione. All'interno di ogni configurazione saranno presenti i relativi file csv ottenuti da seriale e da ThingSpeak e i relativi grafici.

### Case prototipo 3D
Qui sono inseriti i file STL del case del prototipo e dell'antenna creata, già orientati e settati correttamente per la fase di stampa.

### Codici Python, lettura seriale e grafici
Qui è presente il codice python usato per leggere e salvare i dati dalla seriale in fase di test e un notebook usato per passare dai dati csv ai grafici.


Inoltre, sono contenuti l'analisi delle diverse antenne usate in fase di test, lo schematico KiCad per il collegamento tra il modulo GPS e la Heltec, i codici MATLAB usati su ThingSpeak per spacchettare i buffer di dati ricevuti per poterne plottare i grafici, i driver CP210 scaricati, i paper o gli articoli utilizzati nella fase di ricerca dello stato dell'arte e i datasheet dei moduli hardware usati.
