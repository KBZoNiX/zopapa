/***** TELEMETRÍA (Phase 0 — PLAN-speed-scheduling-v3_1.md) *****/
// Registro en RAM de (dt, posición, corrección) por ciclo de control, con
// volcado por Serial/BT para graficar fuera de línea (tools/). El buffer y
// sus contadores están declarados en zopapa.ino (primer archivo del sketch)
// porque steering.ino, autotune.ino y bt.ino necesitan verlos.

// Registra una muestra. Se llama una vez por ciclo desde correr() y desde el
// bucle de autotuneRelay(). dt se mide con micros() en cada llamada, sin
// decimar, para que T0.3 pueda reportar min/max/mean del período real del
// loop aunque telemetryDecimation > 1 descarte la mayoría de las muestras
// del buffer circular.
void logTelemetry(int posicion, int corr) {
  unsigned long now = micros();

  if (telemetryLastSampleMicros != 0) {
    unsigned long dt = now - telemetryLastSampleMicros;

    if (dt < telemetryDtMin) telemetryDtMin = dt;
    if (dt > telemetryDtMax) telemetryDtMax = dt;
    telemetryDtSum += dt;
    telemetryDtCount++;

    telemetryDecimationCounter++;
    if (telemetryDecimationCounter >= telemetryDecimation) {
      telemetryDecimationCounter = 0;

      telemetryBuffer[telemetryHead].dt = (int16_t)min(dt, (unsigned long)32767);
      telemetryBuffer[telemetryHead].posicion = (int16_t)posicion;
      telemetryBuffer[telemetryHead].corr = (int16_t)corr;
      telemetryHead = (telemetryHead + 1) % TELEMETRY_BUFFER_SIZE;
      if (telemetryCount < TELEMETRY_BUFFER_SIZE) telemetryCount++;
    }
  }

  telemetryLastSampleMicros = now;
}

// Vuelca el buffer como CSV (comando 'Z') y lo reinicia para la próxima
// captura — cada 'Z' entrega una ventana fresca, no acumulada entre corridas.
void telemetryDumpCSV() {
  Serial.println(F("--- TELEMETRY DUMP ---"));
  Serial.print(F("KP=")); Serial.print(KP, 4);
  Serial.print(F(" KD=")); Serial.print(KD, 4);
  Serial.print(F(" SPEED=")); Serial.println(SPEED);

  Serial.print(F("loop_dt_us min="));
  Serial.print(telemetryDtCount > 0 ? telemetryDtMin : 0);
  Serial.print(F(" max="));
  Serial.print(telemetryDtMax);
  Serial.print(F(" mean="));
  Serial.println(telemetryDtCount > 0 ? (telemetryDtSum / (float)telemetryDtCount) : 0);

  Serial.print(F("samples=")); Serial.print(telemetryCount);
  Serial.print(F(" decimation=")); Serial.println(telemetryDecimation);

  Serial.println(F("dt_us,posicion,corr"));

  int start = (telemetryCount < TELEMETRY_BUFFER_SIZE) ? 0 : telemetryHead;
  for (int i = 0; i < telemetryCount; i++) {
    int idx = (start + i) % TELEMETRY_BUFFER_SIZE;
    Serial.print(telemetryBuffer[idx].dt);
    Serial.print(',');
    Serial.print(telemetryBuffer[idx].posicion);
    Serial.print(',');
    Serial.println(telemetryBuffer[idx].corr);
  }

  Serial.println(F("--- END ---"));

  telemetryReset();
}

// Limpia el buffer y las estadísticas de dt. Se llama automáticamente al
// final de cada volcado 'Z'.
void telemetryReset() {
  telemetryHead = 0;
  telemetryCount = 0;
  telemetryDecimationCounter = 0;
  telemetryLastSampleMicros = 0;
  telemetryDtMin = 0xFFFFFFFF;
  telemetryDtMax = 0;
  telemetryDtSum = 0;
  telemetryDtCount = 0;
}
