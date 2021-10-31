#include <math.h>
#include "motor.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
extern int aux_count;
int sign(double t)
{
    if (t < 0.0) return -1;
    else if (t == 0.0 )return 0;
    else return 1;

}

void init_motor(motor_t* mt, char ref, int maxcounter, double spd, double tick, double maxspd, double accel,int back)
{
    mt->speed = 0;
    mt->targetspeed = spd;
    mt->position = 0;
    mt->timertick = tick * 1e-6;
    mt->maxcounter = maxcounter; //8000*6*180;//
    mt->resolution = (2.0 * M_PI) / mt->maxcounter; //radians
    if (accel > 0.0)mt->acceleration = accel * SEC_TO_RAD ;
    else mt->acceleration = 20e-4;
    mt->id = ref;
    mt->slewing = 0;
    mt->maxspeed = maxspd;
    set_motor_max_counter(ref, maxcounter);
    mt->backslash=back;
    setbackslash(mt,back);
}

void setspeed(motor_t* mt, double tspeed)
{
    int base, postscaler; //ispeed
    mt->current_speed = tspeed;
    base = 0;  //timer2 preload
    postscaler = 0; //PIC timer2 iterations
    if (tspeed != 0.0)
    {
        base = fabs((mt->resolution) / ( tspeed * (mt->timertick)));
        postscaler = base / 65535;
        if (postscaler > 0)
        {
            base = base / (postscaler + 1);
        }   ;
        postscaler++;
    }
    else base = 65535;
    motor_set_period (mt->id, 65535 - base, sign(tspeed)*postscaler);
}




void setpositionf(motor_t* mt, double pos)
{
    mt->position = pos;
    mt->counter = trunc(mt->position / mt->resolution);
    set_motor_counter(mt->id, mt->counter);
}

void setposition(motor_t* mt, int pos)
{
    set_motor_counter(mt->id, pos);
    mt->counter = pos;
    mt->position =  mt->resolution * pos;
}
void go_to(motor_t* mt, double position, double speed)
{
    mt->slewing = true;
}

int readcounter(motor_t* mt)
{
    int temp = readcounters(mt->id);
    if ((temp >= 0) && (temp <= mt->maxcounter))
    {
        mt->auxcounter = aux_count;
        mt->position = mt->resolution * (mt->counter = temp);
        mt->delta= mt->position-mt->target;
    }
    else return -1;
}
int readcounter_n(motor_t* mt)
{
    int temp = readcounters(mt->id);
    if ((temp >= 0) && (temp <= mt->maxcounter))
    {
        mt->auxcounter = aux_count;
        mt->counter = temp;
        if (temp > (mt->maxcounter / 2))
        {
            temp -= mt->maxcounter;
        }
        mt->position = mt->resolution * (temp);
        //    mt->delta= mt->position-mt->target;
    }
    else return -1;

}


void setmaxcounter(motor_t* M, int value)
{
    set_motor_max_counter(M->id, value);
    M->maxcounter = value;
}
void settarget(motor_t* mt, int pos)
{
    set_motor_target(mt->id, pos);
}

void speed_up_down(motor_t* mt)
{
    if  (mt->speed != mt->targetspeed)
    {
        if  (fabs(mt->targetspeed - mt->speed) < fabs(mt->acceleration))
        {
            mt->speed = mt->targetspeed;
        }
        if (mt->speed < mt->targetspeed) mt->speed = mt->speed + mt->acceleration;
        else if (mt->speed > mt->targetspeed)   mt->speed = mt->speed - mt->acceleration;
        setspeed(mt, mt->speed);
    }
}

void  setcounter(motor_t* mt, int count)
{
    set_motor_counter(mt->id, count);
}

void  loadconf(motor_t* mt, char* name) {}


void  savemotorcounter(motor_t* mt)
{
    save_counters (mt->id);
}

void settargetspeed(motor_t* mt, double tspeed)

{
    if (fabs(tspeed) <= mt->maxspeed) mt->targetspeed = tspeed;
    else mt->targetspeed = mt->maxspeed * sign (tspeed);
}

void setbackslash(motor_t* mt,int back)
{
    setmotorbackslash(mt->id,abs(back));
    set_motor_back_slash_mode(mt->id,(back>0)? 1:0);
}
