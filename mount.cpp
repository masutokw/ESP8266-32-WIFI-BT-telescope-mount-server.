#include "mount.h"
#include "misc.h"

extern long sdt_millis;
extern c_star  st_now, st_target, st_current, st_1, st_2;
extern int  focuspeed;
extern int  focuspeed_low;
extern int focusmax;
#include <Ticker.h>
Ticker pulse_dec_tckr, pulse_ra_tckr;
char sel_flag;
char volatile sync_target = TRUE;//
mount_t* create_mount(void)

{
    int maxcounter = AZ_RED;
    int maxcounteralt = ALT_RED;
    mount_t *m;
    m = (mount_t*)malloc(sizeof(mount_t));
    //if (m) return NULL;
    m->azmotor = (motor_t*)malloc(sizeof(motor_t));
    m->altmotor = (motor_t*)malloc(sizeof(motor_t));
    m->track = 1;
    m->rate[3][0] = RATE_SLEW;
    m->rate[2][0] = RATE_FIND;
    m->rate[1][0] = RATE_CENTER;
    m->rate[0][0] = RATE_GUIDE;
    m->rate[3][1] = RATE_SLEW;
    m->rate[2][1] = RATE_FIND;
    m->rate[1][1] = RATE_CENTER;
    m->rate[0][1] = RATE_GUIDE;
    m->srate = 0;
    m->maxspeed[0] = (m->rate[3][0] *  SID_RATE_RAD);
    m->maxspeed[1] = (m->rate[3][1] *  SID_RATE_RAD);
    m->longitude = LOCAL_LONGITUDE;
    m->lat = LOCAL_LATITUDE;
    m->time_zone = TIME_ZONE;
    m->prescaler = 0.4;
    // init_motor( m->azmotor, AZ_ID, maxcounter, SID_RATE_RAD, m->prescaler, m->maxspeed[0], 400, 0);
    // init_motor( m->altmotor,  ALT_ID, maxcounteralt, 0, m->prescaler, m->maxspeed[1], 400, 0);
    m->is_tracking = TRUE;
    m->mount_mode = ALTAZ;
    //m->mount_mode = EQ;
    m->sync = FALSE;
    m->smode = 0;
    m->track_speed = SID_RATE_RAD;
    return m;
}

int  destroy_mount(mount_t* m)
{
    free(m->azmotor);
    free(m->altmotor);
    free(m);
}

void thread_motor(mount_t* m)
{
    speed_up_down(((mount_t*)m)->altmotor);
    speed_up_down(((mount_t*)m)->azmotor);

}
void thread_motor2(mount_t* m)
{
    speed_up_down(((mount_t*)m)->altmotor);
    speed_up_down(((mount_t*)m)->azmotor);
    if (sel_flag)
    {
        pollcounters( AZ_ID);
        sel_flag = false;
    }

}

void thread_counter(mount_t* mt1)
{
    double  s;
    double delta;
    double sgndelta;
    double speed;
    int aval = Serial.available();
    if (aval >= 9)
    {
        while (Serial.available() > 9) Serial.read();
        if (sel_flag)
        {
            readcounter(mt1->altmotor);
            //goto -------------------------------------------------------------------------
            if ( mt1->altmotor->slewing)
            {
                sgndelta = (sign(delta = mt1->altmotor->delta));
                if (fabs(delta) > (M_PI)) sgndelta = -sgndelta;

                if ( fabs(delta / (SEC_TO_RAD)) >= 1.0)
                {
                    speed = fmin(mt1->maxspeed[1], fabs(delta)) * sgndelta;
                    mt1->altmotor->targetspeed = -speed;
                }
                else
                {
                    mt1->altmotor->targetspeed = 0.0;
                    mt1->altmotor->slewing = 0;
                }
            }
        }
        else
        {
            readcounter(mt1->azmotor);
            sgndelta = sign (delta =  (mt1->azmotor->delta = mt1->azmotor->position - calc_Ra(mt1->azmotor->target, mt1->longitude)));
            //  sgndelta = sign (delta = mt1->track * (mt1->azmotor->delta = mt1->azmotor->position - calc_Ra(mt1->azmotor->target, mt1->longitude)));
            if ( mt1->azmotor->slewing)
            {
                //  sgndelta = sign (delta =mt1->azmotor->delta= mt1->azmotor->pos_angle - calc_Ra(mt1->azmotor->target, mt1->longitude));
                if (fabs(delta) > (M_PI)) sgndelta = -sgndelta;
                if ( fabs(delta / (SEC_TO_RAD)) > ARC_SEC_LMT)
                {
                    speed = fmin(mt1->maxspeed[0], fabs(delta)) * sgndelta;
                    mt1->azmotor->targetspeed = -(speed) + ( SID_RATE_RAD);
                }
                else
                {
                    mt1->azmotor->targetspeed =  mt1->track_speed ;// * mt1->track;
                    mt1->azmotor->slewing = 0;
                }
            }
        }

        while (Serial.available())Serial.read();
    };
    if (sel_flag) pollcounters(AZ_ID) ;
    else pollcounters(ALT_ID);
    sel_flag = !sel_flag;
}
int goto_ra_dec(mount_t *mt, double ra, double dec)
{
    mt->is_tracking = TRUE;
    st_target.ra = ra;
    st_target.dec = dec;

}

int sync_ra_dec(mount_t *mt)
{
    // one=FALSE;

    while (Serial.available()) Serial.read();
    st_current.timer_count = ((millis() - sdt_millis) / 1000.0); //chrono_read(&ti);
    st_current.dec = st_target.dec = mt->dec_target;
    st_current.ra = st_target.ra = mt->ra_target;
    to_alt_az(&st_current);

    setpositionf(mt->azmotor, st_current.az);

    if (st_current.alt >= 0.0)
        setpositionf(mt->altmotor, st_current.alt);
    else
        setpositionf(mt->altmotor, M_2PI + st_current.alt );
    mt->sync = FALSE;
}
int sync_eq(mount_t *mt)
{
    mt->altmotor->slewing = mt->azmotor->slewing = FALSE;

    eq_to_enc(&(mt->azmotor->target), &(mt->altmotor->target),
              mt->ra_target, mt->dec_target, get_pierside(mt));

    setpositionf(mt->altmotor, mt->altmotor->target);
    setpositionf( mt->azmotor, calc_Ra(mt->azmotor->target, mt->longitude));
    mt->altmotor->slewing = mt->azmotor->slewing = FALSE;
}

int mount_stop(mount_t *mt, char direction)
{
    char n = 0;
    char top = 200;
    mt->altmotor->slewing = mt->azmotor->slewing = FALSE;
    if (mt->mount_mode != EQ)
    {
        switch (direction)
        {
        case 'n':
        case 's':
            mt->altmotor->targetspeed = 0.00001;
            do
            {
                yield();
                delay(5);
                n++;
            }
            while ((n < top) && (fabs(mt->altmotor->current_speed) > 0.00001));
            break;


        case 'w':
        case 'e':
            mt->azmotor->targetspeed = 0.00001;
            do
            {
                yield();
                delay(5);
                n++;
            }
            while ((n < top) && (fabs(mt->azmotor->current_speed) > 0.00001));


            break;

        default:
            mt->altmotor->targetspeed = 0.0;

            break;
        };
        sync_target = TRUE;
        // mt->is_tracking = TRUE;

    }
    else
    {
        mt->altmotor->slewing = mt->azmotor->slewing = FALSE;
        switch (direction)
        {
        case'n':
        case's':
            if ((mt->srate) == 0) mt->altmotor->locked = 1;
            else mt->altmotor->locked = 0;
            mt->altmotor->targetspeed = 0.0;
            break;
        case 'w':
        case 'e':
            mt->azmotor->targetspeed = mt->track_speed ;//* mt->track;
            break;
        default:
            mt->altmotor->targetspeed = 0.0;
            mt->azmotor->targetspeed =  mt->track_speed ;//* mt->track;
            break;
        }
    }
}


void mount_move(mount_t *mt, char dir)
{
    mt->altmotor->slewing = mt->azmotor->slewing = FALSE;
    mt->is_tracking = FALSE;
    int srate = mt->srate;
    int invert = (get_pierside(mt)) ? -1 : 1;
    int  sid = (srate == 0) ? 1 : -1;
    if (mt->mount_mode != EQ) sid = 0;
    switch (dir)
    {
    case 'n':
        mt->altmotor->targetspeed =  SID_RATE_RAD * mt->rate[srate][1] * invert;
        break;
    case 's':
        mt->altmotor->targetspeed = - SID_RATE_RAD * mt->rate[srate][1] * invert;
        break;
    case 'w':
        mt->azmotor->targetspeed =  SID_RATE_RAD * (mt->rate[srate][0] + sid);
        break;
    case 'e':
        mt->azmotor->targetspeed = - SID_RATE_RAD * (mt->rate[srate][0] - sid);
        break;
    };


}
void pulse_stop_dec(mount_t *mt)
{
    mt->altmotor->slewing = FALSE;
    mt->altmotor->locked = 1;
    mt->altmotor->targetspeed = 0.0;
    //  pulse_dec_tckr.detach();
}
void pulse_stop_ra(mount_t *mt)
{
    mt->azmotor->slewing = FALSE;
    mt->azmotor->targetspeed =  mt->track_speed ;//* mt->track;
    // pulse_ra_tckr.detach();

}

void pulse_guide(mount_t *mt, char dir, int interval)
{
    mt->altmotor->slewing = mt->azmotor->slewing = FALSE;
    int srate = mt->srate;
    int invert = (get_pierside(mt)) ? -1 : 1;
    int  sid = (srate == 0) ? 1 : -1;
    switch (dir)
    {
    case 'n':
        mt->altmotor->targetspeed =  SID_RATE_RAD * mt->rate[0][1] * invert;
        pulse_dec_tckr.once_ms(interval, pulse_stop_dec, mt);
        break;
    case 's':
        mt->altmotor->targetspeed = - SID_RATE_RAD * mt->rate[0][1]  * invert;
        pulse_dec_tckr.once_ms(interval, pulse_stop_dec, mt);
        break;
    case 'w':
        mt->azmotor->targetspeed =  SID_RATE_RAD * (mt->rate[0][0] + sid) ;
        pulse_ra_tckr.once_ms(interval, pulse_stop_ra, mt);
        break;
    case 'e':
        mt->azmotor->targetspeed = - SID_RATE_RAD * (mt->rate[0][0] - sid);
        pulse_dec_tckr.once_ms(interval, pulse_stop_ra, mt);
        break;
    };

}

void select_rate(mount_t *mt, char dir)
{
    switch (dir)
    {
    case 'C':
        mt->srate = 1;
        break;
    case 'G':
        mt->srate = 0;
        break;
    case 'M':
        mt->srate = 2;
        break;
    case 'S':
        mt->srate = 3;
        break;


    };

}

int mount_slew(mount_t *mt)
{
    eq_to_enc(&(mt->azmotor->target), &(mt->altmotor->target),
              mt->ra_target, mt->dec_target, get_pierside(mt));
    mt->azmotor->slewing = mt->altmotor->slewing = true;
}

int get_pierside(mount_t *mt)
{

    return (((mt->altmotor->counter) > (mt->altmotor->maxcounter / 4 )) && ((mt->altmotor->counter) < (3 * mt->altmotor->maxcounter / 4 )));
}

void mount_lxde_str(char* message, mount_t *mt)

{
    double ang = mt->altmotor->position;
    if (ang > 1.5 * M_PI) ang = ang - (M_PI * 2.0)
                                    ;
    else if (ang > M_PI / 2.0) ang = M_PI - ang;

    int x = ang * RAD_TO_DEG * 3600.0;
    char c = '+';
    if (x < 0)
    {
        x = -x;
        c = '-';
    }
    int gra = x / 3600;
    int temp = (x % 3600);
    int min = temp / 60;
    int sec = temp % 60;
    sprintf(message, "%c%02d%c%02d:%02d#", c, gra, 225, min, sec);


};
void mount_lxra_str(char *message, mount_t *mt)
{

    double ang = calc_Ra(mt->azmotor->position, mt->longitude);
    if (get_pierside(mt))
    {
        if (ang < M_PI) ang += M_PI;
        else ang -= M_PI;
    }
    int seconds = ang * RAD_TO_DEG * 3600.0;
    int x = trunc (seconds) / 15.0;
    int rest = ((seconds % 15) * 2) / 3;
    rest %= 15;
    rest *= 10;
    int gra = x / 3600;
    int temp = (x % 3600);
    int min = temp / 60;
    int sec = temp % 60;
    sprintf(message, "%02d:%02d:%02d.%02d#", gra, min, sec, rest);
    //  sprintf(message, "%02d:%02d:%02d#", gra, min, sec);
    //sprintf(message, "%02d:%02d.%1d#", gra, min, rest);
};

int readconfig(mount_t *mt)
{
    int maxcounter, maxcounteralt, back_az, back_alt;
    double tmp, tmp2;
    File f = SPIFFS.open("/mount.config", "r");
    if (!f)
    {
        init_motor( mt->azmotor, AZ_ID, AZ_RED,  SID_RATE_RAD, mt->prescaler, mt->maxspeed[0], 0, 0);
        init_motor( mt->altmotor,  ALT_ID, ALT_RED, 0, mt->prescaler, mt->maxspeed[1], 0, 0);
        return -1;
    }
    String s = f.readStringUntil('\n');
    maxcounter = s.toInt();
    s = f.readStringUntil('\n');
    maxcounteralt = s.toInt();
    for (int j = 0; j < 2; j++)
        for (int n = 0; n < 4; n++)
        {
            s = f.readStringUntil('\n');
            mt->rate[n][j] = s.toFloat();
        };
    mt->srate = 0;
    mt->maxspeed[0] = (mt->rate[3][0] *  SID_RATE_RAD);
    mt->maxspeed[1] = (mt->rate[3][1] *  SID_RATE_RAD);
    s = f.readStringUntil('\n');
    mt->prescaler = s.toFloat();
    if ((mt->prescaler < 0.3) || (mt->prescaler > 2.0)) mt->prescaler = 0.4;
    s = f.readStringUntil('\n');
    mt->longitude = s.toFloat();
    s = f.readStringUntil('\n');
    mt->lat = s.toFloat();
    s = f.readStringUntil('\n');
    mt->time_zone = s.toInt();

    //f.close();

    s = f.readStringUntil('\n');
    focusmax = s.toInt();
    s = f.readStringUntil('\n');
    focuspeed_low = s.toInt();
    s = f.readStringUntil('\n');
    focuspeed = s.toInt();
    s = f.readStringUntil('\n');
    tmp = s.toFloat();
    s = f.readStringUntil('\n');
    tmp2 = s.toFloat();
    s = f.readStringUntil('\n');
    back_az = s.toInt();
    s = f.readStringUntil('\n');
    back_alt = s.toInt();
    s = f.readStringUntil('\n');
    mt->mount_mode = (mount_mode_t)s.toInt();
    s = f.readStringUntil('\n');
    mt->track = (s.toInt() > 0);
    set_track_speed(mt, s.toInt());
    init_motor( mt->azmotor, AZ_ID, maxcounter, 0, mt->prescaler, mt->maxspeed[0], tmp, back_az);
    init_motor( mt->altmotor,  ALT_ID, maxcounteralt, 0, mt->prescaler, mt->maxspeed[1], tmp2, back_alt);
    return 0;


}
void mount_track_off(mount_t *mt)

{
    mt->altmotor->slewing = mt->azmotor->slewing = mt->is_tracking = FALSE;
    mt->altmotor->targetspeed = 0.0;
    mt->azmotor->targetspeed = 0.0;
}
void mount_park(mount_t *mt)

{
    mt->altmotor->slewing = mt->azmotor->slewing = mt->is_tracking = FALSE;
    mt->altmotor->targetspeed = 0.0;
    mt->azmotor->targetspeed = 0.0;

    delay(100);
    save_counters(ALT_ID);
    delay(10);
    save_counters(AZ_ID);
    delay(10);
}

void mount_home_set(mount_t *mt)

{
    mt->altmotor->slewing = mt->azmotor->slewing = mt->is_tracking = FALSE;
    mt->altmotor->targetspeed = 0.0;
    mt->azmotor->targetspeed = 0.0;
    delay(100);
    switch (mt->mount_mode)
    {
    case ALTAZ:
        setpositionf(mt->azmotor, M_PI );
        delay(10);
        setpositionf(mt->altmotor, M_PI / 4 );
        break;
    case ALIGN:
        setpositionf(mt->azmotor, (3.0 * M_PI / 2.0) );
        delay(10);
        setpositionf(mt->altmotor, M_PI) ;
        break;
    case EQ:
        setpositionf(mt->azmotor, M_PI / 2 );
        delay(10);
        setpositionf(mt->altmotor, M_PI );
        break;
    }
    //   save_counters(ALT_ID);
    //  delay(10);
    //  save_counters(AZ_ID);
    //  delay(10);
}
void  meridianflip(mount_t *mt, int side)
{
    int  artemp, count, dectemp;
    if (side != get_pierside(mt))
    {
        mt->altmotor->slewing = mt->azmotor->slewing = FALSE;
        if  (mt->altmotor->counter <= ( mt->altmotor->maxcounter / 2))
            dectemp = (mt->altmotor->maxcounter / 2) - mt->altmotor->counter;
        else
            dectemp = (mt->altmotor->maxcounter + mt->altmotor->maxcounter / 2) - mt->altmotor->counter ;
        artemp = (mt->azmotor->counter + mt->azmotor->maxcounter / 2) % mt->azmotor->maxcounter;
        setposition(mt->altmotor, dectemp);
        setposition(mt->azmotor, artemp );
    }
}
void  tak_init(mount_t *mt)
{

    reset_transforms(0.0, 0.0, 0.0);
    double temp = sidereal_timeGMT (mt->longitude, mt->time_zone) * 15.0;
    sdt_millis = millis();

    if  (mt->mount_mode == ALTAZ)
    {

        set_star(&st_1, temp + 90.0, 0.0, 90.0, 0.0, 0);
        //   init_star(1, &st_now);
        set_star(&st_2, temp, mt->lat, 180.00, 89.99, 0);
        //   init_star(2, &st_now);

    }
    else if ((mt->mount_mode == ALIGN) || (mt->mount_mode == EQ))
    {
        double ra    ;
        set_star(&st_1,  temp, 0.0, 180.0, 0.0, 0);
        //   init_star(1, &st_now);
        ra = st_1.ra + M_PI / 2.0;
        if (ra < 0) ra += M_2PI;
        if (mt->lat >= 0.0)
            set_star(&st_2, ra * RAD_TO_DEG, 45, 90, 45, 0);
        else
            set_star(&st_2, ra * RAD_TO_DEG, -45, 270, 45, 0);
        //  init_star(2, &st_now);


    }
    compute_trasform(&st_1, &st_2);
    set_star(&st_now, temp, mt->lat, 0.0, 0.0, 0);
    to_alt_az(&st_now);
    //  is_aligned=0;
    //  is_slewing='0';
    //  counter_x=counter_y=0;

}


void track(mount_t *mt)
{
    double d_az_r, d_alt_r;

    int aval = Serial.available();
    if (aval >= 18)
    {
        while (Serial.available() > 18) Serial.read();
        readcounter_n(mt->altmotor);
        readcounter(mt->azmotor);
        st_target.timer_count = st_current.timer_count = ((millis() - sdt_millis) / 1000.0);
        st_current.az = mt->azmotor->position;
        st_current.alt = mt->altmotor->position;
        st_current.p_mode = st_target.p_mode = get_pierside(mt);

        //compute ecuatorial current equatorial values to be send out from LX200 protocol interface
        to_equatorial(&st_current);
        if (sync_target )
        {
            st_target.ra = mt->ra_target = st_current.ra;
            st_target.dec = mt->dec_target = st_current.dec;
            sync_target = FALSE;
            mt->is_tracking = TRUE;
        }

        if (mt->is_tracking)
        {
            //compute next alt/az mount values  for target next lap second
            st_target.timer_count += 1.0;
            to_alt_az(&st_target);
            //compute delta values :next values from actual values for desired target coordinates
            d_az_r = (st_target.az) - st_current.az;
            // if (fabs(d_az_r) > (M_PI)) d_az_r -= M_2PI;
            if (fabs(d_az_r) > (M_PI)) d_az_r -= (M_2PI * sign( d_az_r));
            d_alt_r = (st_target.alt) - st_current.alt;;
            if (fabs(d_alt_r) > (M_PI)) d_alt_r -= M_2PI;

            // Compute and set timer intervals for stepper  rates
            settargetspeed(mt->azmotor, d_az_r);
            settargetspeed(mt->altmotor, d_alt_r);

        }

    }
    while (Serial.available()) Serial.read();
    if (mt->sync) sync_ra_dec(mt);
    pollcounters( ALT_ID);
    sel_flag = true;

}

void align_sync_all(mount_t *mt, long ra, long dec)
{
    switch (mt->smode)
    {
    case 0:
        mt->altmotor->slewing = mt->azmotor->slewing = FALSE;
        mt->ra_target = ra * 15.0 * SEC_TO_RAD;
        mt->dec_target = dec * SEC_TO_RAD;
        mt->sync = TRUE;
        break;
    case 1:
        reset_transforms(0.0, 0.0, 0.0);
        set_star(&st_1, ra * (15.0 / 3600.0), dec / 3600.0, mt->azmotor->position * RAD_TO_DEG, RAD_TO_DEG * mt->altmotor->position,  ((millis() - sdt_millis) / 1000.0));
        // init_star(1, &st_1);
        break;
    case 2:
        set_star(&st_2, ra * (15.0 / 3600.0), dec / 3600.0, RAD_TO_DEG * mt->azmotor->position, RAD_TO_DEG * mt->altmotor->position,  ((millis() - sdt_millis) / 1000.0));
        // init_star(2, &st_2);
        mt->is_tracking = FALSE;
        sync_target = TRUE;
        compute_trasform(&st_1, &st_2);
        mt->is_tracking = TRUE;

        break;
    default:
        break;
    }

};


void set_track_speed(mount_t *mt, int index)
{
    if (index < 5) mt->track = index;
    else mt->track = 1;
    switch (mt->track)
    {
    case 0:
        mt->track_speed = 0.0;
        break;
    case 1:
        mt->track_speed = SID_RATE_RAD;
        break;
    case 2:
        mt->track_speed = SOLAR_RATE * SEC_TO_RAD;
        break;
    case 3:
        mt->track_speed = LUNAR_RATE * SEC_TO_RAD;
        break;
    case 4:
        mt->track_speed = KING_RATE * SEC_TO_RAD;
        break;
    default:
        mt->track_speed = SID_RATE_RAD;
        break;
    }
    mt->azmotor->targetspeed = mt->track_speed;
}
