#include "a9.h"

int tenMinutes = 0;
int global_socket = 0;

#define RECEIVE_BUFFER_MAX_LENGTH 128

char socket_buffer[RECEIVE_BUFFER_MAX_LENGTH] = {0};
int socket_length = 0;

char network_state = 0;
time_t last_profile_query = 0;
bool ProfileUpdated = false;

// char *fota_buffer = NULL;
// int fota_buffer_offset = 0;

API_Event_ID_t socket_event_id = API_EVENT_ID_MAX;
bool wait_socket_event(API_Event_ID_t event_id, int len) {
    int timeout = NETWORK_TIMEOUT;
    while (event_id != socket_event_id) {
        OS_Sleep(100);
        /*if (len != -1 && len == socket_length) {
            break;
        }*/
        if (timeout -- <= 0) {
            return false;
        }
    }
    return true;
}

void EventDispatchSocket(API_Event_t *pEvent) {
    switch (pEvent->id) {
        case API_EVENT_ID_SOCKET_CONNECTED: {
                Trace(1, "**** INTERNET EVENT API_EVENT_ID_SOCKET_CONNECTED");
                if (settings->network_log) {
                    char temp[1024];
                    sprintf(temp, "API_EVENT_ID_SOCKET_CONNECTED, socket=%d", pEvent->param1);
                    WriteLog(temp);
                }
                // int fd = pEvent->param1;
                socket_event_id = pEvent->id;
            }
            break;

        case API_EVENT_ID_SOCKET_SENT: {
                Trace(1, "**** INTERNET EVENT API_EVENT_ID_SOCKET_SENT");
                // int fd = pEvent->param1;
                socket_event_id = pEvent->id;
            }
            break;
 
        case API_EVENT_ID_SOCKET_RECEIVED: {
                Trace(1, "**** INTERNET EVENT API_EVENT_ID_SOCKET_RECEIVED [%d]", pEvent->param2);
                //if (fota_buffer == NULL) {
                    int fd = pEvent->param1;
                    int length = pEvent->param2 > RECEIVE_BUFFER_MAX_LENGTH ? RECEIVE_BUFFER_MAX_LENGTH : pEvent->param2;
                    socket_event_id = pEvent->id;

                    socket_length = length;
                    Socket_TcpipRead(fd, socket_buffer, length);
                /*} else {
                    int fd = pEvent->param1;
                    int length = pEvent->param2;
                    socket_event_id = pEvent->id;

                    socket_length = length;
                    Socket_TcpipRead(fd, fota_buffer + fota_buffer_offset, length);
                    fota_buffer_offset += length;
                }*/
            }
            break;

        case API_EVENT_ID_SOCKET_CLOSED: {
                Trace(1, "**** INTERNET EVENT API_EVENT_ID_SOCKET_CLOSED");
                if (settings->network_log) {
                    char temp[1024];
                    sprintf(temp, "API_EVENT_ID_SOCKET_CLOSED, socket=%d", pEvent->param1);
                    WriteLog(temp);
                }
                // int fd = pEvent->param1;
                socket_event_id = pEvent->id;
            }
            break;

        case API_EVENT_ID_SOCKET_ERROR: {
                Trace(1, "**** INTERNET EVENT API_EVENT_ID_SOCKET_ERROR");
                if (settings->network_log) {
                    char temp[1024];
                    sprintf(temp, "API_EVENT_ID_SOCKET_ERROR, socket=%d", pEvent->param1);
                    WriteLog(temp);
                }
                // int fd = pEvent->param1;
                socket_event_id = pEvent->id;
            }
            break;

        default:
            break;
    }
}

int sockrecv(int listen_sd, char *buf, int len, int dummy) {
	if (wait_socket_event(API_EVENT_ID_SOCKET_RECEIVED, len)) {
        if (socket_length > 0 && socket_length <= len) {
            memcpy(buf, socket_buffer, socket_length);
            return socket_length;
        }
    }
    return -1;
}

bool check_profile() {
    time_t curTime = 0;
    time(&curTime);
    bool error = false;
    if (last_profile_query == 0 || (curTime - last_profile_query >= 30 * 60)) { // update profile every 30 minutes, or by server command, or when first start
        const char profile_packet[] = { 253 };
        socket_event_id = API_EVENT_ID_MAX;
        Socket_TcpipWrite(global_socket, (uint8_t *)profile_packet, sizeof(profile_packet));

        char profile_reply[25] = {0};
        memcpy(&profile_reply[1], &Profile[0], 24);
        int ret = sockrecv(global_socket, profile_reply, sizeof(profile_reply), 0);
        if (ret > 0) {
            if (profile_reply[0] == 0x00) {
                last_profile_query = curTime;

                int sum = 0;
                for (int ii = 0; ii < 24; ii ++) {
                    sum += profile_reply[ii + 1];
                }
                Trace(1, "*** INTERNET PROFILE [%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
                    profile_reply[0], profile_reply[1], profile_reply[2], profile_reply[3], 
                    profile_reply[4], profile_reply[5], profile_reply[6], profile_reply[7], 
                    profile_reply[8], profile_reply[9], profile_reply[10], profile_reply[11], 
                    profile_reply[12], profile_reply[13], profile_reply[14], profile_reply[15], 
                    profile_reply[16], profile_reply[17], profile_reply[18], profile_reply[19], 
                    profile_reply[20], profile_reply[21], profile_reply[22], profile_reply[23]);
                if (sum > 0) {
                    TIME_System_t sysTime;
                    TIME_GetSystemTime(&sysTime);
                    bool diff = (profile_reply[sysTime.hour + 1] != Profile[sysTime.hour]);
                    Trace(1, "*** INTERNET PROFILE updating for current hour [%s] [now->%d, was->%d, hour->%d]", diff ? "true" : "false", profile_reply[sysTime.hour + 1], Profile[sysTime.hour], sysTime.hour);

                    // memcpy(&Profile[0], &profile_reply[1], 24);
                    for (int ii = 0; ii < 24; ii ++) {
                        Profile[ii] = profile_reply[ii + 1];
                    }
                    Trace(1, "*** INTERNET PROFILE UPDATED [%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
                        Profile[0], Profile[1], Profile[2], Profile[3], 
                        Profile[4], Profile[5], Profile[6], Profile[7], 
                        Profile[8], Profile[9], Profile[10], Profile[11], 
                        Profile[12], Profile[13], Profile[14], Profile[15], 
                        Profile[16], Profile[17], Profile[18], Profile[19], 
                        Profile[20], Profile[21], Profile[22], Profile[23]);
                    
                    ProfileUpdated = diff;
                    update_profile();
                } else {
                    Trace(1, "*** INTERNET PROFILE SETTING ERROR");
                }
            } else {
                error = true;
                Trace(1, "*** INTERNET PROFILE SERVER REPLY ERROR");
            }
        } else {
            error = true;
            Trace(1, "*** INTERNET PROFILE NETWORK ERROR");
        }
    }
    return !error;
}

bool send_and_receive(const char *packet, int length, int fd) {
    /*if (time_set) {
        last_send_and_recv = TIME_GetIime() + MKTIME_SECONDS;
    }*/
    Trace(1, "**** INTERNET send_and_receive SENDING [%d] bytes", length);

	if (settings->write_log) {
        WriteLog("send_and_receive: send START");
    }

    socket_event_id = API_EVENT_ID_MAX;
    int ret = Socket_TcpipWrite(fd, (uint8_t *)packet, length);
    Trace(1, "**** INTERNET send_and_receive SENT [%d] bytes", length);
    if (ret < 0) {
        Trace(1, "* socket send fail - need to reconnect");
        if (settings->write_log) {
            WriteLog("send_and_receive: send ERROR");
        }
    } else {
    	Trace(1, "* socket send success #2 (len=%d bytes YYY)", ret);
    	if (settings->write_log) {
            WriteLog("send_and_receive: send DONE, recv START");
        }

	    char send_reply[1] = { 0xFF };
        Trace(1, "**** INTERNET send_and_receive WAITING RECV");
        ret = sockrecv(fd, send_reply, sizeof(send_reply), 0);
        Trace(1, "**** INTERNET send_and_receive WAITING RECV DONE [ret=%d]", ret);
        if (ret > 0) {
            Trace(1, "* SUCCESSFULL SEND !!!! recv len:%d, data:[%d]", ret, send_reply[0]);
            if (send_reply[0] == 0x00) {
                if (settings->write_log) {
                	char temp[100];
                	sprintf(temp, "send_and_receive: recv DONE [%d bytes]", ret);
                    WriteLog(temp);
                }
                return true;
            } else if (send_reply[0] == 0x01) { // update profile
                last_profile_query = 0;
                return check_profile();
            } else {
                if (settings->write_log) {
                	char temp[100];
                	sprintf(temp, "send_and_receive: recv DONE WITH ERROR [%d bytes]", ret);
                    WriteLog(temp);
                }
            }
        } else {
            Trace(1, "* recv error - need to reconnect");
            if (settings->write_log) {
            	char temp[100];
            	sprintf(temp, "send_and_receive: recv ERROR [%d bytes]", ret);
                WriteLog(temp);
            }
        }
    }
    return false; // will reconnect to server
}

void network_thread(int fd) {
	char coordinate[sizeof(current_coordinate)];

	int timer = BATTERY_SECONDS, i; // timer = BATTERY_SECONDS, will send battery once started	
	while (true) {
        if (timer ++ >= BATTERY_SECONDS || tenMinutes >= BATTERY_INTERVAL) { // approx 20 seconds for 'timer' and 10 minutes for 'tenMinutes'
            Trace(1, "** SENDING BATTERY **");
        	if (settings->write_log) {
                WriteLog("NETWORK: sending battery");
            }

            char battery = GetVoltage();//0;
            //PM_Voltage((uint8_t *)&battery);

            char battery_packet[2] = { 0xFF, (battery > 100 ? 100 : battery < 0 ? 0 : battery) };
            // char battery_packet[3] = { 0xFF, (battery > 100 ? 100 : battery < 0 ? 0 : battery), (lowbattery ? 0xF0 : 0x00) | (charging ? 0x0F : 0x00) };

            if (!send_and_receive(&battery_packet[0], sizeof(battery_packet), fd)) {
            	if (settings->write_log) {
                    WriteLog("NETWORK: ERROR sending battery");
                }
                Trace(1, "** ERROR SENDING BATTERY **");
                return; // break; <- will reconnect
            }

            timer = 0;
            if (tenMinutes >= BATTERY_INTERVAL) {
                tenMinutes = 0;
            }
        }
        
        if (now == HP__Online) {
            if (current_coordinate[0] > 0) {
                memcpy(&coordinate[0], &current_coordinate[0], sizeof(coordinate));
                current_coordinate[0] = 0; // flush flag

                if (settings->write_log) {
                    WriteLog("NETWORK: sending coordinates START");
                }
                Trace(1, "** XXX SENDING %d COORDINATES, %d bytes ***", coordinate[0], 1 + coordinate[0] * sizeof(current));

                if (!send_and_receive(&coordinate[0], 1 + coordinate[0] * sizeof(current), fd)) {
                    Trace(1, "** ERROR SENDING CURRENT COORDINATES **");
                    if (settings->write_log) {
                        WriteLog("NETWORK: sending coordinates ERROR");
                    }
                    return; // break; <- will reconnect
                }
                if (settings->write_log) {
                    WriteLog("NETWORK: sending coordinates DONE");
                }

                timer = 0;
            }
        }

        bool any_sector_sent = false;

        // internal flash
	    while (sectorInfo.sent2 < sectorInfo.total2) {
	    	Trace(1, "** READING UNSENT INTERNAL SECTOR DATA AT OFFSET [%ld] (last sent sector is %ld of %ld total) **", (long int)(sectorInfo.sent2 * SECTOR_SIZE), (long int)sectorInfo.sent2, (long int)sectorInfo.total2);

	    	bool success = false;
	    	for (i = 0; i < 1; i ++) {
	    		if (settings->write_log) {
                    WriteLog("NETWORK: reading data file START");
                }
                OS_LockMutex(data_lock);
		        int32_t ffd = API_FS_Open(DATA2_FILE_NAME, FS_O_RDONLY | FS_O_CREAT, 0);
			    if (fd >= 0) {
			        API_FS_Seek(ffd, sectorInfo.sent2 * SECTOR_SIZE, FS_SEEK_SET);
			        char sector_data[SECTOR_SIZE + 1];
			    	bool canSend = (API_FS_Read(ffd, (uint8_t *)&sector_data[1], SECTOR_SIZE) == SECTOR_SIZE);
			        API_FS_Close(ffd);
                    OS_UnlockMutex(data_lock);
                    if (settings->write_log) {
                        WriteLog("NETWORK: reading data file DONE");
                    }

			        if (canSend) {
			        	Trace(1, "*** SENDING SECTOR DATA ***");
			        	if (settings->write_log) {
                            WriteLog("NETWORK: sending sector");
                        }
			        	sector_data[0] = 0xFE; // sector data packet id

			        	if (!send_and_receive(&sector_data[0], sizeof(sector_data), fd)) {
			        		Trace(1, "** ERROR SENDING SECTOR DATA **");
			        		if (settings->write_log) {
                                WriteLog("NETWORK: ERROR sending sector");
                            }
                            if (any_sector_sent) {
                                if (settings->write_log) {
                                    WriteLog("NETWORK: updating config START");
                                }
                                update_config();
                                if (settings->write_log) {
                                    WriteLog("NETWORK: updating config DONE");
                                }
                            }
			        		return; // break; <- will reconnect
			        	}

                        any_sector_sent = true;

			        	if (settings->write_log) {
                            WriteLog("NETWORK: sending sector DONE");
                        }
			        	success = true;
			        	break;
			        } else {
			        	Trace(1, "*** ERROR READING SECTOR DATA #2 ***");

                        char temp[100];
                        API_FS_INFO info;
                        if (API_FS_GetFSInfo(FS_DEVICE_NAME_FLASH, &info) == 0) {
                            sprintf(temp, "NETWORK #2: INTERNAL FLASH: total=[%ld], used=[%ld]", info.totalSize, info.usedSize);
                        } else {
                            sprintf(temp, "NETWORK #2: INTERNAL FLASH - ERROR API_FS_GetFSInfo()");
                        }
                        Trace(1, temp);

			        	if (settings->write_log) {
                            WriteLog("NETWORK: reading sector NOT SENDING");
                        }
			        }
			    } else {
                    OS_UnlockMutex(data_lock);

                    char temp[100];
                    API_FS_INFO info;
                    if (API_FS_GetFSInfo(FS_DEVICE_NAME_FLASH, &info) == 0) {
                        sprintf(temp, "NETWORK #1: INTERNAL FLASH: total=[%ld], used=[%ld]", info.totalSize, info.usedSize);
                    } else {
                        sprintf(temp, "NETWORK #1: INTERNAL FLASH - ERROR API_FS_GetFSInfo()");
                    }
                    Trace(1, temp);

			    	Trace(1, "*** ERROR READING SECTOR DATA #1 ***");
			    	if (settings->write_log) {
                        WriteLog("NETWORK: reading sector ERROR");
                    }
			    }

			    OS_Sleep(500);
			}

			if (!success) {
				break;
			}

            sectorInfo.sent2 ++;

    		timer = 0;
    	}

        // external sd card
        if (sectorInfo.sent != UNINITIALIZED) {
            while (sectorInfo.sent < sectorInfo.total) {
                Trace(1, "** READING UNSENT SECTOR DATA AT OFFSET [%ld] (last sent sector is %ld of %ld total) **", (long int)(sectorInfo.sent * SECTOR_SIZE), (long int)sectorInfo.sent, (long int)sectorInfo.total);

                bool success = false;
                for (i = 0; i < 1; i ++) {
                    if (settings->write_log) {
                        WriteLog("NETWORK: reading data file START");
                    }
                    OS_LockMutex(data_lock);
                    int32_t ffd = API_FS_Open(DATA_FILE_NAME, FS_O_RDONLY | FS_O_CREAT, 0);
                    if (fd >= 0) {
                        API_FS_Seek(ffd, sectorInfo.sent * SECTOR_SIZE, FS_SEEK_SET);
                        char sector_data[SECTOR_SIZE + 1];
                        bool canSend = (API_FS_Read(ffd, (uint8_t *)&sector_data[1], SECTOR_SIZE) == SECTOR_SIZE);
                        API_FS_Close(ffd);
                        OS_UnlockMutex(data_lock);
                        if (settings->write_log) {
                            WriteLog("NETWORK: reading data file DONE");
                        }

                        if (canSend) {
                            Trace(1, "*** SENDING SECTOR DATA ***");
                            if (settings->write_log) {
                                WriteLog("NETWORK: sending sector");
                            }
                            sector_data[0] = 0xFE; // sector data packet id

                            if (!send_and_receive(&sector_data[0], sizeof(sector_data), fd)) {
                                Trace(1, "** ERROR SENDING SECTOR DATA **");
                                if (settings->write_log) {
                                    WriteLog("NETWORK: ERROR sending sector");
                                }
                                if (any_sector_sent) {
                                    if (settings->write_log) {
                                        WriteLog("NETWORK: updating config START");
                                    }
                                    update_config();
                                    if (settings->write_log) {
                                        WriteLog("NETWORK: updating config DONE");
                                    }
                                }
                                return; // break; <- will reconnect
                            }

                            any_sector_sent = true;

                            if (settings->write_log) {
                                WriteLog("NETWORK: sending sector DONE");
                            }
                            success = true;
                            break;
                        } else {
                            Trace(1, "*** ERROR READING SECTOR DATA #2 ***");

                            char temp[100];
                            API_FS_INFO info;
                            if (API_FS_GetFSInfo(FS_DEVICE_NAME_T_FLASH, &info) == 0) {
                                sprintf(temp, "NETWORK #2: SD CARD: total=[%ld], used=[%ld]", info.totalSize, info.usedSize);
                            } else {
                                sprintf(temp, "NETWORK #2: SD CARD - ERROR API_FS_GetFSInfo()");
                            }
                            Trace(1, temp);

                            if (settings->write_log) {
                                WriteLog("NETWORK: reading sector NOT SENDING");
                            }
                        }
                    } else {
                        OS_UnlockMutex(data_lock);

                        char temp[100];
                        API_FS_INFO info;
                        if (API_FS_GetFSInfo(FS_DEVICE_NAME_T_FLASH, &info) == 0) {
                            sprintf(temp, "NETWORK #1: SD CARD: total=[%ld], used=[%ld]", info.totalSize, info.usedSize);
                        } else {
                            sprintf(temp, "NETWORK #1: SD CARD - ERROR API_FS_GetFSInfo()");
                        }
                        Trace(1, temp);

                        Trace(1, "*** ERROR READING SECTOR DATA #1 ***");
                        if (settings->write_log) {
                            WriteLog("NETWORK: reading sector ERROR");
                        }
                    }

                    OS_Sleep(500);
                }

                if (!success) {
                    break;
                }

                sectorInfo.sent ++;

                timer = 0;
            }
        }

        if (any_sector_sent) {
            if (settings->write_log) {
                WriteLog("NETWORK: updating config START");
            }
            update_config();
            if (settings->write_log) {
                WriteLog("NETWORK: updating config DONE");
            }
        }


		OS_Sleep(100);
	}

	Trace(1, "* network_thread finished! - need to reconnect");
}

static void fota_handler(const unsigned char *data, int len) {
    if (len) {
        MEMBLOCK_Trace(1, (uint8_t*)data, (uint16_t)len, 16);
        Trace(1,"fota total len:%d data:%s", len, data);
        if (!API_FotaInit(len)) {
            goto upgrade_faile;
        }
        API_FotaReceiveData((unsigned char*)data, (int)len);
    } else {//error
        Trace(1,"fota total len:%d data:%s", len, data);
        goto upgrade_faile;
    }
    return;

upgrade_faile:
    Trace(1,"server fota false");
    API_FotaClean();
}

void ThirdTask(void *pData) {
    Trace(1, "*************** INIT: total: %ld, unsent: %ld", (long int)sectorInfo.total, (long int)(sectorInfo.total - sectorInfo.sent));

    /*if (!INFO_GetIMEI(settings->IMEI)) {
        sprintf(settings->IMEI, "012345678901234");
    }
    sprintf(DATA_FILE_NAME, FS_DEVICE_NAME_T_FLASH "/%s.gbt", settings->IMEI);*/

    Trace(1, "**** INTERNET USING domain [%s:%d]", settings->domain, settings->port);
    if (settings->write_log) {
        char temp[100];
        sprintf(temp, "**** INTERNET using domain [%s:%d]", settings->domain, settings->port);
        WriteLog(temp);
    }

    uint8_t ip_address[16];
    // int no_connect = 0;
    while (true) {
        network_state = 1; // resolving dns

        if (settings->write_log) {
            WriteLog("ThirdTask: resolving domain");
        }

        {
            char temp[1000];
            sprintf(temp, "*** CONFIG [log: %d, domain: %s, port: %d, apn_apn: %s, apn_user: %s, apn_passwd: %s, imei: %s, ver: %s]",
                settings->write_log, settings->domain, settings->port, settings->apn_apn, settings->apn_user, settings->apn_password, settings->IMEI, settings->SOFTWARE_VERSION);
            Trace(1, temp);
        }

        bool _continue = true;
        memset(ip_address, 0, sizeof(ip_address));
        _continue = (DNS_GetHostByName2(settings->domain, ip_address) == 0);

        TIME_System_t sysTime;
        TIME_GetSystemTime(&sysTime);

        Trace(1, "**** INTERNET [%d-%02d-%02d %02d:%02d:%02d] [imei:%s, ver:%s] USING [%s] _continue is %s",
            sysTime.year, sysTime.month, sysTime.day, sysTime.hour, sysTime.minute, sysTime.second,
            settings->IMEI, settings->SOFTWARE_VERSION,
            ip_address, _continue ? "YES" : "NO");

        if (_continue) {
        	Trace(1, "**** INTERNET UP");
            if (settings->write_log) {
                WriteLog("**** INTERNET UP");
            }

            Trace(1, "**** INTERNET CONNECTING");

            if (settings->airplane_log) airplane_log("network, connecting to server");

            socket_event_id = API_EVENT_ID_MAX;
            global_socket = Socket_TcpipConnect(TCP, ip_address, settings->port);
            if (wait_socket_event(API_EVENT_ID_SOCKET_CONNECTED, -1)) {
                if (settings->airplane_log) airplane_log("network, connected to server, authorizing");

                // no_connect = 0;
                Trace(1, "**** INTERNET CONNECTED");

                network_state = 2; // internet connected

                if (settings->network_log) {
                    WriteLog("Network: connected");
                }

                const char auth_packet[] = {
                    0x07,                          // proto_ver=7 (managed)
                    15,
                        settings->IMEI[0],  settings->IMEI[1],  settings->IMEI[2],
                        settings->IMEI[3],  settings->IMEI[4],  settings->IMEI[5],
                        settings->IMEI[6],  settings->IMEI[7],  settings->IMEI[8],
                        settings->IMEI[9],  settings->IMEI[10], settings->IMEI[11],
                        settings->IMEI[12], settings->IMEI[13], settings->IMEI[14],
                    3, 'A', '9', 'G',              // man
                    10,
                        settings->SOFTWARE_VERSION[0], settings->SOFTWARE_VERSION[1],
                        settings->SOFTWARE_VERSION[2], settings->SOFTWARE_VERSION[3],
                        settings->SOFTWARE_VERSION[4], settings->SOFTWARE_VERSION[5],
                        settings->SOFTWARE_VERSION[6], settings->SOFTWARE_VERSION[7],
                        settings->SOFTWARE_VERSION[8], settings->SOFTWARE_VERSION[9]
                };
                Trace(1, "**** INTERNET SENDING AUTH");

                socket_event_id = API_EVENT_ID_MAX;
                Socket_TcpipWrite(global_socket, (uint8_t *)auth_packet, sizeof(auth_packet));

                char auth_reply[3] = {0, 0, 0};
                int ret = sockrecv(global_socket, auth_reply, sizeof(auth_reply), 0);
                if (settings->write_log) {
                    char temp[100];
                    sprintf(temp, "auth recv [%d bytes]", ret);
                    WriteLog(temp);
                }

                if (settings->airplane_log) {
                    char temp[100];
                    sprintf(temp, "network, auth result [%d], bytes [%02d %02d %02d]", ret, auth_reply[0], auth_reply[1], auth_reply[2]);
                    airplane_log(temp);
                }

                if (ret < 0) {
                    Trace(1, "**** INTERNET * recv error");
                } else if (ret == 0) {
                    Trace(1, "**** INTERNET * ret == 0");
                } else {
                    Trace(1, "**** INTERNET * SUCCESSFULL AUTH !!!! recv len:%d, data:[%d %d %d]", ret, auth_reply[0], auth_reply[1], auth_reply[2]);

                    if (auth_reply[0] == 0x00) {        // AUTH OK
                    } else if (auth_reply[0] == 0x01) { // HAS NEW VERSION
                        int size = auth_reply[1] * 256 + auth_reply[2];
                        Trace(1, "fota new version update %d bytes", size);
                        if (size > 0) {
                            char url[256];
                            sprintf(url, "http://v2.regatav6.ru/temp/ota/?ver=%s", settings->SOFTWARE_VERSION);
                            Trace(1, "fota url %s", url);
                            API_FotaByServer(url, fota_handler);
                        }
                        /*if (API_FotaInit(size)) {
                            fota_buffer_offset = 0;
                            fota_buffer = (char *)malloc(size);
                            if (fota_buffer != NULL) {
                                char update_packet[1] = { 0x01 };
                                socket_event_id = API_EVENT_ID_MAX;
                                Socket_TcpipWrite(global_socket, (uint8_t *)update_packet, sizeof(update_packet));

                                int timeout = FOTA_NETWORK_TIMEOUT, old_fota_buffer_offset = fota_buffer_offset;
                                while (fota_buffer_offset < size) {
                                    OS_Sleep(100);
                                    if (old_fota_buffer_offset != fota_buffer_offset) {
                                        timeout = FOTA_NETWORK_TIMEOUT;
                                        old_fota_buffer_offset = fota_buffer_offset;
                                    } else if (timeout -- <= 0) {
                                        break;
                                    }
                                }
                                if (fota_buffer_offset == size) {
                                    if (API_FotaReceiveData((unsigned char *)fota_buffer, size) == 0) {
                                        API_FotaClean();
                                    }
                                } else {
                                    API_FotaClean();
                                }
                                free(fota_buffer);
                            } else {
                                API_FotaClean();
                            }
                        }*/
                    } else if (auth_reply[0] == 0x02) { // SLEEP
                    }

                    network_state = 3; // normal state

                    if (check_profile()) {
                        network_thread(global_socket);

                        network_state = 4; // connection closed

                        if (settings->network_log) {
                            WriteLog("Network: disconnected");
                        }
                    }
                    if (settings->write_log) {
                        WriteLog("ThirdTask: Disconnected from server");
                    }
                }

                Socket_TcpipClose(global_socket);
            } else {
                if (global_socket != -1) {
                    Socket_TcpipClose(global_socket);
                }
            }
        } else {
            if (settings->network_log) {
                WriteLog("Network: no resolve");
            }
        	if (settings->write_log) {
                WriteLog("ThirdTask: Unable to resolve domain");
            }
        }
        OS_Sleep(10 * 1000);
    }
}

