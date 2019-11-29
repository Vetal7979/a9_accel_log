// https://ai-thinker-open.github.io/GPRS_C_SDK_DOC/en/c-sdk/function-api/file-system.html

#include "a9.h"
#include "9250.h"
#include "api_hal_i2c.h"

#define SOFTWARE_DATE "191129"
enum GPS_Type gpsType = GPS_TYPE__ELA; // GPS_TYPE__A9G; // GPS_TYPE__ELA; // GPS_TYPE__ORIGIN;

static HANDLE mainTaskHandle = NULL, thirdTaskHandle = NULL;
uint8_t signal = 0, gprs_event_state = 0;

double Mid;

extern float _ax, _ay, _az;
extern int16_t _axcounts,_aycounts,_azcounts;
extern  float _accelScale;
// bool xret = false;

#ifdef NEW_PLATA
static GPIO_config_t selfOff = {
    .mode         = GPIO_MODE_OUTPUT,
    .pin          = GPIO_PIN26,
    //!!! .defaultLevel = GPIO_LEVEL_HIGH
    .defaultLevel = GPIO_LEVEL_LOW
};

void OnPinFalling(GPIO_INT_callback_param_t* param) {
    Trace(1,"XYZ OnPinFalling");
    switch (param->pin) {
        case GPIO_PIN3:
            Trace(1,"XYZ gpio3 detect falling edge!");
            GPIO_LEVEL statusNow;
            GPIO_Get(GPIO_PIN2,&statusNow);
            Trace(1,"XYZ gpio3 status now:%d",statusNow);
            Trace(1,"XYZ gpio3 done");

            if (recs > 0) { // save any unsaved data
                flush_sector();
            }
            gps_off();
            Trace(1,"XYZ gpio3 killing self");
            // PM_ShutDown();
            //!!! GPIO_SetLevel(selfOff, GPIO_LEVEL_LOW);
            GPIO_SetLevel(selfOff, GPIO_LEVEL_HIGH);
            Trace(1,"XYZ gpio3 killed self");

            break;
        default:
            break;
    }
}
#endif

bool powerKeyDown = false;

void EventDispatchButtonOnly(API_Event_t *pEvent) {
    switch (pEvent->id) {
    // BUTTONS --------------------------------------

    case API_EVENT_ID_KEY_DOWN:
        if (pEvent->param1 == KEY_POWER) {
            powerKeyDown = true;
        }
        break;

    case API_EVENT_ID_KEY_UP:
        if (pEvent->param1 == KEY_POWER) {
            if (powerKeyDown) {
                if (recs > 0) { // save any unsaved data
                    flush_sector();
                }

                gps_off();                
	        	PM_ShutDown();
	        }
        }
        break;
    default:
        break;
    }
}
void EventDispatchButton(API_Event_t *pEvent) {
    switch (pEvent->id) {
    // BUTTONS --------------------------------------

    case API_EVENT_ID_KEY_DOWN:
        if (pEvent->param1 == KEY_POWER) {
            powerKeyDown = true;
        }
        break;

    case API_EVENT_ID_KEY_UP:
        if (pEvent->param1 == KEY_POWER) {
            if (powerKeyDown) {
                if (recs > 0) { // save any unsaved data
                    flush_sector();
                }

                gps_off();
	        	PM_ShutDown();
	        }
        }
        break;

    // BATTERY INFO ---------------------------------

    case API_EVENT_ID_POWER_INFO: {
            switch (pEvent->param1 >> 16) {
            case PM_CHARGER_STATE_CHRGING:
            case PM_CHARGER_STATE_FINISHED:
                //charging = true;
                break;
            default:
                //charging = false;
                break;
            }

            switch (pEvent->param2 >> 16) {
            case PM_BATTERY_STATE_LOW:
            case PM_BATTERY_STATE_CRITICAL:
            case PM_BATTERY_STATE_SHUTDOWN:
                //lowbattery = true;
                break;
            default:
                //lowbattery = false;
                break;
            }
        }
        break;

    default:
        EventDispatchSocket(pEvent);
        break;
    }
}

long last_gps_receive_time = 0;
#include <time.h>

void EventDispatch(API_Event_t *pEvent) {
    uint8_t status = 0;

    /*char temp[100];
    sprintf(temp, "XGPS last_gps_receive_time=%d", time(NULL) - last_gps_receive_time);
    Trace(1, temp);*/

    //if (gpsType != GPS_TYPE__A9G) {
        if (time(NULL) - last_gps_receive_time >= 5) {
            gps_on();
            last_gps_receive_time = time(NULL);
        }
    //}

    // Trace(1,"TRACKER 9 EVENT %d", pEvent->id);
    switch (pEvent->id) {
        // GPS ------------------------------------------

        case API_EVENT_ID_GPS_UART_RECEIVED:
            if (gpsType == GPS_TYPE__A9G) {
                gps_received(pEvent->param1, pEvent->pParam1);
            }
            break;

        case API_EVENT_ID_UART_RECEIVED:
            if (pEvent->param1 == UART1) {
                if (gpsType != GPS_TYPE__A9G) {
                    last_gps_receive_time = time(NULL);
                    gps_received(pEvent->param2, pEvent->pParam1);
                }
            }
            break;

        // GPRS -----------------------------------------

        case API_EVENT_ID_NO_SIMCARD:
        if (settings->network_log) WriteLog("API_EVENT_ID_NO_SIMCARD");
            break;
        
        case API_EVENT_ID_SIMCARD_DROP:
        if (settings->network_log) WriteLog("API_EVENT_ID_SIMCARD_DROP");
            break;
        
        case API_EVENT_ID_SIGNAL_QUALITY: {
            signal = (uint8_t)pEvent->param1;
            break;
        }
        
        case API_EVENT_ID_NETWORK_REGISTERED_HOME:
        case API_EVENT_ID_NETWORK_REGISTERED_ROAMING: {
            gprs_event_state = 1; // registered

            if (settings->airplane_log) airplane_log("Event=API_EVENT_ID_NETWORK_REGISTERED");

            if (settings->network_log) WriteLog("API_EVENT_ID_NETWORK_REGISTERED");
            Trace(1, "**** INTERNET [imei:%s, ver:%s] event [API_EVENT_ID_NETWORK_REGISTERED]",
                settings->IMEI, settings->SOFTWARE_VERSION);

            Network_GetAttachStatus(&status);
            if (status == 0) {
                Network_StartAttach();
            } else {
                Network_PDP_Context_t context;
                sprintf(context.apn, "%s", settings->apn_apn);
                sprintf(context.userName, "%s", settings->apn_user);
                sprintf(context.userPasswd, "%s", settings->apn_password);
                Network_StartActive(context);
            }
            break;
        }
        case API_EVENT_ID_NETWORK_REGISTER_SEARCHING:
            gprs_event_state = 2; // searching

            if (settings->airplane_log) airplane_log("Event=API_EVENT_ID_NETWORK_REGISTER_SEARCHING");

        if (settings->network_log) WriteLog("API_EVENT_ID_NETWORK_REGISTER_SEARCHING");
            Trace(1, "**** INTERNET [imei:%s, ver:%s] event [API_EVENT_ID_NETWORK_REGISTER_SEARCHING]",
                settings->IMEI, settings->SOFTWARE_VERSION);

            break;
        
        case API_EVENT_ID_NETWORK_REGISTER_DENIED:
            gprs_event_state = 3; // register denied

            if (settings->airplane_log) airplane_log("Event=API_EVENT_ID_NETWORK_REGISTER_DENIED");

        if (settings->network_log) WriteLog("API_EVENT_ID_NETWORK_REGISTER_DENIED");
            Trace(1, "**** INTERNET [imei:%s, ver:%s] event [API_EVENT_ID_NETWORK_REGISTER_DENIED]",
                settings->IMEI, settings->SOFTWARE_VERSION);
            break;

        case API_EVENT_ID_NETWORK_REGISTER_NO:
            gprs_event_state = 4; // registered_no

            if (settings->airplane_log) airplane_log("Event=API_EVENT_ID_NETWORK_REGISTER_NO");
            if (settings->network_log) WriteLog("API_EVENT_ID_NETWORK_REGISTER_NO");
            break;
        
        case API_EVENT_ID_NETWORK_DETACHED:
            gprs_event_state = 5; // detached

            if (settings->airplane_log) airplane_log("Event=API_EVENT_ID_NETWORK_DETACHED");

        if (settings->network_log) WriteLog("API_EVENT_ID_NETWORK_DETACHED");
            Trace(1, "**** INTERNET [imei:%s, ver:%s] event [API_EVENT_ID_NETWORK_DETACHED]",
                settings->IMEI, settings->SOFTWARE_VERSION);
            break;
        
        case API_EVENT_ID_NETWORK_ATTACH_FAILED:
            gprs_event_state = 6; // attach failed

            if (settings->airplane_log) airplane_log("Event=API_EVENT_ID_NETWORK_ATTACH_FAILED");

            if (settings->network_log) WriteLog("API_EVENT_ID_NETWORK_ATTACH_FAILED");
                Trace(1, "**** INTERNET [imei:%s, ver:%s] event [API_EVENT_ID_NETWORK_ATTACH_FAILED]",
                    settings->IMEI, settings->SOFTWARE_VERSION);

            break;

        case API_EVENT_ID_NETWORK_ATTACHED:
            gprs_event_state = 7; // attached

            if (settings->airplane_log) airplane_log("Event=API_EVENT_ID_NETWORK_ATTACHED");

            if (settings->network_log) WriteLog("API_EVENT_ID_NETWORK_ATTACHED");
                Trace(1, "**** INTERNET [imei:%s, ver:%s] event [API_EVENT_ID_NETWORK_ATTACHED]",
                    settings->IMEI, settings->SOFTWARE_VERSION);

            Network_PDP_Context_t context;
            sprintf(context.apn, "%s", settings->apn_apn);
            sprintf(context.userName, "%s", settings->apn_user);
            sprintf(context.userPasswd, "%s", settings->apn_password);
            Network_StartActive(context);
            break;

        case API_EVENT_ID_NETWORK_DEACTIVED:
            gprs_event_state = 8; // deactivated

            if (settings->airplane_log) airplane_log("Event=API_EVENT_ID_NETWORK_DEACTIVED");

        if (settings->network_log) WriteLog("API_EVENT_ID_NETWORK_DEACTIVED");
            Trace(1, "**** INTERNET [imei:%s, ver:%s] event [API_EVENT_ID_NETWORK_DEACTIVED]",
                settings->IMEI, settings->SOFTWARE_VERSION);
            break;
        
        case API_EVENT_ID_NETWORK_ACTIVATE_FAILED:
            gprs_event_state = 9; // activate failed

            if (settings->airplane_log) airplane_log("Event=API_EVENT_ID_NETWORK_ACTIVATE_FAILED");

            if (settings->network_log) WriteLog("API_EVENT_ID_NETWORK_ACTIVATE_FAILED");
                Trace(1, "**** INTERNET [imei:%s, ver:%s] event [API_EVENT_ID_NETWORK_ACTIVATE_FAILED]",
                    settings->IMEI, settings->SOFTWARE_VERSION);

            break;
        
        case API_EVENT_ID_NETWORK_ACTIVATED:
            gprs_event_state = 10; // activated

            if (settings->airplane_log) airplane_log("Event=API_EVENT_ID_NETWORK_ACTIVATED");

        if (settings->network_log) WriteLog("API_EVENT_ID_NETWORK_ACTIVATED");
            Trace(1, "**** INTERNET [imei:%s, ver:%s] event [API_EVENT_ID_NETWORK_ACTIVATED]",
                settings->IMEI, settings->SOFTWARE_VERSION);
            break;

        case API_EVENT_ID_NETWORK_GOT_TIME:
            break;

        // GPRS -----------------------------------------
#ifdef DEBUG_LOG
        case API_EVENT_ID_NETWORK_CELL_INFO: {
            uint8_t number = pEvent->param1;
            Network_Location_t* location = (Network_Location_t*)pEvent->pParam1;
                    
            int32_t fd = API_FS_Open(FS_DEVICE_NAME_T_FLASH "/debug.txt", FS_O_APPEND | FS_O_RDWR | FS_O_CREAT, 0);
            if (fd >= 0) {
                char battery = GetVoltage();
                //PM_Voltage((uint8_t *)&battery);

                TIME_System_t sysTime;
                TIME_GetSystemTime(&sysTime);

                char temp[1024];
                API_FS_Write(fd, (uint8_t *)temp, sprintf(temp, "[%d-%02d-%02d %02d:%02d:%02d] [%s] battery=%d, network_state=%d, network_cells=%d, signal=%d, gprs_event_state=%d\n",
                    sysTime.year, sysTime.month, sysTime.day, sysTime.hour, sysTime.minute, sysTime.second,
                    settings->SOFTWARE_VERSION,
                    battery, network_state, number, signal, gprs_event_state));

                for (int i = 0; i < number; i ++) {
                    API_FS_Write(fd, (uint8_t *)temp, sprintf(temp, "cell=%d\t%d\t%d\t%d\t%d\t%d\r\n",
                        i, location[i].sCellID, location[i].iBsic, location[i].iRxLev, location[i].iRxLevSub, location[i].nArfcn));
                }

                API_FS_Flush(fd);
                API_FS_Close(fd);
            }
        }
        break;
#endif // DEBUG_LOG

        default:
            EventDispatchButton(pEvent);
            break;
    }
}

void network_callback(Network_Status_t status) {
    if (settings->network_log) {
        char temp[1024];
        sprintf(temp, "network_callback: %d", status);
        WriteLog(temp);
    }
}

void AirplaneMode(void *pData) {
    /*while (true) {
        int sizeUsed = 0, sizeTotal = 0;
        char temp[100];
        API_FS_INFO info;

        if (API_FS_GetFSInfo(FS_DEVICE_NAME_T_FLASH, &info) == 0) {
            sizeUsed  = info.usedSize;
            sizeTotal = info.totalSize;

            sprintf(temp, "NETWORK #2: SD CARD: total=[%d], used=[%d]", sizeTotal, sizeUsed);
        } else {
            sprintf(temp, "NETWORK #2: SD CARD - ERROR API_FS_GetFSInfo()");
        }
        Trace(1, temp);

        if (API_FS_GetFSInfo(FS_DEVICE_NAME_FLASH, &info) == 0) {
            sizeUsed  = info.usedSize;
            sizeTotal = info.totalSize;

            // NETWORK #2: FLASH : total=[383488], used=[23552] => 359936 free (703 * 512)
            sprintf(temp, "NETWORK #2: FLASH : total=[%d], used=[%d]", sizeTotal, sizeUsed);
        } else {
            sprintf(temp, "NETWORK #2: FLASH - ERROR API_FS_GetFSInfo()");
        }
        Trace(1, temp);

        OS_Sleep(1000);
    }*/

    TIME_System_t sysTime;
    while (!time_set) {
        // Trace(1, "** XRET=%d", xret);

        Trace(1, "CONFIG [log: %d, domain: %s, port: %d, apn_apn: %s, apn_user: %s, apn_passwd: %s, imei: %s, nmea: %d]",
    		settings->write_log, settings->domain, settings->port, settings->apn_apn, settings->apn_user, settings->apn_password, settings->IMEI, settings->nmea);

        Trace(1, "*** [profile_read=%d, profile_del=%d, profile_upd=%d] AIRPLANE PROFILE WAITING FOR TIME [%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
            profile_read, profile_del, profile_upd,
            Profile[0], Profile[1], Profile[2], Profile[3], 
            Profile[4], Profile[5], Profile[6], Profile[7], 
            Profile[8], Profile[9], Profile[10], Profile[11], 
            Profile[12], Profile[13], Profile[14], Profile[15], 
            Profile[16], Profile[17], Profile[18], Profile[19], 
            Profile[20], Profile[21], Profile[22], Profile[23]);

        TIME_GetSystemTime(&sysTime);
        if (sysTime.year >= 2018 && sysTime.year <= 2025) {
            break;
        }
        OS_Sleep(1000);
    }

    // initial value
    TIME_GetSystemTime(&sysTime);
    now = Profile[sysTime.hour];

    Trace(1, "*** [now=%d] AIRPLANE PROFILE continuing [%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
        profile_read,
        Profile[0], Profile[1], Profile[2], Profile[3], 
        Profile[4], Profile[5], Profile[6], Profile[7], 
        Profile[8], Profile[9], Profile[10], Profile[11], 
        Profile[12], Profile[13], Profile[14], Profile[15], 
        Profile[16], Profile[17], Profile[18], Profile[19], 
        Profile[20], Profile[21], Profile[22], Profile[23]);


    /*bool flight = false;
    if (now == HP__Online) {
        flight = true;
    }*/

    bool initial = true;
    while (true) {
        Trace(1, "CONFIG [log: %d, domain: %s, port: %d, apn_apn: %s, apn_user: %s, apn_passwd: %s, imei: %s, nmea: %d]",
    		settings->write_log, settings->domain, settings->port, settings->apn_apn, settings->apn_user, settings->apn_password, settings->IMEI, settings->nmea);

        // get current time and check if we need to wake up
        TIME_GetSystemTime(&sysTime);
        Trace(1, "*** [profile_read=%d, profile_del=%d, profile_upd=%d] %d batt [%d-%02d-%02d %02d:%02d:%02d] AIRPLANE [now: %d] minute = %d, sent = %ld, total = %ld, unsent = %d",
            profile_read, profile_del, profile_upd,
            GetVoltage(),
            sysTime.year, sysTime.month, sysTime.day, sysTime.hour, sysTime.minute, sysTime.second,
            now, sysTime.minute, (long int)sectorInfo.sent, (long int)sectorInfo.total, (int)(sectorInfo.total - sectorInfo.sent));
        if (sysTime.minute == 0 || ProfileUpdated || initial /*|| flight*/) { // every hour
            now = Profile[sysTime.hour];
            char /*enum HourProfile*/ previous = Profile[sysTime.hour == 0 ? 23 : sysTime.hour - 1];
            if (ProfileUpdated) { // updated by server
                previous = now;
                ProfileUpdated = false;
            }

            Trace(1, "*** AIRPLANE profile switch [was: %d, now: %d]", previous, now);

            bool wasInitial = initial;
            initial = false;

            if (now == HP__Online) {
                Trace(1, "*** AIRPLANE entering online mode");
                if (previous != now || wasInitial) {
                    // flight = false;

                    Trace(1, "*** AIRPLANE turn ON GSM");
                    Network_SetFlightMode(false); // turn on GSM

                    Trace(1, "*** AIRPLANE start network attach");
                    OS_Sleep(30 * 1000);

                    Network_StartAttach();
                    Network_PDP_Context_t context;
                    sprintf(context.apn, "%s", settings->apn_apn);
                    sprintf(context.userName, "%s", settings->apn_user);
                    sprintf(context.userPasswd, "%s", settings->apn_password);
                    Network_StartActive(context);

                    Trace(1, "*** AIRPLANE network attached");
                }

                uint8_t startHour = sysTime.hour;
                while (startHour == sysTime.hour) {
                    Trace(1, "** AIRPLANE WHILE [now=%d]", Profile[sysTime.hour]);

                    TIME_GetSystemTime(&sysTime);
                    if (startHour != sysTime.hour || Profile[sysTime.hour] != HP__Online) {
                        Trace(1, "** AIRPLANE new hour, new rules #1");
                        break; // new hour, new rules
                    }

                    // online
                    while (network_state == 2 || network_state == 3) {
                        Trace(1, "** AIRPLANE ONLINE [now=%d]", Profile[sysTime.hour]);
                        OS_Sleep(10 * 1000);

                        TIME_GetSystemTime(&sysTime);
                        if (startHour != sysTime.hour || Profile[sysTime.hour] != HP__Online) {
                            Trace(1, "** AIRPLANE new hour, new rules #2");
                            break; // new hour, new rules
                        }
                    }
                    if (startHour != sysTime.hour || Profile[sysTime.hour] != HP__Online) {
                        Trace(1, "** AIRPLANE new hour, new rules #3");
                        break; // new hour, new rules
                    }

                    // offline
                    long offline_seconds = 0;
                    while (!(network_state == 2 || network_state == 3)) {
                        Trace(1, "** AIRPLANE OFFLINE [now=%d]", Profile[sysTime.hour]);

                        TIME_GetSystemTime(&sysTime);
                        if (startHour != sysTime.hour || Profile[sysTime.hour] != HP__Online) {
                            Trace(1, "** AIRPLANE new hour, new rules #4");
                            break; // new hour, new rules
                        }

                        offline_seconds += 10;

                        if (offline_seconds >= FLIGHT_MODE_OFFLINE) {
                            if (settings->airplane_log) airplane_log("SET FLIGHT MODE ON");

                            Trace(1, "** AIRPLANE SET FLIGHT MODE ON");
                            Network_SetFlightMode(true);
                            OS_Sleep(FLIGHT_MODE_TIMEOUT);
                            Trace(1, "** AIRPLANE SET FLIGHT MODE OFF");

                            if (settings->airplane_log) airplane_log("SET FLIGHT MODE OFF");
                            Network_SetFlightMode(false);

                            OS_Sleep(30 * 1000);

                            Network_StartAttach();
                            Network_PDP_Context_t context;
                            sprintf(context.apn, "%s", settings->apn_apn);
                            sprintf(context.userName, "%s", settings->apn_user);
                            sprintf(context.userPasswd, "%s", settings->apn_password);
                            Network_StartActive(context);

                            offline_seconds = 0;
                        }

                        OS_Sleep(10 * 1000);
                    }

                    OS_Sleep(10 * 1000);
                }

                Trace(1, "** AIRPLANE HP__Online finished");
            } else if (now == HP__Hourly || previous == HP__Hourly) {
                Trace(1, "*** AIRPLANE entering hourly mode, sent = %ld, total = %ld, unsent = %d", (long int)sectorInfo.sent, (long int)sectorInfo.total, (int)(sectorInfo.total - sectorInfo.sent));
                if ((sectorInfo.sent != UNINITIALIZED && sectorInfo.sent < sectorInfo.total) || (sectorInfo.sent2 < sectorInfo.total2)) { // we have unsent data
                    Trace(1, "*** AIRPLANE turn ON GSM");

                    for (int attempts = 0; attempts < 2; attempts ++) {
                        Network_SetFlightMode(false); // turn on GSM

                        Trace(1, "*** AIRPLANE start network attach");
                        OS_Sleep(30 * 1000);

                        Network_StartAttach();
                        Network_PDP_Context_t context;
                        sprintf(context.apn, "%s", settings->apn_apn);
                        sprintf(context.userName, "%s", settings->apn_user);
                        sprintf(context.userPasswd, "%s", settings->apn_password);
                        Network_StartActive(context);

                        time_t start_time, current_time;
                        time(&start_time);
                        int64_t last_sent = sectorInfo.sent, last_sent2 = sectorInfo.sent2;
                        while ((sectorInfo.sent != UNINITIALIZED && sectorInfo.sent < sectorInfo.total) || (sectorInfo.sent2 < sectorInfo.total2)) { // wait till all data is sent
                            if (last_sent != sectorInfo.sent) {
                                last_sent = sectorInfo.sent;
                                time(&start_time);
                            }
                            if (last_sent2 != sectorInfo.sent2) {
                                last_sent2 = sectorInfo.sent2;
                                time(&start_time);
                            }
                            time(&current_time);
                            Trace(1, "*** AIRPLANE sending data sent = %ld, total = %ld, unsent = %d, before time limit = %d",
                                (long int)sectorInfo.sent, (long int)sectorInfo.total, (int)(sectorInfo.total - sectorInfo.sent),
                                5 * 60 - (current_time - start_time));

                            if (current_time - start_time >= 5 * 60) { // max time to send all data == 5 minutes
                                Trace(1, "*** AIRPLANE 5 minutes exceeded");
                                break;
                            }
                            OS_Sleep(1000);
                        }

                        if ((sectorInfo.sent != UNINITIALIZED && sectorInfo.sent < sectorInfo.total) || (sectorInfo.sent2 < sectorInfo.total2)) {
                            TIME_GetSystemTime(&sysTime);
                            if (sysTime.minute < 30) {
                                Trace(1, "*** AIRPLANE turn OFF GSM");
                                Network_SetFlightMode(true); // turn off GSM

                                // sleep 30 minutes
                                Trace(1, "*** AIRPLANE sleeping for 30 minutes");
                                OS_Sleep(30 * 60 * 1000);
                            } else {
                                break;
                            }
                        } else {
                            break;
                        }
                    }
                }

                Trace(1, "*** AIRPLANE turn OFF GSM");
                Network_SetFlightMode(true); // turn off GSM

                Trace(1, "** AIRPLANE HP__Hourly finished");
            }
            if (now == HP__Sleep) {
                Trace(1, "*** AIRPLANE entering sleep mode");
                Network_SetFlightMode(true); // turn off GSM
                OS_Sleep(10 * 1000); // 10 seconds delay

                if (recs > 0) { // save any unsaved data
                    flush_sector();
                }

                Trace(1, "** AIRPLANE HP__Sleep finished");

                PM_Restart(); // restart and enter sleep mode
                break;
            } else if (now == HP__Offline) {
                Trace(1, "*** AIRPLANE entering offline mode");
                Network_SetFlightMode(true); // turn off GSM
                Trace(1, "** AIRPLANE HP__Offline finished");
                OS_Sleep(120 * 1000); // do nothing this hour
                continue;
            }                

            if (!ProfileUpdated) {
                OS_Sleep(60 * 1000); // skip 1 minute
            }
        }

        OS_Sleep(10 * 1000);
    }
}

/*void Test(void *p) {
    while (true) {
        char temp[1000];
        sprintf(temp, "*** XCONFIG [log: %d, domain: %s, port: %d, apn_apn: %s, apn_user: %s, apn_passwd: %s, imei: %s, ver: %s]",
            settings->write_log, settings->domain, settings->port, settings->apn_apn, settings->apn_user, settings->apn_password, settings->IMEI, settings->SOFTWARE_VERSION);
        Trace(1, temp);

        OS_Sleep(1000);
    }
}*/

/*void uart(UART_Callback_Param_t param) {
    Trace(1, "** XRET %d: %s", param.length, param.buf);
}*/

void MEMS_Task(void *pD)
{
    uint8_t tmp,k=0;
    I2C_Config_t config;

  //  int16_t X,Y,Z;
    uint8_t tmp_l, tmp_h;
   // double aX,aY,aZ;

 //   double _mult_a,vect;
 //
 //   double vect_mass[N];
    
    uint8_t j=0;

 //   char buff[30];
OS_Sleep(1000);
Trace(1,"WHO I_AM start");

    config.freq = I2C_FREQ_400K;
    I2C_Init(I2C_ACC, config);
    
    char st=0;
    int Nizm=0;
    float X[10],Y[10],Z[10];
    int16_t _axc[10],_ayc[10],_azc[10];
  
      st = begin(); // странный глюк при ините магнетометра и далее - выпадает в ошибку
    Trace(1,"status %d",st);
    if (st<0)
    {
        while(1) {
        Trace(1,"IMU SENSOR ERROR");
        OS_Sleep(1000);
        }
    }
    else
    {
      Trace(1,"IMU SENSOR OK");  

    }
    
    while(1)
    {
      readSensor();  

      X[j]=_ax; Y[j]=_ay; Z[j]=_az;
      _axc[j]=_axcounts; _ayc[j]=_aycounts; _azc[j]=_azcounts;
      j++;
    // I2C_ReadMem(I2C_ACC, MPU_9265_ADDR, 0x75, 1, &tmp, 1, I2C_DEFAULT_TIME_OUT);  
    // Trace(1,"WHO I_AM: %d",whoAmI());
     OS_Sleep(100);
    if (j==10) {
     TIME_System_t sysTime;
     TIME_GetSystemTime(&sysTime);
	 OS_LockMutex(log_lock);

    int32_t fd = API_FS_Open(ACCEL_LOG_FILE, FS_O_APPEND | FS_O_RDWR | FS_O_CREAT, 0);
    if (fd >= 0) {
        char temp[1024];
        API_FS_Write(fd, (uint8_t *)temp,sprintf(temp,"Time: [%d-%02d-%02d %02d:%02d:%02d]\r\nLat: %2.6f, Lon: %2.6f\r\n",sysTime.year, sysTime.month, sysTime.day, sysTime.hour, sysTime.minute, sysTime.second,(float)current.lat/1000000,(float)current.lon/1000000));
        for (size_t i = 0; i < 10; i++)
        {
            
           API_FS_Write(fd, (uint8_t *)temp, sprintf(temp, "X: %2.3f Y: %2.3f Z: %2.3f, _ax: %d, _ay: %d, _az: %d\r\n",X[i],Y[i],Z[i],_axc[i],_ayc[i],_azc[i])); /* code */
        }
        
        

        Trace(1, temp);
        Trace(1,"Accel scale = %f",_accelScale);  

        API_FS_Flush(fd);
        API_FS_Close(fd);
    }

    OS_UnlockMutex(log_lock);         
          j = 0; //WriteAccelLog(); 
            Nizm++;          
          }
    }
    
/*

// Настройка LIS331
    tmp=0x27; // PM - Normal(001), DR - 50 Hz (00), X/Y/Z - enable (111) -> 0b00100111
    I2C_WriteMem(I2C_ACC, LIS331_ADDR, CTRL_REG1, 1, &tmp, 1, I2C_DEFAULT_TIME_OUT);
    tmp=0;
    switch (RANGE)
    {
        case RANGE_2G:{ tmp=FS_2G; _mult_a=2.0; }break;
        case RANGE_4G:{ tmp=FS_4G; _mult_a=4.0; }break;
        case RANGE_8G:{ tmp=FS_8G; _mult_a=8.0; }break;
    }
    I2C_WriteMem(I2C_ACC, LIS331_ADDR, CTRL_REG4, 1, &tmp, 1, I2C_DEFAULT_TIME_OUT);

    while(1)
    {

        I2C_ReadMem(I2C_ACC, LIS331_ADDR, OUT_X_L, 1, &tmp_l, 1, I2C_DEFAULT_TIME_OUT);
        I2C_ReadMem(I2C_ACC, LIS331_ADDR, OUT_X_H, 1, &tmp_h, 1, I2C_DEFAULT_TIME_OUT);
        X=(tmp_h<<8 | tmp_l);
        I2C_ReadMem(I2C_ACC, LIS331_ADDR, OUT_Y_L, 1, &tmp_l, 1, I2C_DEFAULT_TIME_OUT);
        I2C_ReadMem(I2C_ACC, LIS331_ADDR, OUT_Y_H, 1, &tmp_h, 1, I2C_DEFAULT_TIME_OUT);
        Y=(tmp_h<<8 | tmp_l);
        I2C_ReadMem(I2C_ACC, LIS331_ADDR, OUT_Z_L, 1, &tmp_l, 1, I2C_DEFAULT_TIME_OUT);
        I2C_ReadMem(I2C_ACC, LIS331_ADDR, OUT_Z_H, 1, &tmp_h, 1, I2C_DEFAULT_TIME_OUT);
        Z=(tmp_h<<8 | tmp_l);               
   
     // проекции на оси в единицах G
        aX=(double)(X*_mult_a/32767.0); 
        aY=(double)(Y*_mult_a/32767.0); 
        aZ=(double)(Z*_mult_a/32767.0); 
        vect=sqrt(aX*aX+aY*aY+aZ*aZ);  //  -> результирующий вектор

        if(vect<1.0) vect=1.0;
        vect-=1;    // вычитаем вектор гравитации

        vect_mass[k++]=vect;
        if (k>=N)
        {
            k=0;
            // ищем среднее и максимальное
            double Summ=0, Max;
            for (int j=0;j<N;j++)
            {
             Summ+=vect_mass[j];
             if (vect_mass[j]>Max) Max=vect_mass[j];
            }
            Mid=Summ/N; // среднее
            Trace(1,"Middle accel vector: %2.2f",Mid);
        }
        else
        OS_Sleep(1000/N);
    }
    */
}

void TrackerMainTask(void *pData) {
    API_Event_t *event = NULL;

    static ADC_Config_t adc_config = {
        .channel      = ADC_CHANNEL_0,
        .samplePeriod = ADC_SAMPLE_PERIOD_100MS
    };
    ADC_Init(adc_config);

    //sd_init();
    //sd_reopen();

#ifdef NEW_PLATA
    GPIO_Init(selfOff);
    // GPIO_SetLevel(selfOff, GPIO_LEVEL_LOW);

    static GPIO_config_t powerKeyMax = {
        .mode               = GPIO_MODE_INPUT_INT,
        .pin                = GPIO_PIN3,
        .defaultLevel       = GPIO_LEVEL_HIGH,
        .intConfig.debounce = 50,
        .intConfig.type     = GPIO_INT_TYPE_RISING_EDGE,
        .intConfig.callback = OnPinFalling
    };
    GPIO_Init(powerKeyMax);
#endif

    led_init();

    if (INFO_GetIMEI(settings->IMEI)) {
        sprintf(DATA_FILE_NAME,  FS_DEVICE_NAME_T_FLASH "/%s.gbt", settings->IMEI); // external
        sprintf(DATA2_FILE_NAME, FS_DEVICE_NAME_FLASH   "/%s.gbt", settings->IMEI); // internal
    }

    data_lock = OS_CreateMutex();
    log_lock  = OS_CreateMutex();
    airplane_log_lock = OS_CreateMutex();

    settings_load();
    load_config();

     OS_CreateTask(MEMS_Task, NULL, NULL, MEMS_TASK_STACK_SIZE, MEMS_TASK_PRIORITY, 0, 0, MEMS_TASK_NAME);


//OS_CreateTask(Test, NULL, NULL, AIRPLANEMODE_TASK_STACK_SIZE, 4, 0, 0, "Test");

    if (settings->write_log || settings->network_log) {
        WriteLog("**** GLOBAL STARTUP ****");
    }

    TIME_SetIsAutoUpdateRtcTime(false);
    Network_SetFlightMode(true);

    memset(&ch, 0, sizeof(ch));
    memset(sector_buffer, 0xFF, SECTOR_SIZE);
    memset(current_coordinate, 0xFF, sizeof(current_coordinate));


//for (int i = 0; i < 10; i ++) {
    Trace(1, "*** AIRPLANE [profile_read=%d] PROFILE INIT [%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
        profile_read,
        Profile[0], Profile[1], Profile[2], Profile[3], 
        Profile[4], Profile[5], Profile[6], Profile[7], 
        Profile[8], Profile[9], Profile[10], Profile[11], 
        Profile[12], Profile[13], Profile[14], Profile[15], 
        Profile[16], Profile[17], Profile[18], Profile[19], 
        Profile[20], Profile[21], Profile[22], Profile[23]);

    if (settings->write_log || settings->network_log) {
        char temp[1000];
        sprintf(temp, "*** AIRPLANE [profile_read=%d] PROFILE INIT [%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
            profile_read,
            Profile[0], Profile[1], Profile[2], Profile[3], 
            Profile[4], Profile[5], Profile[6], Profile[7], 
            Profile[8], Profile[9], Profile[10], Profile[11], 
            Profile[12], Profile[13], Profile[14], Profile[15], 
            Profile[16], Profile[17], Profile[18], Profile[19], 
            Profile[20], Profile[21], Profile[22], Profile[23]);
        WriteLog(temp);
    }

    TIME_System_t sysTime;
    TIME_GetSystemTime(&sysTime);

    Trace(1, "** AIRPLANE [profile_read=%d] date [%d-%02d-%02d %02d:%02d:%02d]",
        profile_read,
        sysTime.year, sysTime.month, sysTime.day, sysTime.hour, sysTime.minute, sysTime.second);

    if (sysTime.year >= 2018 && sysTime.year <= 2025) {
        if (settings->write_log || settings->network_log) {
            WriteLog("time is set correctly");
        }

        Trace(1, "** AIRPLANE [now=%d] date [%d-%02d-%02d %02d:%02d:%02d]",
            Profile[sysTime.hour],
            sysTime.year, sysTime.month, sysTime.day, sysTime.hour, sysTime.minute, sysTime.second);

        if (Profile[sysTime.hour] == HP__Sleep) {
            if (settings->write_log || settings->network_log) {
                WriteLog("**** ENTERING sleep mode");
            }

            PM_SetSysMinFreq(PM_SYS_FREQ_32K);
            PM_SleepMode(true);

            while (Profile[sysTime.hour] == HP__Sleep) {
                TIME_GetSystemTime(&sysTime);
                Trace(1, "*** AIRPLANE [profile_read=%d] [%d-%02d-%02d %02d:%02d:%02d] sleep mode [%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
                    profile_read,
                    sysTime.year, sysTime.month, sysTime.day, sysTime.hour, sysTime.minute, sysTime.second,
                    Profile[0], Profile[1], Profile[2], Profile[3], 
                    Profile[4], Profile[5], Profile[6], Profile[7], 
                    Profile[8], Profile[9], Profile[10], Profile[11], 
                    Profile[12], Profile[13], Profile[14], Profile[15], 
                    Profile[16], Profile[17], Profile[18], Profile[19], 
                    Profile[20], Profile[21], Profile[22], Profile[23]);

                if (settings->write_log || settings->network_log) {
                    char temp[1000];
                    sprintf(temp, "*** AIRPLANE [profile_read=%d] [%d-%02d-%02d %02d:%02d:%02d] sleep mode [%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
                        profile_read,
                        sysTime.year, sysTime.month, sysTime.day, sysTime.hour, sysTime.minute, sysTime.second,
                        Profile[0], Profile[1], Profile[2], Profile[3], 
                        Profile[4], Profile[5], Profile[6], Profile[7], 
                        Profile[8], Profile[9], Profile[10], Profile[11], 
                        Profile[12], Profile[13], Profile[14], Profile[15], 
                        Profile[16], Profile[17], Profile[18], Profile[19], 
                        Profile[20], Profile[21], Profile[22], Profile[23]);
                    WriteLog(temp);
                }

                if (OS_WaitEvent(mainTaskHandle, (void **)&event, OS_TIME_OUT_NO_WAIT)) {
                    EventDispatchButtonOnly(event);

                    OS_Free(event->pParam1);
                    OS_Free(event->pParam2);
                    OS_Free(event);
                }

                OS_Sleep(10 * 1000);
            }

            if (settings->write_log || settings->network_log) {
                WriteLog("**** EXITTING sleep mode");
            }

            PM_SleepMode(false);
            PM_Restart();
            return;
        }
    }
//}

    gps_init();

    //gps_on();

    //GPS_Init();
    //xret = GPS_Open(uart);


    if (gpsType == GPS_TYPE__A9G) {
        sprintf(settings->SOFTWARE_VERSION, "A" SOFTWARE_DATE "RUG"); // supports FOTA update
    } else if (gpsType == GPS_TYPE__ELA) {
#ifdef NEW_PLATA
        sprintf(settings->SOFTWARE_VERSION, "A" SOFTWARE_DATE "RUE"); // supports FOTA update
#else // #ifdef NEW_PLATA
        sprintf(settings->SOFTWARE_VERSION, "A" SOFTWARE_DATE "RHE");
#endif // #ifdef NEW_PLATA
    } else if (gpsType == GPS_TYPE__ORIGIN) {
        sprintf(settings->SOFTWARE_VERSION, "A" SOFTWARE_DATE "RHO");
    }

    int offline_only = true;
    for (int i = 0; i < 24 && offline_only; i ++) {
        if (Profile[i] == HP__Online || Profile[i] == HP__Hourly) {
            offline_only = false;
        }
    }

    Trace(1, "** AIRPLANE [offline_only=%d] date [%d-%02d-%02d %02d:%02d:%02d]",
        offline_only?1:0,
        sysTime.year, sysTime.month, sysTime.day, sysTime.hour, sysTime.minute, sysTime.second);

    if (!offline_only) {
        Network_SetStatusChangedCallback(network_callback);

        thirdTaskHandle = OS_CreateTask(ThirdTask,
            NULL, NULL, THIRD_TASK_STACK_SIZE, THIRD_TASK_PRIORITY, 0, 0, THIRD_TASK_NAME);

        OS_CreateTask(AirplaneMode, NULL, NULL, AIRPLANEMODE_TASK_STACK_SIZE, AIRPLANEMODE_TASK_PRIORITY, 0, 0, AIRPLANEMODE_TASK_NAME);
    }

    PM_SetSysMinFreq(PM_SYS_FREQ_312M);
    PM_SleepMode(false);

    while (1) {
        if (OS_WaitEvent(mainTaskHandle, (void **)&event, OS_TIME_OUT_WAIT_FOREVER)) {
            EventDispatch(event);

            OS_Free(event->pParam1);
            OS_Free(event->pParam2);
            OS_Free(event);
        }
    }
}

void a9_accel_log_Main(void) {
    static struct Settings global_settings = {
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
    };
    settings = &global_settings;


    mainTaskHandle = OS_CreateTask(TrackerMainTask,
        NULL, NULL, MAIN_TASK_STACK_SIZE, MAIN_TASK_PRIORITY, 0, 0, MAIN_TASK_NAME);
    OS_SetUserMainHandle(&mainTaskHandle);
}
