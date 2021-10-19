# PwmOutG4 Library
PwmOutG4 library for Mbed OS. Generate PWM using High Resolution Timers from STM32G4 family. 

This library tried to be compatible with PwmOut objects from MBED-OS.

## Requirements
### Hardware requirements
The following boards are required:
- Zest_Core_STM32G474VET to generate HRTIM PWM specified on STM32G4
- (optional)  Zest_Actuator_HalfBridges to generate power from a Zest Core



## Import using MBED-CLI
To add the library in your the project :
```shell
mbed add https://gitlab.com/catie_6tron/pwmoutg4.git
```

Alternatively:

- Create a `pwmoutg4.lib` file at the root of your project
- Open it and add this git link:
  `https://gitlab.com/catie_6tron/pwmoutg4.git`
- Save and update Mbed project.



## Import using PLATFORMIO

To add the library in the current project, open your `platformio.ini` and add this line in your main environment:

```shell
lib_deps =
	https://gitlab.com/catie_6tron/pwmoutg4.git
```

PIO will automatically download the library in the next run.

