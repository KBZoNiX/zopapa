/*
 * DRV833.h - Libreria para utilizar el driver de motores DRV8833.
 * Data sheet: https://www.pololu.com/file/download/drv8833.pdf?file_id=0J534
 *
 * V1.0 - 20220720
 *
 * Created by Mauricio Venanzoni.
 * Released into the public domain.
*/

#include "Arduino.h"
#include "DRV8833.h"

/* 
 * Constructor: asigna los pines y la configuración inicial 
 *
 * parámetros: pines del Arduino utilizados para controlar el motor
 */
DRV8833::DRV8833(int pin1, int pin2)
{
    _pin1 = pin1;
    _pin2 = pin2;
    _mode = SLOW_DECAY;		// modo por defecto
}

/* 
 * begin(): completa la configuración del motor
 */
void DRV8833::begin()
{
	pinMode(_pin1, OUTPUT);
	pinMode(_pin2, OUTPUT);
	digitalWrite(_pin1, LOW);
	digitalWrite(_pin2, LOW);
}

/* 
 * mode(): completa la configuración del motor
 *
 * parámetro: modo de funcionamiento del motor (FAST_DECAY o SLOW_DECAY)
 */
void DRV8833::mode(motorModeType mode)
{
	_mode = mode;
}


/* 
 * stop(): frena el motor (modo SLOW_DECAY, freno rápido)
 */
void DRV8833::stop()
{
	digitalWrite(_pin1, HIGH);
	digitalWrite(_pin2, HIGH);
}


/* 
 * free(): frena el motor de forma suave (modo FAST_DECAY)
 */
void DRV8833::free()
{
	digitalWrite(_pin1, LOW);
	digitalWrite(_pin2, LOW);
}

/* 
 * speed(): cambia la velocidad del motor
 *
 * parámetro: velocidad del motor (-255 a 255)
 * 		Valores negativos para giro inverso.
 * 		0 = detenido
 */
void DRV8833::speed(int speed)
{
	if (_mode == FAST_DECAY) {
    	byte pwm = abs(speed);
    
		if (speed>0) {
			digitalWrite(_pin1, LOW);
			analogWrite(_pin2, pwm);
		}
		else {
			analogWrite(_pin1, pwm);
			digitalWrite(_pin2, LOW);
		}
	}

	else {   //  SLOW_DECAY
		byte pwm = 255 - abs(speed);

		if (speed>0) {
			analogWrite(_pin1, pwm);
			digitalWrite(_pin2, HIGH);
		}
		else {
			digitalWrite(_pin1, HIGH);
			analogWrite(_pin2, pwm);
		}
	}
}



