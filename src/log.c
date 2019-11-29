#include "a9.h"

void WriteLog(const char *str) {
	OS_LockMutex(log_lock);

    int32_t fd = API_FS_Open(LOG_FILE_NAME, FS_O_APPEND | FS_O_RDWR | FS_O_CREAT, 0);
    if (fd >= 0) {
        TIME_System_t sysTime;
        TIME_GetSystemTime(&sysTime);
        char temp[1024];
        API_FS_Write(fd, (uint8_t *)temp, sprintf(temp, "[%d-%02d-%02d %02d:%02d:%02d %ld] [%s] %s\n",
            sysTime.year, sysTime.month, sysTime.day, sysTime.hour, sysTime.minute, sysTime.second, current.dt,
            settings->SOFTWARE_VERSION,
            str));

        Trace(1, temp);

        API_FS_Flush(fd);
        API_FS_Close(fd);
    }

    OS_UnlockMutex(log_lock);
}


void WriteNmeaLog(const char *str) {
	OS_LockMutex(log_lock);

    int32_t fd = API_FS_Open(NMEA_LOG_NAME, FS_O_APPEND | FS_O_RDWR | FS_O_CREAT, 0);
    if (fd >= 0) {
        API_FS_Write(fd, (uint8_t *)str, strlen(str));

        API_FS_Flush(fd);
        API_FS_Close(fd);
    }

    OS_UnlockMutex(log_lock);
}

void WriteNmeaLogParts(uint8_t *str, int length) {
    int32_t fd = API_FS_Open(NMEA_PARTS_LOG_NAME, FS_O_APPEND | FS_O_RDWR | FS_O_CREAT, 0);
    if (fd >= 0) {
        TIME_System_t sysTime;
        TIME_GetSystemTime(&sysTime);
        char temp[1024];
        API_FS_Write(fd, (uint8_t *)temp, sprintf(temp, "[%d-%02d-%02d %02d:%02d:%02d] [",
            sysTime.year, sysTime.month, sysTime.day, sysTime.hour, sysTime.minute, sysTime.second));

        API_FS_Write(fd, str, length);

        API_FS_Write(fd, (uint8_t *)temp, sprintf(temp, "]\r\n"));

        API_FS_Flush(fd);
        API_FS_Close(fd);
    }
}

void airplane_log(const char *s) {
    OS_LockMutex(airplane_log_lock);

    int32_t fd = API_FS_Open(AIRPLANE_NAME, FS_O_APPEND | FS_O_RDWR | FS_O_CREAT, 0);
    if (fd >= 0) {
        char battery = GetVoltage();//0;
        //PM_Voltage((uint8_t *)&battery);

        TIME_System_t sysTime;
        TIME_GetSystemTime(&sysTime);

        char temp[1024];
        API_FS_Write(fd, (uint8_t *)temp, sprintf(temp, "[%d-%02d-%02d %02d:%02d:%02d] [%s] battery=%d, network_state=%d, message=%s\n",
            sysTime.year, sysTime.month, sysTime.day, sysTime.hour, sysTime.minute, sysTime.second,
            settings->SOFTWARE_VERSION,
            battery, network_state, s));

        API_FS_Flush(fd);
        API_FS_Close(fd);
    }

    OS_UnlockMutex(airplane_log_lock);
}

