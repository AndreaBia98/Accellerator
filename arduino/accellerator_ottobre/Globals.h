// ============================================================
//  globals.h
//  Dichiarazioni extern di tutte le variabili globali condivise
//  tra il .ino e gli header del progetto (gestione_dati.h, ecc.)
//
//  COME FUNZIONA:
//    - Le variabili sono DEFINITE una sola volta nel .ino
//    - Qui vengono solo DICHIARATE con extern
//    - Ogni .h che ne ha bisogno include questo file
// ============================================================
#pragma once

#include <RTCZero.h>
#include "PreciseRTCTimer.h"
#include <PID_v1_bc.h>

// ---- Tipo union float/bytes (deve stare qui, prima degli extern) ----
typedef union {
  float   number;
  uint8_t bytes[4];
} FLOATUNION_t;

// ---- Dati idraulici ------------------------------------------------
extern float qnew1, qnew2;
extern float q1, q2;
extern float m1_set, m2_set;
extern int   pwm1, pwm2;
extern float massa[2];
extern float flusso[2];

extern float flusso_k[2];
extern float V1, V2;
extern float f1, f2;

// ---- Calibrazione --------------------------------------------------
extern float qalfa1, qbeta1;
extern float qalfa2, qbeta2;
extern float c1   , c2;
extern float d1   , d2;
extern float Valfa, Vbeta;
extern float settings[7];

// ---- PID -----------------------------------------------------------
extern float kp, ki, kd;
extern bool  pid_flag;
extern bool  massa_stop;
extern bool  pid_active;     // true = case 2 attivo, TaskPID controlla i motori

extern double setpoint1, input1, pwm1_d;
extern double setpoint2, input2, pwm2_d;
extern PID pid1;
extern PID pid2;

// ---- Pacchetto dati seriale ----------------------------------------
extern FLOATUNION_t dati[9];
extern FLOATUNION_t massa_union;

// ---- RTC / Timer ---------------------------------------------------
extern RTCZero rtc;
extern PreciseRTCTimer timer;