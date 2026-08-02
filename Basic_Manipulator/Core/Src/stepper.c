#include "stepper.h"
#include "stm32f1xx.h"
#include "stdbool.h"
#include "math.h"

volatile float Steps = 0;
volatile bool flag = 0;

float pos[3] = {90, 90, 90};
float prev[2] = {90, 0};

float t[3] = {0};

uint8_t btn_cnt;

void take_object (int x, int y, int z)
{
  CLAW_OPEN; // открываем хват
  inv_kin(x, y, z); // обратная кинематики для захвата
  HAL_Delay(1000); // ждём захвата 500 мс
  /*поднятие руки с объектом*/
  Stepper_Angle(1, 90); //поднялся
  Stepper_Angle(2, 90);
  Servo_SetAngle(0, 90);
  
  
  inv_kin(1, -170, 50); // перемещение объекта к баку
  
  CLAW_OPEN; // объект сброшен в бак

  /*возврат в начальную установку*/
  Stepper_Angle(2, 90); 
  Stepper_Angle(1, 90);
  Servo_SetAngle(0, 90);
  Stepper_Angle(0, 90); 
}

void inv_kin(int x, int y, int z)
{
	float alfa, betta, gamma, delta; // Выходные значения
	float L, P, c, b1, b2; // Промежуточные переменные

	alfa = atan2f(x, y) + pi/2; // расчёт угла нулевого звена поворта башни

    P = sqrt(pow(x, 2) + pow(y, 2)) - 5; // расчёт проекции на плоскость ХУ
	L = sqrt(pow(P, 2) + pow(A - D - z, 2)); // расчёт расстояния между первым и вторым звеньями
    
	/*поиск угла между первым и вторым звеньями*/
	c = acos((pow(B, 2) +  pow(C, 2) - pow(L, 2))  /  (2 * B * C));
    
	gamma = pi - c; // угол второго звена
    
	/*промежуточные углы для нахождения углов первого и третьего звеньев*/
	b1 = acos((pow(B, 2) + pow(L, 2) - pow(C, 2))  /  (2 * B * L));
	b2 = atan((z + D - A) / P);

	betta = b1 + b2; // угол первого звена

	delta = pi - b1 - b2 - c; // угол третьего звена
    
	/*перевод из радиан в углы*/
	alfa = alfa * 180 / pi;
	betta = betta * 180 / pi;
	gamma = gamma * 180 / pi;
	delta = delta * 180 / pi;
    
	/*установка двигателей по найденным углам*/
	Stepper_Angle(0, alfa);
	Servo_SetAngle(0, delta);
	Stepper_Angle(2, gamma);
	Stepper_Angle(1, betta);
	
	CLAW_CLOSE; // захват объекта
	
}

void stepper_init()
{
	/*установка захвата*/
	Servo_SetAngle(0,150);
	flag = 0;
	CLAW_OPEN;
    
	/*Вращение второго звена на 180 пока не заденет концевик*/
	Stepper_Step(2, 1, STEPS_PER_REV*GEAR_RATIO_2/2);
	pos[2] = 159.5; // позиция при касании концевика
	flag = 0; // разрешаем работу двигателя
	Stepper_Angle(2, 90);
    
	/*Вращение первого звена на 180 пока не заденет концевик*/
	Stepper_Step(1, 1, STEPS_PER_REV*GEAR_RATIO_1/2);
	pos[1] = 185.5; // позиция при касании концевика
	flag = 0; // разрешаем работу двигателя
	Stepper_Angle(1, 90); // перемещение в начальную установку
    
	/*Вращение нулевого звена на 180 пока не заденет концевик*/
	Stepper_Step(0, 0, STEPS_PER_REV*GEAR_RATIO_0/2);
	pos[0] = 98; // позиция при касании концевика против часовой стрелки
	if (flag != 1) // если концевик так и не был задет за 180 градусов
	{
		/*двигаемся в обратную сторону на 360 градусов*/
		Stepper_Step(0, 1, STEPS_PER_REV*GEAR_RATIO_0);
		pos[0] = 91.5; // позиция при касании концевика по часовой стрелке
	}
	flag = 0; // разрешаем работу двигателя
	Stepper_Angle(0, 90); // перемещение в начальную установку
}

void Stepper_Angle(uint8_t Link, float Angle) // принимаем номер звена и задающий для него угол
{
	/*Вспомогательные переменные*/ 
	int max_angle = 0, min_angle = 0; // для определения ограничений по углу
	uint8_t gear_ratio = 0;           // для определения передаточного числа
  	bool Direction = 0;               // для направления вращения

   /*Опреатор выбора макс. и мин. угла передвижения звена, передаточного числа редуктора 
    в соответствии с заданным номером звена*/ 
	switch (Link) 
	{                
	case 0:
		max_angle = 360, min_angle = 0, gear_ratio = GEAR_RATIO_0; 
		break;
	case 1:
		max_angle = 180, min_angle = 0, gear_ratio = GEAR_RATIO_1;
		break;
	case 2:
		max_angle = 180, min_angle = 0, gear_ratio = GEAR_RATIO_2;
		break;
	}

    /*Ограничим угол перемещения при превышении минимума или максимума 
	во избежание аварийных ситуаций */
	if (Angle < min_angle) Angle = min_angle;   
	if (Angle > max_angle) Angle = max_angle; 

	/*Если заданный угол больше текущего, то установка единицы на DIR*/
	if (pos[Link] < Angle) Direction = 1;
    
    /*Расчёт количества шагов из текущего положения в заданное*/
	Steps = fabs(pos[Link] - Angle) * ((float)STEPS_PER_REV * gear_ratio / (float)360);

    /*Обновляем текущее положения в соответствии с заданным*/
	pos[Link] = Angle; 
    
	/*Передаём кол-во необходимых шагов для перемещения заданному звену*/
    Stepper_Step(Link, Direction, Steps);
}

void Servo_SetAngle(uint8_t Link, float Angle)
{
    uint16_t value;
	Angle = Angle - 10;
	if(Angle < MIN_ANGLE) Angle = MIN_ANGLE;
	if(Angle > MAX_ANGLE) Angle = MAX_ANGLE;

    __IO uint32_t * regs= (uint32_t *) (TIM4_BASE + (0x34+Link*4));

    while(1)
	{
	   if (prev[Link] > Angle){ 
		prev[Link]--;		
	   }
	   else{
        prev[Link]++;		
	   }
	value = prev[Link] * ((float)MAX_PWM - (float)MIN_PWM) / ((float)MAX_ANGLE - MIN_ANGLE) + (float)MIN_PWM;
	HAL_Delay(20);
	*(__IO uint32_t *)regs = value;
	if(fabs(prev[Link]-Angle) < 1) break;
	}
}


void Stepper_Step(uint8_t Link, bool Direction, uint32_t Steps) //принимаем номер звена и кол-во шагов для него
{   
	/*Вспомогательные переменные*/ 
	uint32_t PUL_Port = 0; // для определения порта, на котором находится пин подачи импульса шага
	uint16_t PUL_Pin, DIR_Pin = 0; // для определения пина подачи импульса шага и направления
	uint8_t Speed = 0; // для определения скорости (длительности импульсов) "шагания"

    /*Оператор выбор порта и пина для подачи импульса шага на PUL, а также для определения скорости*/
	switch (Link)
	{
	case 0:
		PUL_Port = (uint32_t)GPIOA, PUL_Pin = GPIO_PIN_8, DIR_Pin = GPIO_PIN_15;
		Speed = 100;
		break;
	case 1:
		PUL_Port  = (uint32_t)GPIOA, PUL_Pin = GPIO_PIN_15, DIR_Pin = GPIO_PIN_3;
		Speed = 25;
		break;
	case 2:
		PUL_Port  =(uint32_t)GPIOB, PUL_Pin = GPIO_PIN_4, DIR_Pin = GPIO_PIN_5;
		Speed = 25;
		break;
	}
    
	HAL_GPIO_WritePin(GPIOB, DIR_Pin, Direction); // задаём направление
	
    /*Шагаем пока кол-во шагов не опустится до нуля или если флаг разрешения не равен нулю*/
	while (Steps != 0 && flag == 0) 
	{
		/*Делаем шаг*/
		HAL_GPIO_WritePin((GPIO_TypeDef *)PUL_Port, PUL_Pin, GPIO_PIN_SET); // Выставляем высокий уровень на PUL
		us_delay(Speed); //Держим уровень в соотвествии с заданной длительностью
		HAL_GPIO_WritePin((GPIO_TypeDef *)PUL_Port, PUL_Pin, GPIO_PIN_RESET); // Выставляем высокий уровень на PUL
		us_delay(Speed); //Держим уровень в соотвествии с заданной длительностью
		/*Вычитаем шаг*/
		--Steps;
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) // Определяем источник прерывания
{
    /*Если на входе уровень 0, то проверяем кол-во подтверждений*/
		if (!(HAL_GPIO_ReadPin(GPIOB, GPIO_Pin)))
		{
		/*Если меньше 10 подтверждений, то добавляем к счётчику подтверждение*/
			if (btn_cnt < 10)
			{
				btn_cnt++;
			}
		/*Иначе проверяем разрешение(флаг) и выполняем действия по прерыванию*/
			else
			{
				if (flag == 0)
				{
					HAL_GPIO_DeInit(GPIOB, GPIO_Pin); // отключение концевика
					flag = 1; // запрещаем работу двигателя
				}
			}
		}
		else // иначе, если уровень 1, отнимаем подтверждение из счётчика
		{
			if (btn_cnt > 0) // при условии, что значение счётчка больше нуля
			{
				btn_cnt--;
			}
		}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
        if(htim->Instance == TIM3) //check if the interrupt comes from TIM1
        {
            t[0] += 0.00005;
			t[1] += 0.00005;
			t[2] += 0.00005;
        }
}

