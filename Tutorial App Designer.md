# Manuale rapido di App Designer

`accellerator_ottobre2026.mlapp` — controllo di due pompe peristaltiche tramite comunicazione seriale (UART) con un microcontrollore. **Requisiti:** MATLAB R2021a o successivo, Instrument Control Toolbox.

---

## 1. Come è fatta un'app

App Designer ha due viste: **Design View** (si trascinano i componenti) e **Code View** (si scrive il codice). Il file `.mlapp` è in realtà uno zip che contiene una classe MATLAB:

```matlab
classdef accellerator_ottobre2026 < matlab.apps.AppBase
```

Tutto ruota attorno a una parola: **`app`**. È l'oggetto che contiene *sia* i componenti grafici (`app.STOPButton`, `app.UITable`) *sia* le variabili dell'app. Ogni funzione riceve `app` come primo argomento.

In Code View, a sinistra, il **Code Browser** ha tre tab: **Callbacks**, **Functions**, **Properties**. Sono le tre cose da capire.

| Concetto | Cos'è | Esempio nel progetto |
| --- | --- | --- |
| **Property** | Una variabile che vive finché vive l'app | `app.ardObj`, `app.values`, `app.dt` |
| **Function** | Un metodo "di servizio" che chiami tu | `data_send(app, valore)` |
| **Callback** | Una funzione che parte da sola quando succede un evento | `StartProductionButtonPushed(app, event)` |

> Le righe **grigie** nel codice sono generate da App Designer e non si modificano. Si scrive solo nelle zone bianche.

---

## 2. Property

Le property sono di due tipi:

- **Public** — contengono i componenti creati in Design View (generate in automatico): `STOPButton matlab.ui.control.Button`.
- **Private** — sono *lo stato* dell'app, le aggiungi tu (Properties → Add → Private).

Le tue property private sono gli "strumenti" accessibili dell'app:

```matlab
properties (Access = private)
    values                 % ultimi 9 float ricevuti: [dt,q1,V1,q2,V2,f1,f2,m1,m2]
    ardObj;                % oggetto serialport (la connessione con Arduino)
    timerObj_uart;         % timer per il refresh del grafico
    animLine_f1            % linea animata del grafico pompa 1
    dt = 0.2;              % periodo del timer [s]
    fid = -1               % file del logging CSV (-1 = chiuso)
end
```

**Perché servono:** ogni callback ha le proprie variabili locali, che spariscono a fine esecuzione. Per far parlare due callback tra loro (es. `readSerialDataUART` scrive `app.values`, `updatePlot` lo legge) l'unico modo è una property.

Si usano con `app.nomeProperty` sia in lettura che in scrittura. E' fortemente **sconsigliato** l'uso di **variabili globali** come in esempi di codice prima del 2021; sono poco ottimizzate e hanno la stessa funzione delle Property.

---

## 3. Function (metodi di servizio)

Sono funzioni normali, ma scritte *dentro* la classe. Si aggiungono da Functions → Add → Private. Si chiamano con `nome(app, ...)` — `app` è **sempre il primo argomento**.

Nel progetto servono a non ripetere codice. Ad esempio, tutti i pulsanti che parlano con il firmware passano da queste due:

```matlab
function ard_sendControl(app, setting_control, a, b, c, d)
    % setting_control: 0=STOP | -1=correzione flussimetri | -2=PID
    valore = single([setting_control a b c d]);
    data_send(app, valore);
end

function data_send(app, valore)
    % Pacchetto TX: 6 float32 (24 byte) + LF (0x0A) = 25 byte
    valori    = single(valore(1:6));
    pacchetto = [typecast(valori, 'uint8'), uint8(10)];
    write(app.ardObj, pacchetto, 'uint8');
end
```

Uso da un callback: `ard_sendControl(app, 0, 0, 0, 0, 0);` → invia STOP.

**Regola pratica:** il callback deve fare poco (leggere i campi, chiamare una function). La logica vera sta nelle function.

---

## 4. Callback

Un callback è una funzione che MATLAB esegue quando accade un evento. Ne esistono **due famiglie**, ed è il punto che confonde di più chi parte da zero.

### 4a. Callback di componenti (li crea App Designer)

Si creano da Design View: tasto destro sul componente → *Callbacks → Add …*. Firma fissa: `(app, event)`.

```matlab
function ConnessioneButtonValueChanged(app, event)
    value = app.ConnessioneButton.Value;   % oppure event.Value
    if value == 1
        app.ConnessioneButton.BackgroundColor = [0.39 0.83 0.07];  % verde
        connection(app);
    else
        disconnection(app);
        app.ConnessioneButton.BackgroundColor = [1.00 0.41 0.16];  % arancio
    end
end
```

Il collegamento evento → funzione lo scrive App Designer (zona grigia, in `createComponents`):

```matlab
app.ConnessioneButton.ValueChangedFcn = createCallbackFcn(app, @ConnessioneButtonValueChanged, true);
```

Dentro `event` c'è: `event.Value` (nuovo valore), `event.PreviousValue`, `event.Source` (chi ha scatenato l'evento).

**Callback condiviso.** `CALIBRATION_CALLBACK1` è associato a 6 campi numerici diversi: qualunque di essi cambi, si ricalcola la calibrazione. Un callback può servire più componenti.

### 4b. Callback di oggetti non grafici (li colleghi tu a mano)

`serialport` e `timer` **non** sono componenti dell'app, quindi App Designer non crea nulla: si collegano via codice con un *function handle*.

```matlab
configureCallback(app.ardObj, "byte", 38, @app.readSerialDataUART);   % metodo dell'app
app.timerObj_uart = timer('TimerFcn', @(src,evt) updatePlot(app));    % funzione anonima
```

Due forme, stesso scopo: `@app.metodo` (il metodo deve avere firma `(app, src, event)`) oppure `@(src,evt) funzione(app)`.

### Riepilogo: oggetto → callback → function

Il collegamento ha sempre la stessa catena: l'**oggetto** ha una property "callback" (il nome finisce in `Fcn`), legata a un evento; a quella property si assegna una **function** (un metodo dell'app) che MATLAB esegue quando l'evento accade. **Le function sono sempre normali function dell'app**: cambia solo *chi* fa il collegamento e in quale tab del Code Browser si trovano.

| Oggetto | Callback (evento) | Function eseguita | Collegamento |
| --- | --- | --- | --- |
| `StartProductionButton` (Button) | `ButtonPushedFcn` (click) | `StartProductionButtonPushed` | automatico, tab **Callbacks** |
| `ConnessioneButton` (StateButton) | `ValueChangedFcn` (cambia stato) | `ConnessioneButtonValueChanged` | automatico, tab **Callbacks** — esempio in §4a |
| `listaSeriale` (DropDown) | `DropDownOpeningFcn` (si apre il menu) | `listaSerialeDropDownOpening` | automatico, tab **Callbacks** |
| `app.ardObj` (serialport) | `BytesAvailableFcn` (arrivano 38 byte) | `readSerialDataUART` | a mano con `configureCallback`, tab **Functions** — esempio in §8 |
| `app.ardObj` (serialport) | `ErrorOccurredFcn` (errore / USB scollegata) | `disconnessione_cheker` | a mano con `= @app.metodo`, tab **Functions** — esempio in §8 |
| `app.timerObj_uart` (timer) | `TimerFcn` (ogni 0.2 s) | `updatePlot` | a mano nel `timer(...)`, tab **Functions** — esempio in §4b e §6 |

In pratica: le prime tre function le trovi nel tab **Callbacks** perché le ha create App Designer; le ultime tre sono function scritte da te (tab **Functions**) e agganciate a mano. Per la spiegazione passo per passo vedi anche la pagina MathWorks [Callbacks in App Designer](https://it.mathworks.com/help/matlab/creating_guis/write-callbacks-for-gui-in-app-designer.html) (§11).

---

## 5. Componenti chiave

Si leggono e scrivono sempre con `app.NomeComponente.Proprietà`.

| Componente | Proprietà principali | Uso nel progetto |
| --- | --- | --- |
| **Button** | `Text`, `Enable`, `ButtonPushedFcn` | `STOPButton` invia il comando di stop |
| **StateButton** | `Value` (true/false), `Text` | `ConnessioneButton`, `StartLoggingButton` |
| **UIAxes** | `XLim`, `YLim`, `title()`, `xlabel()` | grafici delle due pompe |
| **Gauge** | `Value`, `Limits` | `Volume1Gauge`, `TimeGauge` |
| **UITable** | `Data`, `ColumnName`, `RowName` | mostra i 9 valori grezzi |
| **Lamp** | `Color` | verde = dato ricevuto, giallo = pacchetto errato |
| **Panel** | `Enable` | `FunzionamentoPanel.Enable="off"` blocca tutti i figli |

**Gauge** — `Value` sposta l'ago, `Limits` definisce la scala:

```matlab
app.Volume1Gauge.Limits = [0 app.Massa1EditField.Value];   % scala 0 → massa massima
app.Volume1Gauge.Value  = app.values(8);                   % ago = massa erogata
```

**UITable** — basta assegnare un array a `Data`; i nomi colonna li hai già impostati in Design View (o in `createComponents`):

```matlab
app.UITable.Data = app.values;   % 1×9 → una riga, 9 colonne (dt, q1, V1, ... m2)
```

**UIAxes** — la regola d'oro: *ogni comando grafico deve dichiarare l'asse*, altrimenti MATLAB non sa dove disegnare. Il progetto usa `animatedline`, pensata per aggiungere punti in tempo reale:

```matlab
% una volta, in startupFcn
app.animLine_f1 = animatedline(app.UIAxes_values, 'Color','b', 'MaximumNumPoints', floor(5/app.dt));
% a ogni tick del timer
addpoints(app.animLine_f1, app.t, f1);
drawnow limitrate nocallbacks
```

`MaximumNumPoints` fa scorrere il grafico: tiene solo gli ultimi 5 s di dati. `limitrate` evita di ridisegnare più del necessario; `nocallbacks` impedisce che altri callback si intromettano durante il disegno.

---

## 6. startupFcn

È un callback **speciale**: non appartiene a nessun componente, parte **una sola volta**, subito dopo che l'interfaccia è stata creata e prima che l'utente possa usarla. È il posto giusto per l'inizializzazione. Se serve, si crea da Code View → Callbacks → Add → `startupFcn`.

Ordine di esecuzione all'avvio: costruttore → `createComponents` → `startupFcn`.

Cosa fa quella del progetto, in ordine:

```matlab
function startupFcn(app)
    app.setAppFontToConsolas();                 % 1. font uniforme su tutti i componenti
    app.UIFigure.Position = [x y w h];          % 2. finestra all'80% dello schermo, centrata
    initLogging(app);                           % 3. crea/azzera il CSV temporaneo
    app.StartLoggingButton.Value = false;       %    logging spento

    app.timerObj_uart = timer( ...              % 4. crea il timer (ma NON lo avvia)
        'ExecutionMode','fixedrate', 'Period',app.dt, ...
        'TimerFcn', @(src,event) updatePlot(app));

    app.animLine_f1 = animatedline(app.UIAxes_values, ...);   % 5. linee animate dei grafici
    app.HLine_f1 = yline(app.UIAxes_values, 0, '--r', 'Setpoint');  % 6. linea del setpoint

    app.listaSeriale.Items = uart_serial_reader(app);  % 7. cerca le porte seriali
end
```

**Cosa mettere qui:** valori iniziali, creazione di timer e oggetti grafici, scansione delle porte. **Cosa non metterci:** operazioni lunghe o che dipendono da un'azione dell'utente (es. l'apertura della porta seriale è nel pulsante *Connessione*, non qui).

---

## 7. Timer

Il timer esegue una funzione a intervalli regolari. Nel progetto separa due cose che vanno a ritmi diversi:

```
Arduino ──UART──► buffer ──(ogni 38 byte)──► readSerialDataUART ──► app.values
                                                                       │
                          timer (ogni 0.2 s) ──► updatePlot ◄──────────┘
                                                    ├─► UITable.Data
                                                    ├─► Gauge.Value
                                                    └─► addpoints (grafici)
```

- La **seriale** scrive `app.values` al ritmo del firmware.
- Il **timer** ridisegna a 5 Hz, un ritmo fisso e leggero per l'interfaccia. Disegnare a ogni pacchetto sovraccaricherebbe la grafica.

Ciclo di vita:

| Azione | Dove | Codice |
| --- | --- | --- |
| Creare | `startupFcn` | `timer('ExecutionMode','fixedrate','Period',app.dt,'TimerFcn',...)` |
| Avviare | `timer_plot` (dopo la connessione) | `app.t = 0; start(app.timerObj_uart);` |
| Fermare | `disconnection` | `stop(app.timerObj_uart);` |
| Distruggere | alla chiusura dell'app | `delete(app.timerObj_uart);` (vedi §9) |

`ExecutionMode = 'fixedrate'` = a cadenza fissa, indipendente da quanto dura la funzione. La documentazione MathWorks sconsiglia i timer per applicazioni real-time: qui va bene perché servono solo a visualizzare.

---

## 8. serialport

### Le fasi della comunicazione

1. **Trovare le porte** — `serialportlist()` restituisce l'elenco. `uart_serial_reader` le prova una a una e riconosce quella giusta dal byte di header `'A'`.
2. **Aprire** — nel callback del pulsante *Connessione* (`connection_serial`):

```matlab
app.ardObj = serialport(COM_port, 115200, "Timeout", 60);
app.ardObj.ErrorOccurredFcn = @app.disconnessione_cheker;            % callback errore
flush(app.ardObj);                                                   % svuota il buffer
configureCallback(app.ardObj, "byte", 38, @app.readSerialDataUART);  % callback ricezione
```

3. **Ricevere** — non si fa polling: si dice alla seriale *"chiamami ogni volta che hai 38 byte"* e MATLAB invoca `readSerialDataUART`.
4. **Inviare** — `write(app.ardObj, pacchetto, 'uint8')`, dentro `data_send`.
5. **Chiudere** — `app.ardObj = []` (vedi §9).

### Il protocollo

| Direzione | Contenuto | Byte |
| --- | --- | --- |
| RX (Arduino → PC) | `'A'` + 9 × float32 + `LF` | 1 + 36 + 1 = **38** |
| TX (PC → Arduino) | 6 × float32 + `LF` | 24 + 1 = **25** |

### Il callback di ricezione

```matlab
function readSerialDataUART(app, src, ~, ~)
    raw = read(src, 38, "uint8");            % legge esattamente 38 byte
    app.Lamp.Color = [0 1 0];                % verde: dato arrivato

    if raw(1) ~= uint8('A') || raw(end) ~= uint8(10)
        % pacchetto disallineato → cerca il prossimo 'A' e ricompone (omesso)
        app.Lamp.Color = [1 1 0];            % giallo: problema di sincronizzazione
        return
    end

    payload    = raw(2:37);                              % 36 byte di dati
    app.values = typecast(uint8(payload), 'single');     % → 9 float32
end
```

`typecast` reinterpreta gli stessi byte come `single`, senza convertire i valori: è il passaggio inverso di quello fatto in `data_send`. Il primo argomento del callback, `src`, è la seriale stessa: si legge da `src` (o da `app.ardObj`).

**Callback di errore** (`ErrorOccurredFcn`): se il cavo USB viene scollegato scatta `disconnessione_cheker`, che avvisa l'utente, disabilita il pannello di controllo e azzera `app.ardObj`.

---

## 9. Errori tipici e buone pratiche

- **`clear app.ardObj` non libera la property**: `clear` lavora su variabili, non su property. Per chiudere la porta usare `app.ardObj = [];` (il distruttore chiude la seriale).
- **Timer ancora attivo dopo la chiusura**: il timer tiene un riferimento all'app e continua a partire, generando errori. Creare un callback `CloseRequestFcn` per `UIFigure` (tasto destro sulla finestra in Design View):

```matlab
function UIFigureCloseRequest(app, event)
    try, stop(app.timerObj_uart); delete(app.timerObj_uart); catch, end
    app.ardObj = [];
    delete(app)
end
```

- **`try … catch` vuoti nascondono i bug** (es. in `updatePlot`). Mentre si sviluppa inserire un `disp("errore!")` nel `catch` (vedi §10).
- **Callback brevi.** I callback si interrompono a vicenda: nella ricezione solo lettura e decodifica, la grafica al timer (è già così).
- **Un nome, una sola volta.** Non rinominare i componenti a caso: App Designer aggiorna il codice, ma i riferimenti scritti a mano in stringhe no.

---

## 10. Debugging

Un'app di App Designer è difficile da debuggare per due motivi: i callback partono da soli (non c'è un "main" da seguire dall'alto) e un errore dentro un callback finisce nel Command Window **senza bloccare l'app**, quindi spesso sembra che semplicemente non succeda nulla. Tre tecniche da usare insieme.

### 10a. `try / catch` per far emergere gli errori

Si racchiudono le righe sospette in un `try` e nel `catch` basta un `disp("errore!")`: se compare nel Command Window, l'errore avviene lì dentro. L'errore più comune: dimenticare `.Value`.

```matlab
try
    app.Volume1Gauge = app.values(8);           % SBAGLIATO: sovrascrive il componente
    % app.Volume1Gauge.Value = app.values(8);   % CORRETTO: assegna il valore
catch
    disp("errore!")
end
```

`app.Volume1Gauge` è il *componente*, non un numero: il numero va nella sua proprietà (`.Value`, `.Text`, `.Data`, `.Color`…).

Per mostrare un avviso nell'interfaccia basta un classico `uialert(figura, messaggio, titolo)`:

```matlab
catch
    uialert(app.UIFigure, 'Errore durante il salvataggio', 'saveDatas');
end
```

**Attenzione:** molti `try/catch` del progetto (es. in `updatePlot`) hanno il `catch` vuoto, per non bloccare l'app durante l'uso. Se qualcosa "non si aggiorna", il primo passo è inserire un `disp("errore!")` in quel `catch`: se compare nel Command Window, l'errore c'è e sta avvenendo lì dentro.

### 10b. Breakpoint

Le property sono `private`: dal Command Window, `app.values` dà errore di accesso. Si possono leggere solo quando il codice è **fermo dentro un metodo dell'app**, quindi serve un breakpoint.

1. In Code View clicca sul numero di riga: compare un pallino rosso (es. prima riga di `updatePlot`).
2. Avvia l'app con **Run**.
3. Quando l'evento fa partire quel codice (click, tick del timer, dati sulla seriale) l'esecuzione si ferma sulla riga.
4. Nel Command Window (prompt `K>>`) scrivi `app.values`, `app.ardObj`, `app.t`, oppure guarda `app` nel Workspace.
5. Usa **Step** (riga per riga), **Continue** (riprendi) o **Quit Debugging** (esci).

Trucchi utili:

- **Breakpoint condizionale:** tasto destro sul pallino → *Set/Modify Condition*, ad esempio `app.t > 3`. Indispensabile nel timer, che altrimenti si ferma ogni 0.2 s.
- **Fermarsi sugli errori nascosti:** `dbstop if caught error` nel Command Window ferma il codice sulla riga che genera l'errore *anche se* un `try/catch` lo nasconderebbe. (`dbstop if error` copre solo gli errori non catturati; `dbclear all` toglie tutto.)
- **Un'app in pausa sembra congelata.** Se l'interfaccia non risponde, controlla se il Command Window mostra `K>>`: premi Continue oppure scrivi `dbquit`.
- **Mentre sei fermo, Arduino continua a inviare.** Alla ripartenza il buffer contiene dati vecchi: fai `flush(app.ardObj)` o riconnetti.

### 10c. Le variabili locali non sono globali

Una variabile creata in un callback o in una function (`raw`, `payload`, `f1`) esiste solo lì dentro: finita l'esecuzione sparisce e dal Command Window non si vede. Per controllarla:

- **Stamparla:** `disp(raw)`, `fprintf('f1 = %g\n', f1)`, `size(raw)`, `class(payload)`.
- **Fermarsi con un breakpoint** subito dopo la riga che la crea: a quel punto è nel workspace della funzione.
- **Copiarla nel workspace base**, così resta disponibile anche dopo:

```matlab
assignin('base', 'raw_debug', raw);    % 'raw_debug' compare nel Workspace
```

poi dal Command Window: `typecast(uint8(raw_debug(2:37)), 'single')`.

- **Salvarla in una property** (`app.ultimoRaw = raw;`, da aggiungere in Properties) e leggerla durante un breakpoint.

Togliere `disp`, `fprintf` e `assignin` a fine debug: nel timer (5 volte al secondo) intasano il Command Window.

### Esempio: "il grafico non si aggiorna"

1. La **Lamp** è verde? Se no, i dati non arrivano (porta, cavo, baud rate).
2. Breakpoint in `readSerialDataUART`: `raw` ha 38 elementi, inizia con 65 (`'A'`) e finisce con 10 (LF)?
3. Breakpoint condizionale in `updatePlot`: `app.values` ha 9 elementi? E soprattutto `app.StartLoggingButton.Value` è `true`? Gauge e grafici si aggiornano **solo con il logging attivo**.
4. `dbstop if caught error` per scovare gli errori nascosti dai `catch` vuoti.

---

## 11. Link utili

**Seriale e Arduino**

- [`serialport`](https://it.mathworks.com/help/matlab/ref/serialport.html) — creare e usare la connessione seriale.
- [`configureCallback`](https://it.mathworks.com/help/matlab/ref/serialport.configurecallback.html) — callback su numero di byte o terminatore.
- [Read Streaming Data from Arduino](https://it.mathworks.com/help/matlab/import_export/read-streaming-data-from-arduino.html) — **esempio consigliato**: lettura in streaming, il più vicino a quello che fa questa app.

**App Designer**
- [Sviluppo di applicazioni con App Designer](https://it.mathworks.com/help/matlab/app-designer.html?s_tid=CRUX_lftnav) - Pagina principale di App designer.
- [Grid Layout](https://it.mathworks.com/help/matlab/creating_guis/using-grid-layout-managers.html) - Corretto impaginazione degli elementi in app. 
- [Callbacks in App Designer](https://it.mathworks.com/help/matlab/creating_guis/write-callbacks-for-gui-in-app-designer.html) — argomenti `app` ed `event`, come si creano.
- [Code View, property e dati condivisi](https://it.mathworks.com/help/matlab/creating_guis/app-designer-code-generation.html) — codice generato e condivisione dati tra callback.
- [App multifinestra](https://it.mathworks.com/help/matlab/creating_guis/creating-multiwindow-apps-in-app-designer.html) — funzioni pubbliche e `startupFcn` con argomenti in ingresso.
- [Esempi di App](https://it.mathworks.com/help/matlab/examples.html?s_tid=CRUX_topnav&category=app-designer) - Esempi e spunti di applicazioni per matlab app designer.

**Componenti e oggetti**
- [`timer`](https://it.mathworks.com/help/matlab/ref/timer.html) — `TimerFcn`, `ExecutionMode`, `Period`.
- [`animatedline`](https://it.mathworks.com/help/matlab/ref/animatedline.html) — grafici in tempo reale.
- [`uigauge`](https://it.mathworks.com/help/matlab/ref/uigauge.html) e [proprietà del Gauge](https://it.mathworks.com/help/matlab/ref/matlab.ui.control.gauge.html).
- [Proprietà del Table UI component](https://it.mathworks.com/help/matlab/ref/matlab.ui.control.table.html) — `Data`, `ColumnName`, ecc.



**Matlab Complier e Generazione codice**
- [Creazione Applicazione](https://it.mathworks.com/help/matlab/creating_guis/app-sharing.html) - Come generare gli esecutivi per utenti finali
- [Compatibilità Toolbox](https://it.mathworks.com/products/compiler/compiler_support.html) - Lista di codici e librerie che possono essere usate in un esecutivo. (e' comunque possibile creare un .mlapp funzionante ma poi non generabile)
