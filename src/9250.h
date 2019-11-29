
#ifndef MPU9250_h
#define MPU9250_h
//#include "stm32f3xx_hal.h"
//#include "cmsis_os.h"
   


enum GyroRange
    {
      GYRO_RANGE_250DPS,
      GYRO_RANGE_500DPS,
      GYRO_RANGE_1000DPS,
      GYRO_RANGE_2000DPS
    };
    enum AccelRange
    {
      ACCEL_RANGE_2G,
      ACCEL_RANGE_4G,
      ACCEL_RANGE_8G,
      ACCEL_RANGE_16G    
    };
    enum DlpfBandwidth
    {
      DLPF_BANDWIDTH_184HZ,
      DLPF_BANDWIDTH_92HZ,
      DLPF_BANDWIDTH_41HZ,
      DLPF_BANDWIDTH_20HZ,
      DLPF_BANDWIDTH_10HZ,
      DLPF_BANDWIDTH_5HZ
    };
    enum LpAccelOdr
    {
      LP_ACCEL_ODR_0_24HZ = 0,
      LP_ACCEL_ODR_0_49HZ = 1,
      LP_ACCEL_ODR_0_98HZ = 2,
      LP_ACCEL_ODR_1_95HZ = 3,
      LP_ACCEL_ODR_3_91HZ = 4,
      LP_ACCEL_ODR_7_81HZ = 5,
      LP_ACCEL_ODR_15_63HZ = 6,
      LP_ACCEL_ODR_31_25HZ = 7,
      LP_ACCEL_ODR_62_50HZ = 8,
      LP_ACCEL_ODR_125HZ = 9,
      LP_ACCEL_ODR_250HZ = 10,
      LP_ACCEL_ODR_500HZ = 11
    };
    
    void MPU9250(/*TwoWire &bus,*/uint8_t address);
   // void MPU9250(/*SPIClass &bus,*/uint8_t csPin);
    int begin() ;
    int setAccelRange(enum AccelRange range);
    int setGyroRange(enum GyroRange range);
    int setDlpfBandwidth(enum DlpfBandwidth bandwidth);
    int setSrd(uint8_t srd);
    int enableDataReadyInterrupt();
    int disableDataReadyInterrupt();
    int enableWakeOnMotion(float womThresh_mg,enum LpAccelOdr odr);
    int readSensor();
    float getAccelX_mss();
    float getAccelY_mss();
    float getAccelZ_mss();
    float getGyroX_rads();
    float getGyroY_rads();
    float getGyroZ_rads();
    float getMagX_uT();
    float getMagY_uT();
    float getMagZ_uT();
    float getTemperature_C();
    
    int calibrateGyro();
    float getGyroBiasX_rads();
    float getGyroBiasY_rads();
    float getGyroBiasZ_rads();
    void setGyroBiasX_rads(float bias);
    void setGyroBiasY_rads(float bias);
    void setGyroBiasZ_rads(float bias);
    int calibrateAccel();
    float getAccelBiasX_mss();
    float getAccelScaleFactorX();
    float getAccelBiasY_mss();
    float getAccelScaleFactorY();
    float getAccelBiasZ_mss();
    float getAccelScaleFactorZ();
    void setAccelCalX(float bias,float scaleFactor);
    void setAccelCalY(float bias,float scaleFactor);
    void setAccelCalZ(float bias,float scaleFactor);
    int calibrateMag();
    float getMagBiasX_uT();
    float getMagScaleFactorX();
    float getMagBiasY_uT();
    float getMagScaleFactorY();
    float getMagBiasZ_uT();
    float getMagScaleFactorZ();
    void setMagCalX(float bias,float scaleFactor);
    void setMagCalY(float bias,float scaleFactor);
    void setMagCalZ(float bias,float scaleFactor);
    // private functions
    int writeRegister(uint8_t subAddress, uint8_t data);
    int readRegisters(uint8_t subAddress, uint8_t count, uint8_t* dest);
    int writeAK8963Register(uint8_t subAddress, uint8_t data);
    int readAK8963Registers(uint8_t subAddress, uint8_t count, uint8_t* dest);
    int whoAmI();
    int whoAmIAK8963();
    
    int enableFifo(char accel,char gyro,char mag,char temp);
    int readFifo();
    void getFifoAccelX_mss(size_t *size,float* data);
    void getFifoAccelY_mss(size_t *size,float* data);
    void getFifoAccelZ_mss(size_t *size,float* data);
    void getFifoGyroX_rads(size_t *size,float* data);
    void getFifoGyroY_rads(size_t *size,float* data);
    void getFifoGyroZ_rads(size_t *size,float* data);
    void getFifoMagX_uT(size_t *size,float* data);
    void getFifoMagY_uT(size_t *size,float* data);
    void getFifoMagZ_uT(size_t *size,float* data);
    void getFifoTemperature_C(size_t *size,float* data);  
    


#endif
