# AVR_based_digital_timeTimer
AVR (Atmega128a) based digital timeTimer style firmware &amp; device

![image](https://github.com/user-attachments/assets/bad9397e-5d53-45f3-b06a-d81897df847c)

## Requirements 
- Microchip studio (Atmel studio 7.0)
- KD128-pro (atmega128a) board
- 1 piezo buzzer
- 1 variable register
- 8 button switch for External Interrupts

## TOdo
- [x] Specifications
- [x] Block Diagram
- [x] code
 


## feature
- mode select (start/pause, reset, setting/done, mute/beep) : external Interrupt
- time setting mode using variable resistor (00.00 ~ 60.00) : ADC conversion
- Sound (feedback beep / end sound on Alarm mode ) : PWM
- 7segment display
