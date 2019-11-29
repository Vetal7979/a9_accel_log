#include "a9.h"

// settings --------------------------------------------------------------------------------------

char /*enum HourProfile*/ Profile[24] = {
    // 06:00, 07:00, 08:00, 09:00, 10:00, 11:00 разовая передача
    // 12:00-14:00 онлайн
    // 14:00, 15:00 разовая передача
    // 16:00-18:00 режим самописца, модем выключен
    // 18:00 разовая передача
    // 19:00-06:00 полный сон (gps/gsm выключены)
    // 6h*10mA(60mAh) + 6h*95mA(570mAh) + 2h*200mA(400mAh) + 2h*85mA(170mAh) + 1h*95mA(95mAh) + 5h*10mA(50mAh) = 1345mAh (24h)
    HP__Online, // HP__Sleep,   // 00:00 GMT
    HP__Online, // HP__Sleep,   // 01:00 GMT
    HP__Online, // HP__Sleep,   // 02:00 GMT
    HP__Online, // HP__Sleep,   // 03:00 GMT
    HP__Online, // HP__Sleep,   // 04:00 GMT
    HP__Online, // HP__Sleep,   // 05:00 GMT
    HP__Online, // HP__Hourly,  // 06:00 GMT +
    HP__Online, // HP__Hourly,  // 07:00 GMT +
    HP__Online, // HP__Hourly,  // 08:00 GMT +
    HP__Online, // HP__Hourly,  // 09:00 GMT +
    HP__Online, // HP__Hourly,  // 10:00 GMT +
    HP__Online, // HP__Hourly,  // 11:00 GMT +
    HP__Online, // HP__Hourly,  // 12:00 GMT +
    HP__Online, // HP__Hourly,  // 13:00 GMT +
    HP__Online, // HP__Hourly,  // 14:00 GMT +
    HP__Online, // HP__Hourly,  // 15:00 GMT +
    HP__Online, // HP__Hourly,  // 16:00 GMT +
    HP__Online, // HP__Hourly,  // 17:00 GMT +
    HP__Online, // HP__Hourly,  // 18:00 GMT +
    HP__Online, // HP__Sleep,   // 19:00 GMT
    HP__Online, // HP__Sleep,   // 20:00 GMT
    HP__Online, // HP__Sleep,   // 21:00 GMT
    HP__Online, // HP__Sleep,   // 22:00 GMT
    HP__Online  // HP__Sleep    // 23:00 GMT
};
char /*enum HourProfile*/ now = HP__Offline;

enum values {
    PARAMS__LOG = 0,
    PARAMS__NETWORK_LOG,
    PARAMS__AIRPLANE_LOG,
    PARAMS__DOMAIN,
    PARAMS__PORT,
    PARAMS__APN_APN,
    PARAMS__APN_USER,
    PARAMS__APN_PASSWD,
    PARAMS__ADC,
    PARAMS__PROFILE,
    PARAMS__NMEA
};
const char *params[] = {
    "log",
    "network_log",
    "airplane_log",
    "domain",
    "port",
    "apn_apn",
    "apn_user",
    "apn_passwd",
    "adc",
    "profile",
    "nmea",
    NULL
};
/*struct Settings settings = {
    .domain = "trackers.regatav1.ru",
    .IMEI = "",
    .write_log = false,
    .airplane_log = false,
    .network_log = false,
    .port = 80,
    .apn_apn = "internet",
    .apn_user = "",
    .apn_password = "",
    .SOFTWARE_VERSION = "01234567890",
    .adc = false,
    .nmea = false
};*/
struct Settings *settings = NULL;

int settings_freadline(char *line, int maxlen, int32_t ffp) {
    int rd = 0;
    for (; rd < maxlen; rd++) {
        if (API_FS_Read(ffp, &line[rd], 1) != 1) {
            break;
        }
        if (line[rd] == '\r' || line[rd] == '\n') {
            if (rd == 0) {
                rd--;
                continue;
            }
            line[rd] = 0;
            break;
        }
    }
    return rd;
}

void settings_load() {
    int32_t ffd = API_FS_Open(SETTINGS_FILE_NAME, FS_O_RDONLY, 0);
    if (ffd >= 0) {
        while (true) {
            char line[100];
            int n = settings_freadline(line, sizeof(line), ffd);
            if (n > 0) {
                line[n] = 0;
                const char *p = strchr(line, '=');
                if (p != NULL) {
                    p ++;
                    if (p && *p) {
                        for (int i = 0; params[i]; i++) {
                            if (strncmp(line, params[i], strlen(params[i])) == 0) {
                                switch (i) {
                                case PARAMS__LOG:
                                    settings->write_log = (strncmp(p, "1", 1) == 0);
                                    break;
                                case PARAMS__NETWORK_LOG:
                                    settings->network_log = (strncmp(p, "1", 1) == 0);
                                    break;
                                case PARAMS__NMEA:
                                    settings->nmea = (strncmp(p, "1", 1) == 0);
                                    break;
                                case PARAMS__AIRPLANE_LOG:
                                    settings->airplane_log = (strncmp(p, "1", 1) == 0);
                                    break;
                                case PARAMS__ADC:
                                    settings->adc = (strncmp(p, "1", 1) == 0);
                                    break;
                                case PARAMS__PORT: {
                                        int port = atoi(p);
                                        if (port > 0 && port < 65535) {
                                            settings->port = port;
                                        }
                                    }
                                    break;
                                case PARAMS__DOMAIN:
                                    if (strlen(p) > 0) {
                                        strcpy(settings->domain, p);
                                    }
                                    break;
								case PARAMS__PROFILE: {
                                        int val = atoi(p), pp = strlen(params[i]);
                                        if (val >= HP__Sleep && val <= HP__Online) {
                                            if (strncmp(&line[pp], "default=", 8) == 0) {
                                                for (int ii = 0; ii < 24; ii++) {
                                                    Profile[ii] = (char)/*(enum HourProfile)*/val;
                                                }
                                            }
                                            else {
                                                for (int ii = 0; ii < 24; ii++) {
                                                    char prefix[4];
                                                    if (strncmp(&line[pp], prefix, sprintf(prefix, "%d=", ii)) == 0) {
                                                        Profile[ii] = (char)/*(enum HourProfile)*/val;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    break;
                                case PARAMS__APN_APN:
                                    if (strlen(p) > 0) {
                                        strcpy(settings->apn_apn, p);
                                    }
                                    break;
                                case PARAMS__APN_USER:
                                    if (strlen(p) > 0) {
                                        strcpy(settings->apn_user, p);
                                    }
                                    break;
                                case PARAMS__APN_PASSWD:
                                    if (strlen(p) > 0) {
                                        strcpy(settings->apn_password, p);
                                    }
                                    break;
                                }
                                break;
                            }
                        }
                    }
                }
            } else {
                break;
            }
        }
        API_FS_Close(ffd);
    }

    load_profile();

    if (settings->write_log) {
    	char temp[1000];
    	sprintf(temp, "CONFIG [log: %d, domain: %s, port: %d, apn_apn: %s, apn_user: %s, apn_passwd: %s, imei: %s, nmea: %d]",
    		settings->write_log, settings->domain, settings->port, settings->apn_apn, settings->apn_user, settings->apn_password, settings->IMEI, settings->nmea);
    	WriteLog(temp);
    }
}
