/*
 * DRV8833.h - Libreria para utilizar el driver de motores DRV8833.
 * 
 * Programa de ejemplo utilizando 2 motores
 */


#include <DRV8833.h>

DRV8833 motorA(5,6);        // pines del Arduino que utiliza el motor A
DRV8833 motorB(9,10);       // pines del Arduino que utiliza el motor B

void setup() {
  motorA.begin();
  motorB.begin();
}

void loop() {
  motorA.speed(255);          // Velocidad máxima
  motorB.speed(255);
  delay (1000);

  motorA.free();              // freno suave
  motorA.free();
  delay(1000);
  
  motorA.speed(127);          // Velocidad media
  motorA.speed(-127);         // Invierto el sentido de giro
  delay(1000);

  motorA.stop();              // freno rápido
  motorA.stop();
  delay(1000);
  
}
