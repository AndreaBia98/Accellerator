// ============================================================
//  fluigent_RTOS.ino
//  Board : Arduino MKR WiFi 1010 (SAMD21)
//  RTOS  : Arduino-FreeRTOS-SAMD21 by Scott Briscoe
//
//  STRUTTURA TASK:
//
//  [Sensori]  50Hz  prio 4  legge ADC + filtra → flusso[]
//  [Control]  10Hz  prio 3  PID/massa/PWM — path critico
//  [Stampa]   10Hz  prio 2  prepara dati[] + Serial/Serial1
//  [Input]     —    prio 1  riceve comandi USB + UART
//  [LED]       —    prio 1  heartbeat
//  [Monitor]   —    prio 1  diagnostica RTOS ogni 10s
// ============================================================

#include <FreeRTOS_SAMD21.h>
#include <PID_v1_bc.h>
#include <Arduino.h>

#include "globals.h"

// ---- Helper delay (stile Briscoe) ---------------------------
void myDelayMs(int ms) {
  vTaskDelay((ms * 1000) / portTICK_PERIOD_US);
}
void myDelayMsUntil(TickType_t *prev, int ms) {
  vTaskDelayUntil(prev, (ms * 1000) / portTICK_PERIOD_US);
}

// ---- Pin ----------------------------------------------------
#define MOT1        5
#define MOT2        4
#define VOLT        A0
#define VOLT2       A1
#define PIN_PORTATA  A3
#define PIN_PORTATA2 A4
#define RTS         1

// ---- Include header progetto --------------------------------
#include "pwm_high_freq.h"
#include "flussimetri_sonotech.h"
#include "gestione_dati.h"

// ================================================================
//  DEFINIZIONI variabili globali
// ================================================================
float qnew1 = 0, qnew2 = 0;
float q1 = 0, q2 = 0;
float m1_set = 0, m2_set = 0;
int   pwm1 = 0, pwm2 = 0;
float massa[2]  = { 0, 0 };
float flusso[2] = { 0, 0 };
float V1 = 0, V2 = 0;
float f1 = 0, f2 = 0;

// float qalfa1 = 0.734f,  qbeta1 = 1.554f;
// float qalfa2 = 0.734f,  qbeta2 = 1.554f;

float qalfa1 = 0.4817f,  qbeta1 = 3.2620f;
float qalfa2 = 0.4423f,  qbeta2 = 3.9145f;

float c1 =1,  c2 = 1;
float Valfa  = 40.82321678f;
float Vbeta  = 27.12546532f;
float settings[7];

float kp = 10, ki = 10, kd = 0;
bool  pid_flag   = true;
bool  massa_stop = false;
bool  pid_active = false;

double setpoint1, input1, pwm1_d;
double setpoint2, input2, pwm2_d;
PID pid1(&input1, &pwm1_d, &setpoint1, kp, ki, kd, DIRECT);
PID pid2(&input2, &pwm2_d, &setpoint2, kp, ki, kd, DIRECT);

FLOATUNION_t dati[9];
FLOATUNION_t massa_union;

RTCZero rtc;
PreciseRTCTimer timer(rtc);

// ---- Handle task --------------------------------------------
TaskHandle_t Handle_Sensori;
TaskHandle_t Handle_Control;
TaskHandle_t Handle_Stampa;
TaskHandle_t Handle_Input;
TaskHandle_t Handle_LED;
TaskHandle_t Handle_Monitor;

// ---- Mutex --------------------------------------------------
SemaphoreHandle_t xMutexDati;    // flusso[], massa[], dati[]
SemaphoreHandle_t xMutexSerial;  // Serial / Serial1
SemaphoreHandle_t xMutexPWM;     // scrittura PWM

// ================================================================
//  f_massa
// ================================================================
float f_massa(float _flusso, float _massa_old, int dt) {
  if (_flusso < 1) _flusso = 0;
  return _flusso * dt / 60000.0f + _massa_old;
}

// ================================================================
//  TASK 1 — Sensori  20ms / 50Hz  prio 4 (massima)
//  Legge ADC e filtra → aggiorna flusso[]
//  Nessun calcolo pesante, solo lettura e scrittura sotto mutex.
// ================================================================
static void TaskLeggiSensori(void *pvParameters) {
  static const float falfa1 = 18.93939394f;
  static const float fbeta1 = -12.5f;
  TickType_t xLastWakeTime = xTaskGetTickCount();

  while (1) {
    float f0 = 0, f1_local = 0;
    if (q1 > 0) leggiFlussimetro(PIN_PORTATA,  f0,       falfa1, fbeta1, 0.5);
    if (q2 > 0) leggiFlussimetro(PIN_PORTATA2, f1_local, falfa1, fbeta1, 0.5);

    if (xSemaphoreTake(xMutexDati, (10 * 1000) / portTICK_PERIOD_US) == pdTRUE) {
      flusso[0] = f0*c1;//1.0833;//35/30
      flusso[1] =f1_local*c2 ;//*1.3611;// 35/30* 32.5/30;
      xSemaphoreGive(xMutexDati);
    }
    myDelayMsUntil(&xLastWakeTime, 20);
  }
}

// ================================================================
//  TASK 2 — Control  100ms / 10Hz  prio 3
//  Path critico: legge flusso[], aggiorna PID o PWM manuale,
//  calcola massa, controlla soglia, aggiorna dati[] per la stampa.
//  Tutto in un unico task → ordine deterministico garantito.
// ================================================================
static void TaskControl(void *pvParameters) {
  TickType_t xLastWakeTime = xTaskGetTickCount();

  while (1) {
    PreciseRTCTimer::TimeStamp t = timer.update();
    uint32_t dt = timer.getDelta();

    if (xSemaphoreTake(xMutexDati, (10 * 1000) / portTICK_PERIOD_US) == pdTRUE) {

      // 1. Leggi flusso fresco (già aggiornato da TaskSensori)
      input1 = kalman1.updateEstimate(flusso[0]);
      
      input2 = kalman2.updateEstimate(flusso[1]);

      // 2. Aggiorna massa
      massa[0] = f_massa(flusso[0], massa[0], dt);
      massa[1] = f_massa(flusso[1], massa[1], dt);

      // 3. Controllo soglia massa (una sola volta grazie a massa_stop)
      if (!massa_stop && (m1_set + m2_set) > 0 &&
          (massa[0] + massa[1]) >= (m1_set + m2_set)) {
        massa_stop = true;
        q1 = 0; q2 = 0;
        V1 = 0; V2 = 0;
        pwm1_d= 0; pwm2_d = 0;
        pid_active = false;
        pid1.SetMode(MANUAL);
        pid2.SetMode(MANUAL);
        if (xSemaphoreTake(xMutexPWM, (5 * 1000) / portTICK_PERIOD_US) == pdTRUE) {
          TURBO_pwm.analogWrite(MOT1, 0);
          TURBO_pwm.analogWrite(MOT2, 0);
          xSemaphoreGive(xMutexPWM);
        }
      }

      // 4. PID (solo se attivo e soglia non raggiunta)
      if (pid_active) {
        pid1.Compute();
        pid2.Compute();

        if (xSemaphoreTake(xMutexPWM, (5 * 1000) / portTICK_PERIOD_US) == pdTRUE) {
          pwm1_d = constrain((int)pwm1_d, 0, 1000);
          pwm2_d = constrain((int)pwm2_d, 0, 1000);
          V1 = constrain((pwm1_d - Vbeta) / Valfa, 0, 24);
          V2 = constrain((pwm2_d - Vbeta) / Valfa, 0, 24);
          TURBO_pwm.analogWrite(MOT1, pwm1_d);
          TURBO_pwm.analogWrite(MOT2, pwm2_d);
          xSemaphoreGive(xMutexPWM);
        }
      }

      // 5. Prepara pacchetto dati per TaskStampa
      dati[0].number = (float)dt / 1000.0f;
      dati[1].number = q1;
      dati[2].number = V1;
      dati[3].number = q2;
      dati[4].number = V2;
      dati[5].number = flusso[0];
      dati[6].number = flusso[1];
      dati[7].number = massa[0];
      dati[8].number = massa[1];

      xSemaphoreGive(xMutexDati);
    }

    // Notifica TaskStampa che ci sono dati nuovi
    if (Handle_Stampa != NULL) {
      xTaskNotifyGive(Handle_Stampa);
    }

    myDelayMsUntil(&xLastWakeTime, 100);
  }
}

// ================================================================
//  TASK 3 — Stampa  prio 2
//  Aspetta notifica da TaskControl, poi stampa Serial + Serial1.
//  Priorità più bassa del path critico: non rallenta mai il controllo.
// ================================================================
static void TaskStampa(void *pvParameters) {
  // Timestamp locale — letto appena sveglio, prima che TaskControl aggiorni di nuovo
  PreciseRTCTimer::TimeStamp t;

  while (1) {
    // Aspetta notifica da TaskControl (blocca senza consumare CPU)
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    t = timer.update();

    if (xSemaphoreTake(xMutexSerial, (10 * 1000) / portTICK_PERIOD_US) == pdTRUE) {
      char tbuf[24];
      snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d.%03d ",
               t.hours, t.minutes, t.seconds, t.milliseconds);
      Serial.print(tbuf);
      //stampa_bin();
      stampa_bin();
      xSemaphoreGive(xMutexSerial);
    }
  }
}

// ================================================================
//  TASK 4 — Input seriale  prio 1
// ================================================================
static void TaskSerialInput(void *pvParameters) {
  while (1) {
    if (Serial.available() > 0) {
      if (xSemaphoreTake(xMutexDati, (10 * 1000) / portTICK_PERIOD_US) == pdTRUE) {
        if (xSemaphoreTake(xMutexPWM, (5 * 1000) / portTICK_PERIOD_US) == pdTRUE) {
          gestisciComandiSeriali();
          xSemaphoreGive(xMutexPWM);
        }
        xSemaphoreGive(xMutexDati);
      }
    }
    if (Serial1.available() >= 25) {
      if (xSemaphoreTake(xMutexDati, (10 * 1000) / portTICK_PERIOD_US) == pdTRUE) {
        if (xSemaphoreTake(xMutexPWM, (5 * 1000) / portTICK_PERIOD_US) == pdTRUE) {
          gestisciComandiSeriali_UART();
          xSemaphoreGive(xMutexPWM);
        }
        xSemaphoreGive(xMutexDati);
      }
    }
    myDelayMs(5);
  }
}

// ================================================================
//  TASK 5 — LED heartbeat  prio 1
// ================================================================
static void TaskLED(void *pvParameters) {
  while (1) {
    digitalWrite(LED_BUILTIN, HIGH);
    myDelayMs(1000);
    digitalWrite(LED_BUILTIN, LOW);
    myDelayMs(500);
  }
}

// ================================================================
//  TASK 6 — Monitor RTOS  ogni 10s  prio 1
// ================================================================
static char ptrTaskList[400];
static void taskMonitor(void *pvParameters) {
  while (1) {
    myDelayMs(10000);
    if (xSemaphoreTake(xMutexSerial, (20 * 1000) / portTICK_PERIOD_US) == pdTRUE) {
      Serial.println("\n==== RTOS MONITOR ====");
      Serial.print("Free Heap : "); Serial.print(xPortGetFreeHeapSize());            Serial.println(" bytes");
      Serial.print("Min  Heap : "); Serial.print(xPortGetMinimumEverFreeHeapSize()); Serial.println(" bytes");
      Serial.println("Task            State  Prio  Stack  Num");
      vTaskList(ptrTaskList);
      Serial.println(ptrTaskList);
      Serial.println("Stack HWM (bytes liberi):");
      Serial.print("  Sensori : "); Serial.println(uxTaskGetStackHighWaterMark(Handle_Sensori));
      Serial.print("  Control : "); Serial.println(uxTaskGetStackHighWaterMark(Handle_Control));
      Serial.print("  Stampa  : "); Serial.println(uxTaskGetStackHighWaterMark(Handle_Stampa));
      Serial.print("  Input   : "); Serial.println(uxTaskGetStackHighWaterMark(Handle_Input));
      Serial.print("  LED     : "); Serial.println(uxTaskGetStackHighWaterMark(Handle_LED));
      Serial.println("======================");
      Serial.flush();
      xSemaphoreGive(xMutexSerial);
    }
  }
}

// ================================================================
//  SETUP
// ================================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial1.begin(115200);
  Serial.setTimeout(5);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(VOLT,        INPUT);
  pinMode(VOLT2,       INPUT);
  pinMode(PIN_PORTATA, INPUT);
  pinMode(RTS,         INPUT);

  setup_pwm_hig_freq(MOT1, MOT2);
  analogWriteResolution(12);
  analogReadResolution(10);

  pid1.SetOutputLimits(0, 1000); pid1.SetMode(MANUAL); pid1.SetSampleTime(100);
  pid2.SetOutputLimits(0, 1000); pid2.SetMode(MANUAL); pid2.SetSampleTime(100);

  settings[0] = qalfa1; settings[1] = qbeta1;
  settings[2] = kp;     settings[3] = ki;
  settings[4] = kd;     settings[5] = qalfa2;
  settings[6] = qbeta2;

  rtc.begin();
  rtc.setTime(0, 0, 0);
  timer.begin();

  vSetErrorLed(LED_BUILTIN, HIGH);
  vSetErrorSerial(&Serial);

  xMutexDati   = xSemaphoreCreateMutex();
  xMutexSerial = xSemaphoreCreateMutex();
  xMutexPWM    = xSemaphoreCreateMutex();

  //                                              stack  prio
  xTaskCreate(TaskLeggiSensori, "Sensori", 256,  NULL, tskIDLE_PRIORITY + 3, &Handle_Sensori);
  xTaskCreate(TaskControl,      "Control", 512,  NULL, tskIDLE_PRIORITY + 3, &Handle_Control);
  xTaskCreate(TaskStampa,       "Stampa",  512,  NULL, tskIDLE_PRIORITY + 3, &Handle_Stampa);
  xTaskCreate(TaskSerialInput,  "Input",   256,  NULL, tskIDLE_PRIORITY + 2, &Handle_Input);
  xTaskCreate(TaskLED,          "LED",     128,  NULL, tskIDLE_PRIORITY + 1, &Handle_LED);
  xTaskCreate(taskMonitor,      "Monitor", 256,  NULL, tskIDLE_PRIORITY + 1, &Handle_Monitor);

  Serial.println("Scheduler avvio...");
  Serial.flush();
  vTaskStartScheduler();

  while (1) {
    Serial.println("Scheduler Failed!");
    Serial.flush();
    delay(1000);
  }
}

void loop() {}
