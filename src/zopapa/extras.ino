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

