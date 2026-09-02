#include <SimpleKalmanFilter.h>

float measurementError =2;
float estimationError = 2;
float processNoise = 0.2;

SimpleKalmanFilter kalman1(measurementError, estimationError, processNoise);

SimpleKalmanFilter kalman2(measurementError, estimationError, processNoise);




// CODICE ESEMPIO
const float amplitude = 1.0;
const float frequency = 0.5;
const float sampleTime = 0.02;

float t = 0;

void SimpleKalmanFilter_loop_EXAMPLE() {

  // Controlla se sono arrivati nuovi parametri
  if (Serial.available()) {

    float m = Serial.parseFloat();
    float e = Serial.parseFloat();
    float q = Serial.parseFloat();

    if (m > 0 && e > 0 && q > 0) {
      measurementError = m;
      estimationError = e;
      processNoise = q;

      // Ricrea il filtro con i nuovi parametri
      kalman1 = SimpleKalmanFilter(measurementError,
                                  estimationError,
                                  processNoise);

      Serial.print("Nuovi parametri: ");
      Serial.print(measurementError);
      Serial.print(" ");
      Serial.print(estimationError);
      Serial.print(" ");
      Serial.println(processNoise);
    }

    while (Serial.available())
      Serial.read(); // svuota il buffer
  }

  float original = amplitude * sin(2 * PI * frequency * t);
  float noise = random(-300, 301) / 1000.0;
  float noisy = original + noise;

  float filtered = kalman1.updateEstimate(noisy);

  Serial.print(1);
  Serial.print(",");
  Serial.print(-1);
  Serial.print(",");
  Serial.print(original);
  Serial.print(",");
  Serial.print(noisy);
  Serial.print(",");
  Serial.println(filtered);

  t += sampleTime;
  delay(sampleTime * 1000);
}

