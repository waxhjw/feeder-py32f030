#ifndef FEEDER_MOTOR_OFF_H
#define FEEDER_MOTOR_OFF_H

/* 可以在C全局变量初始化之前调用：不依赖RAM变量、延时、中断或时钟频率。
 * 只撤销电机驱动；本模块没有启动、反转或脉冲函数。
 */
void motor_off_hold(void);

#endif
