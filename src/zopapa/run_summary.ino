/***** RESUMEN DE CORRIDA (Tier 1, T-S1 — PLAN-tournament-stopgap-v1.md) *****/
// Estadísticas acumuladas ciclo a ciclo mientras el robot corre (estado 1 o
// 2), impresas por Serial/BT al detenerse. No requiere volcar el buffer de
// telemetría ni correr tools/ — da un número comparable entre intentos de
// ajuste al pie de la pista. Los acumuladores están declarados en zopapa.ino;
// el arranque/impresión se dispara desde el detector de borde en loop().
// También sirve como dato crudo de Phase 1 del plan principal: anotar a mano
// peak/mean/saturación por velocidad probada esta semana es la mitad del
// trabajo de T1.5.

// Reinicia los acumuladores. Llamado al iniciar una corrida (transición
// Detenido -> Corriendo/Turbina).
void runSummaryReset() {
  runSumAbsPos = 0;
  runSampleCount = 0;
  runPeakAbsPos = 0;
  runSaturationCycles = 0;
  runLineLossEvents = 0;
  runLineLossCycles = 0;
  runDtMinUs = 0xFFFFFFFF;
  runDtMaxUs = 0;
  runDtSumUs = 0;
  runDtCount = 0;
}

// Llamada una vez por ciclo desde correr() (incluido el ciclo de
// recuperación de T-S7). lineaPerdida usa el mismo criterio que
// MAX_CICLOS_SIN_DETECTAR; se asume que correr() ya incrementó
// ciclos_sin_detectar antes de llamar acá, así que == 1 marca el primer
// ciclo de un evento de pérdida (para contar eventos, no sólo ciclos).
void runSummaryUpdate(int posicion, bool lineaPerdida) {
  static unsigned long ultimaMuestraMicros = 0;
  unsigned long ahora = micros();

  int absPos = abs(posicion);
  runSumAbsPos += absPos;
  runSampleCount++;
  if (absPos > runPeakAbsPos) runPeakAbsPos = absPos;
  if (absPos >= RELAY_EDGE_ABORT) runSaturationCycles++;

  if (lineaPerdida) {
    if (ciclos_sin_detectar == 1) runLineLossEvents++;
    runLineLossCycles++;
  }

  if (ultimaMuestraMicros != 0) {
    unsigned long dt = ahora - ultimaMuestraMicros;
    if (dt < runDtMinUs) runDtMinUs = dt;
    if (dt > runDtMaxUs) runDtMaxUs = dt;
    runDtSumUs += dt;
    runDtCount++;
  }
  ultimaMuestraMicros = ahora;
}

// Imprime el resumen acumulado. Llamado al detenerse (transición
// Corriendo/Turbina -> Detenido). No hace nada si la corrida no acumuló
// ninguna muestra (p.ej. el gesto de guardar/recalibrar por toque largo, que
// fuerza estado=1 sin haber llamado nunca a correr()).
void runSummaryPrint() {
  if (runSampleCount == 0) return;

  Serial.println(F("--- RESUMEN DE CORRIDA ---"));
  Serial.print(F("muestras=")); Serial.println(runSampleCount);
  Serial.print(F("peak |posicion| = ")); Serial.println(runPeakAbsPos);
  Serial.print(F("mean |posicion| = ")); Serial.println(runSumAbsPos / (float)runSampleCount, 2);
  Serial.print(F("ciclos en saturacion (>=")); Serial.print(RELAY_EDGE_ABORT); Serial.print(F(") = ")); Serial.println(runSaturationCycles);
  Serial.print(F("perdida de linea: eventos=")); Serial.print(runLineLossEvents);
  Serial.print(F(" ciclos=")); Serial.println(runLineLossCycles);
  Serial.print(F("loop dt (us) min=")); Serial.print(runDtCount > 0 ? runDtMinUs : 0);
  Serial.print(F(" max=")); Serial.print(runDtMaxUs);
  Serial.print(F(" mean=")); Serial.println(runDtCount > 0 ? (runDtSumUs / (float)runDtCount) : 0);
  Serial.print(F("KP=")); Serial.print(KP, 4);
  Serial.print(F(" KD=")); Serial.print(KD, 4);
  Serial.print(F(" SPEED=")); Serial.println(SPEED);
  Serial.print(F("perfil activo = "));
  if (activePreset >= 0) {
    Serial.println(activePreset + 1);
  } else {
    Serial.println(F("ninguno (manual)"));
  }
  Serial.println(F("--- FIN RESUMEN ---"));
}
