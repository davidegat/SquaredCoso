# Strumenti di utilità / Utility tools

## Sezione Italiana

### compress_h_rle.py
Script Python che comprime le immagini RGB565 destinate a ESP32 tramite codifica Run-Length Encoding (RLE), riducendo lo spazio occupato nei file header `.h`.

#### Funzionamento
* Legge i file header `.h` contenenti immagini RGB565 (array `uint16_t`).
* Estrae automaticamente larghezza e altezza tramite le macro `#define <NOME>_WIDTH` e `#define <NOME>_HEIGHT`.
* Verifica che il numero di pixel corrisponda a `WIDTH * HEIGHT` e segnala eventuali inconsistenze.
* Applica la codifica RLE producendo coppie `(color, count)` limitate a conteggi massimi di 65535 pixel consecutivi.
* Scrive il risultato in `./compressed/<nome>.h`, mantenendo il nome base del file originale.
* Nel file generato dichiara solo l'array `static const RLERun <nome>[] PROGMEM` e la costante `<nome>_count` senza ridefinire `RLERun`.

#### Utilità
La compressione RLE permette di ridurre la dimensione delle immagini compilate nel firmware, facilitando il caricamento su dispositivi con memoria limitata (es. ESP32) e velocizzando i trasferimenti. Il formato generato è compatibile con gli header esistenti e non richiede modifiche alle strutture dati già incluse altrove nel progetto.

#### Prerequisiti
* Python 3.8 o successivo.
* File `.h` di input che contengano:
  * Definizioni `WIDTH` e `HEIGHT` come costanti preprocessor.
  * Array di valori RGB565 espressi in esadecimale (`0xFFFF`).

#### Utilizzo
1. Comprimere un singolo file:
   ```bash
   python3 compress_h_rle.py immagine.h
   ```
2. Comprimere tutti i file `.h` nella cartella corrente:
   ```bash
   python3 compress_h_rle.py *.h
   ```

I file compressi vengono salvati nella sottocartella `compressed/`. Se la cartella non esiste viene creata automaticamente. Eventuali errori di formato o conteggio pixel vengono segnalati a terminale senza interrompere l'elaborazione degli altri file.

---

## English Section

### compress_h_rle.py
Python script that compresses RGB565 images for ESP32 using Run-Length Encoding (RLE), shrinking the size of `.h` header files.

#### How it works
* Reads `.h` header files containing RGB565 images (`uint16_t` arrays).
* Automatically extracts width and height from `#define <NAME>_WIDTH` and `#define <NAME>_HEIGHT` macros.
* Checks that the pixel count matches `WIDTH * HEIGHT` and reports inconsistencies.
* Applies RLE encoding to generate `(color, count)` pairs, limiting runs to a maximum of 65,535 identical pixels.
* Writes the result to `./compressed/<name>.h`, preserving the original base filename.
* The generated file only declares `static const RLERun <name>[] PROGMEM` and the `<name>_count` constant without redefining `RLERun`.

#### Why it is useful
RLE compression reduces the footprint of images embedded in firmware, making it easier to deploy on memory-constrained devices (e.g., ESP32) and speeding up transfers. The output format remains compatible with existing headers and avoids duplicating data structures already defined elsewhere in the project.

#### Requirements
* Python 3.8 or later.
* Input `.h` files containing:
  * `WIDTH` and `HEIGHT` definitions as preprocessor constants.
  * Arrays of RGB565 values expressed in hexadecimal (`0xFFFF`).

#### Usage
1. Compress a single file:
   ```bash
   python3 compress_h_rle.py image.h
   ```
2. Compress every `.h` file in the current folder:
   ```bash
   python3 compress_h_rle.py *.h
   ```

Compressed files are stored in the `compressed/` subfolder. If the folder does not exist it is created automatically. Format or pixel-count errors are reported to the terminal without stopping processing of the remaining files.
