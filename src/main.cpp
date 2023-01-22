/**
 * A tiny display for Electric Unicycle stats like speed and Duty Cycle
 * 
 * Runs on this board
 * https://www.amazon.com/dp/B099MPFJ9M
 * Get slower shipping but a better price here:
 * https://www.lilygo.cc/products/lilygo%C2%AE-ttgo-t-display-1-14-inch-lcd-esp32-control-board
 * 
 * Nothing to connect or solder
 * 
 * Gets about 5 updates per second from my Sherman Max
 * The display is outputting about 60 FPS
*/
#include <arduino.h>
#include <endian.h>

#include "wheel_io.h"
#include "ui.h"
#include "power.h"

// The wheel's MAC address
const char* wheel_mac_addr = "88:25:83:f3:62:fa";  // jason
//const char* wheel_mac_addr = "88:25:83:F3:61:F5";  // sam


void maintain_wheel_connection()
{
    WheelData* wheel = getWheelData();
    if (wheel->connected == false) 
    {
        draw_ui();
        bool ret = findAndConnectToWheel(wheel_mac_addr);
        if(!ret)
        {
            Serial.println("Failed to connect");
            Serial.println("Can't connect so powering off");
            displayMsg("!FOUND", 2400);
            displayMsg("OFF", 1300);
            deep_sleep();     
        }  
    }     
}


void setup(void) 
{
    Serial.begin(115200);
    Serial.println("Starting up...");
    init_power();
    init_ui();
    init_wheel_io();
}


void loop() 
{
    draw_ui();
    maintain_wheel_connection();
}
