//
// Created by achauvel on 10/02/2021.
//

#include "PwmOutG4.h"

// Define static members
HRTIM_HandleTypeDef PwmOutG4::_hhrtim1;
bool PwmOutG4::_hrtim_initialized = false;// Static, so initialized only one time
uint8_t PwmOutG4::_tim_initialized[NUM_TIM_MAX] = {0}; // Static, so initialized only one time
uint8_t PwmOutG4::_tim_general_state[NUM_TIM_MAX] = {0}; // Static, so initialized only one time
uint32_t PwmOutG4::_min_frequ_ckpsc[8] = {0};

PwmOutG4::PwmOutG4(PinName pin, bool inverted, bool rollover) :
        _pin(pin),
        _inverted(inverted),
        _rollover(rollover),
        _frequency(42000) //42kHz by default
        {

    // Init specific registers regarding the PWMx_OUT for the STM32G474VET6
    switch (_pin) {

        case PWM1_OUT: // PB12
            _tim_idx = HRTIM_TIMERINDEX_TIMER_C;
            _tim_reset = HRTIM_TIMERRESET_TIMER_C;
            _tim_output = HRTIM_OUTPUT_TC1;
            _tim_cpr_unit = HRTIM_COMPAREUNIT_1;
            _tim_cpr_reset = HRTIM_OUTPUTRESET_TIMCMP1;
            _gpio_port = GPIOB;
            _gpio_pin = GPIO_PIN_12;
            break;
        case PWM2_OUT: // PB14
            _tim_idx = HRTIM_TIMERINDEX_TIMER_D;
            _tim_reset = HRTIM_TIMERRESET_TIMER_D;
            _tim_output = HRTIM_OUTPUT_TD1;
            _tim_cpr_unit = HRTIM_COMPAREUNIT_1;
            _tim_cpr_reset = HRTIM_OUTPUTRESET_TIMCMP1;
            _gpio_port = GPIOB;
            _gpio_pin = GPIO_PIN_14;
            break;
        case PWM3_OUT: // PC6
            _tim_idx = HRTIM_TIMERINDEX_TIMER_F;
            _tim_reset = HRTIM_TIMERRESET_TIMER_F;
            _tim_output = HRTIM_OUTPUT_TF1;
            _tim_cpr_unit = HRTIM_COMPAREUNIT_1;
            _tim_cpr_reset = HRTIM_OUTPUTRESET_TIMCMP1;
            _gpio_port = GPIOC;
            _gpio_pin = GPIO_PIN_6;
            break;
        case DIO7: // PB15 (default option for PWM4 on ZEST_ACTUATOR_HALFBRIDGES)
            _tim_idx = HRTIM_TIMERINDEX_TIMER_D;
            _tim_reset = HRTIM_TIMERRESET_TIMER_D; // ATTENTION : même timer que PW2_OUT. les sorties doivent etre configurées AVANT d'allumer les 2, sinon le "_inverted" ne sera pas pris en compte par la HAL.
            _tim_output = HRTIM_OUTPUT_TD2;
            _tim_cpr_unit = HRTIM_COMPAREUNIT_3;
            _tim_cpr_reset = HRTIM_OUTPUTRESET_TIMCMP3;
            _gpio_port = GPIOB;
            _gpio_pin = GPIO_PIN_15;
            break;
        case DIO8: // PC7 (secondary option for PWM4 on ZEST_ACTUATOR_HALFBRIDGES)
            _tim_idx = HRTIM_TIMERINDEX_TIMER_F;
            _tim_reset = HRTIM_TIMERRESET_TIMER_F;
            _tim_output = HRTIM_OUTPUT_TF2;
            _tim_cpr_unit = HRTIM_COMPAREUNIT_2;
            _tim_cpr_reset = HRTIM_OUTPUTRESET_TIMCMP2;
            _gpio_port = GPIOC;
            _gpio_pin = GPIO_PIN_7;
            break;
        default:
            while (true) {
                error("ERROR PwmOutG4: object must be initialized with a known pin. Pin %d not recognized. See PwmOutG4.cpp for a list of known pins.", _pin);
            }
    }

    if((_tim_general_state[_tim_idx] == TIM_ROLLOVER_ENABLED) && !_rollover){
        printf("\nWarning PwmOutG4: Timer %lu has already been initialized with rollover mode. Pin %d must be initialized in rollover mode.\n",_tim_idx, _pin);
        _rollover = true;
    }

    if((_tim_general_state[_tim_idx] == TIM_ROLLOVER_DISABLED) && _rollover){
        printf("\nWarning PwmOutG4: Timer %lu has already been initialized in normal mode. Pin %d can't be initialized in rollover mode.\n",_tim_idx, _pin);
        _rollover = false;
    }

    // Special case considering roll over activated.
    if(_rollover){
        _tim_general_state[_tim_idx] = TIM_ROLLOVER_ENABLED;
    } else {
        _tim_general_state[_tim_idx] = TIM_ROLLOVER_DISABLED;
    }

    initPWM();
}

PwmOutG4::~PwmOutG4() {

}

void PwmOutG4::initPWM() {

    // First, setup the HRTIM (master of all HRTIM timers)
    if (!_hrtim_initialized) {
        setupHRTIM1();
        _hrtim_initialized = true; // To be initialized only one time.
    }

    // then, setup period, prescaler, pwm min/max following the request frequency
    setupFrequency();

    // then setup timer if needed
    if(_tim_initialized[_tim_idx] != 0){
        printf("\nWarning PwmOutG4: Timer %lu has already been initialized by pin %d, and can't be setup again,\n",_tim_idx, _tim_initialized[_tim_idx]);
        printf("Warning PwmOutG4: so pin %d will share same frequency/period, prescaler, and HRTIM mode as pin %d.\n",_pin, _tim_initialized[_tim_idx]);
    } else {
        setupPWMTimer();
        _tim_initialized[_tim_idx] = _pin;
    }

    // Then init the PWM output
    setupPWMOutput();
    setupGPIO();
//    startPWM(); // NE PAS START ICI, sinon 2 sorties d'un même timer ne seront pas correct si l'une des 2 est inversée (genre PB14 et PB15)

}

void PwmOutG4::setupFrequency() {

    // Just in case ...
    if(_frequency > SystemCoreClock)
        _frequency = SystemCoreClock;

    // See table 213 of STM32G4 reference manual.
    if(_frequency > _min_frequ_ckpsc[HRTIM_PRESCALERRATIO_MUL32]) { // CKPSC = 0, resolution = 184ps
        _hrtim_prescal = HRTIM_PRESCALERRATIO_MUL32;
    } else if (_frequency >= _min_frequ_ckpsc[HRTIM_PRESCALERRATIO_MUL16]) { // CKPSC = 1, resolution = 368ps at 170Mhz
        _hrtim_prescal = HRTIM_PRESCALERRATIO_MUL16;
    } else if (_frequency >= _min_frequ_ckpsc[HRTIM_PRESCALERRATIO_MUL8]) { // CKPSC = 2, resolution = 735ps at 170Mhz
        _hrtim_prescal = HRTIM_PRESCALERRATIO_MUL8;
    } else if (_frequency >= _min_frequ_ckpsc[HRTIM_PRESCALERRATIO_MUL4]) { // CKPSC = 3, resolution = 1.47ns at 170Mhz
        _hrtim_prescal = HRTIM_PRESCALERRATIO_MUL4;
    } else if (_frequency >= _min_frequ_ckpsc[HRTIM_PRESCALERRATIO_MUL2]) { // CKPSC = 4, resolution = 2.94ns at 170Mhz
        _hrtim_prescal = HRTIM_PRESCALERRATIO_MUL2;
    } else if (_frequency >= _min_frequ_ckpsc[HRTIM_PRESCALERRATIO_DIV1]) { // CKPSC = 5, resolution = 5.88ns at 170Mhz
        _hrtim_prescal = HRTIM_PRESCALERRATIO_DIV1;
    } else if (_frequency >= _min_frequ_ckpsc[HRTIM_PRESCALERRATIO_DIV2]) { // CKPSC = 6, resolution = 11.76ns at 170Mhz
        _hrtim_prescal = HRTIM_PRESCALERRATIO_DIV2;
    } else if (_frequency >= _min_frequ_ckpsc[HRTIM_PRESCALERRATIO_DIV4]) { // CKPSC = 7, resolution = 23.53ns at 170Mhz
        _hrtim_prescal = HRTIM_PRESCALERRATIO_DIV4;
    } else {
        error("PwmOutG4 ERROR: minimum frequency is %ldHz. Pin %d can't be initialized. Please use a normal Timer, or increase frequency to use HRTIM.\n", _min_frequ_ckpsc[HRTIM_PRESCALERRATIO_DIV4], _pin);
    }


    // Compute the right period
    _period = (uint32_t) ((((float)0xFFFF / (float)_frequency) * (float)_min_frequ_ckpsc[_hrtim_prescal]));

    // quick hack to check if another timer in rollover mode will have the same frequency, if not, then the period is not even.
    if ((_period / 2)*2 != _period)
        _period++; // make the _period even

    if(_rollover){
        _period = _period / 2;
    }

    // See table 214 of STM32G4 reference manual.
    switch (_hrtim_prescal) {
        case HRTIM_PRESCALERRATIO_MUL32:
            _duty_cycle_min = 0x0060;
            _duty_cycle_max = 0xFFDF;
            break;
        case HRTIM_PRESCALERRATIO_MUL16:
            _duty_cycle_min = 0x0030;
            _duty_cycle_max = 0xFFEF;
            break;
        case HRTIM_PRESCALERRATIO_MUL8:
            _duty_cycle_min = 0x0018;
            _duty_cycle_max = 0xFFF7;
            break;
        case HRTIM_PRESCALERRATIO_MUL4:
            _duty_cycle_min = 0x000C;
            _duty_cycle_max = 0xFFFB;
            break;
        case HRTIM_PRESCALERRATIO_MUL2:
            _duty_cycle_min = 0x0006;
            _duty_cycle_max = 0xFFFD;
            break;
        default:
            _duty_cycle_min = 0x0003;
            _duty_cycle_max = 0xFFFD;
    }

    // Be sure to not exceed the period of the timer
    if(_duty_cycle_max > _period){

        if(_rollover)
            _duty_cycle_max = _period - ((_duty_cycle_min / 3)*2); //because of rollover, two period of fHRTIM instead of only one.
        else
            _duty_cycle_max = _period - (_duty_cycle_min / 3); //divide by 3 because _duty_cycle_min is 3x period of fHRTIM (see table 214).

    }
}

void PwmOutG4::setupHRTIM1() {

    // Start HRTIM clock
    __HAL_RCC_HRTIM1_CLK_ENABLE();

    _hhrtim1.Instance = HRTIM1;
    _hhrtim1.Init.HRTIMInterruptResquests = HRTIM_IT_NONE;
    _hhrtim1.Init.SyncOptions = HRTIM_SYNCOPTION_NONE;
    if (HAL_HRTIM_Init(&_hhrtim1) != HAL_OK) {
        printf("Error while initializing HRTIM1.\n");
    }
    if (HAL_HRTIM_DLLCalibrationStart(&_hhrtim1, HRTIM_CALIBRATIONRATE_3) != HAL_OK) {
        printf("Error while calibrating HRTIM1 DLL.\n");
    }
    if (HAL_HRTIM_PollForDLLCalibration(&_hhrtim1, 10) != HAL_OK) {
        printf("Error while waiting for DLL calibration\n");
    }

    // Compute the minimum PWF frequency for each prescaler, following the current system clock. See table 213 of STM32G4 reference manual.
    for (int i=0 ; i<8 ; i++){
        _min_frequ_ckpsc[i] = ((uint64_t)SystemCoreClock / 100) * (32 * 100 / ((uint32_t)pow(2,i))) / 0xFFFF;
    }

}

void PwmOutG4::setupPWMTimer() {

    HRTIM_TimeBaseCfgTypeDef pTimeBaseCfg = {0};
    HRTIM_TimerCtlTypeDef pTimerCtl = {0};
    HRTIM_TimerCfgTypeDef pTimerCfg = {0};

    pTimeBaseCfg.Period = _period;
    pTimeBaseCfg.RepetitionCounter = 0x00;
    pTimeBaseCfg.PrescalerRatio = _hrtim_prescal;
    pTimeBaseCfg.Mode = HRTIM_MODE_CONTINUOUS;
    if (HAL_HRTIM_TimeBaseConfig(&_hhrtim1, _tim_idx, &pTimeBaseCfg) != HAL_OK) {
        printf("Error while configuring HRTIM1 timebase.\n");
    }

    if(_rollover)
        pTimerCtl.UpDownMode = HRTIM_TIMERUPDOWNMODE_UPDOWN;
    else
        pTimerCtl.UpDownMode = HRTIM_TIMERUPDOWNMODE_UP;

    pTimerCtl.GreaterCMP3 = HRTIM_TIMERGTCMP3_EQUAL; // always setup comparators 3 and 1 regarding the timer, just in case
    pTimerCtl.GreaterCMP1 = HRTIM_TIMERGTCMP1_EQUAL;
    pTimerCtl.DualChannelDacEnable = HRTIM_TIMER_DCDE_DISABLED;
    if (HAL_HRTIM_WaveformTimerControl(&_hhrtim1, _tim_idx, &pTimerCtl) != HAL_OK) {
        printf("Error while init HRTIM1 waveform.\n");
    }

    if(_rollover){
        if (HAL_HRTIM_RollOverModeConfig(&_hhrtim1, _tim_idx, HRTIM_TIM_FEROM_BOTH|HRTIM_TIM_BMROM_BOTH
                                                                             |HRTIM_TIM_ADROM_CREST|HRTIM_TIM_OUTROM_BOTH
                                                                             |HRTIM_TIM_ROM_BOTH) != HAL_OK) {
            printf("Error while setting up rollover mode.\n");
        }
    }

    pTimerCfg.InterruptRequests = HRTIM_TIM_IT_NONE;
    pTimerCfg.DMARequests = HRTIM_TIM_DMA_NONE;
    pTimerCfg.DMASrcAddress = 0x0000;
    pTimerCfg.DMADstAddress = 0x0000;
    pTimerCfg.DMASize = 0x1;
    pTimerCfg.HalfModeEnable = HRTIM_HALFMODE_DISABLED;
    pTimerCfg.InterleavedMode = HRTIM_INTERLEAVED_MODE_DISABLED;
    pTimerCfg.StartOnSync = HRTIM_SYNCSTART_DISABLED;
    pTimerCfg.ResetOnSync = HRTIM_SYNCRESET_DISABLED;
    pTimerCfg.DACSynchro = HRTIM_DACSYNC_NONE;
    pTimerCfg.PreloadEnable = HRTIM_PRELOAD_ENABLED; // cleaner
    pTimerCfg.UpdateGating = HRTIM_UPDATEGATING_INDEPENDENT;
    pTimerCfg.BurstMode = HRTIM_TIMERBURSTMODE_MAINTAINCLOCK;
    pTimerCfg.RepetitionUpdate = HRTIM_UPDATEONREPETITION_DISABLED; // Not necessary. See st cookbook.
    pTimerCfg.PushPull = HRTIM_TIMPUSHPULLMODE_DISABLED;
    pTimerCfg.FaultEnable = HRTIM_TIMFAULTENABLE_NONE;
    pTimerCfg.FaultLock = HRTIM_TIMFAULTLOCK_READWRITE;
    pTimerCfg.DeadTimeInsertion = HRTIM_TIMDEADTIMEINSERTION_DISABLED;
    pTimerCfg.DelayedProtectionMode = HRTIM_TIMER_D_E_DELAYEDPROTECTION_DISABLED; // always 0 regarding the timer.
    pTimerCfg.UpdateTrigger = HRTIM_TIMUPDATETRIGGER_NONE;
    pTimerCfg.ResetTrigger = HRTIM_TIMRESETTRIGGER_NONE;
    pTimerCfg.ResetUpdate = HRTIM_TIMUPDATEONRESET_ENABLED; // Needed to get the timer reboot at each cycle.
    pTimerCfg.ReSyncUpdate = HRTIM_TIMERESYNC_UPDATE_CONDITIONAL; // cleaner
    if (HAL_HRTIM_WaveformTimerConfig(&_hhrtim1, _tim_idx, &pTimerCfg) != HAL_OK) {
        printf("Error while configuring general HRTIM1 waveform.\n");
    }

}

void PwmOutG4::setupPWMOutput() {

    HRTIM_CompareCfgTypeDef pCompareCfg = {0};
    HRTIM_OutputCfgTypeDef pOutputCfg = {0};

    pCompareCfg.CompareValue = 0x0000;
    if (HAL_HRTIM_WaveformCompareConfig(&_hhrtim1, _tim_idx, _tim_cpr_unit, &pCompareCfg) !=
        HAL_OK) {
        printf("Error while configuring compare1 for HRTIM1 waveform.\n");
    }
    if (_inverted)
        pOutputCfg.Polarity = HRTIM_OUTPUTPOLARITY_LOW;
    else
        pOutputCfg.Polarity = HRTIM_OUTPUTPOLARITY_HIGH;

    if(_rollover)
        pOutputCfg.SetSource = HRTIM_OUTPUTSET_NONE;
    else
        pOutputCfg.SetSource = HRTIM_OUTPUTSET_TIMPER;

    pOutputCfg.ResetSource = _tim_cpr_reset;
    pOutputCfg.IdleMode = HRTIM_OUTPUTIDLEMODE_NONE;
    pOutputCfg.IdleLevel = HRTIM_OUTPUTIDLELEVEL_INACTIVE;
    pOutputCfg.FaultLevel = HRTIM_OUTPUTFAULTLEVEL_NONE;
    pOutputCfg.ChopperModeEnable = HRTIM_OUTPUTCHOPPERMODE_DISABLED;
    pOutputCfg.BurstModeEntryDelayed = HRTIM_OUTPUTBURSTMODEENTRY_REGULAR;
    if (HAL_HRTIM_WaveformOutputConfig(&_hhrtim1, _tim_idx, _tim_output, &pOutputCfg) != HAL_OK) {
        printf("Error while configuring TIMER HRTIM1 waveform output.\n");
    }

}

void PwmOutG4::setupGPIO() {


    // init gpio struct
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF13_HRTIM1;


    // can't use switch case, because gpio port is a type def structure
    if (_gpio_port == GPIOA)
        __HAL_RCC_GPIOA_CLK_ENABLE();

    if (_gpio_port == GPIOB)
        __HAL_RCC_GPIOB_CLK_ENABLE();

    if (_gpio_port == GPIOC)
        __HAL_RCC_GPIOC_CLK_ENABLE();

    if (_gpio_port == GPIOD)
        __HAL_RCC_GPIOD_CLK_ENABLE();

    if (_gpio_port == GPIOE)
        __HAL_RCC_GPIOE_CLK_ENABLE();

    if (_gpio_port == GPIOF)
        __HAL_RCC_GPIOF_CLK_ENABLE();

    if (_gpio_port == GPIOG)
        __HAL_RCC_GPIOG_CLK_ENABLE();

    // setup gpio output
    GPIO_InitStruct.Pin = _gpio_pin;
    HAL_GPIO_Init(_gpio_port, &GPIO_InitStruct);

}

void PwmOutG4::startPWM() {

    // Start HRTIM base Output. Needed for MBED, because of __HAL_HRTIM_ENABLE.
    HAL_HRTIM_SimpleBaseStart(&_hhrtim1, _tim_idx);

    if (HAL_HRTIM_WaveformOutputStart(&_hhrtim1, _tim_output) != HAL_OK) {
        printf("Error while starting %lu waveform output.\n", _tim_output);
    }

//    // Start HRTIM's TIMER (DOES NOT SEEM NECESSARY WHEN waveformOutputStart IS ALREADY EXECUTE)
//    if (HAL_HRTIM_WaveformCounterStart(&_hhrtim1, _tim_idx) != HAL_OK) {
//        printf("Error while starting %lu waveform counter\n", _tim_output);
//    }
}

void PwmOutG4::stopPWM() {

    if (HAL_HRTIM_WaveformOutputStop(&_hhrtim1, _tim_output) != HAL_OK) {
        printf("Error while stopping %lu waveform output.\n", _tim_output);
    }
}


void PwmOutG4::write(float pwm) {

    // Input Security. Should be between 0.0f and 1.0f like MBED.
    if (pwm > 1.0f)
        _pwm = 1.0f;
    else if (pwm < 0.0f)
        _pwm = 0.0f;
    else
        _pwm = pwm;

    // Duty cycle security. Consider MAX and MIN following the prescaler.
    // See setupFrequency() function.
    if (_pwm == 0.0f){
        // Special case, HRTIM can do null duty cycle, even if it is below minimum.
        _duty_cycle = 0x0000;
    } else {
        _duty_cycle = uint32_t((float)_period * _pwm);

        // See setupFrequency() function for the min max.
        if (_duty_cycle < _duty_cycle_min)
            _duty_cycle = _duty_cycle_min;
        else if (_duty_cycle > _duty_cycle_max)
            _duty_cycle = _duty_cycle_max;
    }

    // Setup comparator using HAL, resulting in PWM output.
    __HAL_HRTIM_SetCompare(&_hhrtim1, _tim_idx, _tim_cpr_unit, _duty_cycle);
}


// Quick HACK to sync different timers (PWM output) when they have the same frequency.
// Only work after starting both PWM.
void PwmOutG4::syncWith(PwmOutG4 *other) {

    // Don't sync if there are from the same timer (because there are already sync)
    if (_tim_idx == other->_tim_idx)
        return;

    if (HAL_HRTIM_WaveformOutputStop(&_hhrtim1, (_tim_output + other->_tim_output)) != HAL_OK) {
        printf("Error while stopping %lu et %lu waveform output.\n", _tim_output, other->_tim_output);
    }

    if (HAL_HRTIM_SoftwareReset(&_hhrtim1, (_tim_reset + other->_tim_reset)) != HAL_OK) {
        printf("Error while resetting %lu and %lu timers count.\n", _tim_idx, other->_tim_idx);
    }

    // Alternative to reset counter manually :
    // See https://www.st.com/resource/en/reference_manual/dm00093941-stm32f334xx-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf
    // part "21.5.40 HRTIM Control Register 2 (HRTIM_CR2)"
    //HRTIM1_COMMON->CR2 |= 0b00000000000000000001100000000000; // binary
    //HRTIM1_COMMON->CR2 |= 0x00001800; // hex

    if (HAL_HRTIM_WaveformOutputStart(&_hhrtim1, (_tim_output + other->_tim_output)) != HAL_OK) {
        printf("Error while starting %lu and %lu waveform output.\n", _tim_output, other->_tim_output);
    }

}

void PwmOutG4::syncWith(PwmOutG4 *other1, PwmOutG4 *other2) {

    if (HAL_HRTIM_WaveformOutputStop(&_hhrtim1, (_tim_output + other1->_tim_output + other2->_tim_output)) != HAL_OK) {
        printf("Error while stopping %lu, %lu and %lu waveform output.\n", _tim_output, other1->_tim_output, other2->_tim_output);
    }

    if (HAL_HRTIM_SoftwareReset(&_hhrtim1, (_tim_reset + other1->_tim_reset + other2->_tim_reset)) != HAL_OK) {
        printf("Error while resetting %lu, %lu and %lu timers count.\n", _tim_idx, other1->_tim_idx, other2->_tim_idx);
    }

    if (HAL_HRTIM_WaveformOutputStart(&_hhrtim1, (_tim_output + other1->_tim_output + other2->_tim_output)) != HAL_OK) {
        printf("Error while starting %lu, %lu and %lu waveform output.\n", _tim_output, other1->_tim_output, other2->_tim_output);
    }

}
