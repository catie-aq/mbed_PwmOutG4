/*
 * Copyright (c) 2017, CATIE, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#ifndef PWMOUTG4_H
#define PWMOUTG4_H

#define NUM_TIM_MAX            6
#define TIM_ROLLOVER_ENABLED   0x02
#define TIM_ROLLOVER_DISABLED  0x01

#define DEFAULT_FREQUENCY   42000
#define DEFAULT_DEADTIME   0.00f

#define ADC_TRIG_POSTSCALER 10

#include <mbed.h>


/*!
 *  \class PwmOutG4
 *  High-resolution Timers STM32 Driver
 */
class PwmOutG4 {

public:

    /*!
     *  Default PwmOutG4 contructor
     *
     *  @param pin Pin for the Pwm Output
     *  @param frequency Frequency in Hz of the PWM (default = 42000Hz)
     *  @param inverted Inverted output (default = false)
     *  @param rollover Rollover mode, specific to the HRTIM, ideal to get proper AC triggered (default = false)
     *  @param deadtime Add a deadtime. To be used when setting for example two complementary pwm.
     *      Unit of deadtime is in percent of PWM, eg with 0.02f of deadtime, and two PWM:
     *          - PWM 1 (non-inverted)  --> duty cycle will lose 2%
     *          - PWM 2 (inverted)      --> duty cycle will gain 2%
     */
    PwmOutG4(PinName pin,
             uint32_t frequency = DEFAULT_FREQUENCY,
             bool inverted = false,
             bool rollover = false,
             float deadtime = DEFAULT_DEADTIME);

    ~PwmOutG4();

    /** Resume PWM operation
     *
     * In this case, this function will start PWM.
     * THIS FUNCTION MUST BE CALLED AFTER CREATING ALL PwmOutG4 OBJECTS
     *
     */
    void resume();

    /** Suspend PWM operation
     *
     * In this case, this function will stop PWM
     */
    void suspend();

    /** Set the output duty-cycle, specified as a percentage (float)
     *
     *  @param pwm A floating-point value representing the output duty-cycle,
     *    specified as a percentage. The value should lie between
     *    0.0f (representing on 0%) and 1.0f (representing on 100%).
     *    Values outside this range will be saturated to 0.0f or 1.0f.
     */
    void write(float pwm);


    // These functions does not relate from PwmOut MBED Object, and are specific to the use of HRTIM :
    void syncWith(PwmOutG4 *other);
    void syncWith(PwmOutG4 *other1, PwmOutG4 *other2);

private:

    // Base HRTIM1 initialization : only one time
    static HRTIM_HandleTypeDef _hhrtim1;
    static bool _hrtim_initialized;
    static bool _adctriggered_initialized;
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

    uint32_t _adc_update_src;
    uint32_t _adc_trig;

    float _pwm;
    float _deadtime;
    uint32_t _duty_cycle_max;
    uint32_t _duty_cycle_min;

    uint32_t _tim_idx;
    uint32_t _tim_reset;
    uint32_t _tim_output;
    uint32_t _tim_cpr_unit;
    uint32_t _tim_cpr_reset;

    GPIO_TypeDef *_gpio_port;
    uint32_t _gpio_pin;


    void initPWM();

    void setupFrequency();

    void setupHRTIM1();

    void setupAdcTrig();

    void setupPWMTimer();

    void setupPWMOutput();

    void setupGPIO();

};


#endif //PWMOUTG4_H
