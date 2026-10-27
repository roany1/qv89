# QV-89 — plugin multi-effetto ispirato all'Alesis QuadraVerb (1989)

Plugin **VST3 / AU / Standalone** (Windows, macOS, Linux) scritto in C++ con JUCE 8.
Contiene **due motori**:

- **ENGINE: ROM** (predefinito quando l'EPROM è presente): emulazione della macchina. Il firmware originale v1.07
  gira su un core 8031 emulato, e il microcodice che il firmware carica nell'ASIC DASP viene eseguito da un modello
  del DSP ricavato con reverse engineering della ROM. LCD, tasti, LED, memoria a batteria, MIDI e SysEx sono quelli
  del firmware vero; l'audio gira a 41,667 kHz (frequenza nativa) con conversione verso la frequenza dell'host.
- **ENGINE: MODEL**: il motore ricostruito a mano (algoritmi scritti da zero sulla base dei manuali), usabile
  anche senza ROM.

Dettagli tecnici del reverse engineering in [`docs/REVERSE_ENGINEERING.md`](docs/REVERSE_ENGINEERING.md).

Autore: Roany Romano. Progetto non affiliato ad Alesis; "QuadraVerb" è un marchio Alesis.

## Cosa fa
| Config | Catena | Note |
|---|---|---|
| 1 QuadMode | EQ 3 bande > Pitch > Delay > Reverb | 4 effetti insieme, delay max 800 ms mono / 400 ms stereo |
| 2 Rotary | Rotary speaker > Delay > Reverb | rotori con inerzia, slow/fast/stop, separazione stereo, livello tromba |
| 3 Graphic | EQ grafico 11 bande ±14 dB > Delay | delay max 1500 / 750 ms |
| 4 5-Band | EQ parametrico 5 bande > Pitch > Delay | |
| 5 3-Band>Rev | EQ 3 bande > Reverb "2" | riverbero a densità doppia + reverb chorus |

- **Reverb**: Plate, Room, Chamber, Hall, Reverse; pre-delay + pre-delay mix, diffusion, density (gap della prima riflessione), decay separato basse/alte, gate (soglia −18 dBFS, hold, release, gated level). Ingressi 1/2 con mix come sull'hardware.
- **Pitch**: chorus mono/stereo (LFO triangolo/quadra), flanger mono/stereo con trigger, detune ±99 cent, phaser.
- **Delay**: mono, stereo, ping-pong; feedback 0–99 % (99 = ripetizione infinita).
- **Mix**: direct pre/post EQ, master effects, livelli per effetto (logica del manuale, fig. 17).
- **MOD**: 8 slot sorgente MIDI (pitch bend, aftertouch, nota, velocity, CC 0–127) → bersaglio → ampiezza ±99.
- **Vintage 16-bit**: ingresso effetti sommato in mono (come l'hardware) e quantizzato a 16 bit.
- **Programmi**: 90 programmi ROM originali (00–89, se l'EPROM è presente in `rom/` al momento della build), 25 programmi QV-89 originali, più quelli importati. Program Change MIDI li richiama per numero.
- **SysEx**: import di dump reali QuadraVerb (singolo, edit buffer, banco da 100) ed export del programma corrente come `.syx` per l'hardware.
- **EPROM**: "LOAD QV ROM" carica a runtime qualsiasi immagine 27C512 del QuadraVerb (le linee A0–A7 sono invertite sul PCB; il plugin le riordina e trova la tabella programmi).

## Interfaccia
Pannello stile rack: LCD 2×16, tasti funzione con LED, PAGE/VALUE, INPUT/OUTPUT, LED di headroom.
Sotto: schema della configurazione cliccabile e manopole della funzione selezionata (doppio click = default).
Finestra ridimensionabile. In alto: **ENGINE** (ROM/MODEL) e **FILE...** (salva/carica, SysEx, EPROM).

In modalità ROM i tasti del pannello sono i tasti veri della macchina emulata (pressione e rilascio, l'auto-repeat
lo fa il firmware), l'LCD e i LED sono quelli pilotati dal firmware, e il tasto UTIL diventa **MIDI** come
sull'hardware. Le manopole restano utilizzabili: ogni modifica (anche l'automazione dell'host) viene inviata
alla macchina come messaggio SysEx di edit parametro, esattamente come farebbe un editor esterno; le modifiche
fatte dal pannello emulato tornano indietro nei parametri del plugin. La lista programmi mostra le 100 memorie
dell'unità (00–89 Alesis, 90–99 utente); i programmi QV-89 originali e quelli importati vengono inviati come dump SysEx.
Lo stato salvato nel progetto include l'intera RAM a batteria (32 KB), quindi i programmi memorizzati con STORE
restano nel progetto.

## Build
Requisiti: CMake ≥ 3.22, compilatore C++17 (Visual Studio 2022 / Xcode 14+ / GCC 11+). JUCE viene scaricato
automaticamente (oppure `-DJUCE_DIR=/percorso/JUCE`).

```bash
# macOS (universal, include AU)
cmake -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release
# Windows
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
I plugin sono in `build/QV89_artefacts/Release/` (VST3, AU, Standalone).
Installazione: VST3 in `C:\Program Files\Common Files\VST3` o `~/Library/Audio/Plug-Ins/VST3`,
AU in `~/Library/Audio/Plug-Ins/Components`.

**GitHub Actions**: `.github/workflows/build.yml` compila Windows e macOS, esegue i test e pluginval
e pubblica i binari come artifact. Se `rom/qv_v107.bin` è nel repository, il repository deve essere **privato**
(il firmware è materiale protetto da copyright Alesis). Senza ROM il plugin compila lo stesso e funziona in modalità
MODEL; un'immagine EPROM può essere caricata a runtime da FILE... → "Load QuadraVerb EPROM image".

## Test
- `qv89_dsp_test`: stabilità di tutte le configurazioni, RT60, tempi di delay, detune, gate, EQ, CPU.
- `qv89_rom_test rom/qv_v107.bin out.syx`: estrae i 90 programmi, verifica il roundtrip SysEx, scrive il banco.
- `qv89_proc_test` (opzione CMake `-DQV89_PROC_TEST=ON`): avvio dell'emulazione, cambio programma, edit via SysEx
  da parametro host, tasti del pannello, roundtrip dello stato, commutazione ROM/MODEL.
- pluginval strictness 10 superato (VST3 Linux, motore ROM attivo).

## Limiti noti (onestà tecnica)
- **Motore ROM**: il firmware è eseguito com'è, ma il comportamento del DSP (semantica dei 16 opcode, larghezze
  in bit di accumulatore e memoria, saturazione) è stato **dedotto** dal microcodice e verificato con test indiretti
  (risposte di EQ, tempi di delay, stabilità di tutti i 90 programmi), non misurato su un'unità reale né ricavato da
  documentazione Alesis. Alcuni dettagli restano stime: memoria modellata a 20 bit, saturazione a ±1, legge del
  VCA del segnale diretto supposta lineare, taratura del rilevatore di livello (gate) approssimata.
  Sono possibili differenze di colore/rumore rispetto all'hardware; il confronto A/B con un QuadraVerb vero è il
  passo successivo per affinarli.
- Il motore ROM aggiunge circa 100 campioni di latenza (conversione di frequenza), dichiarata all'host.
- **Motore MODEL**: gli algoritmi sono **ricostruzioni**; le mappature di valore (decay → RT60, LFO speed → Hz,
  detune → cent) sono stime ragionevoli.
- Configurazioni del QuadraVerb **Plus** (ring mod, resonator, sampling, multi-tap) non implementate:
  i programmi che le usano vengono importati in QuadMode/stereo delay.
- Codifica mod-source verificata sui programmi ROM (es. "SUSPEDAL LEZLY" → CC 64, "TAJ MAHAL modw" → CC 1);
  decay basse/alte memorizzato come modulo 0–60 (verificato sui default della ROM).
