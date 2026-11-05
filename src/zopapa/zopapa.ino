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

// Tier 0 (PLAN-tournament-stopgap-v1.md): no existe gain scheduling en este
// build — no hay `gainSchedulingActive` que forzar a false ni código que
// compilar afuera. KP/KD sólo cambian por 'K'/'D', autotune, o carga de
// EEPROM/perfil; 'V' nunca los toca. Confirmar con 'R' después de 'V' si hace
// falta re-verificarlo en hardware.

// Perfiles de ganancia (Tier 1, T-S5/T-S6 — ver presets.ino). -1 = ninguno
// activo (valores manuales, no coinciden necesariamente con un perfil
// guardado). Se invalida en cualquier 'K'/'D'/'V' manual.
int8_t activePreset = -1;

// Declarados acá (no en presets.ino) porque bt.ino los necesita y sólo este
// archivo tiene garantizado compilarse antes que todos los demás — mismo
// motivo que el buffer de telemetría más abajo.
const byte PRESET_COUNT = 3;
const int PRESET_STRIDE = 9;  // KP (float, 4B) + KD (float, 4B) + SPEED (byte, 1B)

// Región propia, separada de la ranura manual KP/KD/SPEED de eeprom.ino
// (direcciones 0/4/8/12) y de la calibración (1-2, 16-31) — ver docs/FSD.html
// §10. Marcador de 2 bytes: mismo motivo que el de calibración (evitar que un
// byte de fábrica/sketch anterior se lea como "válido" por casualidad).
const int EEPROM_ADDR_PRESET_FLAG = 40;  // ocupa 2 bytes: 40 y 41
const byte PRESET_MAGIC_0 = 0x9E;
const byte PRESET_MAGIC_1 = 0x47;
const int EEPROM_ADDR_PRESET_BASE = 42;  // 42..(42+3*9-1) = 42..68

// Perfiles por defecto (T-S6): filas de la tabla §11.1, extremos y punto
// medio. "Conservador" para clasificar, "Agresivo" para la final — pensados
// como punto de partida a ajustar a mano, no como objetivo.
const byte PRESET_DEFAULT_SPEED[PRESET_COUNT] = { 150, 200, 255 };
const float PRESET_DEFAULT_KP[PRESET_COUNT]   = { 1.15, 2.0, 2.8 };
const float PRESET_DEFAULT_KD[PRESET_COUNT]   = { 5.0, 10.0, 14.0 };

// Arranque suave (Tier 2, T-S8). Deshabilitado por defecto: reduce el
// patinado de arranque pero no fue validado en pista todavía — habilitar y
// probar antes de usar en torneo.
const bool SOFT_START_ENABLED = false;
const unsigned long SOFT_START_MS = 150;
unsigned long softStartBeginMs = 0;

// Recuperación acotada de pérdida de línea (Tier 2, T-S7). Deshabilitada por
// defecto: revisar el reglamento del torneo antes de activarla — ver
// docs/Reglamento-Carreras.md §8.2/§10 (salida completa de pista / reinicio
// sin reconfiguración) — y validarla en pista sacando el robot de la línea a
// mano.
const bool LINE_LOSS_RECOVERY_ENABLED = false;
const int LINE_LOSS_RECOVERY_CYCLES = 50;  // ciclos adicionales tras MAX_CICLOS_SIN_DETECTAR (~100ms a LOOP_MS=2)


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
const int DIP_SW1 = 8;  // Fuerza carga del perfil 1 al arrancar
const int DIP_SW2 = 7;  // Fuerza carga del perfil 2 al arrancar
const int DIP_SW3 = 6;  // Fuerza carga del perfil 3 al arrancar
const int DIP_SW4 = 4;  // Habilita turbina (arranque por pulsador o por BT 'S')

// Configuración de motores:
DRV8833 motorIzq(11, 3);  // pines del Arduino que utiliza el motor izquierdo
DRV8833 motorDer(9, 10);  // pines del Arduino que utiliza el motor derecho

// Configuración de la regleta de sensores:
const unsigned char SENSORES[] = { A0, A1, A2, A3, A4, A5, A6, A7 };  // Sensores de linea {pines}
const int NRO_SENSORES = sizeof(SENSORES);                            // Número de sensores de linea
ATRSensors Regleta(SENSORES, NRO_SENSORES);


/*** GLOBAL VARIABLES ***/
unsigned char sensor_values[NRO_SENSORES];
byte estado = 0;                            // 0 = Detenido; 1 = Normal; 2 = Turbina; 3 = Recto (lazo abierto, bench-test T1.1)
int ultima_posicion = 0;
unsigned long lastLoopTime = 0;             // Guarda último valor de millis() para controlar el tiempo del loop
// Bluetooth
String btCommand = "";
bool btEnabled = true;
int ciclos_sin_detectar = 0;


/*** Telemetry (Phase 0 — PLAN-speed-scheduling-v3_1.md §0.1) ***/
// Buffer circular en RAM: (dt en µs desde la muestra anterior, posición,
// corrección) por ciclo de control. Declarado en este archivo (el primero
// del sketch al compilar) porque steering.ino, autotune.ino y bt.ino lo
// referencian y sólo zopapa.ino tiene garantizado compilarse antes que todos
// ellos. Las funciones que lo usan (logTelemetry(), telemetryDumpCSV()) están
// en telemetry.ino.
struct TelemetrySample {
  int16_t dt;         // µs, saturado a 32767 si el ciclo tardó más (no debería)
  int16_t posicion;
  int16_t corr;
};
const int TELEMETRY_BUFFER_SIZE = 150;      // ~900 bytes; ver §0.1 del plan para el presupuesto de SRAM
TelemetrySample telemetryBuffer[TELEMETRY_BUFFER_SIZE];
int telemetryHead = 0;                      // próximo índice de escritura
int telemetryCount = 0;                     // muestras válidas (satura en TELEMETRY_BUFFER_SIZE)
byte telemetryDecimation = 1;               // registra 1 de cada N ciclos (comando 'N'); no afecta las estadísticas de dt
byte telemetryDecimationCounter = 0;
unsigned long telemetryLastSampleMicros = 0;
unsigned long telemetryDtMin = 0xFFFFFFFF;
unsigned long telemetryDtMax = 0;
unsigned long telemetryDtSum = 0;
unsigned long telemetryDtCount = 0;


/*** Resumen de corrida (Tier 1, T-S1 — PLAN-tournament-stopgap-v1.md) ***/
// Estadísticas acumuladas durante una corrida (estado 1 o 2), impresas al
// detenerse — ver run_summary.ino. Sustituyen a la telemetría completa como
// número comparable trackside cuando no hay forma de correr tools/.
long runSumAbsPos = 0;
unsigned long runSampleCount = 0;
int runPeakAbsPos = 0;
unsigned long runSaturationCycles = 0;   // |posicion| >= RELAY_EDGE_ABORT
unsigned long runLineLossEvents = 0;
unsigned long runLineLossCycles = 0;
unsigned long runDtMinUs = 0xFFFFFFFF;
unsigned long runDtMaxUs = 0;
unsigned long runDtSumUs = 0;
unsigned long runDtCount = 0;


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
// No-const (a diferencia del resto de estos parámetros): el valor original
// (2000) contradecía su propio comentario ("25 s"), y T1.2 puede necesitar
// más tiempo a velocidades con Pu lento — ver Phase -1 / T-1.2 y §1.1 del
// plan. Ajustable en caliente con el comando 'M'.
// Subido a 8s para el torneo (T-S9 del stopgap): un timeout de 2s corta la
// prueba antes de tiempo con más frecuencia de la deseada, pero saltar
// directo a los 25s del comentario original multiplicaría por ~12 el tiempo
// que el robot puede quedar oscilando en la pista. 8s es intermedio y se
// apoya en el edge-abort de RELAY_EDGE_ABORT (abajo) como red de seguridad:
// si ese abort no está confirmado funcionando, bajar esto de nuevo con 'M' y
// ajustar a mano en su lugar.
unsigned long autotuneTimeoutMs = 8000;           // ms; comando 'M' lo cambia
// Autotune corre siempre a esta velocidad fija (Tier 0 del stopgap, también
// listado como "branch-independent" en el plan principal): antes probaba a
// la SPEED vigente al momento de invocar 'T', lo que hacía que la amplitud de
// excitación (0.5*SPEED) y por lo tanto las ganancias resultantes dependieran
// de qué SPEED hubiera quedado seteada de antes. Con SPEED_REF fijo, 'T' es
// seguro de invocar en cualquier momento y las corridas quedan comparables
// entre sí — ver autotuneRelay() en autotune.ino.
const byte AUTOTUNE_SPEED_REF = 100;
const unsigned long LOOP_MS = 2;                 // periodo de ejecución de la función correr() (ms)
const unsigned long AUTOTUNE_LOOP_MS = LOOP_MS;   // periodo de autotune (ms)
int relayAmplitudeOverride = 0;                   // 0 = usar relayAmplitude() proporcional; >0 = amplitud fija (comando 'A' — ver T1.2 del plan)
const int RELAY_EDGE_ABORT = 340;                 // aborta el relay test si |posicion| se acerca al límite de la regleta (±350) — T1.0



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

  // Calibración de la regleta: usa la guardada en EEPROM si existe: si no,
  // requiere el barrido manual sobre la línea (y la guarda para la próxima).
  if (cargarCalibracion()) {
    Serial.println(F("Calibración cargada desde EEPROM."));
    beep(2, 80, 7);
    imprimirCalidadCalibracion();  // Tier 1, T-S2
  } else {
    calibrarRegletaManual();
  }

  // Carga los valores guardados en EEPROM
  if (EEPROM_AUTOLOAD) {
    leerConstantesPID();
  }

  // Perfiles de ganancia (Tier 1, T-S5/T-S6): si la EEPROM no tiene la
  // sección de perfiles (primer flasheo de este layout), la siembra con los
  // valores de la tabla §11.1 de la FSD.
  seedPresetsIfNeeded();

  // DIP 1/2/3 activada (LOW) fuerza la carga del perfil correspondiente. Sin
  // ninguna activada quedan los valores por defecto y se controla por BT. Con
  // más de una no se carga ninguno: elegir en silencio sería peligroso en
  // pista, así que se avisa y quedan los valores por defecto.
  cargarPerfilPorDip();

  indicar_estado(estado);

}  // FIN SETUP()


/**** CALIBRAR REGLETA (barrido manual) ***/
// Barrido manual de 3 segundos: mover la regleta sobre la línea. Guarda el
// resultado en EEPROM al terminar, para no repetirlo en el próximo arranque.
void calibrarRegletaManual() {
  // Descarta cualquier calibración ya cargada (EEPROM o un barrido previo en
  // esta misma sesión). ATRSensors::calibrate() sólo reinicia los límites a
  // sus valores centinela (255/0) cuando calibratedMinimum/Maximum están en
  // NULL; si no, sólo los ensancha. Sin este reseteo, un barrido "nuevo"
  // termina siendo una unión con el anterior en vez de reemplazarlo.
  if (Regleta.calibratedMinimum != 0) {
    free(Regleta.calibratedMinimum);
    Regleta.calibratedMinimum = 0;
  }
  if (Regleta.calibratedMaximum != 0) {
    free(Regleta.calibratedMaximum);
    Regleta.calibratedMaximum = 0;
  }

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

  guardarCalibracion();
  imprimirCalidadCalibracion();  // Tier 1, T-S2
}  // FIN calibrarRegletaManual()


/**** LOOP ***/
void loop() {

  // Tier 1 (T-S1) / Tier 2 (T-S8): detecta el borde Detenido<->Corriendo una
  // sola vez por vuelta de loop(), sin importar qué código (pulsador, comando
  // BT, o el auto-stop por pérdida de línea en correr()) cambió `estado` —
  // todos pasan por acá antes de que se note la próxima vez. Evita instrumentar
  // cada uno de esos puntos por separado.
  static byte estadoPrevio = 0;
  if (estado != estadoPrevio) {
    bool estabaCorriendo = (estadoPrevio == 1 || estadoPrevio == 2);
    bool estaCorriendo = (estado == 1 || estado == 2);
    if (!estabaCorriendo && estaCorriendo) {
      runSummaryReset();
      softStartBeginMs = millis();
    }
    if (estabaCorriendo && !estaCorriendo) {
      runSummaryPrint();
    }
    estadoPrevio = estado;
  }

  if(digitalRead(PULSADOR) == LOW) {
    unsigned long t0 = millis();
    // espera y detecta si se mantiene pulsado
    while (digitalRead(PULSADOR) == LOW) {
      // Toque largo (>= 2000 ms): recalibra la regleta. Guardar KP/KD/SPEED
      // es sólo por BT ('W').
      if (millis() - t0 > 2000) {
        calibrarRegletaManual();
        estado = 1;  // fuerza que, al soltar el botón, el robot quede Detenido (ver debajo del while)
        delay(500);
      }
    }
    if (estado != 0) {
      estado = 0;
    } else {
      estado = estadoDeArranque();
    }
    indicar_estado(estado);
  }

  if (estado != 0) {
    if (millis() - lastLoopTime > LOOP_MS) {
      if (estado == 3) {
        conducirRecto();  // lazo abierto, sin corrección — bench-test T1.1
      } else {
        correr();
      }
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


/**** ESTADO DE ARRANQUE ***/
// 2 (con turbina) si DIP 4 está activada (LOW) en este instante; 1 si no.
// Lo usan el pulsador y el comando BT 'S' para que arranquen igual.
byte estadoDeArranque() {
  return (digitalRead(DIP_SW4) == LOW) ? 2 : 1;
}


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
