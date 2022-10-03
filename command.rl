/*
 * Parses LX200 protocol you must process this file with RAGEL compiler to get command.cpp file
 */

#define ADD_DIGIT(var,digit) var=var*10+digit-'0';
#define APPEND strcat(response,tmessage);
#define SYNC_MESSAGE "sync#"
//#define SYNC_MESSAGE "Coordinates     matched.        #"

#include <string.h>
#include <stdio.h>
#include "mount.h"
#include "misc.h"
#include <math.h>
#include "focus.h"
#include "piclevel.h"

char response [200];
char tmessage[50];
extern c_star st_now, st_target, st_current;
extern char volatile sync_target;
struct _telescope_
{   long dec_target,ra_target;
    long alt_target,az_target;
    long current_dec,current_ra;
    long lat,longitude,h_secs;
    int day,month,year,dayofyear;
    int hour,min,sec;

}
rmount;
extern long sdt_millis;
extern mount_t *telescope;
extern int  focuspeed;
extern int  focuspeed_low;
extern int focusmax;
void lxprintsite(void)
{
    sprintf(tmessage,"Site Name#");APPEND;
};


void set_cmd_exe(char cmd,long date)
{ int temp ;
    switch (cmd)
    {
    case 'r':
       if (telescope->mount_mode) rmount.ra_target=date;
	   else
	    telescope->azmotor->target=telescope->ra_target=date*SEC_TO_RAD*15.0;
        break;
    case 'd':
        if (telescope->mount_mode) rmount.dec_target=date;
		else
		{  telescope->dec_target=date*SEC_TO_RAD;
        if  (telescope->dec_target<0.0)
            telescope->altmotor->target=2*M_PI+telescope->dec_target;
        else
            telescope->altmotor->target=telescope->dec_target;}
        break;
    case 'a':
        rmount.alt_target=date;
        break;
    case 'z':
        rmount.az_target=date ;
        break;
    case 't':
        rmount.lat=date ;
        telescope->lat=date/3600.0;

        break;
    case 'g':
        rmount.longitude=date ;
		if (date <648000)
        telescope->longitude=-date/3600.0; else
		telescope->longitude=(1296000.0-date)/3600.0;

        break;
    case 'L' :
        rmount.h_secs=date;
        rmount.hour=date/3600;
        temp = (date % 3600);
        rmount.min = temp / 60;
        rmount.sec = temp % 60;
        //setclock (mount.year,mount.month,mount.day,mount.hour,mount.min,mount.sec,telescope->time_zone)
        setclock (22,8,01,14,6,12,telescope->time_zone);
        break;
    case 'S':
        break;

    }
}
void set_date( int day,int month,int year)
{   rmount.month=month-1;
    rmount.day=day;
    rmount.year=100+year;
    setclock (rmount.year,rmount.month,rmount.day,rmount.hour,rmount.min,rmount.sec,telescope->time_zone);
	  if (telescope->mount_mode == EQ) {
    sdt_init(telescope->longitude, telescope->time_zone);
    }
  else
  { telescope->is_tracking = FALSE;
    sync_target = TRUE;
    tak_init(telescope);
  	telescope->azmotor->targetspeed=0.0;
    telescope->altmotor->targetspeed=0.0;
  }
	sprintf(tmessage,"%cUpdating Planetary Data#     #",'1');APPEND;
}
void set_time( int hour,int min,int sec)
{
    rmount.min=min;
    rmount.hour=hour;
    rmount.sec=sec;
    sprintf(tmessage,"1");APPEND;
}

//----------------------------------------------------------------------------------------
long command( char *str )
{
    char *p = str, *pe = str + strlen( str );
    int cs;
    char stcmd;
    long deg=0;
    int min=0;
    int sec=0;
    int neg = 1;
    tmessage[0]=0;
    response[0]=0;
	int pulse=0;
	int focus_counter=0;

    %%{
        machine command;
        write data;
    }%%




    %%{
#Actions
        action getgrads {ADD_DIGIT(deg,fc); }
        action getmin {ADD_DIGIT(min,fc); }
        action getsec {ADD_DIGIT(sec,fc); }
		action getpulse{ADD_DIGIT(pulse,fc);}
		action getfocuscounter{ADD_DIGIT(focus_counter,fc);}
        action neg { neg=-1;}
        action dir {mount_move(telescope,stcmd);}
		action pulse_dir{pulse_guide(telescope,stcmd,pulse);}
        action Goto {if (telescope->mount_mode)
					{goto_ra_dec(telescope,rmount.ra_target*15.0*SEC_TO_RAD,rmount.dec_target*SEC_TO_RAD);}
					 else mount_slew(telescope);
					 sprintf(tmessage,"0");APPEND;}
        action stop {mount_stop(telescope,stcmd);}
        action rate {select_rate(telescope,stcmd); }
        action return_ra {if (telescope->mount_mode) lxprintra1(tmessage, st_current.ra);
							else mount_lxra_str(tmessage,telescope); APPEND;}
        action return_dec {if (telescope->mount_mode) lxprintde1(tmessage, st_current.dec);
							else mount_lxde_str(tmessage,telescope); APPEND;}
        action return_az { lxprintaz1(tmessage, st_current.az); APPEND;}
        action return_alt {lxprintde1(tmessage, st_current.alt); APPEND;}
        action return_ra_target { lxprintra1(tmessage, st_target.ra); APPEND;}
        action return_dec_target {lxprintde1(tmessage, st_target.dec); APPEND;}
        action return_date {lxprintdate1(tmessage);APPEND;}
        action return_site { lxprintsite();}
        action ok  {sprintf(tmessage,"1");APPEND;deg=sec=min=0;}
        action return_longitude {lxprintlong1(tmessage,telescope->longitude);APPEND;}
        action return_lat {lxprintlat1(tmessage,telescope->lat);APPEND;}
        action return_sid_time { ;}

        action sync {if (telescope->mount_mode)
						align_sync_all(telescope,rmount.ra_target,rmount.dec_target);
						else
						{sync_eq(telescope);telescope->altmotor->slewing= telescope->azmotor->slewing=FALSE;}
						sprintf(tmessage,"sync#");APPEND;
						}
        action rafrac {deg+=(fc-'0')*6;}
        action return_local_time { lxprinttime1(tmessage);APPEND;}
        action set_cmd_exec {set_cmd_exe(stcmd,(neg*(deg )));
                             sprintf(tmessage,"1");APPEND;deg=sec=min=0;
                            }
        action addmin {deg=deg*3600+min*60;}
        action addsec {deg+=sec;}
        action storecmd {stcmd=fc;}
        action setdate {set_date(min,deg,sec);}
        action return_align{if (telescope->mount_mode==ALTAZ) sprintf(tmessage,"A");else sprintf(tmessage,"P") ; APPEND; }
		action set_gmt_offset{ telescope->time_zone=deg;}
		action return_GMT_offset {lxprintGMT_offset(tmessage,telescope->time_zone );APPEND}
        action settime{set_time(deg,min,sec);}
		action fmove_in {gotofocuser(telescope->azmotor, 0, focuspeed_low);}
		action fmove_out { gotofocuser(telescope->azmotor, focusmax, focuspeed_low);}
		action fmove_rel {gotofocuser(telescope->azmotor, focus_counter, focuspeed_low);}
		action fmove_to { gotofocuser(telescope->azmotor, focus_counter, focuspeed_low);}
		action fstop { stopfocuser(telescope->azmotor);}
		action fsync_to{stopfocuser(telescope->azmotor);set_aux_counter(telescope->azmotor->id,focus_counter);}
		action fquery{sprintf(tmessage,"%05d#",telescope->azmotor->auxcounter);APPEND;}
# LX200  auxiliary terms syntax definitions
        sexmin =  ([0-5][0-9])$getmin@addmin ;
        sex= ([0-5][0-9] )$getsec@addsec (('.'digit{1,2}){,1});
		grad=([\+] |''|space | [\-]@neg) ((digit @getgrads){2,3});
        deg = grad punct sexmin (any  sex)? ;
        RA = ([0-2] digit) $getgrads   (':'|'/') sexmin ('.'digit@rafrac | (':'|'/') sex) ;

#Lx200 commands syntax definition
        Poll= 'G'( 'R'%return_ra |
                   'D'%return_dec|
                   'r'%return_ra_target |
                   'd'%return_dec_target |
                   'Z'%return_az |
                   'A'%return_alt |
                   'G'%return_GMT_offset |
                   'L'%return_local_time |
                   'S'%return_local_time|
                   'C'%return_date|
                   'M'%return_site|
                   'g'%return_longitude|
                   't'%return_lat);

        Move = 'M' (([nswe]@storecmd %dir) | ('S'%Goto)|('g'[nsew]@storecmd digit{4}$getpulse %pulse_dir));
        Rate = 'R' [CGMS]@storecmd (''|[0-4]) %rate;
		Timezone='G'(''|space)([\+] | [\-]@neg)((digit @getgrads){1,2}) ('.' digit)? %set_gmt_offset%ok;
        date ='C' (''|space)  (digit@getgrads){2} '/' (digit @getmin){2} '/' (digit @getsec){2}%setdate;
        time ='L' (''|space) (digit @getgrads){2}':'(digit@getmin){2} ':'(digit @getsec){2}%settime;
        Set='S'((([dazgt]@storecmd (''|space) deg ) |([rS]@storecmd (''|space) RA))%set_cmd_exec| Timezone |date | time) ;

        Sync = "CM"(''|'R')%sync;
        Stop ='Q' (''|[nsew])@storecmd %stop;
       	ACK = 0x06 @return_align;
		f_in = '-'%fmove_in;
		f_out = '+'%fmove_out;
		f_stop ='Q'%fstop;
		f_rel='P'([\+]|[\-]@neg)digit{5}$getfocuscounter %fmove_rel;
		f_sync='LS1'([\+]|[\-]@neg)digit{5}$getfocuscounter %fsync_to;
		f_query='p'%fquery;
		f_go='A'([\+]|[\-]@neg)digit{5}$getfocuscounter %fmove_to;
		
		
#focuser
		Focuser='F'(f_in|f_out|f_go|f_query|f_stop|f_sync|f_rel); 		
        main :=   ((ACK|''|'#')':' (Set | Move | Stop|Rate | Sync | Poll|Focuser) '#')* ;

# Initialize and execute.
        write init;
        write exec;
    }%%

//---------------------------------------------------------------------------------------------------------------------
    if ( cs < command_first_final )   return  neg;
		else return 0;//	fprintf( stderr, "LX command:  error\n" );

       
};



