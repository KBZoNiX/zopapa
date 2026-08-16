/*
 * DRV8833.h - Libreria para utilizar el driver de motores DRV8833.
 * 
 * Programa de ejemplo
 */

#include <DRV8833.h>

DRV8833 motor(5,6);        // pines del Arduino que utiliza el motor

void setup() {
  motor.begin();           // completa la configuración del motor

}

void loop() {
  motor.mode(FAST_DECAY);    // modo FAST_DECAY

  motor.speed(255);          // Velocidad máxima
  delay (1000);

  motor.speed(127);          // Velocidad media
  delay(1000);

  motor.free();              // freno suave
  delay(1000);

  motor.mode(SLOW_DECAY);     // modo SLOW_DECAY (es el modo por defecto)

  motor.speed(-255);          // Velocidad máxima, sentido inverso
  delay (1000);

  motor.speed(127);          // Velocidad media, sentido inverso
  delay(1000);

  motor.stop();              // freno rápido
  delay(1000);
  
}
