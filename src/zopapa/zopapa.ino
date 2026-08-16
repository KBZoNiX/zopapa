// Sketch con Autotune (Relay) integrado

#include <DRV8833.h>     // Librería para controlar el driver de motores
#include <ATRSensors.h>  // Librería para utilizar la regleta de sensores
#include <EEPROM.h>




/*** CONFIGURACIÓN ***/
const int COLOR_LINEA = 1;  // Color de la linea a seguir (1: blanca / 0: negra):
const int EEPROM_AUTOLOAD = 0;   // Carga valores de EEPROM al arrancar
const int MAX_CICLOS_SIN_DETECTAR = 100; // Si no detecta linea para el robot



// constantes PID (ahora variables para permitir actualización por autotune):
// PROBADOS  SPEED    KP    KD
// P.CORTA   150      1.15  5
// P.CORTA   160      1.33  5.5
// P.CORTA   180      1.75  7.5
// P.CORTA   200      2     10
// P.CORTA   220      2.4   12
// P.CORTA   255      2.8   14

// byte SPEED = 130;       // 0..255
// float KP = 2.5 ;         // gains iniciales (serán sobrescritos por autotune si se ejecuta)
// float KD = 14 ;
byte SPEED = 100;       // 0..255
float KP = 2 ;         // gains iniciales (serán sobrescritos por autotune si se ejecuta)
float KD = 10 ;


// Limita la corrección máxima. Función (no const) porque SPEED puede cambiar
// en tiempo de ejecución (comando 'V', carga de EEPROM, autotune guardado).
int maxCorrection() {
  return SPEED * 2;
}


// Pines del Arduino utilizados
const int PULSADOR = 2;
const int TURBINA = 5;
const int LED = 12;
const int BUZZER = 13;
const int DIP_SW1 = 8;  // Incrementar velocidad
const int DIP_SW2 = 7;  // Incrementar KP y KD
const int DIP_SW3 = 6;  // Habilita turbina
const int DIP_SW4 = 4;  // Autotune

// Configuración de motores:
DRV8833 motorIzq(11, 3);  // pines del Arduino que utiliza el motor izquierdo
DRV8833 motorDer(9, 10);  // pines del Arduino que utiliza el motor derecho

// Configuración de la regleta de sensores:
const unsigned char SENSORES[] = { A0, A1, A2, A3, A4, A5, A6, A7 };  // Sensores de linea {pines}
const int NRO_SENSORES = sizeof(SENSORES);                            // Número de sensores de linea
ATRSensors Regleta(SENSORES, NRO_SENSORES);


/*** GLOBAL VARIABLES ***/
unsigned char sensor_values[NRO_SENSORES];
byte estado = 0;                            // 0 = Detenido; 1 = Normal; 2 = Turbina
int ultima_posicion = 0;
unsigned long lastLoopTime = 0;             // Guarda último valor de millis() para controlar el tiempo del loop
// Bluetooth
String btCommand = "";
bool btEnabled = true;
int ciclos_sin_detectar = 0;


/*** Autotune parameters ***/
// Amplitud de relé (magnitud de la corrección aplicada durante la prueba).
// Función (no const): debe reflejar el SPEED vigente al momento de ejecutar
// el autotune, no el valor por defecto con el que arrancó el sketch.
int relayAmplitude() {
  return SPEED * 0.5;
}
const int RELAY_SETTLE_CYCLES = 3;                // ciclos descartados (de estabilización)
const int RELAY_MEASURE_CYCLES = 6;               // ciclos usados para cálculo
const float AUTOTUNE_SAFETY_FACTOR = 0.9;         // Factor de seguridad para las constantes calculadas (Factor < 1 → más conservador / Factor = 1 → más agresivo)
const unsigned long AUTOTUNE_TIMEOUT_MS = 2000;   // seguridad: 25 s timeout
const unsigned long LOOP_MS = 2;                 // periodo de ejecución de la función correr() (ms)
const unsigned long AUTOTUNE_LOOP_MS = LOOP_MS;   // periodo de autotune (ms)



/**** SETUP ***/
void setup() {
  Serial.begin(9600);
  Serial.println(F("Iniciando..."));

  pinMode(PULSADOR, INPUT_PULLUP);
  pinMode(DIP_SW1, INPUT_PULLUP);
  pinMode(DIP_SW2, INPUT_PULLUP);
  pinMode(DIP_SW3, INPUT_PULLUP);
  pinMode(DIP_SW4, INPUT_PULLUP);
  pinMode(TURBINA, OUTPUT);
  pinMode(LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
    
  pararTurbina();
  motorIzq.begin();
  motorDer.begin();

  // calibrar sensores: mover regleta sobre la linea.
  Serial.println(F("Calibrar regleta..."));
  digitalWrite(LED, HIGH);
  beep(2, 100, 7);
  digitalWrite(LED, LOW);

  Serial.println(F("Calibrar regleta..."));
  for (int i = 0; i < 30; i++) {  // 3 segundos de calibración
    beep(1, 30, 7);
    long int ms = millis();
    while (millis() < ms + 100) {
      Regleta.calibrate();
    }
  }
  Serial.println(F("Calibrar regleta..."));
  
  // Carga los valores guardados en EEPROM
  if (EEPROM_AUTOLOAD) {
    leerConstantesPID(); 
  }
  
  // Entra en modo autotune
  if (digitalRead(DIP_SW4) == LOW) {
    confirmarInicioAutoTune();
  }

  indicar_estado(estado);

}  // FIN SETUP()


/**** LOOP ***/
void loop() {

  if(digitalRead(PULSADOR) == LOW) {
    unsigned long t0 = millis();
    // espera y detecta si se mantiene pulsado
    while (digitalRead(PULSADOR) == LOW) {
      // Modo autotune, toque largo en PULSADOR (>= 2000 ms) guarda los valores nuevos:
      if (digitalRead(DIP_SW4) == LOW && millis() - t0 > 2000) {
        guardarConstantesPID(KP, KD, SPEED);
        estado = 1;
        digitalWrite(LED, HIGH);
        beep(2,500,8);
        digitalWrite(LED, LOW);
        delay(500);
      }
    }
    if (estado != 0) {
      estado = 0;
    } else if (digitalRead(DIP_SW3) == 0) {
      estado = 2;
    } else {
      estado = 1;
    }
    indicar_estado(estado);
  }

  if (estado != 0) {
    if (millis() - lastLoopTime > LOOP_MS) {
      correr();
      lastLoopTime = millis();
    }
    btListenStopOnly();
  } else {
    pararTurbina();
    motorIzq.stop();
    motorDer.stop();
    btListen();
  }
}  // FIN LOOP()


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


/**** INDICAR ESTADO ***/
void indicar_estado(byte status) {

  if (status != 0) {
    digitalWrite(LED, HIGH);
    beep(1, 100, 10);
  } else {
    digitalWrite(LED, HIGH);
    beep(3, 100, 7);
    digitalWrite(LED, LOW);
  }

  Serial.println(F("READY!"));
}  // FIN indicar_estado()
