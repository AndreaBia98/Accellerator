const float alfa_filtro = 0.05;

// variabile persistente del filtro
static float tensioneFiltrata = 0.0;

// // 1. Lettura ADC -> tensione
// float letturaGrezza = analogRead(PIN_PORTATA2) * (3.3 / 1023.0);

// // 2. Filtro esponenziale sulla tensione
// tensioneFiltrata = (alfa_filtro * letturaGrezza) + ((1.0 - alfa_filtro) * tensioneFiltrata);

// // 3. Scalatura in portata
// float flussoFiltrato = 154.37 * tensioneFiltrata - 153.89;

// // 4. Output
// flusso[0] = constrain(flussoFiltrato,0,1000);   // flusso filtrato
// flusso[1] = tensioneFiltrata;    // tensione grezza

float falfa1=18.93939394;//119.43;//154.37;
float fbeta1=-12.5;
//118.57;//153.89;

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

    // 3. Scalatura 
    float flussoFiltrato =(falfa_ * tensioneFiltrata[idx] + fbeta_);
        // if(flussoFiltrato<0){
            
        // }

    // 4. Output
    flusso_out =constrain(flussoFiltrato, -1000, 1000);
    //tensione_out = tensioneFiltrata[idx];
}