#include <SAMD21turboPWM.h>

TurboPWM TURBO_pwm;

void setup_pwm_hig_freq(int _mot1,int _mot2) {
 
  TURBO_pwm.setClockDivider(1, false);  // 48 MHz
  TURBO_pwm.timer(0, 16, 1200, true);

  /* * CALCOLO FREQUENZA PWM (SAMD21 Turbo PWM):
 * Formula: Frequenza = Clock / (Prescaler * Steps)
 * * Con Clock a 48MHz (standard MKR 1010):
 * f = 48.000.000 / (8 * 1200) = 5.0 00 Hz (5kHz)
 * * - Prescaler: 8 (Riduce la velocità del clock di base)
 * - Steps: 1200 (Definisce la risoluzione del Duty Cycle)
 * -range
 */
  pinMode(0, INPUT);


  TURBO_pwm.analogWrite(_mot1, 0);
  TURBO_pwm.analogWrite(_mot2, 0);
// TURBO_pwm.analogWrite([pin number], [0-1000]);
}


int calcolaPWM_channel_fast(float &q, float &Vout,  float qalfa_, float qbeta_, float Valfa_, float Vbeta_) {

  if (q == 0.0f) {
    // // scan_mlpx();//reset
    Vout = 0.0f;
    return 0;
  }
 float V_max=(1000-Vbeta_) / Valfa_;
 
 float v= qalfa_ * q + qbeta_;

  Vout =constrain(v,0,V_max);
  q=constrain(q,0, (V_max - qbeta_) / qalfa_);
  

 float pwm=Vout*(Valfa_)+Vbeta_;

  
  return constrain(round(pwm), 0, 1000);
}





/* CODICE ESEMPIIO FUNZIONAMENTO
void loop() {

  if (Serial.available() > 0) {

    v4 = Serial.parseInt();  // primo numero

    v5 = Serial.parseInt();  // secondo numero

    secondi = Serial.parseInt();  // secondo numero

    while (Serial.available() > 0) {
      Serial.read();
    }
    running = true;

    previousMillis = millis();


    // sicurezza
    v4 = constrain(v4, 0, 1000);
    v5 = constrain(v5, 0, 1000);
    secondi = constrain(secondi, 1, 120);
    TURBO_pwm.analogWrite(4, v4);
    TURBO_pwm.analogWrite(5, v5);

    Serial.print(v4);
    Serial.print("\t");
    Serial.print(v5);

    Serial.print("\t");
    Serial.println(secondi);
  }
  deltat = millis() - previousMillis;
  if (running) {
    Serial.print(running);
    Serial.print("\t");
    Serial.print(deltat);
    Serial.print("\t");
    Serial.print(secondi);
    Serial.print("\t");
    Serial.print(v4);
    Serial.print("\t");
    Serial.println(v5);
  }
  if (deltat >= secondi * 1000 && running) {
    v4 = 0;
    v5 = 0;

    Serial.print("STOP");
    TURBO_pwm.analogWrite(4, v4);
    TURBO_pwm.analogWrite(5, v5);

    running = false;
  }
  delay(100);
}


*/
