/*
 * DRV8833.h - Libreria para utilizar el driver de motores DRV8833.
 * Data sheet: https://www.pololu.com/file/download/drv8833.pdf?file_id=0J534
 *
 * V1.0 - 20220719
 *
 * Created by Mauricio Venanzoni.
 * Released into the public domain.
*/

#ifndef DRV8833_H
#define DRV8833_H

#include "Arduino.h"

enum motorModeType {
  			COAST = 0, FAST_DECAY = 0, LIBRE = 0,
  			BRAKE = 1, SLOW_DECAY = 1, FRENO = 1
};

class DRV8833
{
public:
	
	// Constructor for the class:
	DRV8833(int pin1, int pin2);

	void begin();

	// Motor control functions:
	void mode(motorModeType mode);
	void stop();
	void free();
	void speed(int speed);

private:
	int _pin1;
    int _pin2;
    motorModeType _mode;

};

#endif // DRV8833_H
