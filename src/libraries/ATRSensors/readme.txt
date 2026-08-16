
***************************************************************************************************
ATRSensors: Librería para leer regleta de sensores de linea infrarojos

Basada en la librería QTRSensors Pololu. Optimizada para funcionar solo con sensores analógicos en 
placas Arduino AVR de 8-bits (Uno, Nano, Mega...). 

Modificada por Mauricio Venanzoni.
Versión 1.0 - Julio de 2022
***************************************************************************************************


Implementa una función más rápida para el conversor analógico-digital y captura sólo una muestra
para cada sensor por pasada. Devuelve el valor como un número de 8 bits, suficiente para 
interpretar el contraste entre blanco y negro.

La función readLine() devuelve un valor de 100 por sensor en lugar de 1000 de la librería original.

Algunas funciones adicionales fueron desactivadas (Ej: control de LEDs IR)


Mediciones del tiempo de ejecución de la función read():
  QTRSensors -> 450us por sensor (4 muestras de 110us c/u).
  ATRSensors -> 45us por sensor (toma una sola muestra).

Mediciones del tiempo de ejecución de la función readLine() utilizando 4 
sensores:
  QTRSensors -> 2100us
  ATRSensors -> 250us


***************************************************************************************************
 Written by Ben Schmidel et al., October 4, 2010
 Copyright (c) 2008-2012 Pololu Corporation. For more information, see

 http://www.pololu.com
 http://forum.pololu.com
 http://www.pololu.com/docs/0J19

 You may freely modify and share this code, as long as you keep this
 notice intact (including the two links above).  Licensed under the
 Creative Commons BY-SA 3.0 license:

 http://creativecommons.org/licenses/by-sa/3.0/

 Disclaimer: To the extent permitted by law, Pololu provides this work
 without any warranty.  It might be defective, in which case you agree
 to be responsible for all resulting costs and damages.
***************************************************************************************************

*** INSTALACIÓN: ***

1. Copiar la carpeta ATRSensors dentro de la carpeta "libraries" de Arduino 
   (generalmente en "Documentos\Arduino\libraries")
   
2. Reiniciar Arduino IDE para que la reconozca


*** COMO USAR: ***

1. Al principio del programa incluir la librería y definir pines: 

	#include <ATRSensors.h>
	
	const unsigned char SENSORES[] = { A1, A2, A3, A4 }; 		// Sensores de linea {pines}
	const unsigned char NRO_SENSORES = sizeof(SENSORES); 		// Calcula el número de sensores de linea
	ATRSensors Regleta(SENSORES, NRO_SENSORES);                 // Define una "Regleta" con los sensores
	
2. Métodos de la librería:

	calibrate(): calibración de los sensores. Guarda el valor mínimo y máximo leido en cada sensor.
		Debe llamarse antes de utilizar las funciones readCalibrate o readLine.
		
	readLine(): retorna una posición estimada con respecto a la linea. Puede configurarse para leer
		linea blanca o negra.

	readCalibrate(): retorna un valor calibrado entre 0 y 100 para cada sensor, donde 0 corresponde
	al mínimo valor de calibración y 100 al máximo. Generalmente no se utiliza de forma directa, 
	puede ser útil para probar el funcionamiento de la regleta.
		
	read(): Lee el valor de cada sensor (sin calibrar). Generalmente no se utiliza de forma 
		directa, puede ser útil para probar el funcionamiento de la regleta
		
  		
*** EJEMPLOS ***  (En construcción...)
		
En la carpeta "Ejemplos" pueden ver algunos programas de muestra.
