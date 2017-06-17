#include <unistd.h>
#include <stdio.h>
#include <wiringPi.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "auto-rele.h"
#include "auto-rele-defaults.h"
#include <iniparser.h>
#include <time.h>
#include <systemd/sd-daemon.h>
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
#include <math.h>



struct channel {
	const char* name;
	int pin;
	int enable;
	int man_enable;
	int m_pin;
	int man_flag;
	const char *on_time; 
	const char *off_time;
	int on_min,off_min;
	int value; /* channel status */
	void (*isr) ();
};

/* We define our globals here */
channel_t ch1,ch2,ch3;
static volatile int stay_in_the_loop = 1;
static timer_t ticker=NULL;
int daemonized=0;

/* our ISR and signal handlers */

/* each ISR must:
 *    check with previous timestamp. if less than debounce time. ignore.
 *    debounce.
 *    If triggered: run the action and timestamp for further checking.
 */
void
ch1_isr(void) {
	static long prev_timestamp=0;
	long timestamp;

	timestamp=timestamp_in_ms();

	
	/* this is a phantom trigger */
	if(labs(timestamp - prev_timestamp) < DEBOUNCE_MS + 100)
		return; 
	
	if(debounce(&ch1)) {
		info("%s: ISR triggered\n\t prev_timestamp=%ld\n\ttimestamp=%ld\n",ch1.name,prev_timestamp,timestamp);
		flip_manual(&ch1);
		prev_timestamp = timestamp;
	}

}

void
ch2_isr(void) {
	static long prev_timestamp=0;
	long timestamp;

	timestamp=timestamp_in_ms();

	
	/* this is a phantom trigger */
	if(labs(timestamp - prev_timestamp) < DEBOUNCE_MS + 100)
		return; 
	
	if(debounce(&ch2)){
		info("%s: ISR triggered\n\t prev_timestamp=%ld\n\ttimestamp=%ld\n",ch2.name,prev_timestamp,timestamp);
		flip_manual(&ch2);
		prev_timestamp = timestamp;
	}

}

void
ch3_isr(void) {
	static long prev_timestamp=0;
	long timestamp;

	timestamp=timestamp_in_ms();

	
	/* this is a phantom trigger */
	if(labs(timestamp - prev_timestamp) < DEBOUNCE_MS + 100)
		return; 
	
	if(debounce(&ch3)) {
		info("%s: ISR triggered\n\t prev_timestamp=%ld\n\ttimestamp=%ld\n",ch3.name,prev_timestamp,timestamp);
		flip_manual(&ch3);
		prev_timestamp = timestamp;
	}

}

long
timestamp_in_ms (void)
{
    long            ms; // Milliseconds
    time_t          s;  // Seconds
    struct timespec spec;
    long	ret; 
    clock_gettime(CLOCK_MONOTONIC_RAW, &spec);
    s  = spec.tv_sec;
    ms = round(spec.tv_nsec / 1.0e6); // Convert nanoseconds to milliseconds

    ret = s*1000 + ms ;
    /* trying to go around the wraparound corner case*/
	return (ret + DEBOUNCE_MS+100 < DEBOUNCE_MS+100 ? DEBOUNCE_MS+100 : ret );
}

static void child_handler(int signum) {
	switch(signum) {
		case SIGALRM: run_calendar(); break;
		case SIGTERM: stay_in_the_loop = 0; break;
		case SIGINT: stay_in_the_loop = 0; break;
		case SIGHUP: stay_in_the_loop = 2; break;
	}
}

/* if Daemonized: notify systemd.
 * else print to stdout
 */
void
info(const char *format, ...) {
	
	va_list arg;

	va_start(arg, format);

	if(daemonized) {
		vfprintf(stderr,format, arg);
	}
	else {
		vprintf(format, arg);
	}

	va_end(arg);
}

/* Exit function. Should clean all before we exit */
void
auto_rele_exit(dictionary *config) {
	FILE *fp;
	if(config) { 
 		iniparser_freedict(config);
	}
	if (ticker) {
		timer_delete(ticker);
	}
	if( access( "/run/" PID_FILE, F_OK ) != -1 ) {
		    // file exists
			remove("/run/" PID_FILE);
		    } else {
		     // file doesn't exist
	}
}

/* reads the config file and populates the channel_t variables 
 * receives a dictionary pointer from the iniparser lib.
 * returns 0 on success, -1 if fails */
int
load_config(dictionary *config) {

	/* we need to disable the timer actions while we are here */
	    signal(SIGALRM,SIG_IGN);

	config = iniparser_load("/etc/auto-rele/auto-rele.conf");
	
	if (!config) {
		return -1;
	}

	ch1.name = iniparser_getstring(config,"channel 1:name",CHAN1_NAME);
	info("%s: ch1 name \n",ch1.name);
	ch2.name = iniparser_getstring(config,"channel 2:name",CHAN2_NAME);
	info("%s: ch2 name \n",ch2.name);
	ch3.name = iniparser_getstring(config,"channel 3:name",CHAN3_NAME);
	info("%s: ch3 name \n",ch3.name);

	ch1.pin = iniparser_getint(config,"channel 1:pin",CHAN1_PIN);
	ch2.pin = iniparser_getint(config,"channel 2:pin",CHAN2_PIN);
	ch3.pin = iniparser_getint(config,"channel 3:pin",CHAN3_PIN);

	ch1.enable = iniparser_getboolean(config,"channel 1:enable", CHAN1_EN);
	ch2.enable = iniparser_getboolean(config,"channel 2:enable", CHAN2_EN);
	ch3.enable = iniparser_getboolean(config,"channel 3:enable", CHAN3_EN);

	ch1.man_enable = iniparser_getboolean(config,"channel 1:manual_switch", CHAN1_MAN_EN);
	ch2.man_enable = iniparser_getboolean(config,"channel 2:manual_switch", CHAN2_MAN_EN);
	ch3.man_enable = iniparser_getboolean(config,"channel 3:manual_switch", CHAN3_MAN_EN);

	ch1.m_pin = iniparser_getint(config,"channel 1:switch_pin",CHAN1_MAN_PIN);
	ch2.m_pin = iniparser_getint(config,"channel 2:switch_pin",CHAN2_MAN_PIN);
	ch3.m_pin = iniparser_getint(config,"channel 3:switch_pin",CHAN3_MAN_PIN);
	
	ch1.on_time = iniparser_getstring(config,"channel 1:on_time",CHAN1_ON_TIME);
	ch2.on_time = iniparser_getstring(config,"channel 2:on_time",CHAN2_ON_TIME);
	ch3.on_time = iniparser_getstring(config,"channel 3:on_time",CHAN3_ON_TIME);

	ch1.off_time = iniparser_getstring(config,"channel 1:off_time",CHAN1_OFF_TIME);
	ch2.off_time = iniparser_getstring(config,"channel 2:off_time",CHAN2_OFF_TIME);
	ch3.off_time = iniparser_getstring(config,"channel 3:off_time",CHAN3_OFF_TIME);
	
	ch1.isr=&ch1_isr;
	ch2.isr=&ch2_isr;
	ch3.isr=&ch3_isr;

	ch1.on_min= (int) (strtol(ch1.on_time,NULL,10)*60 + strtol(&ch1.on_time[3],NULL,10));
	ch2.on_min= (int) (strtol(ch2.on_time,NULL,10)*60 + strtol(&ch2.on_time[3],NULL,10));
	ch3.on_min= (int) (strtol(ch3.on_time,NULL,10)*60 + strtol(&ch3.on_time[3],NULL,10));

	ch1.off_min= (int) (strtol(ch1.off_time,NULL,10)*60 + strtol(&ch1.off_time[3],NULL,10));
	ch2.off_min= (int) (strtol(ch2.off_time,NULL,10)*60 + strtol(&ch2.off_time[3],NULL,10));
	ch3.off_min= (int) (strtol(ch3.off_time,NULL,10)*60 + strtol(&ch3.off_time[3],NULL,10));

	if(ch1.enable)
		init_chvalue(&ch1);
	if(ch2.enable)
		init_chvalue(&ch2);
	if(ch3.enable)
		init_chvalue(&ch3);

	ch1.man_flag=RELE_OFF;
	ch2.man_flag=RELE_OFF;
	ch3.man_flag=RELE_OFF;

	signal(SIGALRM,child_handler);

	return 0;
}

void
init_chvalue(channel_t *ch) {
	time_t rawtime;
	struct tm *now;
	int now_min;

	rawtime = time(NULL);
	now = localtime(&rawtime);
	now_min=60*now->tm_hour+now->tm_min;

	if(ch->on_min < ch->off_min) {
		if(ch->on_min <= now_min && ch->off_min > now_min)
			ch->value=RELE_ON;
		else
			ch->value=RELE_OFF;
	}
	else if(ch->on_min > ch->off_min) {
		if(ch->off_min <= now_min && ch->on_min > now_min)
			ch->value=RELE_OFF;
		else
			ch->value=RELE_ON;
	}
	else
		ch->value=RELE_OFF;


}

/* tests if the daemon exists 
 * return 1 if daemon runs
 * return 0 if it does not run */

pid_t
daemon_running(void) {
	FILE *fp;
	pid_t pid = 0;
	char *line = NULL;
	char *endptr;
	size_t len =0;

	if( access( "/run/" PID_FILE, F_OK ) != -1 ) 
	{	
		fp=fopen("/run/" PID_FILE,"r");
		if (fp == NULL) {
			return -1;
		}
		getline(&line, &len, fp);
		errno = 0;    /* To distinguish success/failure after call */
		pid = strtol(line, &endptr, 10);
	 /* Check for various possible errors */
		 if ((errno == ERANGE && (pid == LONG_MAX || pid == LONG_MIN)) || (errno != 0 && pid == 0)) {
			info("failed to read Pid from running daemon");
			return -1;
		 }
                if (endptr == line) {
			return -1;
		}

			 /* If we got here, strtol() successfully parsed a number */
		if(line) free(line);
		fclose(fp);
		
		return pid;
	}
	else
		return 0;
}

/* initialize the hardware with the channel info from the config
 * file 
 */
int
init_rele(channel_t *ch) {

	/* disable the timer actions while we init */
	signal(SIGALRM,SIG_IGN);

	if(ch->enable) {
		/* do something */
			info("%s: enabled\n",ch->name);
		pinMode(ch->pin,OUTPUT);
		info("%s: Writing %d on pin %d\n",ch->name,ch->value,ch->pin);
		digitalWrite(ch->pin, ch->value);
		if(ch->man_enable) { 
			info("%s: Manual pin is %d\n",ch->name,ch->m_pin);
			if(ch->m_pin != -1){
			#ifdef ACTIVE_LOW
				pullUpDnControl(ch->m_pin, PUD_UP);
				wiringPiISR(ch->m_pin, INT_EDGE_FALLING,ch->isr);
			#else
				pullUpDnControl(ch->m_pin, PUD_DOWN);
				wiringPiISR(ch->m_pin, INT_EDGE_RISING,ch->isr);
			#endif
			}
		}
	}	

	signal(SIGALRM,child_handler);
	return ch->enable;
}


/* receives a channel_t pointer and tries to calculeate
 * its expected value 
 * if -1: then we should not change the value*/
int
calculate_output(channel_t *ch) {
	if (!ch->enable) 
		return -1;

	time_t rawtime;
	struct tm *now;
	int now_min;

	/* what i need from *now:
	 int tm_min;         minutes, range 0 to 59           
	 int tm_hour;        hours, range 0 to 23             */

	rawtime = time(NULL);
	now = localtime(&rawtime);
	
	now_min=60*now->tm_hour+now->tm_min;
	
	// when both are equal. the automation is disabled.
	if (ch->off_min == ch->on_min) { 
		return -1;
	}

	/* if the rele was set/cleared manually. we have to consider the situation where
	 * the output is already on and the time to set it on is now or viceversa. in this case.
	 * we dont do anything.
	 * in the case that the manual flag was set, but we have to set/clear because we are the set/clear position
	 * we clear the manual flag and return the corresponding value.
	 * it is complicated and the process can probably be simplified a bit more
	 */
	if(ch->man_flag) {
		if(ch->value == RELE_ON) {
			if(ch->off_min == now_min) {
				ch->man_flag = RELE_OFF;
				return RELE_OFF;
			}
			if(ch->on_min == now_min)
				return -1;
		}
		else {
			if(ch->on_min == now_min) {
				ch->man_flag = RELE_OFF;
				return RELE_ON;
			}
			if(ch->on_min==now_min)
				return -1;
		}
	}


	if(ch->off_min == now_min) {
			return RELE_OFF;
	}
	if (ch->on_min == now_min) {
			return RELE_ON;
	}
	// we are inbetween on/off states. value stays.
	return -1;

}
/* receives a channel_t * and debounces.
 * this f. is called after a ISR
 * ret 0 if failed
 * ret 1 if button still on.
 */
int
debounce(channel_t *ch) {
	delay(DEBOUNCE_MS);
	int i=digitalRead(ch->m_pin);
#ifdef ACTIVE_LOW
	return i==0 ? 1:0;
#else
	return i==1 ? 1:0;
#endif
}

/* receives a channel_t * and flips de output.
 */
void
flip_manual(channel_t *ch) {

	switch(ch->value) {
		case RELE_OFF:
			ch->value=RELE_ON;
			break;
		case RELE_ON:
			ch->value=RELE_OFF;
			break;
	}
	info ("%s: Change output from %d --> %d\n",ch->name, ch->value == RELE_OFF ? RELE_ON:RELE_OFF, ch->value);
	digitalWrite(ch->pin,ch->value);
	return;
}


void
update_output(channel_t *ch) {
	if (!ch->enable) 
		return ;
	int i=calculate_output(ch);
	if(i == -1)
		return ;
	if(ch->value != i) {
		flip_manual(ch);
	}

	return;

}

void
set_timer(int secs) {

	struct itimerspec new_value; 

	new_value.it_value.tv_sec=secs;
	new_value.it_interval.tv_sec=secs; //freerunning. triggers every sec.


	    new_value.it_value.tv_nsec = 0L;
	    new_value.it_interval.tv_nsec = 0L;



	/* timer which when triggers, sends SIGALRM signal*/
//	timer_create(CLOCK_REALTIME,NULL,&ticker); 


	    /* i need to figure out if this fixes the ntp fuckup when time is updated */
	timer_create(CLOCK_MONOTONIC,NULL,&ticker);
	
	timer_settime(ticker,0,&new_value,NULL);

}
/* function called by the SIGALRM signal handler.
 * it runs what we want when the timer overruns.
 * typically once every sec.
 */
void
run_calendar(void) {
     update_output(&ch1);
     update_output(&ch2);
     update_output(&ch3);
}

/* main program.
 * things todo
 * - parse cmd line arguments
 * - drop privileges code
 * - finish manual interrupt and figure how i want it to work
 */
int
main(int argc, char *argv[]) {

	pid_t pid;
	dictionary *config=NULL;
	FILE *fp;
	/*  before we run, we init or test if daemon is running */
	pid = daemon_running();
	if( pid != 0) {
		printf("Daemon running...on pid = %i\n",pid);
		exit(EXIT_SUCCESS);
	}
	
	fp = fopen("/run/" PID_FILE,"w");
	if(!fp) {
		info("failed to open pidfile\n");
		exit(EXIT_FAILURE);
	}

	pid = getpid();
	fprintf(fp,"%i\n",pid);
	fclose(fp);

	if(argc > 1)
		if(!strncmp(argv[1],"-D",2))
			daemonized=1; //daemonize();

	/* Trap signals that we expect to recieve */
	signal(SIGALRM,child_handler);
	signal(SIGHUP,child_handler);
	signal(SIGTERM,child_handler);
	signal(SIGINT,child_handler);

	
	wiringPiSetupGpio();

	info("Rele is on when output = %i\n",RELE_ON);

	/* populate the structs with info from the config file */
	if( load_config(config) ) {
		auto_rele_exit(config);
		exit(EXIT_FAILURE);
	}


	init_rele(&ch1);
	init_rele(&ch2);
	init_rele(&ch3);

	set_timer(1); //we set the calendar timer to 1sec.

	int i;

	if(daemonized) sd_notify(0, "READY=1");

	/* at this point, we start the daemon task*/
	while(stay_in_the_loop) {
		i=stay_in_the_loop; /* i dont want my value changing in the switch */
		switch(i) {

			case 2: 
		/* we received SIGHUP. reload config */
				
				     signal(SIGALRM,SIG_IGN);
				     //sd_notify(0, "RELOADING=1");
				iniparser_freedict(config);   // XXX TODO fix what happens if pin number change
				if( load_config(config) ) {

					auto_rele_exit(config);
					exit(EXIT_FAILURE);
				}
				init_rele(&ch1);
				init_rele(&ch2);
				init_rele(&ch3);

				stay_in_the_loop = 1;
			//Done reloading config. we are ready.	
				//sd_notify(0, "READY=1");
				signal(SIGALRM,&child_handler);
				break;
			default:
				delay(1000);
		}

	}

	if(daemonized) sd_notify(0, "STOPPING=1");

	info("Received SIGTERM...exiting..\n");
	auto_rele_exit(config);
	
	exit(EXIT_SUCCESS);

}
