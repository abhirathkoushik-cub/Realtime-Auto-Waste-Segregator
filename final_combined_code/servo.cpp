#include <iostream>
#include <softPwm.h>
#include <unistd.h>
#include <wiringPi.h>
#include "servo.hpp"


void init_servos() {
    wiringPiSetup();

    if (softPwmCreate(SERVO1_GPIO, 0, 200) != 0) {
        std::cerr << "Failed to initialize servo 1\n";
        exit(1);
    }
    if (softPwmCreate(SERVO2_GPIO, 0, 200) != 0) {
        std::cerr << "Failed to initialize servo 2\n";
        exit(1);
    }
}

void set_servo2_initial()
{
    std::cout << "Setting Servo 2 at GPIO 27 to Initial Position"<<std::endl;
    softPwmWrite(SERVO2_GPIO, 17);
    sleep(1); 

    softPwmWrite(SERVO2_GPIO, 0);
}

void sweep_servo_2() 
{
    std::cout << "Sweeping Servo 2 on GPIO 27" << std::endl;
    for (int pulse = 17; pulse >= 9  ; --pulse) {
        softPwmWrite(SERVO2_GPIO, pulse);
        usleep(30000);
    }
    usleep(1000000);
    for (int pulse = 9; pulse <= 17; ++pulse) {
        softPwmWrite(SERVO2_GPIO, pulse);
        usleep(30000);
    }
    softPwmWrite(SERVO2_GPIO, 0);
}

void set_servo1_initial()
{
    std::cout << "Setting Servo 1 at GPIO 17 to Initial Position"<<std::endl;
    softPwmWrite(SERVO1_GPIO, 15);
    sleep(1); 

    softPwmWrite(SERVO1_GPIO, 0);
}

void sweep_servo_1() 
{
    std::cout << "Sweeping Servo 1 on GPIO 17" << std::endl;
    for (int pulse = 15; pulse <= 23; ++pulse) {
        softPwmWrite(SERVO1_GPIO, pulse);
        usleep(30000);
    }
    usleep(1000000);
    for (int pulse = 23; pulse >= 15  ; --pulse) {
        softPwmWrite(SERVO1_GPIO, pulse);
        usleep(30000);
    }
    softPwmWrite(SERVO1_GPIO, 0);
}
