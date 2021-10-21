//
// Created by achauvel on 10/02/2021.
//

#ifndef PWMOUTG4_H
#define PWMOUTG4_H

#define NUM_TIM_MAX            6
#define TIM_ROLLOVER_ENABLED   0x02
#define TIM_ROLLOVER_DISABLED  0x01


#include <mbed.h>

class PwmOutG4 {

public:

    PwmOutG4(PinName pin, bool inverted=false, bool rollover=false);
    ~PwmOutG4();


    void startPWM(); // could be called "resume()" like PwmOut MBED object
    void stopPWM();// could be called "suspend()" like PwmOut MBED object

    void write(float pwm);
    void syncWith(PwmOutG4 *other);
    void syncWith(PwmOutG4 *other1, PwmOutG4 *other2);

private:

    // Base HRTIM1 initialization : only one time
    static HRTIM_HandleTypeDef _hhrtim1;
    static bool _hrtim_initialized;
    static uint8_t _tim_initialized[NUM_TIM_MAX];
    static uint8_t _tim_general_state[NUM_TIM_MAX];

    static uint32_t _min_frequ_ckpsc[8];

    PinName _pin;
    bool _inverted;
    bool _rollover;
    uint32_t _frequency;
    uint32_t _duty_cycle;
    uint32_t _period;
    uint32_t _hrtim_prescal;

    float _pwm;
    uint32_t _duty_cycle_max;
    uint32_t _duty_cycle_min;

    uint32_t _tim_idx;
    uint32_t _tim_reset;
    uint32_t _tim_output;
    uint32_t _tim_cpr_unit;
    uint32_t _tim_cpr_reset;

    GPIO_TypeDef* _gpio_port;
    uint32_t _gpio_pin;


    void initPWM();
    void setupFrequency();
    void setupHRTIM1();
    void setupPWMTimer();
    void setupPWMOutput();
    void setupGPIO();

};


#endif //PWMOUTG4_H
