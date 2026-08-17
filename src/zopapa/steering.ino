/**** CORRER ***/
// lee la posición sobre la regleta, calcula la corrección y la aplica a los motores
void correr(void) {

  // leo la posición:
  const int centro_de_linea = (NRO_SENSORES - 1) * 100 / 2;
  int posicion = centro_de_linea - Regleta.readLine(sensor_values, COLOR_LINEA);

  // verifica si el robot se salió de la línea y lo detiene
  if (abs(posicion) == centro_de_linea) {
    ciclos_sin_detectar += 1;
    if (ciclos_sin_detectar >= MAX_CICLOS_SIN_DETECTAR) {
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

  logTelemetry(posicion, correccion_pid);  // Phase 0 — ver PLAN-speed-scheduling-v3_1.md

  if (estado == 2) {
    actualizarTurbina(correccion_pid); // Actualiza velocidad turbina
  }

  // Aplico la corrección:
  if (correccion_pid > 0) {
    correccion_pid = min(correccion_pid, maxCorrection());
    motorIzq.speed(SPEED);
    motorDer.speed(SPEED - correccion_pid);
  } else {
    correccion_pid = max(correccion_pid, -maxCorrection());
    motorIzq.speed(SPEED + correccion_pid);
    motorDer.speed(SPEED);
  }

}  // FIN CORRER()


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
