/*
    Fan control kernel module for FSC Amilo XA3530 computers
    Copyright (C) 2011-2014 Chi Hoang

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _FSCXA3530FAN_H
#define _FSCXA3530FAN_H

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/acpi.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/uaccess.h>
#include <linux/jiffies.h>
#include <linux/timer.h>
#include <linux/workqueue.h>    /* We schedule tasks here */
#include <linux/sched.h>        /* We need to put ourselves to sleep 
                                   and wake up later */
                                   
/* Define string constants */
#define DRIVER_DESC		"FSC Amilo XA3530 Fan Controller"
#define DRIVER_AUTHOR		"Chi Hoang <info_at_chihoang.de>"
#define CLASS_NAME		"FSCXA3530_fan"
#define DEV_ENTRY_NAME		"fanctl"
#define DEVICE_NAME 		"fanctl"	/* Name as it appears in /proc/devices   */

/* Define memory locations in embedded controller */

// Read TMP
#define EC_RTMP			169
#define EC_SFAN			93
// RPM
#define EC_FRPM 		149

/* Define misc numeric constants */
#define TIMER_FREQ 10		                //secs
#define SUCCESS 		0
#define MSG_BUF_LEN 		512		/* Max length of the message from the device */
#define VERBOSE			0
#define DEBUG_OUTPUT		0

/* Define non-numeric command characters */
#define FAN_ON_CHAR		'X'
#define FAN_OFF_CHAR		'O'
#define FAN_AUTO_CHAR		'A'
#define FAN_SOFTWARE_CHAR	'S'
#define FAN_LOOP                10
#define FAN_JIFFIES_MS          10
#define FAN_UPTHRESHOLD_TEMP    85
#define FAN_SAFE_TEMP           70              /* optimum idle temperature, is used for fan noise, too */
#define FAN_CRITICAL_TEMP       95
#define FAN_MAXSPEED           100
#define FAN_MINSPEED             0
#define FAN_STEPS               25
#define FAN_MAXSPEED_OFFSET    102
#define FAN_MINSPEED_OFFSET    127  

/* ioctl definitions */
#define EFANCTL_ECERR	2

/* IO ports */
const uint16_t EC_AEIC = 0x066; // command register
const uint16_t EC_AEID = 0x062; // data register

/* External function prototypes - called by kernel */
static int __init fanmodule_init(void);
static void __exit fanmodule_exit(void);
static int fanmodule_resume(struct device * dev);
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);
struct workqueue_struct *my_workqueue;

/* Internal function prototypes */
static void update_message(void);
static int read_fan_speed(void);
static void work_queue_func(struct work_struct *test);
static void set_fan_enable(void);
static void set_fan_disabled(void);
static void set_fan_critical(void);
static void set_fan_auto_mode(void);
static void set_fan_software_mode(void);
static void set_fan_speed(uint8_t Arg0);
static void WMFN(uint8_t Arg0);
static void WEIE(void);
static bool process_cmd_char(const char cmd_char);
static DECLARE_DELAYED_WORK(work_object, work_queue_func);

#endif /* _FSCXA3530FAN_H */
