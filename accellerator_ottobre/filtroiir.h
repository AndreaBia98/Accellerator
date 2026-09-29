// Coefficienti filtro (comuni ai due segnali)
float b[2] = {0.0501f, 0.0501f};
float a[2] = {1.0000f, -0.8997f};
float bc[4]={2.91464944656977e-05,	8.74394833970930e-05,	8.74394833970930e-05,	2.91464944656977e-05};
float ac[4]={1,	-2.87435689267748,	2.75648319522570,	-0.881893130592486};
// Stato per segnale 1
float x_hist1[2] = {0}, y_hist1[2] = {0};

// Stato per segnale 2
float x_hist2[2] = {0}, y_hist2[2] = {0};

// Filtro generico per array storico
float IIR_filter(float x, float* x_hist, float* y_hist) {
  // Shift
  x_hist[1] = x_hist[0];
  x_hist[0] = x;

  y_hist[1] = y_hist[0];

  // Formula differenziale
  float y = b[0]*x_hist[0] + b[1]*x_hist[1] - a[1]*y_hist[1];

  y_hist[0] = y;
  return y;
}



/*
// Stato per segnale 1
float x_hist1[4] = {0}, y_hist1[4] = {0};

// Stato per segnale 2
float x_hist2[4] = {0}, y_hist2[4] = {0};

// Filtro generico per array storico
float IIR_filter(float x, float* x_hist, float* y_hist) {
  // Shift degli ingressi
  x_hist[3] = x_hist[2];
  x_hist[2] = x_hist[1];
  x_hist[1] = x_hist[0];
  x_hist[0] = x;

  // Shift degli output
  
  y_hist[3] = y_hist[2];
  y_hist[2] = y_hist[1];
  y_hist[1] = y_hist[0];

  // Formula differenziale completa
  float y = bc[0] * x_hist[0]
          + bc[1] * x_hist[1]
          + bc[2] * x_hist[2]
          + bc[3] * x_hist[3]
          - ac[1] * y_hist[0]
          - ac[2] * y_hist[1]
          - ac[3] * y_hist[2];

  // Salvo nuovo output nello storico
  y_hist[0] = y;

  return y;
}
*/