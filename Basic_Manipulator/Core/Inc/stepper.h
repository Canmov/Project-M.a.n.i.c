#ifndef __STEPPER_H
#define __STEPPER_H

#include "main.h"
#include "stdbool.h"

#define MIN_ANGLE	0.0F
#define MAX_ANGLE	180.0F
#define MIN_PWM	25.0F
#define MAX_PWM	125.0F

#define CLAW_OPEN (TIM4->CCR2 = 132) //значения для полного раскртытия/заркытия клешни
#define CLAW_CLOSE (TIM4->CCR2 = 22)

#define STEPS_PER_REV 6400 //кол-во шагов на оборот (устанавливается на драйвере)

#define GEAR_RATIO_0 6 // передаточое число редуктора
#define GEAR_RATIO_1 99
#define GEAR_RATIO_2 81

#define SHIFT_A 0 // Длины звеньев
#define A 148  
#define B 164
#define C 123.5
#define D 100

#define pi	3.14159265358979323846F	/* pi */

void take_object(int x, int y, int z);
void inv_kin(int x, int y, int z);
void stepper_init();
void Stepper_Angle(uint8_t Channel, float Angle);
void Stepper_Step(uint8_t Link, bool Direction, uint32_t Steps); 
void Servo_SetAngle(uint8_t channel, float Angle);
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

#endif /* __STEPPER_H */