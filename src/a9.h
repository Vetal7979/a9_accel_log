#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <errno.h>

#include "api_os.h"
#include "api_hal_pm.h"
#include "api_debug.h"
#include "api_event.h"
#include "api_info.h"
#include "api_gps.h"
#include "api_hal_gpio.h"
#include "api_fs.h"
#include "api_charset.h"
#include "api_socket.h"
#include "api_network.h"
#include "api_hal_adc.h"
#include "api_key.h"
#include "api_fota.h"
#include "api_hal_uart.h"
#include "api_hal_i2c.h"

// #define NEW_PLATA

void sd_init();
void sd_reopen();

#define MAIN_TASK_STACK_SIZE      (2048 * 3)
#define MAIN_TASK_PRIORITY        0
#define MAIN_TASK_NAME            "TrackerMain Task"

#define THIRD_TASK_STACK_SIZE     (2048 * 3)
#define THIRD_TASK_PRIORITY       1
#define THIRD_TASK_NAME           "NetworkMain Task"

#define UART_TASK_STACK_SIZE     (2048 * 3)
#define UART_TASK_PRIORITY       2
#define UART_TASK_NAME           "UART Task"

#define AIRPLANEMODE_TASK_STACK_SIZE     (2048 * 3)
#define AIRPLANEMODE_TASK_PRIORITY       3
#define AIRPLANEMODE_TASK_NAME           "Airplane Task"

#define MEMS_TASK_STACK_SIZE     (2048 * 3)
#define MEMS_TASK_PRIORITY       4 //??
#define MEMS_TASK_NAME           "MEMS Task"

#define I2C_ACC     I2C2
//#define G           9.81            // м/с2
#define MPU_9265_ADDR   0x68

extern char DATA_FILE_NAME[128];
extern char DATA2_FILE_NAME[128];

#define CONFIG_DATA_FILE    FS_DEVICE_NAME_T_FLASH "/config.bin"   // external sd card
#define CONFIG2_DATA_FILE   FS_DEVICE_NAME_FLASH   "/config.bin"   // internal flash
#define PROFILE_DATA_FILE   FS_DEVICE_NAME_FLASH   "/profile.bin"  // internal flash
#define DEL_PROFILE_FILE    FS_DEVICE_NAME_T_FLASH "/profile.txt"  // external sd card - if exists, will delete internal profile.bin
// #define DEL_PROFILE_FILE    "profile.txt" // FILE NAME only, external sd card - if exists, will delete internal profile.bin
#define LOG_FILE_NAME       FS_DEVICE_NAME_T_FLASH "/log.txt"
#define AIRPLANE_NAME       FS_DEVICE_NAME_T_FLASH "/airplane.txt"
#define NMEA_LOG_NAME       FS_DEVICE_NAME_T_FLASH "/nmea.txt"
#define NMEA_PARTS_LOG_NAME FS_DEVICE_NAME_T_FLASH "/nmea_parts.txt"
#define SETTINGS_FILE_NAME  FS_DEVICE_NAME_T_FLASH "/settings.txt"

#define ACCEL_LOG_FILE    FS_DEVICE_NAME_T_FLASH "/accel.log"




void WriteLog(const char *str);
void airplane_log(const char *s);
void WriteNmeaLog(const char *str);
void WriteNmeaLogParts(uint8_t *str, int length);
void WriteAccelLog(void);

void settings_load();

enum GPS_Type {
    GPS_TYPE__A9G = 0,
    GPS_TYPE__ELA,
    GPS_TYPE__ORIGIN
};
extern enum GPS_Type gpsType;

void gps_received(int length, uint8_t *buffer);
void gps_init();
void gps_on();
void gps_off();

void led_init();
void led_blink();
void led_off();
void GPS_Init();
void load_config();
void update_config();
void flush_sector();
extern int recs;

void network_thread(int fd);
void ThirdTask(void *pData);
void EventDispatchSocket(API_Event_t *pEvent);

extern HANDLE data_lock, log_lock, airplane_log_lock;
extern int tenMinutes;


struct Settings {
    char domain[100], IMEI[16], SOFTWARE_VERSION[11];
    bool write_log, network_log, airplane_log, adc, nmea;
    int port;
    char apn_apn[100], apn_user[100], apn_password[100];
};
extern struct Settings *settings;

#define UNINITIALIZED (int64_t)-1

struct SectorInfo {
	int64_t sent, total;
    int64_t sent2, total2;
};
extern struct SectorInfo sectorInfo;

#pragma pack(push, 1)
struct Controller_ControlFrame {
    long dt, lat, lon, alt, course, speed; // 6*4 bytes
 //   long accel; // 4 bytes
};
struct Controller_DiffFrame {
    char dlon, dlat, dalt, dcourse, dspeed; // 5*1 bytes
};
struct Controller_byte {
    unsigned char a : 1, b : 1, c : 1, d : 1, e : 1, f : 1, g : 1, h : 1;
};
struct Controller_Header {
    struct Controller_byte hdr[13]; // 13 bytes
};
union Controller_Converter {
    unsigned char val;
    struct Controller_byte bval;
};
#pragma pack(pop)

#define SECTOR_SIZE 512

extern struct Controller_ControlFrame current;
extern struct Controller_Header ch;
extern bool time_set;

#define BUFFERED_COORDINATES      30
#define BUFFERED_COORDINATES_SIZE (1 + sizeof(struct Controller_ControlFrame) * BUFFERED_COORDINATES)
extern char current_coordinate[BUFFERED_COORDINATES_SIZE];

extern char sector_buffer[SECTOR_SIZE];

#define BATTERY_SECONDS  20*10 // 20 seconds
#define BATTERY_INTERVAL 600

#define NETWORK_TIMEOUT      100 // 10 seconds
#define FOTA_NETWORK_TIMEOUT 600 // 60 seconds

// #define DEBUG_LOG
extern char network_state;

#define FLIGHT_MODE_OFFLINE 3 * 60 // 3 minutes
#define FLIGHT_MODE_TIMEOUT 60 * 1000

char GetVoltage();

/*enum HourProfile {
    HP__Sleep   = 0x00,
    HP__Offline = 0x01,
    HP__Hourly  = 0x02,
    HP__Online  = 0x03
};*/

#define HP__Sleep    (char)0x00
#define HP__Offline  (char)0x01
#define HP__Hourly   (char)0x02
#define HP__Online   (char)0x03

extern int profile_read, profile_del, profile_upd;
extern char /*enum HourProfile*/ Profile[24];
extern char /*enum HourProfile*/ now;
extern bool ProfileUpdated;


void load_profile();
void update_profile();