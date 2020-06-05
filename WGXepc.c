
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <sys/errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <machine/cpufunc.h>
#include <machine/sysarch.h>
#include <kenv.h>

/* W83627 control registers */

/*
 *	You must set these - there is no sane way to probe for this board.
 *	You can use w83627=x to set these now.
 */

#define EFER		0x2e	/* Extended Function Enable Registers */
#define EFIR		0x2e	/* Ext Funct Index Reg (same as EFER) */
#define EFDR		0x2f	/* Extended Func Data Register */
#define	LDR	    	0x07	/* Logical device register */
#define bank_select	0x4e	/* W83627 Bank Select Register*/
#define sensor2_hb	0x50	/* W83627 Temperature Sensor 2 on Bank1 High Byte*/
#define sensor2_lb	0x51	/* W83627 Temperature Sensor 2 on Bank1 Low Byte*/
 
#define io		0x2e	/* standard SuperIO config address */
#define def_xe_siobase	0x290	/* The default SuperIO base address on the X-e box */
//#define siobase	0x295	/* standard SuperIO data address */
//#define siobase2	0xa05	/* XTM5 SuperIO data address */
#define gpiobase	0x480	/* standard ICH7, ICH6 and ICH5 address */
#define gpiobase2	0x4080	/* gpiobase for ICH2 in X-core model */
#define gpiobase3	0x1c00	/* gpiobase for C226 in M400/500 model */
#define parallel_port	0x378 	/* parallel port address in the firebox */

/* Program History
 *
 * Version 1.0 of this program is not significant other than it adds 64bit capability
 * Version 1.1 fixed a bug in get_w83627_addr_port left the chip in extended function mode
 *	 	       Added a function to check the value of the Superio base address and reset it
 *		       if neccessary on the X-e box. Testing has shown that it can change value
 *		       rendering it unreadabel at any address
 * Version 1.2 Added M400/500 support
 * Version 1.3 Added XTM8 support
 * Version 1.4 Added XTM800/1500 detection
 *             Added M370/470/570/670 support 
 * Version 1.5 Added M270 support
 */


/*

lcdproc code starts here

*/


/* Read a byte from port */
static inline int port_in (unsigned short port) {
        return inb(port);
}

/* Write a byte 'val' to port */
static inline void port_out (unsigned short port, unsigned char val) {
        outb(port,val);
}


/* Get access to a specific port */
static inline int port_access (unsigned short port) {
	static FILE *port_access_handle = NULL;
	
	if (port_access_handle
		|| (port_access_handle = fopen("/dev/io", "rw")) != NULL) {
			return (0); /* Success */
		}
	else {
		return (-1); /* Failure */
		};
	
	return -1;
	}
	
/* Close access to a specific port */
static inline int port_deny (unsigned short port) {
	/* Can't close /dev/io... */
	return 0;
	}
	
/* Get access to multiple ports at once */
static inline int port_access_multiple (unsigned short port, unsigned short count) {
 	return port_access(port); /* /dev/io gives you access to all ports. */
	}
	
/* Close access to multiple ports at once */
static inline int port_deny_multiple (unsigned short port, unsigned short count) {
	/* Can't close /dev/io... */
	return 0;
	}

/*

lcdproc code stops here

*/

/*

Functions for interacting with the Winbond W83627 SuperIO chip 

*/

/* Function to set W83627 in extended function mode */
static void w83627_enter_ext_mode(void)
{
	int retval = 0;  
	retval = port_access(EFER);
 	 if (retval==0)
 	 {
 		port_out(EFER, 0x87);		/* Enter extended function mode */
 		port_out(EFER, 0x87);		/* Do it twice !! */
		// printf("Entered Extended Function Mode\n");
		retval = port_deny(EFER);
    		if (retval==0)
    		{}
    		else
    		{
     		printf("close fail\n");
    		}
  	  }
	 else
  	  {
   	  printf("open fail\n");
  	  }
}



/* Function to set W83627 in normal mode */
static void w83627_leave_ext_mode(void)
{
	int retval = 0;  
	retval = port_access(EFER);
 	 if (retval==0)
 	 {
 		port_out(EFER, 0xAA);		/* Leave extended function mode */
 		// printf("Left Extended Function Mode\n");
		retval = port_deny(EFER);
    		if (retval==0)
    		{}
    		else
    		{
     		printf("close fail\n");
    		}
  	  }
	 else
  	  {
   	  printf("open fail\n");
  	  }
}

/* Function to get config registers from w83627 */

int get_w83627_config(int reg1)
{
  int val1 = 0;
  int retval = 0;  
 
  retval = port_access_multiple(EFIR,2);
  if (retval==0)
  {
	port_out(EFIR, reg1);
	val1 = port_in(EFDR);	
    retval = port_deny_multiple(EFIR,2);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
  return val1;
}

/* Function to set config registers on w83627 */

void set_w83627_config(int reg1, int val1)
{
  int retval = 0;  
 
  retval = port_access_multiple(EFIR,2);
  if (retval==0)
  {
	port_out(EFIR, reg1);
	port_out((EFDR), val1);	
	// printf("Setting %x to %x\n",reg1, val1);
    retval = port_deny_multiple(EFIR,2);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
}

/* Function to get W83627 address port */
int get_w83627_addr_port(void)
{
	int sio_addr_port = 0x0;
	int hi_byte = 0x0;
	int lo_byte = 0x0; 
 
	w83627_enter_ext_mode();		/* Enter extended function mode */
	set_w83627_config(LDR, 0x0b);		/* Set logical device register to B */
	hi_byte = get_w83627_config(0x60);	/* Read SIO base address high byte */
	lo_byte = get_w83627_config(0x61);	/* Read SIO base address low byte */
	sio_addr_port = ((hi_byte * 0x100) + lo_byte + 0x05);	/* Calculate SIO data address */
	//printf("SuperIO address port is %x\n",sio_addr_port);	
	w83627_leave_ext_mode();		/* Leave extended function mode */

return sio_addr_port;
}

/* Function to set W83627 base address */
void set_w83627_base_addr(int sio_port)
{
	int hi_byte = 0x0;
	int lo_byte = 0x0; 
	hi_byte = (sio_port / 0x100);		/* Calculate high byte */
	lo_byte = (sio_port - (hi_byte * 0x100));	/* Calculate low byte */
	//printf("Calculated high byte is %x, low byte is %x\n", hi_byte, lo_byte);
 
	w83627_enter_ext_mode();
	set_w83627_config(LDR, 0x0b);		/* Set logical device register to B */
	set_w83627_config(0x60, hi_byte);	/* Set SIO base address high byte */
	set_w83627_config(0x61, lo_byte);	/* Set SIO base address low byte */
	printf("SuperIO base address set to %x\n",sio_port);	
	w83627_leave_ext_mode();
}

/* Function to check W83627 base address */
void check_siobase(void)
{
	if ((get_w83627_addr_port() - 0x05) != def_xe_siobase)
		{
		printf("SuperIO data address has changed! Resetting to default value (0x290)\n");
		set_w83627_base_addr(def_xe_siobase);
		}
	else
		{
		//printf("All is well!\n");
		}
}

/* Function to get data from w83627 */
int getw83627(int reg1)
{
  int val1 = 0;
  int retval = 0;  
  int siobase = get_w83627_addr_port();

  retval = port_access_multiple(siobase,2);
  if (retval==0)
  {
	port_out(siobase, reg1);
	val1 = port_in(siobase+1);	
    retval = port_deny_multiple(siobase,2);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
  return val1;
}

/* Function to set data on w83627 */

void setw83627(int reg1,int val1)
{
  int retval = 0; 
  int siobase = get_w83627_addr_port(); 
 
  retval = port_access_multiple(siobase,2);
  if (retval==0)
  {
	port_out(siobase, reg1);
	port_out((siobase+1), val1);	
    retval = port_deny_multiple(siobase,2);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
}



/*

Functions for interacting with the ITE IT8718F SuperIO chip 

*/

/* Function to set IT87 in MBPnP mode */
static void it87_enter_mbpnp_mode(void)
{
	int retval = 0;  
	retval = port_access(EFER);
 	 if (retval==0)
 	 {
 		port_out(EFER, 0x87);
 		port_out(EFER, 0x01);
 		port_out(EFER, 0x055);
 		port_out(EFER, 0x055);
		// printf("Entered MBPnP Mode\n");
		retval = port_deny(EFER);
    		if (retval==0)
    		{}
    		else
    		{
     		printf("close fail\n");
    		}
  	  }
	 else
  	  {
   	  printf("open fail\n");
  	  }
}



/* Function to set IT87 in normal mode */
static void it87_leave_mbpnp_mode(void)
{
	int retval = 0;  
     retval = port_access_multiple(EFER,2);
 	 if (retval==0)
 	 {
 		port_out(EFER, 0x02);		/* Leave extended function mode */
 		port_out(EFDR, 0x02); 
		// printf("Left MBPnP Mode\n");
		retval = port_deny_multiple(EFER,2);
    		if (retval==0)
    		{}
    		else
    		{
     		printf("close fail\n");
    		}
  	  }
	 else
  	  {
   	  printf("open fail\n");
  	  }
}

/* Function to get config registers from IT87 */

int get_it87_config(int reg1)
{
  int val1 = 0;
  int retval = 0;  
 
  retval = port_access_multiple(EFIR,2);
  if (retval==0)
  {
	port_out(EFIR, reg1);
	val1 = port_in(EFDR);	
    retval = port_deny_multiple(EFIR,2);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
  return val1;
}

/* Function to set config registers on IT87 */

void set_it87_config(int reg1, int val1)
{
  int retval = 0;  
 
  retval = port_access_multiple(EFIR,2);
  if (retval==0)
  {
	port_out(EFIR, reg1);
	port_out((EFDR), val1);	
	// printf("Setting %x to %x\n",reg1, val1);
    retval = port_deny_multiple(EFIR,2);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
}

/* Function to get IT87 address port */
int get_it87_addr_port(void)
{
	int sio_addr_port = 0x0;
	int hi_byte = 0x0;
	int lo_byte = 0x0; 
 
	it87_enter_mbpnp_mode();		/* Enter MNPnP mode */
	set_it87_config(LDR, 0x04);		/* Set logical device register to B */
	hi_byte = get_it87_config(0x60);	/* Read SIO base address high byte */
	lo_byte = get_it87_config(0x61);	/* Read SIO base address low byte */
	sio_addr_port = ((hi_byte * 0x100) + lo_byte + 0x05);	/* Calculate SIO data address */
	//printf("SuperIO address port is %x\n",sio_addr_port);	
	it87_leave_mbpnp_mode();		/* Leave MBPnP mode */

return sio_addr_port;
}


/* Function to get data from it87 */
int get_it87(int reg1)
{
  int val1 = 0;
  int retval = 0;  
  int siobase = get_it87_addr_port();

  retval = port_access_multiple(siobase,2);
  if (retval==0)
  {
	port_out(siobase, reg1);
	val1 = port_in(siobase+1);	
    retval = port_deny_multiple(siobase,2);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
  return val1;
}

/* Function to set data on it87 */

void set_it87(int reg1,int val1)
{
  int retval = 0; 
  int siobase = get_it87_addr_port(); 
 
  retval = port_access_multiple(siobase,2);
  if (retval==0)
  {
	port_out(siobase, reg1);
	port_out((siobase+1), val1);	
    retval = port_deny_multiple(siobase,2);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
}



/* Function to set gpio led register on ICH6 for Watchguard Xe box*/

void setledXe(int val1)
{
  int retval = 0;  
 
  retval = port_access(gpiobase+0x0f);
  if (retval==0)
  {
	port_out((gpiobase+0x0f),val1);	
    retval = port_deny(gpiobase+0x0f);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
}

/* Function to set gpio flash register on ICH6 for Watchguard Xe box*/

void setflashXe(int val1)
{
  int retval = 0;  
 
  retval = port_access(gpiobase+0x1b);
  if (retval==0)
  {
	port_out((gpiobase+0x1b),val1);	
    retval = port_deny(gpiobase+0x1b);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
}

/* Function to set gpio led register on ICH5 for Watchguard X-Peak box*/

void setledXp(int val1)
{
  int retval = 0;  
 
  retval = port_access(gpiobase+0x39);
  if (retval==0)
  {
	port_out((gpiobase+0x39),val1);	
    retval = port_deny(gpiobase+0x39);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
}

/* Function to set gpio led register on ICH2 for Watchguard X-Core box*/

void setledXc(int val1, int val2)
{
  int retval = 0;  
 
  retval = port_access(gpiobase2+0x0f);
  if (retval==0)
  {
	port_out((gpiobase2+0x0f),val1);	
    retval = port_deny(gpiobase2+0x0f);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
  retval = port_access(gpiobase2+0x0e);
  if (retval==0)
  {
	port_out((gpiobase2+0x0e),val2);	
    retval = port_deny(gpiobase2+0x0e);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }

}

/* Function to set the SDECLCD backlight*/

void setbacklight(int val1)
{
  int retval = 0;  
 
  retval = port_access(parallel_port+0x02);
  if (retval==0)
  {
	port_out((parallel_port+0x02),val1);	
    retval = port_deny(parallel_port+0x02);
    if (retval==0)
    {}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }
}

/* Function to get cpu temperature value*/
int getcputemp(void)
{
	int temp_hb = 0;
	int temp_lb = 0;
	setw83627(bank_select, 0x1);		/* Select bank 1 */
	temp_hb = getw83627(sensor2_hb);	/* Read Sensor 2 high byte register */
	temp_lb = getw83627(sensor2_lb);	/* Read Sensor 2 low byte register */
	setw83627(bank_select, 0x0);		/* Reset to bank 0 */
	return temp_hb;				/* Return only high byte, 1 degree resolution is fine */
}

/* Function to set the IT87 fan control at sane defaults*/
void default_XTM8_fan(void)
{
    if ((get_it87(0x61)) == 0x3c){		/* Has the fans been setup yet? */
    set_it87(0x63, 0x38);		/* Set CPU fans minimum PWM value */
    set_it87(0x6b, 0x38);		/* Set System fan minimum PWM value */
    set_it87(0x61, 0x28);		/* Set CPU start fan slope start temp 40C*/
    set_it87(0x69, 0x23);		/* Set System start fan slope start temp 35C*/
    set_it87(0x64, 0x08);		/* Set temp/PWM slope to 1 */
    set_it87(0x6c, 0x08);		/*  */
    set_it87(0x65, 0x62);		/* Set hysteresis to 2 */
    set_it87(0x6d, 0x62);		/*  */
    printf("IT87 Fanctl set to sane defaults\n");
    }
}

/* Function to set the configure the XTM8 LED GPIOs correctly*/
void config_XTM8_led(void)
{
    it87_enter_mbpnp_mode();
    set_it87_config(LDR, 0x07);             /* Set logical device register to 7 */
    if ((get_it87_config(0x27)) ==0)		/* Is GPIO3 still disabled? */
            {
			set_it87_config(0x27,0x03);	/* Set pins as GPIO, GP30 and GP31 */
			set_it87_config(0xba,0x03);	/* Enable internal pull-up */
			set_it87_config(0xca,0x03);	/* Set as output */
			set_it87_config(0xf8,0x18);	/* Set GPIO blink 1 to pin 19 */
			set_it87_config(0xf9,0x01);	/* Set GPIO blink 1 always low (bit 0) */
			set_it87_config(0xfa,0x19);	/* Set GPIO blink 2 to pin 18 */
			set_it87_config(0xfb,0x01);	/* Set GPIO blink 2 always low (bit 0) */			
            printf("IT87 GPIO pins configured\n");
            }
    it87_leave_mbpnp_mode();
}

 
/* Function to display correct program usage */
void usage(void)
{
printf("WGXepc Version 1.5 5/6/2020 stephenw10\n");
printf("WGXepc can accept two arguments:\n");
printf(" -f (CPU fan) will return the current and minimum fan speed or if followed\n");
printf("    by a number in hex, 00-FF, will set it.\n");
printf(" -f2 (System fan) will return the current and minimum fan speed or if followed\n");
printf("    by a number in hex, 00-FF, will set it.\n");
printf(" -l (led) will set the arm/disarm led state to the second argument:\n");
printf("    red, green, red_flash, green_flash, red_flash_fast, green_flash_fast, off\n");
printf(" -b (backlight) will set the lcd backlight to the second argument:\n");
printf("    on or off. Do not use with LCD driver.\n");
printf(" -t (temperature) shows the current CPU temperature reported by the\n");
printf("    SuperIO chip. X-e box only.\n");
printf("Not all functions are supported by all models\n");
}

/* function to determine which Firebox model we are using */
/* 1=X-Core, 2=X-Peak, 3=X-E, 4=XTM5 */

int getmodel(void)
{
int retval = 0;
char product[20];  

/* Look for unique X-e registers */ 
  retval = port_access(0x480);
  if (retval==0)
  {
	if (port_in(0x480)==0x81)
		{
		printf("Found Firebox X-E\n");
		return 3;
		}
    retval = port_deny(0x480);
    if (retval==0){}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }

/* Look for unique X-peak registers */ 
  retval = port_access(0x482);
  if (retval==0)
  {
	if (port_in(0x482)==0xa0)
		{
		printf("Found Firebox X-Peak\n");
		return 2;
		}
    retval = port_deny(0x482);
    if (retval==0){}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }

/* Look for unique X-core registers */ 
  retval = port_access(0x4082);
  if (retval==0)
  {
	if (port_in(0x4082)==0x20)
		{
		printf("Found Firebox X-Core\n");
		return 1;
		}
    retval = port_deny(0x4082);
    if (retval==0){}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }

/* Look for unique XTM5 registers */ 
  retval = port_access(0x480);
  if (retval==0)
  {
	if (port_in(0x480)==0xc1)
		{
		printf("Found Firebox XTM5\n");
		return 4;
		}
    retval = port_deny(0x480);
    if (retval==0){}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }

/* Look for unique XTM8 registers */ 
  retval = port_access(0x480);
  if (retval==0)
  {
	if (port_in(0x480)==0xc3)
		{
		printf("Found Firebox XTM8\n");
		return 5;
		}
    retval = port_deny(0x480);
    if (retval==0){}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }

/* Look for unique M400/500 registers */ 
  retval = port_access(0x1c00);
  if (retval==0)
  {
	if (port_in(0x1c00)==0xc3)
		{
		printf("Found Firebox M400/500\n");
		return 6;
		}
    retval = port_deny(0x1c00);
    if (retval==0){}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }

/* Look for unique XTM800/1500 registers */ 
  retval = port_access(0x500);
  if (retval==0)
  {
	if (port_in(0x500)==0xc7)
		{
		printf("Found Firebox XTM800/1500\n");
		return 7;
		}
    retval = port_deny(0x500);
    if (retval==0){}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }

/* Look for M370/470/570/670 product ID */ 

  if (kenv(KENV_GET, "smbios.system.product", product, sizeof(product)) > 0) 
     {
        //printf("Product is %s\n",product);
        if (strcmp(product,"NCB-4210WG")==0)
            {
            printf("Found Firebox M370/470/570/670.\n");
            return 8;
            }           
     }

/* Look for unique M270 registers */ 
  retval = port_access_multiple(0x80,8);
  if (retval==0)
  {
	if ((port_in(0x80)==0xaa)&&(port_in(0x84)==0x00)&&(port_in(0x88)==0x00))
		{
		printf("Found Firebox M270\n");
		return 9;
		}
    retval = port_deny_multiple(0x80,8);
    if (retval==0){}
    else
    {
     printf("close fail\n");
    }
  }
  else
  {
   printf("open fail\n");
  }

/* Check for VBox ;) */
  if (kenv(KENV_GET, "smbios.system.product", product, sizeof(product)) > 0) 
     {
        //printf("Product is %s\n",product);
        if (strcmp(product,"VirtualBox")==0)
            {
            printf("Found VirtualBox.\n");
            return 42;
            }           
     }
  
return 0;
}

/*
main function 
sets PWM and GPIO registers on SuperIO chip and ICH chip.
*/

int main(int argc, char *argv[])
{
  int fanspeed = 0x00;
  int fanspeed_min = 0x00;
  int cpu_temp = 0x00;
  int system_temp = 0x00;
  int reg_test = 0x00;
  int model;
  
  model = getmodel();
  
if (argc ==1) /*Check that correct number of arguments have been given */
	{usage();
	return 0;
	}

switch (model)
{

case 9:		// M270
	
if (strcmp(argv[1],"-b")==0) 
    {
	printf("The M270 has no LCD\n");
	return 0;	
	}
else	
if (strcmp(argv[1],"-f")==0) /* The fan uses the CPUfan output on Bank2 */
    {
	if (argc==2)  /* Are we looking for the fan speed or setting it */
	{
    setw83627(bank_select,0x02); /* Set Bank2 */
	fanspeed = getw83627(0x09);	/* Current system fan speed register */
	fanspeed_min = getw83627(0x27); /* T1 fan speed */
	printf("Current fanspeed is %x, minimum fanspeed is %x\n",fanspeed ,fanspeed_min);
	}
	else
	{
    setw83627(bank_select,0x02); /* Set Bank2 */
	setw83627(0x21,0x1e);        /* Set T1 point at 30C */
	setw83627(0x22,0x2d);        /* Set T2 point at 45C */
	fanspeed_min = strtol(argv[2],NULL,16); /*Convert argument to hex integer*/
	setw83627(0x27,fanspeed_min);
	printf("Minimum fanspeed set to %x at 30°C or less\n",fanspeed_min);
	} 
	}
else	
if (strcmp(argv[1],"-l")==0)
	{
			if (argc ==2) /*Check that correct number of arguments have been given */
			{
			usage();
			return 0;
			}
                                        /* The NCT6776F in the M270 is compatible with the W83627 */
			w83627_enter_ext_mode();	/* Enter extended function mode */
			set_w83627_config(LDR,0x07);		/* Set logial device register to device 7, the LED is on GPIO7 bits 2 and 3*/
			
		if (strcmp(argv[2],"red")==0)
			{
			set_w83627_config(0xe1,0xf9);		/* Set bit 3 high */
			}
	else	if (strcmp(argv[2],"green")==0)
			{
			set_w83627_config(0xe1,0xf5);		/* Set bit 2 high */
			}
	else	if (strcmp(argv[2],"off")==0)
			{
			set_w83627_config(0xe1,0xf1);		/* Set bits 2 and 3 low */			
			}
	else	if (strcmp(argv[2],"red_flash")==0)
			{
			printf("Flashing is not available on the M270\n");
			}
	else	if (strcmp(argv[2],"green_flash")==0)
			{
			printf("Flashing is not available on the M270\n");	
			}
	else	if (strcmp(argv[2],"red_flash_fast")==0)
			{
			printf("Flashing is not available on the M270\n");
			}
	else	if (strcmp(argv[2],"green_flash_fast")==0)
			{
			printf("Flashing is not available on the M270\n");	
			}	

	else{
	usage();
	}
	w83627_leave_ext_mode();
	}
	else{
	usage();
	}
	
	break;

case 8:		// M370/470/570/670
	
if (strcmp(argv[1],"-b")==0) 
    {
	printf("The M370/470/570/670 has no LCD\n");
	return 0;	
	}
else	
if (strcmp(argv[1],"-f")==0) /* All three fans use the CPUfan output on Bank2 */
    {
	if (argc==2)  /* Are we looking for the fan speed or setting it */
	{
    setw83627(bank_select,0x02); /* Set Bank2 */
	fanspeed = getw83627(0x09);	/* Current system fan speed register */
	fanspeed_min = getw83627(0x27); /* T1 fan speed */
	printf("Current fanspeed is %x, minimum fanspeed is %x\n",fanspeed ,fanspeed_min);
	}
	else
	{
    setw83627(bank_select,0x02); /* Set Bank2 */
	setw83627(0x21,0x1e);        /* Set T1 point at 30C */
	fanspeed_min = strtol(argv[2],NULL,16); /*Convert argument to hex integer*/
	setw83627(0x27,fanspeed_min);
	printf("Minimum fanspeed set to %x at 30°C or less\n",fanspeed_min);
	} 
	}
else	
if (strcmp(argv[1],"-l")==0)
	{
			if (argc ==2) /*Check that correct number of arguments have been given */
			{
			usage();
			return 0;
			}
                                        /* The NCT6776F in the M370/470/570/670 is compatible with the W83627 */
			w83627_enter_ext_mode();	/* Enter extended function mode */
			set_w83627_config(LDR,0x07);		/* Set logial device register to device 7, the LED is on GPIO7 bits 2 and 3*/
			
		if (strcmp(argv[2],"red")==0)
			{
			set_w83627_config(0xe1,0xfb);		/* Set bit 3 high */
			}
	else	if (strcmp(argv[2],"green")==0)
			{
			set_w83627_config(0xe1,0xf7);		/* Set bit 2 high */
			}
	else	if (strcmp(argv[2],"off")==0)
			{
			set_w83627_config(0xe1,0xf3);		/* Set bits 2 and 3 low */			
			}
	else	if (strcmp(argv[2],"red_flash")==0)
			{
			printf("Flashing is not available on the M370/470/570/670\n");
			}
	else	if (strcmp(argv[2],"green_flash")==0)
			{
			printf("Flashing is not available on the M370/470/570/670\n");	
			}
	else	if (strcmp(argv[2],"red_flash_fast")==0)
			{
			printf("Flashing is not available on the M370/470/570/670\n");
			}
	else	if (strcmp(argv[2],"green_flash_fast")==0)
			{
			printf("Flashing is not available on the M370/470/570/670\n");	
			}	

	else{
	usage();
	}
	w83627_leave_ext_mode();
	}
	else{
	usage();
	}
	
	break;


case 7:		// XTM800/1500
	
if (strcmp(argv[1],"-b")==0) 
    {
	if (argc==2)  /* Can only set the backlight */
	{
	usage();
	return 0;	
	}
	if (strcmp(argv[2],"on")==0)
		{
		setbacklight(0x00);   /* Strobe bit on control port is inverted */
		}
	else	if (strcmp(argv[2],"off")==0)
		{
		setbacklight(0x01);
		}
	else 	{
		usage();
		return 0;
		}
	}
else	
if (strcmp(argv[1],"-f")==0) 
    {
	printf("There is no fan control on the XTM800/1500 yet\n");
	return 0;	
	}
else	
if (strcmp(argv[1],"-l")==0) 
    {
	printf("There is no LED control on the XTM800/1500 yet\n");
	return 0;	
	}
else
    {
	usage();
	}
	
break;


case 6:		// M400/500
	
if (strcmp(argv[1],"-b")==0) 
    {
	printf("The M400/500 has no LCD\n");
	return 0;	
	}
else	
if (strcmp(argv[1],"-f")==0) 
    {
	printf("There is no fan control on the M400/500 yet\n");
	return 0;	
	}
else	
if (strcmp(argv[1],"-l")==0)
	{
			if (argc ==2) /*Check that correct number of arguments have been given */
			{
			usage();
			return 0;
			}
                                        /* The NCT6776F in the M400/500 is compatible with the W83627 */
			w83627_enter_ext_mode();	/* Enter extended function mode */
			set_w83627_config(LDR,0x07);		/* Set logial device register to device 7, the LED is on GPIO7 bits 2 and 3*/
			
		if (strcmp(argv[2],"red")==0)
			{
			set_w83627_config(0xe1,0x3b);		/* Set bit 3 high */
			}
	else	if (strcmp(argv[2],"green")==0)
			{
			set_w83627_config(0xe1,0x37);		/* Set bit 2 high */
			}
	else	if (strcmp(argv[2],"off")==0)
			{
			set_w83627_config(0xe1,0x33);		/* Set bits 2 and 3 low */			
			}

	else	if (strcmp(argv[2],"red_flash")==0)
			{
			printf("Flashing is not available on the M400/500\n");
			}
	else	if (strcmp(argv[2],"green_flash")==0)
			{
			printf("Flashing is not available on the M400/500\n");	
			}
	else	if (strcmp(argv[2],"red_flash_fast")==0)
			{
			printf("Flashing is not available on the M400/500\n");
			}
	else	if (strcmp(argv[2],"green_flash_fast")==0)
			{
			printf("Flashing is not available on the M400/500\n");	
			}	

	else{
	usage();
	}
	w83627_leave_ext_mode();
	}
	else{
	usage();
	}
	
	break;


case 5:		// XTM8
	
if (strcmp(argv[1],"-b")==0) 
    {
	if (argc==2)  /* Can only set the backlight */
	{
	usage();
	return 0;	
	}
	if (strcmp(argv[2],"on")==0)
		{
		setbacklight(0x00);   /* Strobe bit on control port is inverted */
		}
	else	if (strcmp(argv[2],"off")==0)
		{
		setbacklight(0x01);
		}
	else 	{
		usage();
		return 0;
		}
	}
else
if (strcmp(argv[1],"-f")==0) 
    {
	if (argc==2)  /* Are we looking for the fan speed or setting it */
	{
	//fanspeed = get_it87(0x01);	/* Current system fan speed register */
	fanspeed_min = get_it87(0x63); /* Minimum FANCTL_1 speed in SmartGuardian mode */
	printf("Current CPU fanspeed is ???, minimum is %x\n",fanspeed_min);
	}
	else
	{
    default_XTM8_fan();
	//fanspeed = get_it87(0x01);
	fanspeed_min = strtol(argv[2],NULL,16); /*Convert argument to hex integer*/
	set_it87(0x063,fanspeed_min);  /* Minimum FANCTL_1 speed in SmartGuardian mode */
	printf("Minimum CPU fanspeed set to %x\n",fanspeed_min);
	} 				
	}
else	
if (strcmp(argv[1],"-f2")==0) 
    {
	if (argc==2)  /* Are we looking for the fan speed or setting it */
	{
	//fanspeed = get_it87(0x01);	/* Current system fan speed register */
	fanspeed_min = get_it87(0x6b); /* Minimum FANCTL_2 speed in SmartGuardian mode */
	printf("Current System fanspeed is ???, minimum is %x\n",fanspeed_min);
	}
	else
	{
    default_XTM8_fan();
	//fanspeed = get_it87(0x01);
	fanspeed_min = strtol(argv[2],NULL,16); /*Convert argument to hex integer*/
	set_it87(0x06b,fanspeed_min);  /* Minimum FANCTL_2 speed in SmartGuardian mode */
	printf("Minimum System fanspeed set to %x\n",fanspeed_min);
	} 				
	}
else	
if (strcmp(argv[1],"-l")==0)
	{
			if (argc ==2) /*Check that correct number of arguments have been given */
			{
			usage();
			return 0;
			}

            config_XTM8_led(); /* Check and set the GPIO pins for LED operation */

            it87_enter_mbpnp_mode();

    		if (strcmp(argv[2],"red")==0)
			{
			set_it87_config(0xb2,0x01);	/* Invert Red, high */
			set_it87_config(0xf9,0x01);	/* Red blink low */
			set_it87_config(0xfb,0x01);	/* Green blink low */	
			}
	else	if (strcmp(argv[2],"green")==0)
			{
			set_it87_config(0xb2,0x02);	/* Invert Green, high */
			set_it87_config(0xf9,0x01);	/* Red blink low */
			set_it87_config(0xfb,0x01);	/* Green blink low */
			}
	else	if (strcmp(argv[2],"off")==0)
			{
			set_it87_config(0xb2,0x00);	/* None inverted */
			set_it87_config(0xf9,0x01);	/* Red blink low */
			set_it87_config(0xfb,0x01);	/* Green blink low */			
			}
	else	if (strcmp(argv[2],"red_flash")==0)
			{
			set_it87_config(0xb2,0x01);	/* Invert Red, high */
			set_it87_config(0xf9,0x01);	/* Red blink low */
			set_it87_config(0xfb,0x02);	/* Green blink 1Hz */
			}
	else	if (strcmp(argv[2],"green_flash")==0)
			{
			set_it87_config(0xb2,0x02);	/* Invert Green, high */
			set_it87_config(0xf9,0x02);	/* Red blink 1Hz */
			set_it87_config(0xfb,0x01);	/* Green blink low */
			}
	else	if (strcmp(argv[2],"red_flash_fast")==0)
			{
			set_it87_config(0xb2,0x01);	/* Invert Red, high */
			set_it87_config(0xf9,0x01);	/* Red blink low */
			set_it87_config(0xfb,0x00);	/* Green blink 4Hz */
			}
	else	if (strcmp(argv[2],"green_flash_fast")==0)
			{
			set_it87_config(0xb2,0x02);	/* Invert Green, high */
			set_it87_config(0xf9,0x00);	/* Red blink 4Hz */
			set_it87_config(0xfb,0x01);	/* Green blink low */	
			}	
	else{
	usage();
	}
	it87_leave_mbpnp_mode();
	}
else
if (strcmp(argv[1],"-t")==0) 
   {
	cpu_temp = get_it87(0x29);
	system_temp = get_it87(0x2a);
	printf("CPU temp:%d, System temp:%d  \n",cpu_temp, system_temp);
	}

else{
	usage();
	}
break;


case 4:		// XTM5
	
if (strcmp(argv[1],"-b")==0) 
    {
	if (argc==2)  /* Can only set the backlight */
	{
	usage();
	return 0;	
	}
	if (strcmp(argv[2],"on")==0)
		{
		setbacklight(0x00);   /* Strobe bit on control port is inverted */
		}
	else	if (strcmp(argv[2],"off")==0)
		{
		setbacklight(0x01);
		}
	else 	{
		usage();
		return 0;
		}
	}
else
if (strcmp(argv[1],"-f")==0) 
    {
	printf("The CPU fans are not yet controllable on the XTM5\n");
	return 0;	
	}
else	
if (strcmp(argv[1],"-f2")==0) 
    {
	if (argc==2)  /* Are we looking for the fan speed or setting it */
	{
	fanspeed = getw83627(0x01);	/* Current system fan speed register */
	fanspeed_min = getw83627(0x08); /* Minimum syetem fan speed in Thermal Cruise mode */
	printf("Current fanspeed is %x, minimum fanspeed is %x\n",fanspeed ,fanspeed_min);
	}
	else
	{
	fanspeed = getw83627(0x01);
	fanspeed_min = strtol(argv[2],NULL,16); /*Convert argument to hex integer*/
	setw83627(0x08,fanspeed_min);
	printf("Minimum fanspeed set to %x\n",fanspeed_min);
	if (fanspeed_min>fanspeed)	/* To increase the fan speed to the new setting we */
		{			/* have to restart Thermal Cruise mode */
		printf("Restarting Thermal Cruise mode\n");			
		setw83627(0x04,0x08);	/* Set the sytem fan mode to fan cruise */
		//setw83627(0x01,fanspeed);	/* Keep current speed    */
		setw83627(0x04,0x04);	/* Set is back to Thermal Cruise    */
		}			/* Reg 04h bits 3-2 control the system fan mode */
	} 				/* 10 is fan, 01 is Thermal Cruise */
	//get_w83627_addr_port();
	}
else	
if (strcmp(argv[1],"-l")==0)
	{
			if (argc ==2) /*Check that correct number of arguments have been given */
			{
			usage();
			return 0;
			}

			w83627_enter_ext_mode();	/* Enter extended functio mode */
			set_w83627_config(0x07,0x08);		/* Set logial device register to device 8 (GPIO2) */
			if ((get_w83627_config(0x30)) ==0){		/* Is SuperIO chip GPIO2 enabled? */
			printf("Enabling GPIO2\n");
			set_w83627_config(0x30,0x01);
			}
			if ((get_w83627_config(0xf0)) ==0xff){		/* Is GPIO2 set as output? */
			printf("Setting GPIO2 pins as output\n");
			set_w83627_config(0xf0,0xcf);			/* 1100 1111, 0xcf */
			}

		if (strcmp(argv[2],"red")==0)
			{
			set_w83627_config(0xf1,0x20);		/* Set bit 5 high */
			}
	else	if (strcmp(argv[2],"green")==0)
			{
			set_w83627_config(0xf1,0x10);		/* Set bit 4 high */
			}
	else	if (strcmp(argv[2],"off")==0)
			{
			set_w83627_config(0xf1,0x00);		/* Set bits 4 and 5 low */			
			}

	else	if (strcmp(argv[2],"red_flash")==0)
			{
			printf("Flashing is not available on the XTM5\n");
			}
	else	if (strcmp(argv[2],"green_flash")==0)
			{
			printf("Flashing is not available on the XTM5\n");	
			}
	else	if (strcmp(argv[2],"red_flash_fast")==0)
			{
			printf("Flashing is not available on the XTM5\n");
			}
	else	if (strcmp(argv[2],"green_flash_fast")==0)
			{
			printf("Flashing is not available on the XTM5\n");	
			}	

	else{
	usage();
	}
	w83627_leave_ext_mode();
	}
	else{
	usage();
	}
	
	break;

case 3:		// X-e

	
if (strcmp(argv[1],"-b")==0) 
    {
	if (argc==2)  /* Can only set the backlight */
	{
	usage();
	return 0;	
	}
	if (strcmp(argv[2],"on")==0)
		{
		setbacklight(0x00);   /* Strobe bit on control port is inverted */
		}
	else	if (strcmp(argv[2],"off")==0)
		{
		setbacklight(0x01);
		}
	else 	{
		usage();
		return 0;
		}
	}
else	
if (strcmp(argv[1],"-f")==0) 
    {
	check_siobase();
	if (argc==2)  /* Are we looking for the fan speed or setting it */
	{
	fanspeed = getw83627(0x5a);
	printf("Fanspeed is %x\n",fanspeed);
	}
	else
	{
	fanspeed = strtol(argv[2],NULL,16); /*Convert argument to hex integer*/
	setw83627(0x5a,fanspeed);
	printf("Fanspeed set to %x\n",fanspeed);
	}
	}
else	
if (strcmp(argv[1],"-l")==0)
	{
			if (argc ==2)
			{
			usage();
			return 0;
			}
			if (strcmp(argv[2],"red")==0)
			{
			setledXe(0x0b);
			setflashXe(0x00);
			}
	else	if (strcmp(argv[2],"green")==0)
			{
			setledXe(0x13);
			setflashXe(0x00);
			}
	else	if (strcmp(argv[2],"red_flash")==0)
			{
			setledXe(0x0b);
			setflashXe(0x08);
			}
	else	if (strcmp(argv[2],"green_flash")==0)
			{
			setledXe(0x13);
			setflashXe(0x10);		
			}
	else	if (strcmp(argv[2],"red_flash_fast")==0)
			{
			printf("Fast flashing is not available on the X-e\n");
			}
	else	if (strcmp(argv[2],"green_flash_fast")==0)
			{
			printf("Fast flashing is not available on the X-e\n");	
			}
	else	if (strcmp(argv[2],"off")==0)
			{
			setledXe(0x03);
			setflashXe(0x00);
			}
	else{
	usage();
	}
	}
else
if (strcmp(argv[1],"-t")==0) 
   {
	check_siobase();
	cpu_temp = getcputemp();
	printf("SuperIO sensor 2 reads:\n%d \n",cpu_temp);
	}

	else{
	usage();
	}
	break;
	
case 2:		// X-Peak
	
if (strcmp(argv[1],"-b")==0) 
    {
	if (argc==2)  /* Can only set the backlight */
	{
	usage();
	return 0;	
	}
	if (strcmp(argv[2],"on")==0)
		{
		setbacklight(0x00);   /* Strobe bit on control port is inverted */
		}
	else	if (strcmp(argv[2],"off")==0)
		{
		setbacklight(0x01);
		}
	else 	{
		usage();
		return 0;
		}
	}
else	
if (strcmp(argv[1],"-f")==0) 
    {
	printf("Fanspeed is not available on the X-Peak\n");
	}
else	
if (strcmp(argv[1],"-l")==0)
	{
			if (argc ==2)
			{
			usage();
			return 0;
			}
			if (strcmp(argv[2],"red")==0)
			{
			setledXp(0x0d);
			}
	else	if (strcmp(argv[2],"green")==0)
			{
			setledXp(0x0e);
			}
	else	if (strcmp(argv[2],"red_flash")==0)
			{
			printf("Flashing is not available on the X-Peak\n");
			}
	else	if (strcmp(argv[2],"green_flash")==0)
			{
			printf("Flashing is not available on the X-Peak\n");	
			}
	else	if (strcmp(argv[2],"red_flash_fast")==0)
			{
			printf("Flashing is not available on the X-Peak\n");
			}
	else	if (strcmp(argv[2],"green_flash_fast")==0)
			{
			printf("Flashing is not available on the X-Peak\n");	
			}	
	else	if (strcmp(argv[2],"off")==0)
			{
			setledXp(0x0c);			
			}
	else{
	usage();
	}
	}
	else{
	usage();
	}
	break;
	
case 1:		// X-Core
	
if (strcmp(argv[1],"-b")==0) 
    {
	if (argc==2)  /* Can only set the backlight */
	{
	usage();
	return 0;	
	}
	if (strcmp(argv[2],"on")==0)
		{
		setbacklight(0x00);   /* Strobe bit on control port is inverted */
		}
	else	if (strcmp(argv[2],"off")==0)
		{
		setbacklight(0x01);
		}
	else 	{
		usage();
		return 0;
		}
	}
else	
if (strcmp(argv[1],"-f")==0) 
    {
	printf("Fanspeed is not available on the X-Core\n");
	}
else	
if (strcmp(argv[1],"-l")==0)
	{
			if (argc ==2)
			{
			usage();
			return 0;
			}
			if (strcmp(argv[2],"red")==0)
			{
			setledXc(0x09, 0xbf);
			}
	else	if (strcmp(argv[2],"green")==0)
			{
			setledXc(0x19, 0xbf);
			}
	else	if (strcmp(argv[2],"red_flash")==0)
			{
			setledXc(0x09, 0x3f);
			}
	else	if (strcmp(argv[2],"green_flash")==0)
			{
			setledXc(0x19, 0x3f);
			}
	else	if (strcmp(argv[2],"red_flash_fast")==0)
			{
			setledXc(0x01, 0xbf);
			}
	else	if (strcmp(argv[2],"green_flash_fast")==0)
			{
			setledXc(0x11, 0xbf);
			}	
	else	if (strcmp(argv[2],"off")==0)
			{
			setledXc(0x01, 0x3f);
			}
	else{
	usage();
	}
	}
	else{
	usage();
	}
	break;
	
case 0:
	printf("Firebox not detected.\n");
	printf("If this is a Firebox it's either one we don't know about or it's running a bios we haven't seen.\n");
	printf("Hit the pfSense forums to let us know more.\n");
	break;
}
				
return 0;
}
