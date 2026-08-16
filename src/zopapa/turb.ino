
/* FUNCIONES DE CONTROL DE TURBINA */


// Parámetros turbina
const int MIN_SUCTION = 20;     // 0-255 (255 = 100%) - NO SUPERAR 128!!! (Aprox. 4V en la turbina)
const int MAX_SUCTION = 80;     // 0-255 (255 = 100%) - NO SUPERAR 128!!! (Aprox. 4V en la turbina)
const int RAMP_STEP = 5;       // step de rampa por ciclo (ajustar)
const unsigned long RAMP_DELAY = 20; // ms entre pasos de rampa

int currentSuction = 0;  // valor PWM actual (0-255)
int targetSuction = 0;


// Desactiva la turbina y resetea las variables de control
void pararTurbina() {
  currentSuction = 0;
  targetSuction = 0;
  digitalWrite(TURBINA, LOW);
}

// Actualiza de manera gradual el valor PWM de la turbina, según la corrección calculada por el controlador PID
void actualizarTurbina(int correction) {
  
  // Asigna la velocidad de la turbina a un valor proporcional a la corrección detectada (mayor corrección mayor velocidad turbina)
  targetSuction = map(abs(correction), 0, maxCorrection(), MIN_SUCTION, MAX_SUCTION);

  // Rampa suave hacia targetSuction
  if (currentSuction < targetSuction) {
    currentSuction = min(currentSuction + RAMP_STEP, targetSuction);
  } else if (currentSuction > targetSuction) {
    currentSuction = max(currentSuction - RAMP_STEP, targetSuction);
  }

  analogWrite(TURBINA, currentSuction);

}
