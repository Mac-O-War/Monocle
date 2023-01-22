#include <endian.h>


// after parsing the raw notifications, this struct holds wheel data
struct WheelData 
{
    float voltage;
    float speed;
    float speedPeak;
    float trip;
    float current;
    float temp;
    float odometer;
    uint16_t powerOffTime;
    uint16_t chargeMode;
    uint16_t alarmSpeed;
    uint16_t version;
    uint16_t tiltbackSpeed;
    uint16_t pedalMode;
    uint16_t roll;
    uint16_t dutyCycle;
    int dutyPeak;
    unsigned long dutyPeakTime;
    bool connected;
    bool doConnect;
    float msgPerSec; // number of primary data msg per second
};

typedef struct WheelData WheelData;

void init_wheel_io();

bool findAndConnectToWheel(const char* mac_address);

void scanForWheels();

WheelData* getWheelData();

