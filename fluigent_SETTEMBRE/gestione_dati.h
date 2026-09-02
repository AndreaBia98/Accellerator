// gestione_dati.h — invariato nella logica, aggiunto solo globals.h
#pragma once
#include "globals.h"     // ← unica aggiunta: rende visibili tutte le variabili globali
extern bool massa_stop;  // reset quando arriva nuovo comando
extern bool pid_active;  // ← unica aggiunta: rende visibili tutte le variabili globali

void input_managment(float ricevuti[]) {

  switch ((int)ricevuti[0]) {

    case 0:
      qnew1 = ricevuti[1];
      qnew2 = ricevuti[2];
      q1 = qnew1;
      q2 = qnew2;
      m1_set = ricevuti[3];
      m2_set = ricevuti[4];
      massa[0] = 0;
      massa[1] = 0;
      massa_stop = false;  // nuovo comando → riabilita il controllo soglia
      pid_active = false;  // torna a comando manuale, esce dal PID
      pid1.SetMode(MANUAL);
      pid2.SetMode(MANUAL);
      pwm1 = calcolaPWM_channel_fast(q1, V1, qalfa1, qbeta1, Valfa, Vbeta);
      pwm1_d=pwm1;

      pwm2 = calcolaPWM_channel_fast(q2, V2, qalfa2, qbeta2, Valfa, Vbeta);
      
      pwm2_d=pwm2;
      TURBO_pwm.analogWrite(MOT1, pwm1);
      TURBO_pwm.analogWrite(MOT2, pwm2);
      break;

    case 1:
      qnew1 = ricevuti[1];
      qnew2 = ricevuti[2];
      q1 = qnew1;
      q2 = qnew2;

      m1_set = ricevuti[3];
      m2_set = ricevuti[4];
      massa[0] = 0;
      massa[1] = 0;
      massa_stop = false;  // nuovo comando → riabilita il controllo soglia
      pid_active = false;  // torna a comando manuale, esce dal PID
      pid1.SetMode(MANUAL);
      pid2.SetMode(MANUAL);
      pwm1 = calcolaPWM_channel_fast(q1, V1, 1, 0, Valfa, Vbeta);
      pwm2 = calcolaPWM_channel_fast(q2, V2, 1, 0, Valfa, Vbeta);
      TURBO_pwm.analogWrite(MOT1, pwm1);
      TURBO_pwm.analogWrite(MOT2, pwm2);
      break;

    case 2:
      // Attiva il controllo PID di portata
      // ricevuti[1] = setpoint q1 (target flusso canale 1)
      // ricevuti[2] = setpoint q2 (target flusso canale 2)
      // ricevuti[3] = m1_set (massa target canale 1, opzionale)
      // ricevuti[4] = m2_set (massa target canale 2, opzionale)

      setpoint1 = ricevuti[1];
      setpoint2 = ricevuti[2];
      q1 = setpoint1;
      q2 = setpoint2;
      m1_set = ricevuti[3];
      m2_set = ricevuti[4];
      massa[0] = 0;
      massa[1] = 0;
      massa_stop = false;

      pid_active = true;  // TaskPID prende il controllo dei motori
                          // Reset memoria PID: azzera integrale e stato interno
      pid1.SetMode(MANUAL);
      pid2.SetMode(MANUAL);

      // Seed output dalla curva di calibrazione
      {
        float q1_est = setpoint1;
        float q2_est = setpoint2;
        float V1_est, V2_est;
        pwm1_d = calcolaPWM_channel_fast(q1_est, V1_est, qalfa1, qbeta1, Valfa, Vbeta);
        pwm2_d = calcolaPWM_channel_fast(q2_est, V2_est, qalfa2, qbeta2, Valfa, Vbeta);
      }

      pid1.SetMode(AUTOMATIC);
      pid2.SetMode(AUTOMATIC);
      break;
    case -1:
      c1 = ricevuti[1];
      c2 = ricevuti[2];

      break;


    case -10: //SETTING DEI Qs
      qalfa1 = ricevuti[1];
      qbeta1 = ricevuti[2];
      qalfa2 = ricevuti[3];
      qbeta2 = ricevuti[4];
      break;

    case -2:

      kp = ricevuti[1];
      ki = ricevuti[2];
      kd = ricevuti[3];
      pid1.SetTunings(kp, ki, kd);
      pid2.SetTunings(kp, ki, kd);

      pid_flag = ricevuti[4];
      if (kp == 0 && ki == 0 && kd == 0) pid_flag = false;
      break;

    case 12:
      //rtc.begin();
      rtc.setTime(int(ricevuti[1]), int(ricevuti[2]), int(ricevuti[3]));
      //timer.begin();
      break;

    default:
      break;
  }

  settings[0] = qalfa1;
  settings[1] = qbeta1;
  settings[2] = kp;
  settings[3] = ki;
  settings[4] = kd;
  settings[5] = qalfa2;
  settings[6] = qbeta2;
}

// ---- Ricezione comandi USB (testo) ---------------------------------
float ricevuti[6];

void gestisciComandiSeriali() {
  if (Serial.available() > 0) {
    for (int i = 0; i < 5; i++) {
      ricevuti[i] = Serial.parseFloat();
      Serial.print(ricevuti[i]);
      Serial.print("\t");
    }
    
      Serial.println("\t");
    input_managment(ricevuti);
  }
}

// ---- Ricezione comandi UART (binario) ------------------------------
struct PayloadRicezione {
  FLOATUNION_t valori[6];
  char terminatore;
} datiIn;

void gestisciComandiSeriali_UART() {
  if (Serial1.available() >= 25) {
    Serial1.readBytes((uint8_t*)&datiIn, 25);

    if (datiIn.terminatore == '\n') {
      for (int i = 0; i < 6; i++) {
        ricevuti[i] = datiIn.valori[i].number;
        Serial.print(ricevuti[i]);
        Serial.print(" ");
      }
      Serial.println(datiIn.terminatore);
      input_managment(ricevuti);
    } else {
      while (Serial1.available() > 0) Serial1.read();
    }
  }
}

// ---- Stampa dati su Serial + Serial1 -------------------------------
const char* nomi[] = { "dt", "q1", "V1", "q2", "V2", "f1", "f2", "m1", "m2" };

void stampa() {
  Serial1.write('A');
  Serial.print(" ");
  for (int i = 0; i < 9; i++) {
    Serial.print(nomi[i]);
    Serial.print(":");
    Serial.print(dati[i].number, 3);
    Serial.print(" ");
    Serial1.write(dati[i].bytes, 4);
  }
  Serial.print(" pid_active:");
  Serial.print(pid_active);
  Serial.print(" input1:");
  Serial.print(input1);

  Serial.print(" input2:");
  Serial.print(input2);
  Serial.print(" pwm1_d:");
  Serial.print(pwm1_d);

  Serial.print(" pwm2_d:");
  Serial.print(pwm2_d);

  Serial.println();
  Serial1.print('\n');
}



void stampa_bin() {
  Serial1.write('A');
  static char buffer[150];
  int pos = 0;

  pos += snprintf(buffer + pos, sizeof(buffer) - pos, " ");

  for (int i = 0; i < 9; i++) {
    pos += snprintf(buffer + pos, sizeof(buffer) - pos,
                    "%s:%.3f ",
                    nomi[i],
                    dati[i].number);

    Serial1.write(dati[i].bytes, 4);
  }

  pos += snprintf(buffer + pos, sizeof(buffer) - pos,
                  "pid_active:%d",
                  pid_active);

  Serial.print(buffer);

  Serial.print(" input1:");
  Serial.print(input1);

  Serial.print(" input2:");
  Serial.print(input2);
  Serial.print(" pwm1_d:");
  Serial.print(pwm1_d, 0);

  Serial.print(" pwm2_d:");
  Serial.print(pwm2_d, 0);

  Serial.println(" ");
  Serial1.print('\n');
}