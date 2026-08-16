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
 * Written by Ben Schmidel et al., October 4, 2010
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

#ifndef ATRSensors_h
#define ATRSensors_h


// This class cannot be instantiated directly (it has no constructor).
// Instead, you should instantiate one of its two derived classes (either the
// QTR-A or QTR-RC version, depending on the type of your sensor).
class ATRSensors
{
  public:


// if this constructor is used, the user must call init() before using
    // the methods in this class
    ATRSensors();

    // this constructor just calls init()
    ATRSensors(unsigned char* pins, unsigned char numSensors);

    // the array 'pins' contains the Arduino analog pin assignment for each
    // sensor.  For example, if pins is {0, 1, 7}, sensor 1 is on
    // Arduino analog input 0, sensor 2 is on Arduino analog input 1,
    // and sensor 3 is on Arduino analog input 7.

    // 'numSensors' specifies the length of the 'analogPins' array (i.e. the
    // number of QTR-A sensors you are using).  numSensors must be
    // no greater than 16.
    void init(unsigned char* pins, unsigned char numSensors);


    // Reads the sensor values into an array. There *MUST* be space
    // for as many values as there were sensors specified in the constructor.
    // Example usage:
    // unsigned char sensor_values[8];
    // sensors.read(sensor_values);
    // The values returned are a measure of the reflectance with higher values 
    // corresponding to lower reflectance (e.g. a black surface or a void).
    void read(unsigned char *sensor_values);

    // Reads the sensors for calibration.  The sensor values are
    // not returned; instead, the maximum and minimum values found
    // over time are stored internally and used for the
    // readCalibrated() method.
    void calibrate();

    // Returns values calibrated to a value between 0 and 100, where
    // 0 corresponds to the minimum value read by calibrate() and 100
    // corresponds to the maximum value.  Calibration values are
    // stored separately for each sensor, so that differences in the
    // sensors are accounted for automatically.
    void readCalibrated(unsigned char *sensor_values);

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
    int readLine(unsigned char *sensor_values, unsigned char white_line = 0);

    // Calibrated minumum and maximum values. These start at 1000 and
    // 0, respectively, so that the very first sensor reading will
    // update both of them.
    //
    // The pointers are unallocated until calibrate() is called, and
    // then allocated to exactly the size required.
    //
    // These variables are made public so that you can use them for
    // your own calculations and do things like saving the values to
    // EEPROM, performing sanity checking, etc.
    unsigned char *calibratedMinimum;
    unsigned char *calibratedMaximum;

    ~ATRSensors();


  private:

    unsigned char *_pins;
    unsigned char _numSensors;
    int _lastValue;
};



#endif
