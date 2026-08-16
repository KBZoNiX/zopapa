

/* FUNCIONES DEL MÓDULO BLUETOOTH
 *
 * Comando	Función	              Ejemplo
 * S	      Start	                S
 * P	      Stop	                P
 * T	      AutoTune	            T
 * Vxxx	    Ajustar velocidad     V180
 * Kxxx	    Ajustar KP	          K22.5
 * Dxxx	    Ajustar KD	          D15.3
 * R	      Leer parám. actuales  R
 * L        Cargar parámetros     L
 * W        Guardar parámetros    W
 * Q        Probar regleta        Q
 * H        Help!
 */


// Lectura e interpretación de comandos
void btListenStopOnly() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'P' || c == 'p') {
      estado = 0;
      Serial.println(F("Detenido"));
    } 
  }
}

// Lectura e interpretación de comandos
void btListen() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (btCommand.length() > 0) {
        btProcessCommand(btCommand);
        btCommand = "";
      }
    } else {
      btCommand += c;
    }
  }
}

// Mostrar comandos
void btPrintHelp() {
Serial.println(F("Comando  Función               Ejemplo"));
Serial.println(F("S        Start                 S"));
Serial.println(F("P        Stop                  P"));
Serial.println(F("T        AutoTune              T"));
Serial.println(F("Vxxx     Ajustar velocidad     V180"));
Serial.println(F("Kxxx     Ajustar KP            K22.5"));
Serial.println(F("Dxxx     Ajustar KD            D15.3"));
Serial.println(F("R        Leer parám. actuales  R"));
Serial.println(F("L        Cargar parámetros     L"));
Serial.println(F("W        Guardar parámetros    W"));
Serial.println(F("Q        Probar regleta        Q"));
Serial.println(F("H        Help!"));
  
}

void probar_regleta() {

  while (digitalRead(PULSADOR) == 1) {
    Regleta.read(SENSORES);
    for (byte i = 0; i < NRO_SENSORES; i++) {
      Serial.print(SENSORES[i]);
      Serial.print(F("\t"));
    }
    Serial.println();
    delay(200);
  }
  
}


// Procesador de comandos
void btProcessCommand(String cmd) {
  char action = toupper(cmd.charAt(0)); // Primera letra
  String value = (cmd.length() > 1) ? cmd.substring(1) : "";

  switch (action) {
    case 'H':
      btPrintHelp();
      break;

    case 'Q':
      probar_regleta();
      break;

    case 'S':
      estado = true;
      Serial.println(F("Arrancando..."));
      break;
    
    case 'P':
      estado = false;
      Serial.println("Detenido");
      break;

    case 'T':
      Serial.println(F("Iniciando AutoTune..."));
      digitalWrite(LED, HIGH);
      beep(5, 1000, 7);
      autotuneRelay();  // Llamada al autotune
      digitalWrite(LED, LOW);
      Serial.println(F("AutoTune Finalizado."));      
      break;

    case 'V':
      SPEED = value.toInt();
      Serial.print(F("Velocidad = ")); Serial.println(SPEED);
      break;

    case 'K':
      KP = value.toFloat();
      Serial.print("KP = "); Serial.println(KP);
      break;

    case 'D':
      KD = value.toFloat();
      Serial.print("KD = "); Serial.println(KD);
      break;

    case 'R':
      Serial.print("KP: "); Serial.print(KP);
      Serial.print(" | KD: "); Serial.print(KD);
      Serial.print(" | Vel: "); Serial.println(SPEED);
      break;

    case 'L':
      leerConstantesPID(); 
      break;
      
    case 'W':
      guardarConstantesPID(KP, KD, SPEED);
      break;

    default:
      Serial.print("Comando inválido: "); Serial.println(cmd);
      break;
  }
}
