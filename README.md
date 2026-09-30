# Accellerator
Controller di portata e massa a doppio canale per Arduino MKR WiFi 1010 (SAMD21), basato su FreeRTOS. Gestisce due pompe a PWM ad alta frequenza con lettura di flussimetri analogici, controllo PID di portata, accumulo di massa e comunicazione seriale bidirezionale.

---

## Struttura del progetto

```
accellerator_ottobre.ino   # Entry point — setup, definizione task RTOS
Globals.h                # Dichiarazioni extern di tutte le variabili globali
flussimetri_sonotech.h   # Lettura ADC flussimetri + filtro IIR
kalman.h                 # Filtri di Kalman (SimpleKalmanFilter)
pwm_high_freq.h          # Configurazione PWM 5 kHz (TurboPWM SAMD21)
gestione_dati.h          # Parser comandi seriali + funzioni di stampa
PreciseRTCTimer.h        # Timer ad alta precisione basato su RTCZero + millis()
```

---

## Hardware richiesto

| Componente | Dettaglio |
|---|---|
| MCU | Arduino MKR WiFi 1010 (SAMD21G18A) |
| Pompe | 2× pompa con motore DC (canale 1 e 2) |
| Flussimetri | 2× sensore Sonotech 4-20mA |
| PCB | DSFLAB pcb generata tramite Kicad  |

### Pinout

| Pin | Funzione |
|---|---|
| D4 | PWM Motore 2 (MOT2) |
| D5 | PWM Motore 1 (MOT1) |
| A0 | Tensione alimentazione canale 1 (VOLT) |
| A1 | Tensione alimentazione canale 2 (VOLT2) |
| A3 | Flussimetro canale 1 (PIN\_PORTATA) |
| A4 | Flussimetro canale 2 (PIN\_PORTATA2) |
| D1 | RTS (ingresso) |
| TX/RX Serial1 | Comunicazione UART con host esterno |

---

## Librerie necessarie

Installare tramite Library Manager di Arduino IDE o PlatformIO:

| Libreria | Uso |
|---|---|
| `FreeRTOS_SAMD21` (Scott Briscoe) | Scheduler RTOS |
| `PID_v1_bc` | Controllo PID |
| `SAMD21turboPWM` | PWM ad alta frequenza (5 kHz) |
| `RTCZero` | RTC interno SAMD21 |
| `SimpleKalmanFilter` | Filtro di Kalman sui flussi |

---

## Architettura RTOS

Il firmware è interamente basato su FreeRTOS. Il `loop()` è vuoto; tutto il lavoro avviene nei task.

```
Task            Frequenza   Priorità   Stack   Descrizione
─────────────────────────────────────────────────────────────────
TaskLeggiSensori  50 Hz      3 (alta)   256    Lettura ADC + filtro IIR
TaskControl       10 Hz      3 (alta)   512    PID / massa / PWM — path critico
TaskStampa        (notifica) 3          512    Output seriale (attende notifica da Control)
TaskSerialInput   polling    2          256    Ricezione comandi USB + UART
TaskLED           heartbeat  1          128    Lampeggio LED built-in
taskMonitor       ogni 10 s  1          256    Diagnostica heap e stack RTOS
```

### Mutex condivisi

| Mutex | Risorse protette |
|---|---|
| `xMutexDati` | `flusso[]`, `massa[]`, `dati[]` |
| `xMutexSerial` | `Serial`, `Serial1` |
| `xMutexPWM` | scrittura PWM motori |

---

## Pipeline di misura

```
ADC (10-bit, 0–3.3 V)
  → Filtro IIR esponenziale  Y(n) = α·x(n) + (1-α)·Y(n-1)   α = 0.5
  → Scalatura lineare        flusso = falfa · V + fbeta
  → Filtro di Kalman         (SimpleKalmanFilter, σ_misura=2, σ_stima=2, Q=0.2)
  → input PID / accumulo massa
```

**Coefficienti di calibrazione flussimetri (default):**

| Parametro | Valore |
|---|---|
| `falfa` | 18.939 |
| `fbeta` | −12.5 |
| `alfa_filtro` IIR | 0.5 |

---

## Modalità operative e comandi seriali

I comandi si inviano come sequenza di 5 float via USB (`Serial`) o UART binario (`Serial1`, pacchetto 25 byte).

### Formato USB (testo)
```
<cmd> <p1> <p2> <p3> <p4>
```

### Formato UART (binario)
Struttura `PayloadRicezione`: 6 × `FLOATUNION_t` (4 byte ciascuno) + terminatore `'\n'` = 25 byte totali.

### Tabella comandi

| `cmd` | Significato | p1 | p2 | p3 | p4 |
|---|---|---|---|---|---|
| `0` | Comando manuale con calibrazione Q→V | q1 | q2 | m1\_set | m2\_set |
| `1` | Comando manuale senza calibrazione (V diretto) | q1 | q2 | m1\_set | m2\_set |
| `2` | **Attiva PID di portata** | setpoint1 | setpoint2 | m1\_set | m2\_set |
| `-1` | Imposta fattori di correzione flusso | c1 | c2 | d1 | d2 |
| `-2` | Imposta guadagni PID | Kp | Ki | Kd | — |
| `-10` | Imposta coefficienti calibrazione Q→V | qalfa1 | qbeta1 | qalfa2 | qbeta2 |
| `12` | Imposta ora RTC | HH | MM | SS | — |

**Nota:** i comandi `0`, `1`, `2` azzerano la massa accumulata e riabilitano il controllo soglia.

---

## Controllo PID

Il PID di portata è attivato dal comando `2`. Usa la libreria `PID_v1_bc` su entrambi i canali con:

- Output: segnale PWM (0–1000, dove 1000 = 100% duty cycle a 5 kHz)
- Sample time: 100 ms
- Seed iniziale: calcolato dalla curva di calibrazione Q→V→PWM

La funzione di calibrazione applica la relazione lineare:

```
V = qalfa · q + qbeta
PWM = V · Valfa + Vbeta
```

**Coefficienti default:**

| Canale | qalfa | qbeta |
|---|---|---|
| 1 | 0.4817 | 3.2620 |
| 2 | 0.4423 | 3.9145 |

`Valfa = 40.823`, `Vbeta = 27.125`

---

## Accumulo di massa e soglia di stop

La massa viene integrata numericamente a ogni ciclo di `TaskControl` (10 Hz):

```
massa += flusso · dt / 60000
```

dove `dt` è in millisecondi e la massa risultante è in grammi (supponendo flusso in mL/min).

Quando `massa[0] + massa[1] ≥ m1_set + m2_set`, il sistema:
1. Ferma immediatamente entrambe le pompe (PWM = 0)
2. Disattiva il PID
3. Imposta il flag `massa_stop` (previene reset multipli)

---

## Output seriale

`TaskStampa` emette una riga ogni volta che `TaskControl` produce dati nuovi (~10 Hz).

**Formato:**
```
HH:MM:SS.mmm  dt:<s> q1:<mL/min> V1:<V> q2:<mL/min> V2:<V> f1:<mL/min> f2:<mL/min> m1:<g> m2:<g> pid_active:<0|1> ...
```

Su `Serial1` viene inviato contemporaneamente un pacchetto binario: byte `'A'` + 9 × 4 byte float + `'\n'`.

---

## Diagnostica RTOS

Ogni 10 secondi `taskMonitor` stampa su `Serial`:
- Heap libero corrente e minimo storico
- Lista task (stato, priorità, stack rimasto)
- Stack High Water Mark per ogni task

Usare per verificare che nessun task stia esaurendo lo stack.

---

## Avvio rapido

1. Installare tutte le librerie elencate sopra.
2. Aprire `accellerator_ottobre.ino` in Arduino IDE (selezionare board **Arduino MKR WiFi 1010**).
3. Caricare il firmware. Il LED built-in lampeggerà con pattern 1 s ON / 0.5 s OFF a conferma del corretto avvio dello scheduler.
4. Aprire il monitor seriale a **115200 baud**.
5. Inviare un comando di test (es. modalità manuale canale 1 a 50 mL/min, 100 g):
   ```
   0 50 0 100 0
   ```

---

## Note e limitazioni note

- Il rollover mezzanotte in `PreciseRTCTimer::getDelta()` è gestito restituendo un delta fisso di 100 ms; il calcolo preciso è commentato.
- Il filtro di Kalman (`kalman.h`) include codice di esempio non utilizzato in produzione (`SimpleKalmanFilter_loop_EXAMPLE`).
- I coefficienti `falfa1`/`fbeta1` dichiarati globalmente in `flussimetri_sonotech.h` sono ridefiniti come `static const` dentro `TaskLeggiSensori`; i valori globali non vengono usati.
- La variabile `pid_flag` è presente ma non controlla direttamente l'attivazione del PID (la variabile attiva è `pid_active`).
