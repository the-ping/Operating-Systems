/*
 * CMOS Real-time Clock
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (1)
 */

/*
 * STUDENT NUMBER: s1770036
 */
#include <infos/drivers/timer/rtc.h>
#include <infos/util/lock.h> //handling interrupts
#include <arch/x86/pio.h> //for outb, __inb
#include <infos/kernel/log.h> //for syslog


using namespace infos::drivers;
using namespace infos::drivers::timer;
using namespace infos::arch::x86; //for outb, __inb
using namespace infos::kernel; //for syslog




class CMOSRTC : public RTC {
public:
	static const DeviceClass CMOSRTCDeviceClass;

	const DeviceClass& device_class() const override
	{
		return CMOSRTCDeviceClass;
	}

	/**
	 * Interrogates the RTC to read the current date & time.
	 * @param tp Populates the tp structure with the current data & time, as
	 * given by the CMOS RTC device.
	 */

	 //variables
	 int CURRENT_YEAR = 2021;
	 int century_register = 0x00;

	 unsigned char second;
	 unsigned char minute;
	 unsigned char hour;
	 unsigned char day;
	 unsigned char month;
	 unsigned int year;
	 unsigned char registerB;

	 unsigned char century;
	 unsigned char last_second;
	 unsigned char last_minute;
	 unsigned char last_hour;
	 unsigned char last_day;
	 unsigned char last_month;
	 unsigned char last_year;
	 unsigned char last_century;

	 //writing reading CMOS memory
	 enum {
			cmos_address = 0x70, //write to this port
			cmos_data    = 0x71 //read value from this port
	 };

	void read_timepoint(RTCTimePoint& tp) override
	{
		//disable interrupts
		infos::util::UniqueIRQLock();

		//read the registers into variables
		read_registers();

		//convert BCD to binary
		convert_BCD();

		//return in RTCTimePoint structure
		tp.seconds = second;
		tp.minutes = minute;
		tp.hours = hour;
		tp.day_of_month = day;
		tp.month = month;
		tp.year = year;

	}


	//-----METHODS:-----
	//Reg A: is an update in progress?
	int get_update_in_progress_flag() {
		__outb(cmos_address, 0x0A); //send RegA here
		return (__inb(cmos_data) & 0x80); //read the 0x80 bit that controls NMI
	}

	//retrieve contents from RTC register
	unsigned char get_RTC_register(int reg) {
      __outb(cmos_address, reg); //send reg here
      return __inb(cmos_data); //read value of reg here
	}

	//convert BDC to binary
	void convert_BCD() {
		if (!(registerB & 0x04)) { //if bit2 is cleared = convert to bindary
			second = (second & 0x0F) + ((second / 16) * 10);
			minute = (minute & 0x0F) + ((minute / 16) * 10);
      hour = ( (hour & 0x0F) + (((hour & 0x70) / 16) * 10) ) | (hour & 0x80);
      day = (day & 0x0F) + ((day / 16) * 10);
      month = (month & 0x0F) + ((month / 16) * 10);
      year = (year & 0x0F) + ((year / 16) * 10);

      if(century_register != 0) {
            century = (century & 0x0F) + ((century / 16) * 10);
      }
		}
	}

	//convert 12 hour clock to 24 hour clock
	void convert_clock() {
		if (!(registerB & 0x02) && (hour & 0x80)) {
			hour = ((hour & 0x7F) + 12) % 24;
		}
	}

	//read registers
	void read_registers() {

		while (get_update_in_progress_flag()); //while update isn't in progress
		second = get_RTC_register(0x00);
    minute = get_RTC_register(0x02);
    hour = get_RTC_register(0x04);
    day = get_RTC_register(0x07);
    month = get_RTC_register(0x08);
    year = get_RTC_register(0x09);

		if(century_register != 0) {
			century = get_RTC_register(century_register);
		}

		do {
			last_second = second;
      last_minute = minute;
      last_hour = hour;
      last_day = day;
      last_month = month;
      last_year = year;
			last_century = century;

			while (get_update_in_progress_flag()); //check if update in progress is clear, AGAIN
			second = get_RTC_register(0x00);
      minute = get_RTC_register(0x02);
      hour = get_RTC_register(0x04);
      day = get_RTC_register(0x07);
      month = get_RTC_register(0x08);
      year = get_RTC_register(0x09);

			if(century_register != 0) {
				century = get_RTC_register(century_register);
			}
		}
			//if 1st and 2nd values are same, values are read correctly
			//hence, read CMOS until they are the same
			while ( (last_second != second) || (last_minute != minute) || (last_hour != hour) ||
							(last_day != day) || (last_month != month) || (last_year != year) ||
							(last_century != century)	);

			registerB = get_RTC_register(0x0B); //to check if values in reg are bindary/BCD
	}


};

const DeviceClass CMOSRTC::CMOSRTCDeviceClass(RTC::RTCDeviceClass, "cmos-rtc");

RegisterDevice(CMOSRTC);
