/**
 * Electric Unicycles use Bluetooth Low Energy for transmitting 
 * stats like speed and battery voltage.
 * 
 * To get this data the code must listen for BLE Advertisements,
 * connect to the wheel, find the BLE Service, find the characteristic 
 * that contains the wheel's statistics, and subscribe to notifications
 * for the stats characteristic.
 * 
 * The wheel will then continuously send two types of messages at
 * about 5 times per second.
 * 
 * The Primary wheel data message is always 20 bytes and always starts 
 * with a 4 byte magic number. See the below struct `WheelDataMsg`
 * for the format of this message.
 * 
 * The extended wheel data message is 16 bytes. It does not have a
 * magic number and is less standardized between wheels. 
 * See the struct `WheelDataExtendedMsg`
*/

#include "wheel_io.h"

#include <arduino.h>
#include <endian.h>
#include <SPI.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <esp_bt_main.h>
#include <esp_bt_device.h>


#define WHEEL_MAGIC 0xDC5A5C20  // primary message start with this 32bit value
#define SCAN_SECONDS   12       // time to scan for wheel in seconds before going to sleep

#define PEAK_HOLD_MSEC 1000


// Raw data struct that contains a BLE notification (20 bytes)
struct WheelDataMsg
{
    uint32_t header;       // big endian - should always be WHEEL_MAGIC
    uint16_t voltage;      // big endian - divide by 100 to get volts
    uint16_t speed;        // big endian - divide by 10 to get kilometers per hour
    uint32_t trip;         // big endian - divide by 1000 to get kilometers
    uint32_t odometer;     // big endian - divide by 1000 to get kilometers
    uint16_t phaseCurrent; // big endian - divide by 10 to get Amps
    uint16_t temp;         // big endian - divide by 100 to get celsius
};


// Raw data struct for the extended message that contains a BLE notification (16 bytes)
struct WheelDataExtendedMsg
{
    uint16_t powerOffTime;  // big endian - in seconds
    uint16_t chargeMode;    // big endian 
    uint16_t alarmSpeed;    // big endian - divide by 10 to get kilometers per hour
    uint16_t tiltbackSpeed; // big endian - divide by 10 to get kilometers per hour
    uint16_t version;       // big endian 
    uint16_t pedalMode;     // big endian 
    uint16_t roll;          // big endian 
    uint16_t dutyCycle;     // big endian - divide by 100 to get DutyCycle
};

struct WheelData currEucState = {};
       

// EUC manufactures reused the most generic service UUID out there.
// It appears that they just took this UUID straight from example code.
// This means its difficult to tell what BLE devices are wheels and
// which devices are other generic devices.
// This is why EUC World shows you everything when you are trying to
// find your wheel
static BLEUUID serviceUUID("0000ffe0-0000-1000-8000-00805f9b34fb"); 

// object that represents the above service
static BLEAdvertisedDevice* wheelDevice;

// The characteristic that contains wheel data.
// Reading this characteristic isn't too useful until you 
// subscribe to notifications
static BLEUUID    charUUID("0000FFE1-0000-1000-8000-00805F9B34FB"); 

// object that represents the above BLE characteristic
static BLERemoteCharacteristic* pRemoteCharacteristic;



/*
 * Call this is the setup() function
*/
void init_wheel_io()
{
    currEucState.connected = false;
    currEucState.doConnect = false;
    BLEDevice::init("");
}

/*
 * The UI should call this function to get the most
 * current stats from the wheel.
*/
WheelData* getWheelData()
{
    return &currEucState;
}


class WheelScanCallbacks: public BLEAdvertisedDeviceCallbacks 
{
    std::string targetMAC;

    public:
    WheelScanCallbacks(std::string macStr) : BLEAdvertisedDeviceCallbacks()
    {
        targetMAC = macStr;
    }

    void onResult(BLEAdvertisedDevice advertisedDevice) 
    {
        Serial.print("BLE Advertised: ");
        Serial.println(advertisedDevice.toString().c_str());
        // without this check it would connect to lots of things and wheels that use that overused service UUID
        if (advertisedDevice.getAddress().equals(targetMAC))
        {
            Serial.println("MAC Address matches");
        }
        else
        {
            //Serial.println("MAC Address does not match");
            return;     
        }

        // found a device, see if has the wheel stat service
        if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) 
        {
            Serial.println("Yep, it's running the generic wheel service"); 
            currEucState.doConnect = true;    
            BLEDevice::getScan()->stop();
            wheelDevice = new BLEAdvertisedDevice(advertisedDevice);       
        } 
    }
};


// calculate how often the wheel is sending us updates
static float calcUpdateFreq() 
{
    static unsigned long lastTime = 0;
    unsigned long currentTime = millis();
    float ups = 1000.0f / (currentTime - lastTime);
    lastTime = currentTime;
    return ups;
}


static void printhex(uint8_t* raw, size_t length)
{
    //Serial.print("Hex Len: ");
    //Serial.println(length);
    for(int i = 0; i < length; i++)  
    {
        Serial.print("0x");
        Serial.print(raw[i], HEX);
        Serial.print(", ");
    }
    Serial.println(""); 
}


/*
 * Takes the raw message from the wheel and unpack it into
 * data we can use. Everything is still left in metric.
 * The ui can change it to imperial if it wants to.
*/
static void parseWheelDataMsg( 
                            uint8_t* raw,
                            size_t length) 
{  
    if(be32dec(raw) != WHEEL_MAGIC)
    {
        Serial.print("Bad header magic number: 0x");
        Serial.println(be32dec(raw), HEX);
        return;
    }
    struct WheelDataMsg* msg = (struct WheelDataMsg*)raw;
    currEucState.voltage   = be16dec(&msg->voltage)      / 100.0f;  
    currEucState.speed     = be16dec(&msg->speed)        / 10.0f;
    currEucState.trip      = be32dec(&msg->trip)         / 1000.0f;
    currEucState.current   = be16dec(&msg->phaseCurrent) / 100.0f;
    currEucState.temp      = be16dec(&msg->temp)         / 100.0f;
    currEucState.odometer  = be32dec(&msg->odometer)     / 1000.0f;
    currEucState.msgPerSec = calcUpdateFreq();
}          


void calcDutyPeak()
{
    if(currEucState.dutyCycle > currEucState.dutyPeak)
    {
        currEucState.dutyPeak = currEucState.dutyCycle;
        currEucState.dutyPeakTime = millis();
        return;
    }
    
    unsigned long currentTime = millis();
    float diff = (currentTime - currEucState.dutyPeakTime);
    if(diff > PEAK_HOLD_MSEC)
    {
        currEucState.dutyPeak -= 3;        
        if(currEucState.dutyPeak < 0)
            currEucState.dutyPeak = 0;
    }    
}


/*
 * Extended msg version of the above function
*/
static void parseWheelDataExtendedMsg(
                          uint8_t* raw,
                          size_t length) 
{
    struct WheelDataExtendedMsg* msg = (struct WheelDataExtendedMsg*)raw;
    currEucState.powerOffTime  = be16dec(&msg->powerOffTime);
    currEucState.chargeMode    = be16dec(&msg->chargeMode);
    currEucState.alarmSpeed    = be16dec(&msg->alarmSpeed) / 10;
    currEucState.tiltbackSpeed = be16dec(&msg->alarmSpeed) / 10;
    currEucState.version       = be16dec(&msg->version);
    currEucState.pedalMode     = be16dec(&msg->pedalMode);
    currEucState.roll          = be16dec(&msg->roll);
    currEucState.dutyCycle     = be16dec(&msg->dutyCycle) / 100;
    calcDutyPeak();
}  


/*
    Receive this message whenever the wheel sends it
*/
static void notifyCallback(
                            BLERemoteCharacteristic* pBLERemoteCharacteristic,
                            uint8_t* raw,
                            size_t length,
                            bool isNotify) 
{
    // figure out what type of message we just got
    switch(length)  
    {
        case sizeof(WheelDataMsg): 
            parseWheelDataMsg(raw, length);
            break;

        case sizeof(WheelDataExtendedMsg):
            parseWheelDataExtendedMsg(raw, length);
            break;

        default:
            Serial.print("Encountered an unknown message type."); 
            printhex(raw, length);
    }
/*
   Serial.printf("Speed: %02.1f   Volts: %.1f   Amps: %.1f   Temp: %.1f   DutyCycle: %u   msgPerSec: %.1f\n", 
                currEucState.speed, 
                currEucState.voltage, 
                currEucState.current, 
                currEucState.temp,  
                currEucState.dutyCycle, 
                currEucState.msgPerSec);
*/
}


class MyClientCallback : public BLEClientCallbacks 
{
    void onConnect(BLEClient* pclient) 
    {
        currEucState.connected = true;
        Serial.println("onConnect");
    }

    void onDisconnect(BLEClient* pclient) 
    {
        memset(&currEucState, 0x0, sizeof(currEucState));
        currEucState.connected = false;        
        Serial.println("onDisconnect");
    }
};


static bool connectToWheel() 
{
    currEucState.connected = false;
    Serial.print("connecting to ");
    Serial.println(wheelDevice->getAddress().toString().c_str());
    
    BLEClient* pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());

    // connect could also accept a BLEAdvertisedDevice
    //if (pClient->connect(std::string(mac_address)) == false)
    if (pClient->connect(wheelDevice) == false)
    {
        Serial.println("connect() failed");
        return false;
    }

    Serial.println("Connected to server");

    // Obtain a reference to the wheel service
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) 
    {
        Serial.print("Failed to find service UUID: ");
        Serial.println(serviceUUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    Serial.println("Wheel Stat service found");

    // Obtain a reference to the stats characteristic in the service
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) 
    {
        Serial.print("Failed to find characteristic UUID: ");
        Serial.println(charUUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    Serial.println("Found characteristic");

    if(pRemoteCharacteristic->canRead()) 
    {
        std::string value = pRemoteCharacteristic->readValue();
        //Serial.print("The stats characteristic value is: ");
        //Serial.println(value.c_str());
    }

    if(pRemoteCharacteristic->canNotify() == false)
    {
        Serial.println("canNotify returned false");
        return false;
    }

    pRemoteCharacteristic->registerForNotify(notifyCallback);
    currEucState.connected = true;
    return true;
}


/*
* We have the MAC address so in theory we can just connect to it.
* But if the wheel is off then the BLE library and RTOS hangs in 
* a ugly way when a connection is attempted. 
* The solution is to always do a scan first to make sure 
* the wheel is on.
*/
bool findAndConnectToWheel(const char* mac_address)
{       
    currEucState.doConnect = false;      
    BLEScan* pBLEScan;
    pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new WheelScanCallbacks(std::string(mac_address)));
    pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);  
    BLEScanResults foundDevices = pBLEScan->start(SCAN_SECONDS, false);
    pBLEScan->clearResults();
    
    // Wait a few ticks to let any outstanding async scan callbacks finish
    delay(200);   

    if(currEucState.doConnect == true)
    {
        currEucState.doConnect = false;
        connectToWheel();        
    }
    return currEucState.connected;
}



