/***** AUTOTUNE (Relay - Åström–Hägglund) *****/
// Ejecuta autotune sobre la corrección de steering.
// Recomendación: antes de ejecutar, coloca el robot sobre guía o con ruedas levantadas.
void autotuneRelay() {

  // Variables para detectar picos (max/min) en la posición (PV)
  const int maxPeaks = RELAY_SETTLE_CYCLES + RELAY_MEASURE_CYCLES + 10;
  double peakAmps[40];
  unsigned long peakTimes[40];
  int peakCount = 0;

  bool relayState = true;  // true -> +d, false -> -d
  int d = relayAmplitude();
  
  Serial.println(F(">>> AUTOTUNE START (relay method)"));
  Serial.print(F("Relay amplitude (d) = "));
  Serial.println(d);

  // Reiniciar tiempos y flags
  unsigned long startTime = millis();
  unsigned long timeoutAt = startTime + AUTOTUNE_TIMEOUT_MS;
  double prevError = 0;
  int lastDerivSign = 0;

  // Clear motors and reset last position
  ultima_posicion = 0;

  // Ejecutar el bucle hasta medir suficientes picos o timeout
  while ((peakCount < (RELAY_SETTLE_CYCLES + RELAY_MEASURE_CYCLES)) && (millis() < timeoutAt)) {
    // leer PV (posición igual que en correr)
    const int centro_de_linea = (NRO_SENSORES - 1) * 100 / 2;
    int posicion = centro_de_linea - Regleta.readLine(sensor_values, COLOR_LINEA);
    double error = (double)posicion;  // PV para la medición

    // aplicar relé (sin PD) sobre la corrección: control diferencial directo
    int corr = relayState ? d : -d;
    // aplico corr como en correr (ajustando velocidades)
    if (corr > 0) {
      int left = SPEED;
      int right = SPEED - corr;
      right = constrain(right, 0, 255);
      motorIzq.speed(left);
      motorDer.speed(right);
    } else {
      int left = SPEED + corr;  // corr es negativo
      int right = SPEED;
      left = constrain(left, 0, 255);
      motorIzq.speed(left);
      motorDer.speed(right);
    }

    // Detección simple de picos: cambio de signo en la derivada (max / min)
    double deriv = error - prevError;
    int derivSign = (deriv > 0.0001) ? 1 : ((deriv < -0.0001) ? -1 : lastDerivSign);

    // Detectar cambio de + -> - => pico máximo (max)
    if (lastDerivSign == 1 && derivSign == -1) {
      Serial.print("Peak! "); Serial.println(error);
      double peakVal = prevError;
      unsigned long peakTime = millis();
      // guardar pico (valor absoluto)
      if (peakCount < 40) {
        peakAmps[peakCount] = fabs(peakVal);
        peakTimes[peakCount] = peakTime;
      }
      peakCount++;

      // alternar relé en cada pico observado (mantiene oscilación)
      relayState = !relayState;
    }
    lastDerivSign = derivSign;
    prevError = error;

    delay(AUTOTUNE_LOOP_MS);
  }  // end while

  // detener motores
  motorIzq.stop();
  motorDer.stop();

  if (peakCount < (RELAY_SETTLE_CYCLES + 2)) {
    Serial.println(F("Autotune failed: not enough peaks detected."));
    beep(1,1000,10);
    return;
  }

  // Calcular amplitude 'a' promedio ignorando los primeros RELAY_SETTLE_CYCLES picos
  int measured = peakCount - RELAY_SETTLE_CYCLES;
  if (measured <= 1) {
    Serial.println(F("Autotune failed: insufficient measured cycles."));
    beep(1,1000,10);
    return;
  }

  double aSum = 0.0;
  int validPeaks = 0;
  for (int i = RELAY_SETTLE_CYCLES; i < min(peakCount, 40); ++i) {
    aSum += peakAmps[i];
    validPeaks++;
  }
  double a = aSum / (double)validPeaks;

  // Calcular Pu promedio (tiempo entre picos sucesivos)
  double PuSum = 0.0;
  int PuCount = 0;
  for (int i = RELAY_SETTLE_CYCLES + 1; i < min(peakCount, 40); ++i) {
    double dt = (peakTimes[i] - peakTimes[i - 1]) / 1000.0;  // s
    PuSum += dt;
    PuCount++;
  }
  double Pu = PuSum / (double)PuCount;

  // Calcular Ku (fórmula del método de relé)
  //const double PI = 3.14159265358979323846;
  double Ku = (4.0 * (double)d) / (PI * a);

  // Reglas Ziegler-Nichols PD (empíricas)
  double suggestedKp = 0.8 * Ku;
  double suggestedKd = Ku * Pu / 8.0;

  Serial.println(F("=== AUTOTUNE RESULTS ==="));
  Serial.print(F("a (PV amp) = "));   Serial.println(a, 4);
  Serial.print(F("Pu (s) = "));       Serial.println(Pu, 4);
  Serial.print(F("Ku = "));           Serial.println(Ku, 4);
  Serial.print(F("Suggested KP = ")); Serial.println(suggestedKp, 4);
  Serial.print(F("Suggested KD = ")); Serial.println(suggestedKd, 4);

  // Instalar ganancias nuevas con factor de seguridad
  KP = suggestedKp * AUTOTUNE_SAFETY_FACTOR;
  KD = suggestedKd * AUTOTUNE_SAFETY_FACTOR;

  Serial.print(F("Installed KP = ")); Serial.println(KP, 4);
  Serial.print(F("Installed KD = ")); Serial.println(KD, 4);
  Serial.println(F(">>> AUTOTUNE END"));

  // un pequeño beep para indicar fin
  beep(2, 100, 8);
  
}  // FIN autotuneRelay()


/***** Gestión de EEPROM para KP y KD *****/
// Guardar los valores actuales de KP y KD en EEPROM y permite leerlos al iniciar el programa.
// Si la EEPROM no fue inicializada carga valores por defecto.

// Direcciones donde se guardan KP y KD
const int EEPROM_ADDR_FLAG = 0;  // Byte para indicar si está inicializada
const int EEPROM_ADDR_KP = 4;    // KP (float ocupa 4 bytes)
const int EEPROM_ADDR_KD = 8;    // KD (float ocupa 4 bytes)
const int EEPROM_ADDR_SPEED = 12;    // KD (float ocupa 4 bytes)

// Funciones para escribir valores float en EEPROM
void eepromWriteFloat(int address, float value) {
  byte *data = (byte *)&value;
  for (int i = 0; i < 4; i++) {
    EEPROM.update(address + i, data[i]);
  }
}

// Funciones para leer valores float en EEPROM
float eepromReadFloat(int address) {
  float value = 0.0;
  byte *data = (byte *)&value;
  for (int i = 0; i < 4; i++) {
    data[i] = EEPROM.read(address + i);
  }
  return value;
}


// Función para verificar si EEPROM ya tiene datos válidos
bool eepromInicializada() {
  return EEPROM.read(EEPROM_ADDR_FLAG) == 1;
}


// Función para guardar KP y KD
void guardarConstantesPID(float kp, float kd, byte speed) {
  eepromWriteFloat(EEPROM_ADDR_KP, kp);
  eepromWriteFloat(EEPROM_ADDR_KD, kd);
  EEPROM.update(EEPROM_ADDR_SPEED, speed);
  EEPROM.update(EEPROM_ADDR_FLAG, 1);  // Marca como inicializada

    Serial.println(F("Constantes PID guardadas en EEPROM:"));
    Serial.print(F("KP = "));     Serial.println(kp);
    Serial.print(F("KD = "));     Serial.println(kd);
    Serial.print(F("SPEED = "));  Serial.println(speed);

}


//Función para leer KP y KD al iniciar
void leerConstantesPID() {
  if (eepromInicializada()) {
    KP = eepromReadFloat(EEPROM_ADDR_KP);
    KD = eepromReadFloat(EEPROM_ADDR_KD);
    SPEED = EEPROM.read(EEPROM_ADDR_SPEED);

    Serial.println(F("Constantes PID cargadas desde EEPROM:"));
    Serial.print(F("KP = "));     Serial.println(KP);
    Serial.print(F("KD = "));     Serial.println(KD);
    Serial.print(F("SPEED = "));  Serial.println(SPEED);

  } else {
      Serial.println(F("EEPROM no inicializada. Usando valores por defecto."));
  }  
}


//Función para confirmar inicio del modo autotune
void confirmarInicioAutoTune() {

  Serial.println(F("DIP_SW4 activado. Presione el pulsador para iniciar AutoTune..."));

  unsigned long tiempoInicio = millis();
  while (millis() - tiempoInicio < 5000) {  // Ventana de 5 segundos para presionar el botón
    if (digitalRead(PULSADOR) == LOW) {     // Botón presionado
      Serial.println(F("Iniciando AutoTune PID..."));
     
      digitalWrite(LED, HIGH);
      beep(5, 1000, 7);
      autotuneRelay();  // Llamada al autotune
      digitalWrite(LED, LOW);
      return;
    }
  }
  
  Serial.println(F("No se presionó el pulsador. Inicio normal."));
}  
