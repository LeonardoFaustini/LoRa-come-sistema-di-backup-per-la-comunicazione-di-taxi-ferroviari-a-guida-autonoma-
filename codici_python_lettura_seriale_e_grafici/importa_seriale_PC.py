import serial
import csv
import time

# --- CONFIGURAZIONE ---
PORT = 'COM3'  # <--- CAMBIA QUESTA con la tua porta reale!
BAUD = 115200
FILE_NAME = r"C:\Users\leona\Desktop\LEO\UNI\TRIENNALE\TIROCINIO_RAILEVO\File_configurazione_ESP32-LoRa\869.525_MHz\file csv - nuova antenna\dati_antenna.csv"

try:
    # Apriamo la porta seriale
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print(f"Logger avviato su {PORT}. Premi Ctrl+C per fermare.")
    
    # Apriamo il file CSV in modalità 'append' (aggiunta)
    with open(FILE_NAME, mode='a', newline='') as f:
        writer = csv.writer(f)
        
        # Se il file è nuovo (zero byte), scriviamo l'intestazione
        if f.tell() == 0:
            writer.writerow(["Timestamp", "NodeID", "PacketID", "Lat", "Lon", "RSSI", "SNR"])
        
        while True:
            # Leggiamo una riga dalla seriale
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            
            # Se la riga inizia con il nostro prefisso DATA:
            if line.startswith("DATA:"):
                # Puliamo la stringa e dividiamo i valori
                valori = line.replace("DATA:", "").split(",")
                timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
                riga_completa = [timestamp] + valori
                
                # Salviamo su file e stampiamo a video per controllo
                writer.writerow(riga_completa)
                f.flush() # Forza la scrittura su disco immediata
                print(f"Ricevuto: {riga_completa}")

except serial.SerialException:
    print(f"Errore: Impossibile aprire la porta {PORT}. Controlla che il Serial Monitor sia CHIUSO.")
except KeyboardInterrupt:
    print("\nRegistrazione interrotta dall'utente. File salvato.")