


/* digitalToggle(): conmuta el estado de una salida.
 *  
 * pin: Salida del Arduino a conmutar.
 */
void digitalToggle(int pin){
  digitalWrite(pin, !digitalRead(pin));
  
}


/* presiono(): lee el estado de un pulsador, evitando rebotes.
 *  
 * Devuelve true si se presionó el pulsador.
 * 
 * pin: Entrada del Arduino conectada al pulsador.
 */
bool presiono(uint8_t pin) {
  
  const uint8_t PRESIONADO = 0;    // Valor que devuelve al presionar el pulsador
  
  if (digitalRead(pin) == PRESIONADO) {
    
    // Evito rebotes del pulsador:
    delay(10);
    while (digitalRead(pin) == PRESIONADO) {
      delay(10);
    }
    return true;
  } 
  else {
    return false;
  }
}


/** beep(): produce beeps de la duración indicada
 * 
 * cantidad: numero de beeps (1..255) 
 *
 * tiempo: duración de cada beep, en ms (0..65000) 
 *
 * volumen: puede usarse para reducir el volumen o desactivalo. (0..10) 
 */
void beep(uint8_t cantidad_beeps, uint8_t tiempo, uint8_t volumen) {
  
  if (volumen > 0) {
    
	  uint8_t t_on = tiempo / (11 - volumen) / 2;
	  uint8_t t_off = tiempo - t_on;
	  
	  for (byte beeps = 0; beeps < cantidad_beeps; beeps++) {
  		digitalWrite(BUZZER, HIGH);
  		delay(t_on);
  		digitalWrite(BUZZER, LOW);
  		delay(t_off);
	  }
	}
}

