/*
 * this small daemon interfaces with the waveshare pi power relay
 * board.
 * it provides a means to automatically set the three channels on or off
 * based on a calendar. This calendar is checked on a daily basis.
 * patches to extend this calendar with a systemd type of calendar
 * are welcome. Maybe if i ever need this i can do it.
 * It also allows you to configure a manual pin for each channel which
 * will turn on/off the relay. during the next trigger where we have to
 * invert its value, this is done. if you need to disable the calendar
 * function. Then time_on should be equal to time_off. (or not existing in the config file).
 * An idea to allow Timer like fucntion (upon manual trigger, or
 * calendar event exists. but it was never implemented.
 *
 * the daemon relies on wiringPi for its lowlevel hardware interactions
 * It is built with systemd in mind. creating a sysv init type daemon should not be too difficult to do. but i dont have a sysv system.
 *
 * For now only tested with archlinuxarm.
 *
 *  (c) Tomas Mezzadra.
 *  licensed under CC, MIT, or whatever license you find useful.
 *
 *  
 *
 *
 *
 */

#ifndef _AUTO_RELE_H
#define _AUTO_RELE_H

#include <iniparser.h>
#include "auto-rele-defaults.h"

#define STATE_EXIT_LOOP	0x00
#define STATE_WAITING	0x01
#define STATE_B_PRESSED 0x81
#define STATE_B_ON	0x91
#define STATE_B_REL	0xA1

#ifdef ACTIVE_LOW

#define RELE_ON 0
#define RELE_OFF 1

#else

#define RELE_ON 1
#define RELE_OFF 0

#endif

#define DEBOUNCE_MS	20

#define PID_FILE	"auto-rele.pid"

/* some variable declarations */

typedef struct channel channel_t;

/* returns a timestamp in milliseconds */
long timestamp_in_ms(void);

/* loads the configuration from the auto-rele.conf file
 * At some point we must free all the memory used
 * calling auto_rele_exit()
 */
int load_config(dictionary *config);
/* tests if the daemon runs already
 * 1=running
 * 0=not running
 */
pid_t daemon_running(void);

/* initializes the channel hardware if enabled
 * and sets the corresponding outputs
 * returns ch->enabled
 */
int init_rele(channel_t *ch);

/* receives a channel_t pointer and tries
 * to calculate its output value based on the time */
int calculate_output(channel_t *ch);


/* cleans up before exit */
void auto_rele_exit(dictionary *config);

/* debounce the manual input from the channel 
 * returns 1 if the bit is still pressed 
 */
int debounce(channel_t *ch);

/* if the channel value is 0. set it to 1 and viceversa.
 */
void flip_manual(channel_t *ch);

/* make the program a daemon */
void daemonize(void);

void update_output(channel_t *ch);
/* function called to run the calendar.
 * it is called by the signal_handler when the timer overruns
 */
void run_calendar(void);
/* during a config load, we dont know what the expected
 * output should be. here we test within calendar events
 */
void init_chvalue(channel_t *ch);

void info(const char* format, ...);
#endif
