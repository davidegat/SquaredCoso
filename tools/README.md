# Strumenti di utilità

## compress_h_rle.py
Script Python per comprimere immagini RGB565 destinate a ESP32 usando codifica RLE.

### Funzionamento
* Legge i file header `.h` contenenti immagini RGB565 (array `uint16_t`).
* Estrae automaticamente larghezza e altezza tramite le macro `#define <NOME>_WIDTH` e `#define <NOME>_HEIGHT`.
* Verifica che il numero di pixel corrisponda a `WIDTH * HEIGHT`.
* Applica la codifica RLE producendo coppie `(color, count)`.
* Scrive il risultato in `./compressed/<nome>.h` mantenendo lo stesso nome base dell'originale.
* Nel file generato dichiara solo l'array `static const RLERun <nome>[] PROGMEM` e la costante `<nome>_count` senza ridefinire `RLERun`.

### Prerequisiti
* Python 3.8 o successivo.
* File `.h` di input che contengano:
  * Definizioni `WIDTH` e `HEIGHT` come costanti preprocessor.
  * Array di valori RGB565 espressi in esadecimale (`0xFFFF`).

### Utilizzo
1. Comprimere un singolo file:
   ```bash
   python3 compress_h_rle.py immagine.h
   ```
2. Comprimere tutti i file `.h` nella cartella corrente:
   ```bash
   python3 compress_h_rle.py *.h
   ```

I file compressi vengono salvati nella sottocartella `compressed/`. Se la cartella non esiste viene creata automaticamente.

### Note
* I run RLE sono limitati a conteggi massimi di 65535 per pixel identici consecutivi.
* Gli errori di formato o conteggio pixel vengono segnalati a terminale senza interrompere gli altri file.
