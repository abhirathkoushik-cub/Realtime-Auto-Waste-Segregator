#ifndef SERVO_H
#define SERVO_H

#define SERVO1_GPIO 0 //SERVO1_GPIO	0	GPIO17	Pin 11
#define SERVO2_GPIO 2 //SERVO2_GPIO	2	GPIO27	Pin 13

void init_servos();
void set_servo2_initial();
void sweep_servo_2();
void set_servo1_initial();
void sweep_servo_1();

#endif // SERVO_H