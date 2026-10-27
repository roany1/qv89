# Reverse engineering dell'EPROM QuadraVerb v1.07

Note di lavoro per il motore **ENGINE: ROM** di QV-89. Tutto ciò che segue è stato ricavato dall'immagine
dell'EPROM (27C512, 64 KB), dal manuale di servizio e da esperimenti sull'emulatore. Dove un punto è una
deduzione non verificata su hardware reale è indicato.

## 1. Immagine EPROM
- Sul PCB le linee d'indirizzo A0–A7 dell'EPROM sono collegate in ordine invertito: il byte all'indirizzo
  logico `a` si trova nel file a `(a & 0xFF00) | reverse8(a & 0xFF)`. `Rom.cpp::descramble` lo riordina.
  Il vettore di reset (`LJMP` a 0x0000) verifica che l'immagine sia corretta.
- Tabella programmi di fabbrica: 90 record da 128 byte a 0xD2FE (immagine riordinata), stesso formato del
  dump SysEx (nome di 14 caratteri a offset 0x6A).

## 2. Hardware visto dal firmware
| Risorsa | Uso |
|---|---|
| CPU | 8031 a 12 MHz (1 µs per ciclo macchina) |
| SRAM 32 KB a 0x8000 | edit buffer a 0x8300, 100 programmi utente a 0xCE00 (128 byte l'uno), flag globali (es. SYSEX ENABLE a 0x8505) |
| 0x0000–0x017F (scrittura esterna) | microcodice DASP: 96 istruzioni × 4 byte |
| 0x0180 / 0x017F | strobe a ogni tick dell'interrupt |
| 0x0200 / 0x0201 | LCD HD44780 comando / dato |
| latch 0x0400 | LED attivi bassi: MIDI b7, BYPASS b6, STORE b5 |
| latch 0x0600 | livello VCA del segnale diretto b0–b4 (il firmware scambia b0↔b4, b1↔b3, routine 0x7803), b5 scarica condensatore piezo, b6 scarica condensatore soglia audio, b7 LED PROGRAM |
| latch 0x0800 | LED funzione attivi bassi: EQ b7, NAME b6, PITCH b5, MOD b4, DELAY b3, MIX b2, REVERB b1, CONFIG b0 |
| P1 | tastiera a matrice 4×4: colonna su P1.7–P1.4, righe lette su P1.0–P1.3 |
| P3.2 | comparatore di livello audio (gate del riverbero) |
| P3.4 / P3.5 | pedali |
| UART | modo 1, 31 250 baud (timer 1 reload 0xFF) |
| Timer 0 | modo 1, reload 0xFF06 → ISR a 0x0608, circa 4 kHz |

Matrice tasti (colonna, riga): MIDI (0,0), BYPASS (0,1), PAGE− (0,2), PAGE+ (0,3), EQ (1,0), NAME (1,1),
VALUE− (1,2), VALUE+ (1,3), DELAY (2,0), MIX (2,1), CONFIG (2,2), REVERB (2,3), PITCH (3,0), MOD (3,1),
STORE (3,2), PROGRAM (3,3).

L'ISR del timer 0 aggiorna le parole di microcodice modulate dagli LFO (chorus, flanger, rotary) e conta i tick
con P3.2 alto nel registro 52h: il gate si apre con conteggio ≥ 6 (circa −18 dBFS, taratura del comparatore stimata).

**SYSEX ENABLE** va attivato dal pannello: il firmware tiene una copia di lavoro fuori dalla RAM a batteria, quindi
scrivere solo il flag memorizzato non basta. All'avvio il plugin esegue la sequenza di tasti MIDI → PAGE+ … →
VALUE+, come farebbe un utente.

## 3. DASP: formato dell'istruzione
Parola di 4 byte:
- byte 0–1: offset d'indirizzo a 16 bit (little endian), sommato a un puntatore di memoria che decresce di 1 a
  ogni campione (memoria circolare: delay line implicite);
- byte 2–3: coefficiente senza segno a 11 bit (/2048) nei bit alti, bit 4 del byte 3 = segno, 4 bit bassi = opcode.

Frequenza di campionamento ≈ 41 666,67 Hz (24 MHz / 576): il firmware usa 42 campioni/ms + offset 1280 per i
delay, e con questa fs le bande dell'EQ grafico cadono esattamente sulle frequenze nominali.

I/O: il campione dell'ADC entra nell'accumulatore allo step 94, che lo scrive in FFFF (diventa M[0] al campione
successivo). DAC sinistro = lettura di memoria allo step 0, destro = lettura allo step 95.

## 4. Semantica degli opcode (dedotta)
Registri: ACC, X, Y. P = ±c·R per le letture (R = memoria letta), ±c·ACC per le scritture; le scritture salvano
il vecchio ACC (`old`).

| Op | Effetto |
|---|---|
| 0 | ACC = P + Y |
| 1 | ACC = ACC + P |
| 2 | ACC = P + X |
| 3 | ACC = P + Y; Y = 0 |
| 4 | ACC = P; X = R |
| 5 | ACC = P; Y = R |
| 6 | ACC = ACC + P; X = R |
| 7 | ACC = ACC + P; Y = R |
| 8 | M = old; ACC = c·old |
| 9 | M = old; ACC = c·old + old |
| A | M = old; ACC = c·old + X |
| B | M = old; ACC = c·old + Y; Y = 0 |
| C | M = old; ACC = c·old; X = old; Y = nuovo ACC |
| D | M = old; ACC = c·old; Y = old |
| E | M = old; ACC = c·old + old; X = old |
| F | M = old; ACC = c·old + old |

Come è stata ricavata: ipotesi successive confrontate con vincoli che il risultato deve rispettare
(i filtri shelving dell'EQ devono avere la risposta prevista, i delay devono dare i tempi indicati sull'LCD,
il room reverb deve essere stabile, tutti i 90 programmi devono produrre suono con gli aggiornamenti LFO attivi).
Esempi: il low shelf richiede op0 = P + Y e opC che memorizza X = old, Y = nuovo ACC; op7 non deve sommare X
(altrimenti il room è instabile) e deve memorizzare Y = R (altrimenti i programmi 27/48/53 sono muti).

Modello numerico: memoria a 20 bit, ACC saturato a ±1, ADC/DAC a 16 bit. **Non verificato su hardware.**
Alcuni programmi mostrano picchi interni dell'accumulatore fino a 1,2–1,7 (e transitori isolati fino a 4 durante
gli aggiornamenti LFO); con la saturazione a ±1 restano stabili, ma la larghezza reale dell'accumulatore è ignota.

## 5. Protocollo SysEx usato dal plugin
- Edit parametro: `F0 00 00 0E 02 01 <funzione> <pagina> v1 v2 v3 F7`, valore LSB-first in un bitstream a 7 bit.
  Dopo ogni edit il firmware mostra la pagina modificata sull'LCD (comportamento dell'hardware).
- Dump: opcode 02, 147 byte di dati per 128 byte di programma; programma 100 = edit buffer.
- Program change MIDI standard per le memorie 00–99.

## 6. Cosa resta da fare
- Confronto A/B con un QuadraVerb reale (risposta all'impulso per programma) per fissare larghezze in bit,
  saturazione, legge del VCA diretto e taratura del rilevatore di livello.
- Il ROM caricato a runtime (FILE → Load EPROM) non è salvato nel progetto: senza ROM incorporata nella build
  va ricaricato a ogni sessione.
