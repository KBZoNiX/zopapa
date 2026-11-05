/***** PERFILES DE GANANCIA (Tier 1, T-S5/T-S6 — PLAN-tournament-stopgap-v1.md) *****/
// Guarda 3 ternas (SPEED, KP, KD) en EEPROM, seleccionables con un comando
// ('G'/'U' en bt.ino) en vez de tener que reescribir K/D/V a mano entre
// carreras bajo presión de tiempo. No agrega matemática de control nueva ni
// depende de `n` — es sólo almacenamiento y selección (T-S5).
//
// Sembrado (T-S6): si la EEPROM no tiene la sección de perfiles, se llena
// con la tabla §11.1 de la FSD (marcada ahí como dato viejo sin verificar,
// pero es el mejor punto de partida disponible esta semana). Cada perfil
// puede sobreescribirse en la práctica con 'U' sin tocar el firmware.

int presetAddr(byte idx) {
  return EEPROM_ADDR_PRESET_BASE + (int)idx * PRESET_STRIDE;
}

// Si el marcador no está presente (primer flasheo de este layout, o EEPROM
// nueva/reusada), siembra los perfiles por defecto y avisa. T-S4: fallback
// explícito y ruidoso a valores conocidos-buenos en vez de EEPROM sin
// inicializar leída como si fuera válida.
void seedPresetsIfNeeded() {
  if (EEPROM.read(EEPROM_ADDR_PRESET_FLAG) == PRESET_MAGIC_0 &&
      EEPROM.read(EEPROM_ADDR_PRESET_FLAG + 1) == PRESET_MAGIC_1) {
    return;
  }

  for (byte i = 0; i < PRESET_COUNT; i++) {
    int addr = presetAddr(i);
    eepromWriteFloat(addr, PRESET_DEFAULT_KP[i]);
    eepromWriteFloat(addr + 4, PRESET_DEFAULT_KD[i]);
    EEPROM.update(addr + 8, PRESET_DEFAULT_SPEED[i]);
  }
  EEPROM.update(EEPROM_ADDR_PRESET_FLAG, PRESET_MAGIC_0);
  EEPROM.update(EEPROM_ADDR_PRESET_FLAG + 1, PRESET_MAGIC_1);

  Serial.println(F("Perfiles de ganancia no encontrados en EEPROM: sembrados con valores por defecto (tabla FSD §11.1)."));
  beep(2, 150, 8);
}

// Carga el perfil idx (0-based) a KP/KD/SPEED en vivo. No valida el rango:
// el llamador (bt.ino) ya lo hace antes de invocar esto.
void cargarPreset(byte idx) {
  int addr = presetAddr(idx);
  KP = eepromReadFloat(addr);
  KD = eepromReadFloat(addr + 4);
  SPEED = EEPROM.read(addr + 8);
  activePreset = idx;

  Serial.print(F("Perfil ")); Serial.print(idx + 1); Serial.println(F(" cargado:"));
  Serial.print(F("KP=")); Serial.print(KP, 4);
  Serial.print(F(" KD=")); Serial.print(KD, 4);
  Serial.print(F(" SPEED=")); Serial.println(SPEED);
}

// Guarda los KP/KD/SPEED en vivo en el perfil idx (0-based), sobreescribiendo
// lo que hubiera. Así el operador puede ajustar a mano en la práctica y
// fijar el resultado como perfil de torneo sin reflashear.
void guardarPreset(byte idx) {
  int addr = presetAddr(idx);
  eepromWriteFloat(addr, KP);
  eepromWriteFloat(addr + 4, KD);
  EEPROM.update(addr + 8, SPEED);
  activePreset = idx;

  Serial.print(F("KP/KD/SPEED actuales guardados en perfil ")); Serial.println(idx + 1);
}

// Carga al arrancar el perfil indicado por DIP 1/2/3 (activada = LOW).
// Una sola activada: la carga. Ninguna: no toca nada. Más de una: no carga
// ninguna, avisa por Serial y da un beep de error.
void cargarPerfilPorDip() {
  const int pines[PRESET_COUNT] = { DIP_SW1, DIP_SW2, DIP_SW3 };
  int activos = 0;
  int elegido = -1;
  for (byte i = 0; i < PRESET_COUNT; i++) {
    if (digitalRead(pines[i]) == LOW) {
      activos++;
      elegido = i;
    }
  }
  if (activos == 1) {
    cargarPreset((byte)elegido);
  } else if (activos > 1) {
    Serial.println(F("Mas de una llave DIP de perfil activa: no se carga ningun perfil."));
    beep(4, 80, 8);
  }
}
