#include "a9.h"

#define GPS_BUFFER_SIZE 1024 // 150
uint8_t nmea[GPS_BUFFER_SIZE], nmea_index = 0, start_sentence = 0;

#define MKTIME_SECONDS 946598400 // 2209075200

bool time_set = false;

struct Controller_Header ch;
int buffer_offset = sizeof(struct Controller_Header);
long lat_prev = 0, lon_prev = 0, alt_prev = 0, course_prev = 0, speed_prev = 0, dt_prev = 0;
bool has_date = false;
TIME_System_t ttime;
unsigned long last_dt = 0;
struct Controller_ControlFrame current = { 0, 0, 0, 0, 0, 0 };
 
char current_coordinate[BUFFERED_COORDINATES_SIZE] = { 0x00 };
char sector_buffer[SECTOR_SIZE];
char DATA_FILE_NAME[128] = { 0 };
char DATA2_FILE_NAME[128] = { 0 };

extern double Mid; 

int controller_control_frame(char *_sector_buffer) {
    memcpy(_sector_buffer, &current, sizeof(current));
    return sizeof(struct Controller_ControlFrame);
}

int controller_diff_frame(char *_sector_buffer, int dlat, int dlon, int dalt, int dcourse, int dspeed) {
    struct Controller_DiffFrame df;
    df.dlat = dlat;
    df.dlon = dlon;
    df.dalt = dalt;
    df.dcourse = dcourse;
    df.dspeed = dspeed;
    memcpy(_sector_buffer, &df, sizeof(df));
    return sizeof(struct Controller_DiffFrame);
}

int recs = 0;

void set_bit(struct Controller_byte *bitmask, int value) {
    int bindex = recs / 8;
    int bbyte = recs % 8;

    switch (bbyte) {
    case 0: bitmask[bindex].a = value; break;
    case 1: bitmask[bindex].b = value; break;
    case 2: bitmask[bindex].c = value; break;
    case 3: bitmask[bindex].d = value; break;
    case 4: bitmask[bindex].e = value; break;
    case 5: bitmask[bindex].f = value; break;
    case 6: bitmask[bindex].g = value; break;
    case 7: bitmask[bindex].h = value; break;
    }

    recs ++;
}

void write_magic_byte(struct Controller_byte *bitmask, int records_in_sector) {
    union Controller_Converter cc;
    cc.val = (char)records_in_sector; // 1..96
    bitmask[12] = cc.bval;
}

HANDLE data_lock = NULL, log_lock = NULL, airplane_log_lock = NULL;

char save_file(const char *fn, int64_t *size) {
    int32_t fd = API_FS_Open(fn, FS_O_APPEND | FS_O_RDWR | FS_O_CREAT, 0);
    if (fd < 0) {
        Trace(1, "*************** [flush_sector] FAILED - API_FS_Open");
        if (settings->network_log || settings->write_log) {
            char tmp[100];
            sprintf(tmp, "[flush_sector] FAILED - API_FS_Open [%s]", fn);
            WriteLog(tmp);
        }
    } else {
        int ret = API_FS_Write(fd, (uint8_t*)&sector_buffer[0], SECTOR_SIZE);
        API_FS_Flush(fd);
        int64_t newSize = API_FS_GetFileSize(fd) / SECTOR_SIZE;
        API_FS_Close(fd);

        if (ret <= 0) {
            Trace(1, "*************** [flush_sector] FAILED - API_FS_Write");
            if (settings->network_log || settings->write_log) {
                WriteLog("[flush_sector] FAILED - API_FS_Write");
            }
        } else {
            Trace(1, "*************** [flush_sector] SUCCESS");

            *size = newSize;

            return 1; // success
        }
    }
    return 0; // error
}
/*
void save_coord(const char *fn) {
    int32_t fd = API_FS_Open(fn, FS_O_APPEND | FS_O_RDWR | FS_O_CREAT, 0);
    if (fd < 0) {
        Trace(1, "*************** [flush_sector] FAILED - API_FS_Open");
        if (settings->network_log || settings->write_log) {
            char tmp[100];
            sprintf(tmp, "[flush_sector] FAILED - API_FS_Open [%s]", fn);
            WriteLog(tmp);
        }
    } else {
        char tmp[100];
        int ret = API_FS_Write(fd, (uint8_t*)tmp, sprintf(tmp, "%ld,%ld,%ld, %ld\n",
            current.dt, current.lat, current.lon, current.accel));
        API_FS_Flush(fd);
        API_FS_Close(fd);
    }
}
*/
// #define FS_TCardInit CSDK_FUNC(FS_TCardInit)

void save_sector() {
    Trace(1, "**** save_sector SAVING %d records to SD CARD %ld (last sent sector is %ld), INTERNAL %ld (last sent sector is %ld)\n", recs, (long int)sectorInfo.total, (long int)sectorInfo.sent,
        (long int)sectorInfo.total2, (long int)sectorInfo.sent2);

    OS_LockMutex(data_lock);
    int64_t size = 0;

    // FS_TCardInit();
    if (save_file(DATA_FILE_NAME, &size)) {
        Trace(1, "*************** [save_sector] saved to SD CARD");
        sectorInfo.total = size;

        // external sd card
        if (sectorInfo.sent == UNINITIALIZED) {
            int32_t fd = API_FS_Open(CONFIG_DATA_FILE, FS_O_RDONLY | FS_O_CREAT, 0);
            if (fd >= 0) {
                if (API_FS_Read(fd, (uint8_t *)&sectorInfo.sent, sizeof(int64_t)) <= 0) {
                    sectorInfo.sent = sectorInfo.total;
                }
                API_FS_Close(fd);
            }
            if (sectorInfo.sent > sectorInfo.total) {
                sectorInfo.sent = sectorInfo.total;
            }
        }
    } else { // error saving to sd-card
        /* sd_reopen(); // flush power on sd-card

        if (save_file(DATA_FILE_NAME, &size)) {
            Trace(1, "*************** [save_sector] saved to SD CARD");
            sectorInfo.total = size;

            // external sd card
            if (sectorInfo.sent == UNINITIALIZED) {
                int32_t fd = API_FS_Open(CONFIG_DATA_FILE, FS_O_RDONLY | FS_O_CREAT, 0);
                if (fd >= 0) {
                    if (API_FS_Read(fd, (uint8_t *)&sectorInfo.sent, sizeof(int64_t)) <= 0) {
                        sectorInfo.sent = sectorInfo.total;
                    }
                    API_FS_Close(fd);
                }
                if (sectorInfo.sent > sectorInfo.total) {
                    sectorInfo.sent = sectorInfo.total;
                }
            }
        } else*/ if (save_file(DATA2_FILE_NAME, &size)) {
            Trace(1, "*************** [save_sector] saved to INTERNAL FLASH");
            sectorInfo.total2 = size;
        } else {
            Trace(1, "*************** [save_sector] error saving");
        }
    }

    OS_UnlockMutex(data_lock);
}

void flush_sector() {
    write_magic_byte(&ch.hdr[0], recs);
    memcpy(sector_buffer, &ch, sizeof(struct Controller_Header));

    save_sector();

    memset(&ch, 0, sizeof(ch));
    memset(sector_buffer, 0xFF, SECTOR_SIZE);

    buffer_offset = sizeof(struct Controller_Header);
    recs = 0;
}

int atoin(const char* s, int n) {
    char buf[10];
    memcpy(buf, s, n);
    buf[n] = '\0';
    return atoi(buf);
}

long my_abs(long a) {
    return (a > 0 ? a : -a);
}

int checksum() {
    int cs = 0;
    for (int i = 1; nmea[i] && nmea[i] != '*'; i ++) {
        cs = cs ^ (signed char)nmea[i];
    }
    return cs;
}

void print_info() {
    led_blink();

    /*Trace(1, "* GPS INFO [%02d.%02d.%d %02d:%02d:%02d] [%ld, %.06f, %.06f, %.02f, %.02f, %.02f] buffer_offset=%d",
        ttime.day, ttime.month, ttime.year, ttime.hour, ttime.minute, ttime.second,
        current.dt, (double)current.lat / 1000000, (double)current.lon / 1000000, (double)current.alt / 10, (double)current.course / 10, (double)current.speed / 10,
        buffer_offset);*/
}

bool valid_coord() {
	bool valid = (
        current.lat >= -180000000L && current.lat <= 180000000L &&
        current.lon >= -180000000L && current.lon <= 180000000L &&
        current.dt >= 1262304000L && current.dt <= 2524608000L // 2010-01-01 .. 2050-01-01
	);

    RTC_Time_t rtcTime;
    TIME_GetRtcTime(&rtcTime);

	Trace(1, "** [%d-%02d-%02d %02d:%02d:%02d] GPS COORD IS %s [lat=%d, lon=%d, dt=%d] [%ld, %.06f, %.06f, %.02f, %.02f, %.02f] **",
        rtcTime.year, rtcTime.month, rtcTime.day, rtcTime.hour, rtcTime.minute, rtcTime.second,
		valid ? "VALID" : "NOT VALID",

		current.lat >= -180000000L && current.lat <= 180000000L ? 1 : 0,
		current.lon >= -180000000L && current.lon <= 180000000L ? 1 : 0,
		current.dt >= 1262304000L && current.dt <= 2524608000L ? 1 : 0,

		current.dt, (double)current.lat / 1000000, (double)current.lon / 1000000, (double)current.alt / 10, (double)current.course / 10, (double)current.speed / 10
	);
	return valid;
}

char parse_packet_nmea(char *_nmea[], bool isGGA) {
	Trace(1, "parse_packet_nmea");
    char flushed = 0;

    char *time0 = _nmea[1];
    char *slat  = _nmea[isGGA ? 2 : 3];
    bool isN    = ((*_nmea[isGGA ? 3 : 4]) == 'N');
    char *slon  = _nmea[isGGA ? 4 : 5];
    bool isE    = ((*_nmea[isGGA ? 5 : 6]) == 'E');

    double lat_start = (double)atoin(slat, 2);
    double lat_end   = (double)atof(slat + 2) / 60;
    double _lat      = lat_start + lat_end;

    double lon_start = (double)atoin(slon, 3);
    double lon_end   = (double)atof(slon + 3) / 60;
    double _lon      = lon_start + lon_end;

    ttime.hour   = atoin(time0, 2);
    ttime.minute = atoin(time0 + 2, 2);
    ttime.second = atoin(time0 + 4, 2);

    double xlat = _lat * (isN ? 1 : -1);
    double xlon = _lon * (isE ? 1 : -1);

    current.lat = xlat * 1000000;
    current.lon = xlon * 1000000;

    if (ttime.year >= 2018 && ttime.year <= 2025) {
        TIME_SetSystemTime(&ttime);
        
        RTC_Time_t rtc_time = {
            .year = ttime.year,
            .month = ttime.month,
            .day = ttime.day,
            .hour = ttime.hour,
            .minute = ttime.minute,
            .second = ttime.second,
            .timeZone = 0,
            .timeZoneMinutes = 0
        };
        TIME_SetRtcTime(&rtc_time);
    }

    long saved_dt = current.dt;
    time((time_t *)&current.dt);

    if (valid_coord()) {
        time_set = true;
        int delta = (int)(current.dt - dt_prev);

        if (delta == 0) {
            return 2; // same coordinate
        }

     //   current.accel = Mid * 1000000; // todo

      //  save_coord(FS_DEVICE_NAME_T_FLASH "/coords.txt");

        if (lat_prev == 0 || (dt_prev != 0 && (delta > 1))) { // first coord or delta(dt) > 1
            if (buffer_offset > SECTOR_SIZE - sizeof(struct Controller_ControlFrame)) { // sector overflow
                flush_sector();
                flushed = 1;
            }

            buffer_offset += controller_control_frame(&sector_buffer[buffer_offset]);
            set_bit(&ch.hdr[0], 1);
        } else {
            long lat_delta    = (current.lat    - lat_prev),
                 lon_delta    = (current.lon    - lon_prev),
                 alt_delta    = (current.alt    - alt_prev),
                 course_delta = (current.course - course_prev),
                 speed_delta  = (current.speed  - speed_prev);
            if (my_abs(lat_delta) > 127 || my_abs(lon_delta) > 127 || my_abs(alt_delta) > 127 || my_abs(course_delta) > 127 || my_abs(speed_delta) > 127) { // deltas overflow
                if (buffer_offset > SECTOR_SIZE - sizeof(struct Controller_ControlFrame)) { // sector overflow
                    flush_sector();
                    flushed = 1;
                }

                buffer_offset += controller_control_frame(&sector_buffer[buffer_offset]);
                set_bit(&ch.hdr[0], 1);
            } else {
                if (buffer_offset > SECTOR_SIZE - sizeof(struct Controller_DiffFrame)) { // sector overflow
                    flush_sector();
                    flushed = 1;

                    buffer_offset += controller_control_frame(&sector_buffer[buffer_offset]);
                    set_bit(&ch.hdr[0], 1);
                } else {
                    buffer_offset += controller_diff_frame(&sector_buffer[buffer_offset], (int)lat_delta, (int)lon_delta, (int)alt_delta, (int)course_delta, (int)speed_delta);
                    set_bit(&ch.hdr[0], 0);
                }
            }
        }

        lat_prev    = current.lat;
        lon_prev    = current.lon;
        alt_prev    = current.alt;
        course_prev = current.course;
        speed_prev  = current.speed;
        dt_prev     = current.dt;
    } else {
        current.dt = saved_dt; // something went wrong: invalid coordinate
    	return 3; // invalid coordinate
    }

    return flushed;
}
/*
void debug() {
// #ifdef DEBUG
    char buff[30];
    Dir_t* dir = API_FS_OpenDir("/");
    Dirent_t* dirent = NULL;
    while(dirent = API_FS_ReadDir(dir))
    {
        Trace(1,"INTERNAL folder:%s",dirent->d_name);
        snprintf(buff,sizeof(buff),"/%s",dirent->d_name);
        int32_t fd = API_FS_Open(buff,FS_O_RDONLY,0);
        if(fd<0)
            Trace(1,"INTERNAL open file %s fail",buff);
        else
        {
            Trace(1,"INTERNAL file %s size:%d",buff,(int)API_FS_GetFileSize(fd));
            API_FS_Close(fd);
        }

    }
    API_FS_CloseDir(dir);

    API_FS_INFO info = { 0, 0 };
    if (API_FS_GetFSInfo(FS_DEVICE_NAME_FLASH, &info) == 0) {
        int u = info.usedSize, t = info.totalSize;
        Trace(1, "INTERNAL USAGE: used %d, total %d", u, t);
    }
    Trace(1, "INTERNAL total: %ld, unsent: %ld, EXTERNAL: total: %ld, unsent: %ld",
        (long int)sectorInfo.total2, (long int)(sectorInfo.total2 - sectorInfo.sent2),
        (long int)sectorInfo.total, (long int)(sectorInfo.total - sectorInfo.sent));
// #endif
}
*/
void parse_nmea() {
    int cs = checksum();

    char *_nmea[30];
    char delims[] = ",";

    char _copy[150];
    sprintf(_copy, "%s\n", nmea);

    int i = 0;
    _nmea[i] = strtok((char *)nmea, delims);
    while (_nmea[i] != NULL && i < sizeof(_nmea) / sizeof(_nmea[0])) {
        _nmea[++ i] = strtok(NULL, delims);
    }

    if (i > 1) {
        char *ptr = strchr(_nmea[i - 1], '*');
        if (ptr != NULL) {
            int ccs = strtol(ptr + 1, NULL, 16);
            if (ccs != cs) {
                Trace(1, "* CHECKSUM ERROR (%d != %d)", ccs, cs);
                return;
            }
        } else {
            Trace(1, "* CHECKSUM ERROR #2");

            char tmp[1024];
            sprintf(tmp, "* CHECKSUM ERROR #2 [%s]", _copy);
            Trace(1, tmp);

            for (int j = 0; j < i; j ++) {
                char xx[100];
                sprintf(xx, "* CHECKSUM ERROR #2 [%d] [%s]", j, _nmea[j]);
                Trace(1, xx);
            }
        }
    } else {
        Trace(1, "* CHECKSUM ERROR #3");
    }

    if (settings->nmea) {
        WriteNmeaLog(_copy);
    }

    char packet_ok = 0, flushed = 0;

    if (strstr(_nmea[0], "GGA") != NULL) {
		tenMinutes ++; // tick every second

        if (i >= 6 && (has_date)) {
            if ((*_nmea[3] == 'N' || *_nmea[3] == 'S') && (*_nmea[5] == 'E' || *_nmea[5] == 'W')) {
                if (i >= 9 && *(_nmea[9]) != 0) {
                    current.alt = atof(_nmea[9]) * 10;
                }
            }
        }

        if (i >= 6 && (has_date)) {
            if ((*_nmea[3] == 'N' || *_nmea[3] == 'S') && (*_nmea[5] == 'E' || *_nmea[5] == 'W')) {
                flushed = parse_packet_nmea(_nmea, true);
                packet_ok = (flushed != 3);
            }
        }

        if (packet_ok) {
            print_info();
        } else {
            led_off();
            TIME_System_t sysTime;
            TIME_GetSystemTime(&sysTime);
            Trace(1, "* [%d-%02d-%02d %02d:%02d:%02d] PACKET GGA IS NOT GOOD [%s]",
                sysTime.year, sysTime.month, sysTime.day, sysTime.hour, sysTime.minute, sysTime.second,
                _copy);
        }
    } else { // RMC

        // debug();

        if (i >= 11 && (*_nmea[2]) == 'V') { // no GPS 3D fix
            // * PACKET RMC IS NOT GOOD [$GNRMC,172025.306,V,5957.0571,N,03017.9388,E,0.001,300.14,161018,,,N*54]
            char *date = _nmea[9];
            char *time0 = _nmea[1];

            memset(&ttime, 0, sizeof(TIME_System_t));

            ttime.day   = atoin(date, 2);
            ttime.month = atoin(date + 2, 2);
            ttime.year  = 2000 + atoin(date + 4, 2);

            ttime.hour   = atoin(time0, 2);
            ttime.minute = atoin(time0 + 2, 2);
            ttime.second = atoin(time0 + 4, 2);
        }

        if (i >= 11 && (*_nmea[2]) == 'A') { // GPS 3D fix
            char *date = _nmea[9];

            memset(&ttime, 0, sizeof(TIME_System_t));

            ttime.day   = atoin(date, 2);
            ttime.month = atoin(date + 2, 2);
            ttime.year  = 2000 + atoin(date + 4, 2);

            if (*(_nmea[7]) != 0) {
                current.speed = my_abs(atof(_nmea[7]) * 0.5144 * 10);
            }

            if (*(_nmea[8]) != 0) {
                current.course = atof(_nmea[8]) * 10;
                current.course = (current.course > 3600 ? 0 : current.course); // fix A9G ?
            }

            has_date = true;

            if (i >= 6 && (has_date)) {
                if ((*_nmea[4] == 'N' || *_nmea[4] == 'S') && (*_nmea[6] == 'E' || *_nmea[6] == 'W')) {
                    flushed = parse_packet_nmea(_nmea, false);
                    packet_ok = (flushed != 3);
                }
            }
        }

        if (!packet_ok) {
            Trace(1, "* PACKET RMC IS NOT GOOD [%s]", _copy);
        }
    }

    if (packet_ok) {
        // TOGO GET ACCEL VALUE

        if (flushed == 1) {
            current_coordinate[0] = 0; // flush buffer
        }

        if (flushed != 2) {
            if (current_coordinate[0] < BUFFERED_COORDINATES) {
                memcpy(&current_coordinate[1 + sizeof(current) * current_coordinate[0]], &current, sizeof(current)); // add tail
                current_coordinate[0] ++; // this will send to network

                Trace(1, "** ZZZ CURRENT BUFFER IS %d ***", current_coordinate[0]);
            }
    	}

		Trace(1, "* PACKET IS GOOD [%s]", _copy);
    }
}

void gps_received(int length, uint8_t *buffer) {
	// start_sentence = 0;
    Trace(1, buffer);

    // WriteNmeaLogParts(buffer, length);

	for (int i = 0; i < length; i ++) {
		if (buffer[i] == '$') {
			start_sentence = 1;
			nmea_index = 0;
        }
		if (start_sentence) {
            if (nmea_index < GPS_BUFFER_SIZE - 1) {
                if (buffer[i] == '\r' || buffer[i] == '\n') {
                    if ((strncmp((const char *)nmea, "$GPRMC", 6) == 0 || strncmp((const char *)nmea, "$GNRMC", 6) == 0) ||
                        (strncmp((const char *)nmea, "$GPGGA", 6) == 0 || strncmp((const char *)nmea, "$GNGGA", 6) == 0) ||
                        (strncmp((const char *)nmea, "$GLRMC", 6) == 0 || strncmp((const char *)nmea, "$GLGGA", 6) == 0)) {
                        parse_nmea();
                    }

                    nmea[0] = start_sentence = 0;
                } else {
                    nmea[nmea_index] = buffer[i];
                    nmea[nmea_index + 1] = 0;
                    nmea_index ++;
                }
            } else {
                nmea[0] = start_sentence = 0;
            }
		}
	}
}

static GPIO_config_t gpsINT = { // GPS INT
    .mode         = GPIO_MODE_OUTPUT,
    .pin          = GPIO_PIN2,
    .defaultLevel = GPIO_LEVEL_LOW
};

static GPIO_config_t gpsPWR = { // GPS VCC
    .mode         = GPIO_MODE_OUTPUT,
    .pin          = GPIO_PIN25,
    .defaultLevel = GPIO_LEVEL_LOW
};

void gps_init() {
    if (gpsType == GPS_TYPE__A9G) {
        GPS_Init();
    } else {
        static UART_Config_t uart_config = {
            .dataBits   = UART_DATA_BITS_8,
            .stopBits   = UART_STOP_BITS_1,
            .parity     = UART_PARITY_NONE,
            .rxCallback = NULL,
            .useEvent   = true
        };
        uart_config.baudRate = (
            gpsType == GPS_TYPE__ORIGIN ?
                UART_BAUD_RATE_4800 :
                UART_BAUD_RATE_9600);
        UART_Init(UART1, uart_config);

        GPIO_Init(gpsPWR);
        GPIO_Init(gpsINT);
    }
}

void gps_on() {
   // Trace(1,"GPS_ON");
    if (gpsType == GPS_TYPE__A9G) {
        GPS_Open(NULL);
    } else if (gpsType == GPS_TYPE__ELA) {
        GPIO_SetLevel(gpsPWR, GPIO_LEVEL_HIGH);

        GPIO_SetLevel(gpsINT, GPIO_LEVEL_LOW);
        OS_Sleep(500);
        GPIO_SetLevel(gpsINT, GPIO_LEVEL_HIGH);
    } else if (gpsType == GPS_TYPE__ORIGIN) {
        Trace(1, "XGPS ENABLING");

        GPIO_SetLevel(gpsINT, GPIO_LEVEL_LOW);
        OS_Sleep(500);
        GPIO_SetLevel(gpsINT, GPIO_LEVEL_HIGH);
        OS_Sleep(100);
        GPIO_SetLevel(gpsINT, GPIO_LEVEL_LOW);

        OS_Sleep(500);
        
        char command[100];
        //UART_Write(UART1, command, sprintf(command, "$PMTK225,9*22\r\n")); // Always Locate
        UART_Write(UART1, command, sprintf(command, "$PMTK225,0*2B\r\n")); // Full Power
    }
}

void gps_off() {
    if (gpsType == GPS_TYPE__A9G) {
        GPS_Close();
    } else if (gpsType == GPS_TYPE__ELA) {
        GPIO_SetLevel(gpsINT, GPIO_LEVEL_LOW);
        GPIO_SetLevel(gpsPWR, GPIO_LEVEL_LOW);
    } else if (gpsType == GPS_TYPE__ORIGIN) {
        Trace(1, "XGPS DISABLING");

        char command[100];
        UART_Write(UART1, command, sprintf(command, "$PMTK161,0*28\r\n")); // Standby Mode

        GPIO_SetLevel(gpsINT, GPIO_LEVEL_LOW);
        OS_Sleep(500);
        GPIO_SetLevel(gpsINT, GPIO_LEVEL_HIGH);
        OS_Sleep(100);
        GPIO_SetLevel(gpsINT, GPIO_LEVEL_LOW);


        OS_Sleep(500);
        GPIO_SetLevel(gpsINT, GPIO_LEVEL_HIGH);
        OS_Sleep(100);
        GPIO_SetLevel(gpsINT, GPIO_LEVEL_LOW);
    }
}
