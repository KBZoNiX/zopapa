/*
 *	ATRSensors: Librería para leer regleta de sensores de linea infrarojos
 *
 *	Basada en la librería QTRSensors Pololu. Optimizada para funcionar 
 *  solo con sensores analógicos en placas Arduino AVR de 8-bits (Uno, Nano,
 *  Mega...). 
 *
 *  Modificada por Mauricio Venanzoni.
 *  Versión 1.0 - Julio de 2022
 *
 *
 *  Implementa una función más rápida para el conversor analógico-digital y 
 *  captura sólo una muestra para cada sensor por pasada. Devuelve el valor
 *  como un número de 8 bits, suficiente para interpretar el contraste entre
 *  blanco y negro.
 *
 *	La función readLine() devuelve un valor de 100 por sensor en lugar de 1000
 *  de la librería original.
 *
 *  Algunas funciones adicionales fueron desactivadas (Ej: control de LEDs IR)
 *
 *
 *  Mediciones del tiempo de ejecución de la función read():
 *    QTRSensors -> 450us por sensor (4 muestras de 110us c/u).
 *    ATRSensors -> 45us por sensor (toma una sola muestra).
 *  
 *  Mediciones del tiempo de ejecución de la función readLine() utilizando 4 
 *  sensores:
 *    QTRSensors -> 2100us
 *    ATRSensors -> 250us
 *
 */

/*
 * Written by Ben Schmidel et al., October 4, 2010.
 * Copyright (c) 2008-2012 Pololu Corporation. For more information, see
 *
 *   http://www.pololu.com
 *   http://forum.pololu.com
 *   http://www.pololu.com/docs/0J19
 *
 * You may freely modify and share this code, as long as you keep this
 * notice intact (including the two links above).  Licensed under the
 * Creative Commons BY-SA 3.0 license:
 *
 *   http://creativecommons.org/licenses/by-sa/3.0/
 *
 * Disclaimer: To the extent permitted by law, Pololu provides this work
 * without any warranty.  It might be defective, in which case you agree
 * to be responsible for all resulting costs and damages.
 */

#include <stdlib.h>
#include "ATRSensors.h"
#include <Arduino.h>


// Lee el ADC con un prescalador de 32 en lugar de 128 de analogRead()
// Devuelve el valor como un entero de 8-bits
// Conversión en 26us + delay carga S/H 5us. Tiempo total ~ 40us
unsigned char fasterAnalogRead(unsigned char pin) {
  
	unsigned char ADCSRAoriginal = ADCSRA;

	if (pin >= 14) pin -= 14; // allow for channel or pin numbers

	// set ADLAR bit to 1 (Left adjust result, for 8-bit read) and select 
	// AVCC Voltage Reference
	ADMUX = B01100000;

	// Select the channel (low 4 bits).
	ADMUX |= pin & 0x07;
  
	// set prescale to 32:
	ADCSRA = (ADCSRA & B11111000) | 5;

	// without a delay, we seem to read from the wrong channel
	delayMicroseconds(5);

	// start the conversion
	bitSet(ADCSRA, ADSC);

	// ADSC is cleared when the conversion finishes
	while (bit_is_set(ADCSRA, ADSC));

	// ADCH register returns highest 8 bits from ADC.
	unsigned char value = ADCH;

	// restore original ADCSRA prescaler
	ADCSRA = ADCSRAoriginal;

	return value;
}




// Constructores de la clase:
ATRSensors::ATRSensors()
{
    calibratedMinimum = 0;
    calibratedMaximum = 0;
    _pins = 0;
}

ATRSensors::ATRSensors(unsigned char* pins,
  unsigned char numSensors)
{
    calibratedMinimum = 0;
    calibratedMaximum = 0;
    _pins = 0;

    init(pins, numSensors);
}

// the array 'pins' contains the Arduino analog pin assignment for each
// sensor.  For example, if pins is {0, 1, 7}, sensor 1 is on
// Arduino analog input 0, sensor 2 is on Arduino analog input 1,
// and sensor 3 is on Arduino analog input 7.

// 'numSensors' specifies the number of sensors you are using.
//  numSensors must be no greater than 16.
void ATRSensors::init(unsigned char* pins,
    unsigned char numSensors)
{
    calibratedMinimum=0;
    calibratedMaximum=0;

    _lastValue=0; // assume initially that the line is left.

    _numSensors = numSensors;
	
    if (_pins == 0)
    {
        _pins = (unsigned char*)malloc(sizeof(unsigned char)*_numSensors);
        if (_pins == 0)
            return;
    }

    unsigned char i;
    for (i = 0; i < _numSensors; i++)
    {
        _pins[i] = pins[i];
    }
}


// Reads the sensor values into an array. There *MUST* be space
// for as many values as there were sensors specified in the constructor.
// Example usage:
// unsigned char sensor_values[8];
// sensors.read(sensor_values);
// The values returned are a measure of the reflectance with higher values 
// corresponding to lower reflectance (e.g. a black surface or a void).
void ATRSensors::read(unsigned char *sensor_values)
{
    unsigned char i;

    for (i = 0; i < _numSensors; i++)
    {
 	    //sensor_values[i] = analogRead(_pins[i]) >> 2;
    	sensor_values[i] = fasterAnalogRead(_pins[i]);
    }
}



// Reads the sensors 10 times and uses the results for
// calibration.  The sensor values are not returned; instead, the
// maximum and minimum values found over time are stored internally
// and used for the readCalibrated() method.
void ATRSensors::calibrate()
{
    int i;
    unsigned char sensor_values[16];
    unsigned char max_sensor_values[16];
    unsigned char min_sensor_values[16];

    // Allocate the arrays if necessary.
    if(calibratedMaximum == 0)
    {
        calibratedMaximum = (unsigned char*)malloc(sizeof(unsigned char)*_numSensors);

        // If the malloc failed, don't continue.
        if(calibratedMaximum == 0)
            return;

        // Initialize the max and min calibrated values to values that
        // will cause the first reading to update them.

        for(i=0;i<_numSensors;i++)
            (calibratedMaximum)[i] = 0;
    }
    if(calibratedMinimum == 0)
    {
        calibratedMinimum = (unsigned char*)malloc(sizeof(unsigned char)*_numSensors);

        // If the malloc failed, don't continue.
        if(calibratedMinimum == 0)
            return;

        for(i=0;i<_numSensors;i++)
            (calibratedMinimum)[i] = 255;
    }

    for(int j=0;j<10;j++)
    {
        read(sensor_values);
        for(i=0;i<_numSensors;i++)
        {
            // set the max we found THIS time
            if(j == 0 || max_sensor_values[i] < sensor_values[i])
                max_sensor_values[i] = sensor_values[i];

            // set the min we found THIS time
            if(j == 0 || min_sensor_values[i] > sensor_values[i])
                min_sensor_values[i] = sensor_values[i];
        }
    }

    // record the min and max calibration values
    for(i=0;i<_numSensors;i++)
    {
        if(min_sensor_values[i] > (calibratedMaximum)[i])
            (calibratedMaximum)[i] = min_sensor_values[i];
        if(max_sensor_values[i] < (calibratedMinimum)[i])
            (calibratedMinimum)[i] = max_sensor_values[i];
    }
}


// Returns values calibrated to a value between 0 and 100, where
// 0 corresponds to the minimum value read by calibrate() and 100
// corresponds to the maximum value.  Calibration values are
// stored separately for each sensor, so that differences in the
// sensors are accounted for automatically.
void ATRSensors::readCalibrated(unsigned char *sensor_values)
{
    unsigned char i;

    // read the needed values
    read(sensor_values);

    for(i=0;i<_numSensors;i++)
    {
        unsigned char calmin,calmax;
        unsigned char denominator;

        calmax = calibratedMaximum[i];
        calmin = calibratedMinimum[i];
        denominator = calmax - calmin;

        signed int x = 0;
        if(denominator != 0)
            x = (((signed int)sensor_values[i]) - calmin) * 100 / denominator;
        
        sensor_values[i] = constrain(x, 0, 100);
    }

}


// Operates the same as read calibrated, but also returns an
// estimated position of the robot with respect to a line. The
// estimate is made using a weighted average of the sensor indices
// multiplied by 100, so that a return value of 0 indicates that
// the line is directly below sensor 0, a return value of 100
// indicates that the line is directly below sensor 1, 200
// indicates that it's below sensor 2, etc.  Intermediate
// values indicate that the line is between two sensors.  The
// formula is:
//
//    0*value0 + 100*value1 + 200*value2 + ...
//   --------------------------------------------
//         value0  +  value1  +  value2 + ...
//
// By default, this function assumes a dark line (high values)
// surrounded by white (low values).  If your line is light on
// black, set the optional second argument white_line to true.  In
// this case, each sensor value will be replaced by (100-value)
// before the averaging.
int ATRSensors::readLine(unsigned char *sensor_values, unsigned char white_line)
{
    unsigned char i, on_line = 0;
    unsigned long avg; // this is for the weighted total, which is long
                       // before division
    unsigned int sum; // this is for the denominator which is <= 64000

    readCalibrated(sensor_values);

    avg = 0;
    sum = 0;

    for(i=0;i<_numSensors;i++) {
        int value = sensor_values[i];
        if(white_line)
            value = 100-value;

        // keep track of whether we see the line at all
        if(value > 20) {
            on_line = 1;
        }

        // only average in values that are above a noise threshold
        if(value > 8) {
            avg += (long)(value) * (i * 100);
            sum += value;
        }
    }

    if(!on_line)
    {
        // If it last read to the left of center, return 0.
        if(_lastValue < (_numSensors-1)*100/2)
            return 0;

        // If it last read to the right of center, return the max.
        else
            return (_numSensors-1)*100;

    }

    _lastValue = avg/sum;

    return _lastValue;
}


// the destructor frees up allocated memory
ATRSensors::~ATRSensors()
{
    if (_pins)
        free(_pins);
    if(calibratedMaximum)
        free(calibratedMaximum);
    if(calibratedMinimum)
        free(calibratedMinimum);
}
