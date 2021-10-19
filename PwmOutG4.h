//
// Created by achauvel on 10/02/2021.
//

#ifndef PWMOUTG4_H
#define PWMOUTG4_H

#include <mbed.h>

class PwmOutG4 {

public:

    PwmOutG4(PinName pin, bool inverted=false);
    ~PwmOutG4();


    void startPWM(); // could be called "resume()" like PwmOut MBED object
    void stopPWM();// could be called "suspend()" like PwmOut MBED object

    void write(float pwm);
    void syncWith(PwmOutG4 *other);
    void syncWith(PwmOutG4 *other1, PwmOutG4 *other2);

private:

    // Base HRTIM1 initialization : only one time
    static HRTIM_HandleTypeDef _hhrtim1;
    static int _hrtim_initialized;

    PinName _pin;
    bool _inverted;
    uint32_t _duty_cycle;
    uint32_t _period;
    uint32_t _hrtim_prescal;

    uint32_t _tim_idx;
    uint32_t _tim_reset;
    uint32_t _tim_output;
    uint32_t _tim_cpr_unit;
    uint32_t _tim_cpr_reset;

    GPIO_TypeDef* _gpio_port;
    uint32_t _gpio_pin;


    void initPWM();
    void setupHRTIM1();
    void setupPWMTimer();
    void setupPWMOutput();
    void setupGPIO();

};


#endif //PWMOUTG4_H
