/***** CHEQUEO PREVIO (Tier 1, T-S3 — PLAN-tournament-stopgap-v1.md) *****/
// Un solo comando ('F', bt.ino) que imprime todo lo que conviene confirmar
// en la línea de largada antes de una carrera, para no competir con una
// configuración que no era la buscada. No mide batería: con los 8 canales
// QTR ocupando A0-A7 no queda una entrada analógica libre (ver plan).

void preflightCheck() {
  Serial.println(F("--- PREFLIGHT ---"));

  Serial.print(F("Calibracion regleta: "));
  if (Regleta.calibratedMinimum != 0 && Regleta.calibratedMaximum != 0) {
    Serial.println(F("cargada"));
    imprimirCalidadCalibracion();
  } else {
    Serial.println(F("SIN CALIBRAR"));
  }

  Serial.print(F("EEPROM ranura manual (L/W): "));
  Serial.println(eepromInicializada() ? F("OK, con datos guardados") : F("sin guardar (valores por defecto)"));

  Serial.print(F("Perfil activo: "));
  if (activePreset >= 0) {
    Serial.println(activePreset + 1);
  } else {
    Serial.println(F("ninguno (valores manuales)"));
  }

  Serial.print(F("KP=")); Serial.print(KP, 4);
  Serial.print(F(" KD=")); Serial.print(KD, 4);
  Serial.print(F(" SPEED=")); Serial.println(SPEED);

  Serial.println(F("Gain scheduling: no implementado en este build (tournament-stopgap)."));
  Serial.print(F("Arranque suave (T-S8): ")); Serial.println(SOFT_START_ENABLED ? F("activo") : F("desactivado"));
  Serial.print(F("Recuperacion perdida de linea (T-S7): ")); Serial.println(LINE_LOSS_RECOVERY_ENABLED ? F("activa") : F("desactivada"));
  Serial.print(F("Timeout autotune = ")); Serial.print(autotuneTimeoutMs); Serial.println(F(" ms"));

  medirDtIdle();

  Serial.println(F("--- FIN PREFLIGHT ---"));
}

// Toma MUESTRAS lecturas de la regleta en reposo (sin mover motores) para
// estimar el período real del loop de control bajo la misma carga de trabajo
// que correr() — la lectura de sensores es lo que más pesa dentro de los 2ms
// del ciclo. Reutiliza sensor_values (global) para no gastar más pila.
void medirDtIdle() {
  const int MUESTRAS = 60;
  unsigned long dtMin = 0xFFFFFFFF;
  unsigned long dtMax = 0;
  unsigned long dtSum = 0;
  unsigned long ultima = 0;

  for (int i = 0; i < MUESTRAS; i++) {
    Regleta.readLine(sensor_values, COLOR_LINEA);
    unsigned long ahora = micros();
    if (ultima != 0) {
      unsigned long dt = ahora - ultima;
      if (dt < dtMin) dtMin = dt;
      if (dt > dtMax) dtMax = dt;
      dtSum += dt;
    }
    ultima = ahora;
  }

  Serial.print(F("loop dt idle (us) sobre ")); Serial.print(MUESTRAS - 1); Serial.print(F(" muestras: min="));
  Serial.print(dtMin); Serial.print(F(" max=")); Serial.print(dtMax);
  Serial.print(F(" mean=")); Serial.println(dtSum / (float)(MUESTRAS - 1));
}
