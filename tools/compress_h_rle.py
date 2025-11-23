#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
===============================================================================
RLE COMPRESSOR PER FILE .h (RGB565)
----------------------------------

✓ Converte QUALSIASI immagine header RGB565 (array uint16_t)
✓ Legge automaticamente WIDTH e HEIGHT dal file (via #define)
✓ Valida il numero totale di pixel
✓ Esegue compressione RLE (color,count)
✓ Genera un NUOVO .h nella sottocartella ./compressed/
✓ Mantiene lo stesso nome dell'originale
✓ Nessuna struct duplicata: nei file generati NON c'è typedef RLERun.
   I file contengono solo:
      static const RLERun nome[] PROGMEM = { ... };

UTILIZZO:
---------
1) Convertire un singolo file:
       python3 compress_h_rle.py immagine.h

2) Convertire tutti gli .h della cartella corrente:
       python3 compress_h_rle.py *.h

3) I file compressi finiscono in:
       ./compressed/<nome>.h
===============================================================================
"""

import re
import os
import sys

# -----------------------------------------------------------------------------
#  Legge WIDTH, HEIGHT e l'array RGB565 dal .h originale
# -----------------------------------------------------------------------------
def load_rgb565_array(path):
    with open(path, "r") as f:
        text = f.read()

    # Cerco WIDTH e HEIGHT dal file .h
    m_w = re.search(r"#define\s+\w+_WIDTH\s+(\d+)", text)
    m_h = re.search(r"#define\s+\w+_HEIGHT\s+(\d+)", text)

    if not m_w or not m_h:
        raise RuntimeError(f"{path}: impossibile trovare WIDTH/HEIGHT")

    width = int(m_w.group(1))
    height = int(m_h.group(1))
    expected = width * height

    # Estrazione dei valori hex dell'immagine
    arr = re.findall(r"0x[0-9A-Fa-f]{4}", text)
    if not arr:
        raise RuntimeError(f"{path}: array RGB565 non trovato")

    if len(arr) != expected:
        raise RuntimeError(
            f"{path}: trovati {len(arr)} pixel, attesi {expected}"
        )

    # Converto in lista di interi
    pixels = [int(v, 16) for v in arr]
    return width, height, pixels


# -----------------------------------------------------------------------------
#  Codifica RLE: produce lista di tuple (color, count)
# -----------------------------------------------------------------------------
def rle_encode(pixels):
    runs = []
    if not pixels:
        return runs

    last = pixels[0]
    count = 1

    for p in pixels[1:]:
        if p == last and count < 65535:
            count += 1
        else:
            runs.append((last, count))
            last = p
            count = 1

    runs.append((last, count))
    return runs


# -----------------------------------------------------------------------------
#  Scrive il file compresso in ./compressed/<same_name>.h
# -----------------------------------------------------------------------------
def write_rle_header(outpath, name, runs, width, height):
    with open(outpath, "w") as f:
        f.write("/* File generato automaticamente - RLE RGB565 */\n")
        f.write(f"#pragma once\n\n")
        f.write(f"// Dimensioni immagine\n")
        f.write(f"#define {name.upper()}_WIDTH {width}\n")
        f.write(f"#define {name.upper()}_HEIGHT {height}\n\n")

        # Attenzione: NON reinserire typedef RLERun
        f.write(f"// Array RLE (color,count)\n")
        f.write(f"static const RLERun {name}[] PROGMEM = {{\n")

        for col, cnt in runs:
            f.write(f"    {{ 0x{col:04X}, {cnt} }},\n")

        f.write("};\n\n")
        f.write(f"static const size_t {name}_count = sizeof({name})/sizeof(RLERun);\n")


# -----------------------------------------------------------------------------
#  Elabora un singolo file .h
# -----------------------------------------------------------------------------
def convert(path):
    base = os.path.basename(path)
    name = os.path.splitext(base)[0]

    print(f"→ converto {base} ...")

    width, height, pixels = load_rgb565_array(path)
    runs = rle_encode(pixels)

    # Sottocartella ./compressed
    outdir = "compressed"
    os.makedirs(outdir, exist_ok=True)
    outpath = os.path.join(outdir, base)

    write_rle_header(outpath, name, runs, width, height)

    print(f"   OK → {outpath} ({len(runs)} run)")


# -----------------------------------------------------------------------------
#  Main
# -----------------------------------------------------------------------------
def main():
    if len(sys.argv) < 2:
        print("Uso: python3 compress_h_rle.py <file.h> oppure *.h")
        return

    files = sys.argv[1:]
    for f in files:
        if not os.path.isfile(f):
            print(f"SKIP: {f} non è un file.")
            continue
        try:
            convert(f)
        except Exception as e:
            print(f"ERRORE: {e}")


if __name__ == "__main__":
    main()
