/***** Gestión de EEPROM para KP y KD *****/
// Guardar los valores actuales de KP y KD en EEPROM y permite leerlos al iniciar el programa.
// Si la EEPROM no fue inicializada carga valores por defecto.

// Direcciones donde se guardan el flag de inicialización, KP, KD y SPEED
const int EEPROM_ADDR_FLAG = 0;  // Byte para indicar si está inicializada
const int EEPROM_ADDR_KP = 4;    // KP (float ocupa 4 bytes)
const int EEPROM_ADDR_KD = 8;    // KD (float ocupa 4 bytes)
const int EEPROM_ADDR_SPEED = 12;    // SPEED (byte ocupa 1 byte)

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
    activePreset = -1;  // T-S5: ranura manual, distinta de los perfiles

    Serial.println(F("Constantes PID cargadas desde EEPROM:"));
    Serial.print(F("KP = "));     Serial.println(KP);
    Serial.print(F("KD = "));     Serial.println(KD);
    Serial.print(F("SPEED = "));  Serial.println(SPEED);

  } else {
      // T-S4: fallback ruidoso, no silencioso — sin esto, un EEPROM sin
      // guardar pasa desapercibido hasta que el comportamiento en pista no
      // coincide con lo esperado.
      Serial.println(F("EEPROM no inicializada. Usando valores por defecto."));
      beep(2, 150, 8);
  }
}


/***** Gestión de EEPROM para la calibración de la regleta *****/
// Guarda/carga los límites min/max de calibración de cada sensor, para no
// tener que repetir el barrido manual sobre la línea en cada arranque.

// Direcciones: flag de calibración (2 bytes) + min/max por sensor (1 byte c/u)
//
// El flag usa un valor "mágico" de 2 bytes en lugar de un simple 1, porque
// una EEPROM sin inicializar (o con datos de un sketch anterior) puede traer
// cualquier byte de fábrica/uso previo — probamos esto en hardware real: el
// byte en la dirección usada originalmente ya contenía 0x01 por pura
// coincidencia, lo que hacía cargar "calibración" que en realidad era
// basura. Dos bytes fijos hacen la colisión mucho menos probable.
const int EEPROM_ADDR_CAL_FLAG = 1;         // ocupa 2 bytes: 1 y 2
const byte CAL_MAGIC_0 = 0xC1;
const byte CAL_MAGIC_1 = 0xB8;
const int EEPROM_ADDR_CAL_MIN = 16;
const int EEPROM_ADDR_CAL_MAX = EEPROM_ADDR_CAL_MIN + NRO_SENSORES;

// Función para guardar la calibración vigente de la regleta
void guardarCalibracion() {
  for (int i = 0; i < NRO_SENSORES; i++) {
    EEPROM.update(EEPROM_ADDR_CAL_MIN + i, Regleta.calibratedMinimum[i]);
    EEPROM.update(EEPROM_ADDR_CAL_MAX + i, Regleta.calibratedMaximum[i]);
  }
  EEPROM.update(EEPROM_ADDR_CAL_FLAG, CAL_MAGIC_0);
  EEPROM.update(EEPROM_ADDR_CAL_FLAG + 1, CAL_MAGIC_1);

  Serial.println(F("Calibración de regleta guardada en EEPROM."));
}

// Función para cargar la calibración guardada al iniciar.
// Devuelve true si había una calibración válida y la cargó.
bool cargarCalibracion() {
  if (EEPROM.read(EEPROM_ADDR_CAL_FLAG) != CAL_MAGIC_0 ||
      EEPROM.read(EEPROM_ADDR_CAL_FLAG + 1) != CAL_MAGIC_1) {
    return false;
  }

  if (Regleta.calibratedMinimum == 0) {
    Regleta.calibratedMinimum = (unsigned char *)malloc(NRO_SENSORES);
  }
  if (Regleta.calibratedMaximum == 0) {
    Regleta.calibratedMaximum = (unsigned char *)malloc(NRO_SENSORES);
  }
  if (Regleta.calibratedMinimum == 0 || Regleta.calibratedMaximum == 0) {
    return false;  // malloc falló
  }

  for (int i = 0; i < NRO_SENSORES; i++) {
    Regleta.calibratedMinimum[i] = EEPROM.read(EEPROM_ADDR_CAL_MIN + i);
    Regleta.calibratedMaximum[i] = EEPROM.read(EEPROM_ADDR_CAL_MAX + i);
  }
  return true;
}


/***** Chequeo de calidad de calibración (Tier 1, T-S2 — PLAN-tournament-stopgap-v1.md) *****/
// La iluminación del lugar del torneo no va a coincidir con la del taller, y
// un sensor sucio o fallado corre el rango efectivo de posición sin ningún
// síntoma obvio hasta que el robot está en la pista. Reporta min/max/spread
// por canal y avisa si algún canal quedó con poco contraste. Puro
// diagnóstico: no cambia nada del control. Se llama después de un barrido
// manual (calibrarRegletaManual()) y desde el chequeo previo ('F').
const byte CAL_SPREAD_WARN_THRESHOLD = 50;  // sobre escala 0-255 (ver ATRSensors::calibrate())

void imprimirCalidadCalibracion() {
  Serial.println(F("Calidad de calibracion (min / max / spread):"));
  bool huboAdvertencia = false;

  for (int i = 0; i < NRO_SENSORES; i++) {
    byte cmin = Regleta.calibratedMinimum[i];
    byte cmax = Regleta.calibratedMaximum[i];
    byte spread = cmax - cmin;  // wrap silencioso si cmax<cmin, imposible salvo hardware roto

    Serial.print(F("  S")); Serial.print(i);
    Serial.print(F(": ")); Serial.print(cmin);
    Serial.print(F(" / ")); Serial.print(cmax);
    Serial.print(F(" / ")); Serial.print(spread);

    if (spread < CAL_SPREAD_WARN_THRESHOLD) {
      Serial.print(F("  <-- BAJO CONTRASTE"));
      huboAdvertencia = true;
    }
    Serial.println();
  }

  if (huboAdvertencia) {
    Serial.println(F("Advertencia: uno o mas sensores con poco contraste. Revisar suciedad/alineacion o recalibrar bajo la luz de la pista."));
    beep(3, 150, 9);
  }
}
