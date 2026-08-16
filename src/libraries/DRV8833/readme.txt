
***************************************************************************************************
DRV8833.h: Libreria para utilizar el driver de motores DRV8833.

Data sheet: https://www.pololu.com/file/download/drv8833.pdf?file_id=0J534

Modificada por Mauricio Venanzoni.
V1.0 - 20220720
***************************************************************************************************


*** INSTALACIÓN: ***

1. Copiar la carpeta DRV8833 dentro de la carpeta "libraries" de Arduino (generalmente en "Documentos\Arduino\libraries")
2. Reiniciar Arduino IDE para que la reconozca


*** COMO USAR: ***

1. Al principio del programa incluir la librería y definir los motores: 

	#include <DRV8833.h>
	DRV8833 motorIzq(5,6); 		// pines del Arduino que utiliza el motor Izquierdo
	
2. En la función setup() completar la configuración del motor llamando a la función begin():

	motorIzq.begin();
	
3. Las siguientes funciones se utilizan para controlar el funcionamiento de los motores:

	mode(modo): modo de funcionamiento del motor (FAST_DECAY o SLOW_DECAY)
		En SLOW_DECAY el motor tiene un freno activo, consume mayor corriente pero tiene una curva de velocidad más lineal al manejarlo con PWM (modo por defecto)
		En FAST_DECAY el motor gira libremente entre cada pulso del PWM, consume menos corriente pero es más dificil de controlar.

	stop(): frena el motor (modo SLOW_DECAY, freno rápido)

	free(): frena el motor de forma suave (modo FAST_DECAY)
	
	speed(velocidad): cambia la velocidad del motor
		La velocidad puede ser un valor de -255 a 255.
		Con valores positivos el motor gira en una dirección. Valores negativos para giro inverso.
  		0 = detenido (es equivalente a stop() o free() según el modo de funcionamiento)
  		
*** EJEMPLOS ***  
		
En la carpeta "Ejemplos" pueden ver algunos programas de muestra.
