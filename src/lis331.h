

#define LIS331_ADDR         0x18
#define L3G4200D_ADDR       0x68


#define RANGE_2G            2
#define RANGE_4G            4
#define RANGE_8G            8

#define WHO_AM_I            0x0F
#define CTRL_REG1           0x20
#define CTRL_REG2           0x21
#define CTRL_REG3           0x22
#define CTRL_REG4           0x23
#define CTRL_REG5           0x24

#define HP_FILTER_RESET     0x25
#define LIS_REFERENCE       0x26
#define STATUS_REG          0x27
#define OUT_X_L             0x28
#define OUT_X_H             0x29
#define OUT_Y_L             0x2A
#define OUT_Y_H             0x2B
#define OUT_Z_L             0x2C
#define OUT_Z_H             0x2D
#define INT1_CFG            0x30
#define INT1_SCR            0x31
#define INT1_THS            0x32
#define INT1_DURATION       0x33
#define INT2_CFG            0x34
#define INT2_SCR            0x35
#define INT2_THS            0x36
#define INT2_DURATION       0x37

#define FS_2G               0x00
#define FS_4G               0x10
#define FS_8G               0x30