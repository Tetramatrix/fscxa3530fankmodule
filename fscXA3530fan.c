/*
    Fan control kernel module for FSC Amilo XA3530 computers
    Copyright (C) 2011-2014  Chi Hoang

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

#include "fscXA3530fan.h"

static char msg[MSG_BUF_LEN];		/* The message provided by the device when read */
static char *msg_read_ptr;		/* Read pointer to the message */
static bool update_message_flag = 1;	/* Message needs to be updated */
static int device_open_flag = 0;	/* Prevent multiple access to device if already open */
static char sfan_val = 0;		/* Last values written to SFAN register */
static uint8_t sfan_speed = 0;		/* Fan speed from 0x66 to 0x7e */
static char cmd_val = FAN_AUTO_CHAR;  	/* Last command issued */
static u8 sfan_temp;
static u8 sfan_temp_old = 0;

static int sloop = FAN_LOOP;
static int speed_switch = 0;

/* Kernel structures */
static struct class *device_class;
static struct cdev *char_device;
static struct device *gen_device;
static dev_t device_num;

/* Function pointers for file operations */
static struct file_operations fops = {
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.release = device_release
};
/*****************************************************************************/
/* Called on module load */
static int __init fanmodule_init(void)
{
	int return_code = 0;
	int init_stage = 0;

	return_code = alloc_chrdev_region(&device_num, 0, 1, DEVICE_NAME);
	if (return_code) goto fail;
	else init_stage = 1;

	char_device = cdev_alloc();
	if (!char_device)
	{
		return_code = -ENOMEM;
		goto fail;
	}
	else init_stage = 2;

	char_device->owner = THIS_MODULE;
	char_device->ops = &fops;
	return_code = cdev_add(char_device, device_num, 1);
	if (return_code) goto fail;
	else init_stage = 3;

	/* Creating a class allows  udev to pick up the module */
	device_class = class_create(THIS_MODULE, CLASS_NAME);
	if (!device_class)
	{
		return_code = -ENOMEM;
		goto fail;
	}
	else init_stage = 4;
	
	set_fan_software_mode();
	
	device_class->resume = fanmodule_resume;
	gen_device = device_create(device_class, NULL, device_num, "%s", DEV_ENTRY_NAME);
	if (!gen_device) goto fail;
	
	printk("Loaded FSC AMILO XA3530 fan control module\n");

	return SUCCESS;

fail:
	/* Report failure and clean up */
	switch (init_stage) {
		case 4: class_destroy(device_class);
		case 3: cdev_del(char_device);
		case 2:
		case 1: unregister_chrdev_region(device_num, 1);
	}
	return return_code;

}
/*****************************************************************************/
/* Called on module unload */
static void __exit fanmodule_exit(void)
{
	//printk ("cmd_val %c\n", cmd_val);
	
	//if (cmd_val == FAN_AUTO_CHAR)
	//{
		cancel_delayed_work(&work_object);     	/* no "new ones" */
		flush_workqueue(my_workqueue);  	/* wait till all "old ones" finished */
		destroy_workqueue(my_workqueue);	
	//}
	
	/* Return fan to hardware control */
	set_fan_auto_mode();

	/* Clean up */
	device_destroy(device_class, device_num);
	class_destroy(device_class);
	cdev_del(char_device);
	unregister_chrdev_region(device_num, 1);
	printk ("Unloaded FSC AMILO XA3530 fan control module.\n");

	return;
}
/*****************************************************************************/
/* Called when waking up from system sleep state */
static int fanmodule_resume(struct device * dev)
{
	/* Process last command again, since SFAN register  will have been cleared by system waking */
	char cmd_val_before_sleep = cmd_val;
#if(VERBOSE)
	printk("FSC AMILO XA3530 Fan Control Module woke up\n");
#endif
	cmd_val = 'O';
	process_cmd_char(cmd_val_before_sleep);
	return SUCCESS;

}
/*****************************************************************************/
/* Called when a process tries to open the device file, like "cat /dev/mycharfile" */
static int device_open(struct inode *inode, struct file *file)
{
	if (device_open_flag) return -EBUSY;

	/* Increment usage count */
	device_open_flag ++;
	try_module_get(THIS_MODULE);

	msg_read_ptr = msg; /* Reset read pointer to start of message */
	update_message_flag = 1;

#if(DEBUG_OUTPUT)
	printk ("FSC AMILO XA3530 Fan Control Module opened\n");
#endif

	return SUCCESS;
}
/*****************************************************************************/
/* Called when a process closes the device file */
static int device_release(struct inode *inode, struct file *file)
{
	/* Decrement the usage count */
	device_open_flag --;
	module_put(THIS_MODULE);

#if(DEBUG_OUTPUT)
	printk ("FSC AMILO XA3530 Fan Control Module closed\n");
#endif
	return SUCCESS;
}
/*****************************************************************************/
/* Update the message returned when the device is read */
static void update_message()
{
	u8 ec_rtmp, ec_sfan;
	char mode[16] = "On";
	char string[64] = "";
	int rpm, error = 0;
	update_message_flag = 0;
	
	/* Get data from embedded controller */
	error |= ec_read(EC_RTMP, &ec_rtmp);
	error |= ec_read(EC_SFAN, &ec_sfan);
	rpm = read_fan_speed();
	if (error || rpm < 0) {
		sprintf(msg, "FSC AMILO XA3530 Fan Fan Control:\n\tError reading from embedded controller\n");
		return;
	}

	/* Determine control mode */
	if (ec_sfan & 0x16 && rpm == 0) strcpy(mode, "Off");
	else if (ec_sfan & 0x16 && rpm > 0) strcpy(mode, "Software");
	else if (ec_sfan & 0x04) strcpy(mode, "Hardware");

	if (cmd_val == FAN_SOFTWARE_CHAR)
	{
		strcpy(string, "Fan control set to software control" );
	} else
	{
		strcpy(string, "Fan control set to hardware control" );
	}

	/* Set message */
	sprintf(msg, "FSC AMILO XA3530 Fan control:\n"
		      "\tCPU temperature: %d C\n"
		      "\tFan speed:       %d RPM\n"
		      "\tFan control:     %s\n"
		      "\tFan mode:        %s\n"
#if(DEBUG_OUTPUT)
		"\tSFAN Register:   %d\n"
		"\tLast SFAN Write: %d\n"
		"\tLast Command:    %c\n"
#endif
		, ec_rtmp, rpm, string, mode
#if(DEBUG_OUTPUT)
		, ec_sfan, sfan_val, cmd_val
#endif
		);

}
/*****************************************************************************/
/* Called when a process, which already opened the dev file, attempts to read from it */
static ssize_t device_read(struct file *filp,
			   char *buffer,
			   size_t length,
			   loff_t * offset)
{
	int bytes_read = 0;

	/* Update the message, if not already done since we opened the device */
	if (update_message_flag) update_message();

	/* If we're at the end of the message, return 0 signifying end of file */
	if (*msg_read_ptr == 0) return 0;

	/* Copy the message data into the user-space buffer */
	while (length && *msg_read_ptr)
	{
		put_user(*(msg_read_ptr++), buffer++);
		length--;
		bytes_read++;
	}

	/* Return the number of bytes put into the buffe */
	return bytes_read;

}
/*****************************************************************************/
/* control fan speed */
static void work_queue_func (struct work_struct *test)
{
	//u8 ec_rtmp;
	int error = 0;
	int jump = 0;
	int deltaT = 0;
	int remainder = 0;
	uint8_t speedstep = 0;
	uint8_t fanstep = 0;
	
	sloop--;
	//printk ( "Loop: %d\n", sloop);
	
	error |= ec_read(EC_RTMP, &sfan_temp);
	
	
	if ( ( sfan_temp < FAN_UPTHRESHOLD_TEMP && sloop < 0 ) ||
		( sfan_temp < FAN_UPTHRESHOLD_TEMP && sloop == FAN_LOOP ) ||
			 ( sfan_temp < FAN_UPTHRESHOLD_TEMP && speed_switch == 1 )	
	    )
	{
		speed_switch = 1;
		sloop = FAN_LOOP;     						// 20 * 10 sec
		printk("Temp %d°C: disabling fan\n", sfan_temp);
		set_fan_disabled();
		sfan_temp_old = sfan_temp;
		queue_delayed_work( my_workqueue, &work_object, FAN_JIFFIES_MS*HZ );
	
	} else if ( sfan_temp > FAN_CRITICAL_TEMP )
	{
		speed_switch = 0;
		printk("Temp %d°C: enable fan\n", sfan_temp);
		set_fan_critical();
		sfan_temp_old = sfan_temp;
		queue_delayed_work( my_workqueue, &work_object, 2*FAN_JIFFIES_MS*HZ );
	}
	else 
	{
		speed_switch = 0;
		
		jump = (FAN_MAXSPEED - FAN_MINSPEED) / (FAN_CRITICAL_TEMP - FAN_SAFE_TEMP);
		
		deltaT = sfan_temp - sfan_temp_old;
		remainder = do_div ( deltaT, sfan_temp );
		
		speedstep = FAN_MINSPEED + jump * (sfan_temp - FAN_SAFE_TEMP) * FAN_STEPS / FAN_MAXSPEED;
		
		if ( speedstep < 0 )
		{
			speedstep *= -1;
		}
		
		fanstep = FAN_MINSPEED_OFFSET - speedstep;
		
		if ( fanstep < FAN_MAXSPEED_OFFSET )
		{
			fanstep = FAN_MAXSPEED_OFFSET;
		} else if ( fanstep > FAN_MINSPEED_OFFSET )
		{
			fanstep = FAN_MINSPEED_OFFSET;
		}
		
		printk("Temp %d°C: fanstep %d: speedstep %d: jump %d: deltaT:%d remainder:%d\n", sfan_temp, fanstep, speedstep, jump, deltaT, remainder);
		set_fan_speed (fanstep);
		sfan_temp_old = sfan_temp;
		queue_delayed_work( my_workqueue, &work_object, 2*FAN_JIFFIES_MS*HZ );
	}
}
/*****************************************************************************/

/*****************************************************************************/
/* Read current fan speed from embedded controller */
static int read_fan_speed()
{
	int rpm;
	u8 ec_rpm;

	if (ec_read(EC_FRPM, &ec_rpm)) return -EFANCTL_ECERR;

	/* Calculate RPM (note: this calculation is purely guessed, but seems resonable) */
	if (ec_rpm) rpm = ( (~ec_rpm) & 0xff ) * 10;
	else rpm = 0;

	return rpm;
}
/*****************************************************************************/
/* Set fan to disabled, i.e. turn it off */
static void set_fan_disabled()
{
#if(VERBOSE)
	printk("Fan set to disabled\n");
#endif
	cmd_val = FAN_OFF_CHAR;
	sfan_val = 0x16;
	WMFN(sfan_val);
	// stop fan speed
	sfan_speed = 0xff;
	set_fan_speed(sfan_speed);
}
/*****************************************************************************/
/* Set fan to enable, i.e. turn it on */
static void set_fan_enable()
{
#if(VERBOSE)
	printk( "Fan speed set to enable\n" );
#endif
	cmd_val = FAN_ON_CHAR;
	sfan_val = 0x04;
	WMFN(sfan_val);
	// start with lowest possible speed
	sfan_speed = 0x7e;
	set_fan_speed(sfan_speed);
}
/*****************************************************************************/
static void set_fan_critical()
{
#if(VERBOSE)
	printk("Fan set to critcal\n");
#endif
	sfan_speed = 0x66;
	set_fan_speed(sfan_speed);
}
/*****************************************************************************/
/* Set fan to auto mode */
static void set_fan_auto_mode()
{
#if(VERBOSE)
	printk( "Fan speed set to auto mode\n" );
#endif
	cmd_val = FAN_AUTO_CHAR;
	sfan_val = 0x04;
	WMFN(sfan_val);
	// start with lowest possible speed
	sfan_speed = 0x7e;
	set_fan_speed(sfan_speed);
}
/*****************************************************************************/
/* Set fan to software control */
static void set_fan_software_mode()
{
#if(VERBOSE)
	printk("Fan speed set to software control\n");
#endif
	if (cmd_val != FAN_SOFTWARE_CHAR)
	{
		cmd_val = FAN_SOFTWARE_CHAR;
		my_workqueue = create_workqueue ( "XA3530FAN" ); 
		queue_delayed_work( my_workqueue, &work_object, FAN_JIFFIES_MS*HZ);
	}
	sfan_val = 0x16;
	WMFN(sfan_val);
	// start with lowest possible speed
	sfan_speed = 0x7e;
	set_fan_speed(sfan_speed);
}
/*****************************************************************************/
// waits for the status bit to clear, max 0x4000 tries
void WEIE()
{
	uint16_t Local0 = 0x4000;
	uint8_t Local1 = inb(EC_AEIC) & 0x02;
	while(Local0 != 0 && Local1 == 0x02)
	{
		Local1 = inb(EC_AEIC) & 0x02;
		Local0--;
	}
}
// sets the fan mode
void WMFN (uint8_t Arg0)
{
	WEIE();
	outb(0x81, EC_AEIC);
	WEIE();
	outb(0x93, EC_AEID);
	WEIE();
	outb(Arg0, EC_AEID);
}
// sets the fan speed
void set_fan_speed (uint8_t Arg0)
{
	WEIE();
	outb(0x81, EC_AEIC);
	WEIE();
	outb(0x94, EC_AEID);
	WEIE();
	outb(Arg0, EC_AEID);
}
/*****************************************************************************/
/* Attempt to process a command character, returning true if successful */
bool process_cmd_char(const char cmd_char)
{
	if (cmd_char == FAN_ON_CHAR)
	{
		set_fan_enable();
		return true;
	}
	else if (cmd_char == FAN_AUTO_CHAR)
	{
		set_fan_auto_mode();
		return true;
	}
	else if (cmd_char == FAN_SOFTWARE_CHAR)
	{
		set_fan_software_mode();
		return true;
	}
	else if (cmd_char == FAN_OFF_CHAR)
	{
		set_fan_disabled();
		return true;
	}
	return false;
}
/*****************************************************************************/
/*
 * Called when a process writes to dev file: echo "hi" > /dev/fanctl
 */
static ssize_t device_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	/* Consider only the last valid control character in the supplied data */
	int charIdx = len - 1;
	for (; charIdx >= 0; -- charIdx)
	{
		if (process_cmd_char(buff[charIdx])) charIdx = -1;
	}
	return len;
}
/*****************************************************************************/
module_init(fanmodule_init);
module_exit(fanmodule_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
