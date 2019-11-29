#include "a9.h"

struct SectorInfo sectorInfo = {
    .sent   = UNINITIALIZED, // external sd card
    .total  = 0, // external sd card
    .sent2  = 0, // internal flash
    .total2 = 0  // internal flash
};

void load_config() {
    // external sd card
    int32_t fd = API_FS_Open(DATA_FILE_NAME, FS_O_RDONLY | FS_O_CREAT, 0);
    if (fd >= 0) {
        sectorInfo.total = API_FS_GetFileSize(fd) / SECTOR_SIZE;
        API_FS_Close(fd);
    }

    fd = API_FS_Open(CONFIG_DATA_FILE, FS_O_RDONLY | FS_O_CREAT, 0);
    if (fd >= 0) {
    	if (API_FS_Read(fd, (uint8_t *)&sectorInfo.sent, sizeof(int64_t)) <= 0) {
    		sectorInfo.sent = sectorInfo.total;
	    }
        API_FS_Close(fd);
    }
    if ((sectorInfo.sent != UNINITIALIZED) && (sectorInfo.sent > sectorInfo.total)) {
    	sectorInfo.sent = sectorInfo.total;
    }

    // internal flash
    fd = API_FS_Open(DATA2_FILE_NAME, FS_O_RDONLY | FS_O_CREAT, 0);
    if (fd >= 0) {
        sectorInfo.total2 = API_FS_GetFileSize(fd) / SECTOR_SIZE;
        API_FS_Close(fd);
    }

    fd = API_FS_Open(CONFIG2_DATA_FILE, FS_O_RDONLY | FS_O_CREAT, 0);
    if (fd >= 0) {
    	if (API_FS_Read(fd, (uint8_t *)&sectorInfo.sent2, sizeof(int64_t)) <= 0) {
    		sectorInfo.sent2 = sectorInfo.total2;
	    }
        API_FS_Close(fd);
    }
    if (sectorInfo.sent2 > sectorInfo.total2) {
    	sectorInfo.sent2 = sectorInfo.total2;
    }
}

void update_config() {
    int32_t fd;

    // external sd card    
    if (sectorInfo.sent != UNINITIALIZED) {
        fd = API_FS_Open(CONFIG_DATA_FILE, FS_O_RDWR | FS_O_CREAT, 0);
        if (fd >= 0) {
            API_FS_Seek(fd, 0, FS_SEEK_SET);
            API_FS_Write(fd, (uint8_t *)&sectorInfo.sent, sizeof(int64_t));
            API_FS_Flush(fd);
            API_FS_Close(fd);
        }
    }

    // internal flash
    if (sectorInfo.sent2 >= sectorInfo.total2 && sectorInfo.sent2 > 0) {
        Trace(1, "*************** DELETE INTERNAL: total: %ld, unsent: %ld", (long int)sectorInfo.total2, (long int)(sectorInfo.total2 - sectorInfo.sent2));
        sectorInfo.sent2 = sectorInfo.total2 = 0;
        OS_LockMutex(data_lock);
        API_FS_Delete(DATA2_FILE_NAME); // delete internal flash data file
        OS_UnlockMutex(data_lock);
    }

    fd = API_FS_Open(CONFIG2_DATA_FILE, FS_O_RDWR | FS_O_CREAT, 0);
    if (fd >= 0) {
        API_FS_Seek(fd, 0, FS_SEEK_SET);
        API_FS_Write(fd, (uint8_t *)&sectorInfo.sent2, sizeof(int64_t));
        API_FS_Flush(fd);
        API_FS_Close(fd);
    }

	Trace(1, "*************** INIT SD: total: %ld, unsent: %ld", (long int)sectorInfo.total, (long int)(sectorInfo.total - sectorInfo.sent));
    Trace(1, "*************** INIT INTERNAL: total: %ld, unsent: %ld", (long int)sectorInfo.total2, (long int)(sectorInfo.total2 - sectorInfo.sent2));
}

int profile_read = 0, profile_del = 0, profile_upd = 0;

void load_profile() {
    profile_del = 0;
    int32_t fd = API_FS_Open(DEL_PROFILE_FILE, FS_O_RDONLY | FS_O_CREAT, 0);
    if (fd >= 0) {
        char value = 0;
        if (API_FS_Read(fd, (uint8_t *)&value, sizeof(char)) == sizeof(char)) {
            if (value == '1') {
                profile_del = 1;
                
                API_FS_Delete(DEL_PROFILE_FILE);
                API_FS_Delete(PROFILE_DATA_FILE);
            }
        }
        API_FS_Close(fd);
    }

    /*profile_del = 0;
    Dir_t *dir = API_FS_OpenDir(FS_DEVICE_NAME_T_FLASH);
    if (dir != NULL) {
        Dirent_t *dirent = NULL;
        while (dirent = API_FS_ReadDir(dir)) {
            if (strnicmp(dirent->d_name, DEL_PROFILE_FILE, strlen(DEL_PROFILE_FILE)) == 0) {
                API_FS_Delete(DEL_PROFILE_FILE);
                API_FS_Delete(PROFILE_DATA_FILE);

                profile_del = 1;
                break;
            }
        }
        API_FS_CloseDir(dir);
    }*/

    fd = API_FS_Open(PROFILE_DATA_FILE, FS_O_RDONLY | FS_O_CREAT, 0);
    if (fd >= 0) {
        profile_read = 1;
        char _Profile[24];
    	if (API_FS_Read(fd, (uint8_t *)_Profile, sizeof(char) * 24) == sizeof(char) * 24) {
            profile_read = 2;


            Trace(1, "*** AIRPLANE [profile_read=%d] PROFILE _file_ READ#0 [%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
                profile_read,
                _Profile[0], _Profile[1], _Profile[2], _Profile[3], 
                _Profile[4], _Profile[5], _Profile[6], _Profile[7], 
                _Profile[8], _Profile[9], _Profile[10], _Profile[11], 
                _Profile[12], _Profile[13], _Profile[14], _Profile[15], 
                _Profile[16], _Profile[17], _Profile[18], _Profile[19], 
                _Profile[20], _Profile[21], _Profile[22], _Profile[23]);

    		for (int i = 0; i < 24; i ++) {
                Profile[i] = _Profile[i];
            }
	    }
        API_FS_Close(fd);
    }

    int sum = 0;
    for (int ii = 0; ii < 24; ii++) {
        sum += Profile[ii];
    }

    Trace(1, "*** AIRPLANE [profile_read=%d] PROFILE _file_ READ [%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
        profile_read,
        Profile[0], Profile[1], Profile[2], Profile[3], 
        Profile[4], Profile[5], Profile[6], Profile[7], 
        Profile[8], Profile[9], Profile[10], Profile[11], 
        Profile[12], Profile[13], Profile[14], Profile[15], 
        Profile[16], Profile[17], Profile[18], Profile[19], 
        Profile[20], Profile[21], Profile[22], Profile[23]);

    if (sum == 0) {
        profile_read += 100;
        Trace(1, "ERROR IN CONFIG");
        if (settings->write_log) {
            WriteLog("ERROR IN CONFIG");
        }
        for (int ii = 0; ii < 24; ii++) {
            Profile[ii] = HP__Online;
        }
    }    

    Trace(1, "*** AIRPLANE [profile_read=%d] PROFILE _file_ READ#2 [%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
        profile_read,
        Profile[0], Profile[1], Profile[2], Profile[3], 
        Profile[4], Profile[5], Profile[6], Profile[7], 
        Profile[8], Profile[9], Profile[10], Profile[11], 
        Profile[12], Profile[13], Profile[14], Profile[15], 
        Profile[16], Profile[17], Profile[18], Profile[19], 
        Profile[20], Profile[21], Profile[22], Profile[23]);
}

void update_profile() {
    profile_upd = 0;

    int32_t fd = API_FS_Open(PROFILE_DATA_FILE, FS_O_RDWR | FS_O_CREAT, 0);
    if (fd >= 0) {
        API_FS_Seek(fd, 0, FS_SEEK_SET);
        API_FS_Write(fd, (uint8_t *)Profile, sizeof(char) * 24);
        API_FS_Flush(fd);
        API_FS_Close(fd);

        profile_upd = 1;

        Trace(1, "*** AIRPLANE PROFILE _file_ UPDATED [%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
            Profile[0], Profile[1], Profile[2], Profile[3], 
            Profile[4], Profile[5], Profile[6], Profile[7], 
            Profile[8], Profile[9], Profile[10], Profile[11], 
            Profile[12], Profile[13], Profile[14], Profile[15], 
            Profile[16], Profile[17], Profile[18], Profile[19], 
            Profile[20], Profile[21], Profile[22], Profile[23]);
    } else {
        profile_upd = 2;

        Trace(1, "*** AIRPLANE PROFILE _file_ NOT UPDATED [%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d]",
            Profile[0], Profile[1], Profile[2], Profile[3], 
            Profile[4], Profile[5], Profile[6], Profile[7], 
            Profile[8], Profile[9], Profile[10], Profile[11], 
            Profile[12], Profile[13], Profile[14], Profile[15], 
            Profile[16], Profile[17], Profile[18], Profile[19], 
            Profile[20], Profile[21], Profile[22], Profile[23]);
    }

    load_profile();
}
