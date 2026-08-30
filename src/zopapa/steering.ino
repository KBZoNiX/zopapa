/**** CORRER ***/
// lee la posición sobre la regleta, calcula la corrección y la aplica a los motores
void correr(void) {

  // Arranque suave (Tier 2, T-S8): rampa la velocidad base de 0 a SPEED
  // durante SOFT_START_MS tras iniciar la corrida, para reducir el patinado
  // en superficie nueva. No toca la ley de control (KP/KD/maxCorrection()
  // siguen referidos a SPEED) — sólo la referencia de avance que usa
  // aplicarCorreccion(). Deshabilitado por defecto — ver zopapa.ino.
  int velocidadBase = SPEED;
  if (SOFT_START_ENABLED) {
    unsigned long transcurrido = millis() - softStartBeginMs;
    if (transcurrido < SOFT_START_MS) {
      velocidadBase = (int)((long)SPEED * transcurrido / SOFT_START_MS);
    }
  }

  // leo la posición:
  const int centro_de_linea = (NRO_SENSORES - 1) * 100 / 2;
  int posicion = centro_de_linea - Regleta.readLine(sensor_values, COLOR_LINEA);
  bool lineaPerdida = (abs(posicion) == centro_de_linea);

  // verifica si el robot se salió de la línea y lo detiene
  if (lineaPerdida) {
    ciclos_sin_detectar += 1;

    if (ciclos_sin_detectar >= MAX_CICLOS_SIN_DETECTAR) {
      // Recuperación acotada de pérdida de línea (Tier 2, T-S7): en vez de
      // detenerse de inmediato, sigue girando a fondo hacia el último lado
      // conocido de la línea por un número limitado de ciclos adicionales.
      // Deshabilitada por defecto — confirmar el reglamento del torneo y
      // validar en pista antes de activarla (ver zopapa.ino).
      if (LINE_LOSS_RECOVERY_ENABLED &&
          ciclos_sin_detectar < (MAX_CICLOS_SIN_DETECTAR + LINE_LOSS_RECOVERY_CYCLES)) {
        int corr = (ultima_posicion >= 0) ? maxCorrection() : -maxCorrection();
        aplicarCorreccion(corr, velocidadBase);
        runSummaryUpdate(posicion, true);   // Tier 1, T-S1
        logTelemetry(posicion, corr);       // Phase 0 — ver PLAN-speed-scheduling-v3_1.md
        return;
      }

      estado = 0;
      indicar_estado(estado);
      Serial.println(F("Fuera de Linea. Detenido"));
      return;
    }
  }
  else {
      ciclos_sin_detectar = 0;
  }

  // calculo la corrección:
  int correccion_pid = (KP * posicion) + (KD * (posicion - ultima_posicion));
    ultima_posicion = posicion;

  runSummaryUpdate(posicion, lineaPerdida);      // Tier 1, T-S1
  logTelemetry(posicion, correccion_pid);        // Phase 0 — ver PLAN-speed-scheduling-v3_1.md

  if (estado == 2) {
    actualizarTurbina(correccion_pid); // Actualiza velocidad turbina
  }

  aplicarCorreccion(correccion_pid, velocidadBase);

}  // FIN CORRER()


// Aplica la corrección de steering a los motores. Recibe velocidadBase en
// vez de leer SPEED directamente para que el arranque suave (T-S8) y la
// recuperación de pérdida de línea (T-S7) puedan pasar una referencia de
// avance distinta sin duplicar esta lógica.
void aplicarCorreccion(int correccion_pid, int velocidadBase) {
  if (correccion_pid > 0) {
    correccion_pid = min(correccion_pid, maxCorrection());
    motorIzq.speed(velocidadBase);
    motorDer.speed(velocidadBase - correccion_pid);
  } else {
    correccion_pid = max(correccion_pid, -maxCorrection());
    motorIzq.speed(velocidadBase + correccion_pid);
    motorDer.speed(velocidadBase);
  }
}


/**** CONDUCIR RECTO (lazo abierto) ***/
// Aplica SPEED a ambas ruedas sin leer la regleta ni corregir. Sólo para el
// bench-test T1.1 (medición de Kv/S0): permite medir tiempo/distancia en
// línea recta sin que la corrección de steering enmascare el mapeo
// PWM->velocidad. Entra por el comando 'O' (bt.ino); sale con 'P' como
// cualquier otro estado.
void conducirRecto(void) {
  motorIzq.speed(SPEED);
  motorDer.speed(SPEED);
}  // FIN CONDUCIRRECTO()
