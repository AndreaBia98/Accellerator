const float alfa_filtro = 0.05;

// variabile persistente del filtro
static float tensioneFiltrata = 0.0;

// +--------+-------+------------+
// | I (mA) | v (V) | q (ml/min) |
// +--------+-------+------------+
// | 4      | 0.66  | 0          |
// | 20     | 3.3   | 50         |
// +--------+-------+------------+
// regressione Q=falfa*V+fbeta

float falfa1=18.93939394;
float fbeta1=-12.5;
float R=144;

//const float alfa_filtro = 1;
#include "kalman.h"
void leggiFlussimetro(uint8_t pin_portata, float &flusso_out, float falfa_,float fbeta_,float alfa_filtro) {
    
    // Stato persistente separato per ogni pin (max 2 sensori)
    static float tensioneFiltrata[2] = {0.0, 0.0};

    // Associa il pin a un indice (0 o 1)
    uint8_t idx = (pin_portata == PIN_PORTATA) ? 0 : 1;

    // 1. Lettura ADC -> tensione
    float letturaGrezza = analogRead(pin_portata) * (3.3 / 1023.0);

    // 2. Filtro esponenziale IIR Y(n)=a*x(n)+(1-a)*Y(n-1)
    tensioneFiltrata[idx] = (alfa_filtro * letturaGrezza) +((1.0 - alfa_filtro) * tensioneFiltrata[idx]);

    // 3. Scalatura 165/144 
    float flussoFiltrato =(falfa_ * 165/R*tensioneFiltrata[idx] + fbeta_);

    // 4. Output
    flusso_out =constrain(flussoFiltrato, -1000, 1000);
    //tensione_out = tensioneFiltrata[idx];
}