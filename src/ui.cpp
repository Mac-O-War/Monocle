#include "ui.h"
#include "wheel_io.h"
#include "power.h"

#include <TFT_eSPI.h>
#include <Button2.h>

#define _countof(x) (sizeof(x) / sizeof (x[0]))

// display turns red if you go about the red line in MPH or duty cycle
#define SPEED_RED_LINE      40.0f   // in MPH
#define DUTY_CYCLE_RED_LINE    80

Button2 btn1(BUTTON_1);
Button2 btn2(BUTTON_2);

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);





static inline float kilometers2miles(float kilometers)
{
    return kilometers * 0.621371f;
}


static float calcFPS() 
{
    static unsigned long lastTime = 0;
    unsigned long currentTime = millis();
    float ups = 1000.00 / (currentTime - lastTime);
    lastTime = currentTime;
    return ups;
}


static void printFps()
{
    float fps = calcFPS();
    Serial.print("FPS: ");
    Serial.println(fps);
}


void displayMsg(const char* msg, int timeDelay)
{
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(1);
    tft.setTextSize(6);
    tft.drawString(msg,  tft.width() / 2, tft.height() / 2 );   
    delay(timeDelay);
}


// This can be drawn over any other display
void drawDutyCycleBar()
{
    WheelData* wheel = getWheelData();
    const static uint32_t barHeight = 30;    
    uint32_t peakWidth = 8; 
    int32_t barWidth = IWIDTH * (wheel->dutyCycle / 100.f);
    int32_t peakPos = IWIDTH * (wheel->dutyPeak / 100.f);  
    uint32_t barColor = (wheel->dutyCycle > DUTY_CYCLE_RED_LINE) ? TFT_RED : TFT_ORANGE;
    spr.fillRect(0, IHEIGHT - barHeight, IWIDTH,    barHeight, TFT_BLACK); // clear
    spr.fillRect(0, IHEIGHT - barHeight, barWidth,  barHeight,  barColor); // draw bar
    spr.fillRect(peakPos - peakWidth, IHEIGHT - barHeight, peakWidth,  barHeight,  TFT_RED); // draw peak
    spr.drawRect(0, IHEIGHT - barHeight, IWIDTH,    barHeight, TFT_WHITE); // draw boarder
}


void drawSpeed()
{  
    static char speedString[16] = {0};    // speed string

    spr.setTextFont(7);
    spr.setTextSize(2);
    spr.setCursor(15, 0);
    WheelData* wheel = getWheelData();
    float mphSpeed = kilometers2miles(wheel->speed);

    if(wheel->connected)
        dtostrf(mphSpeed, 4, 1, (char*)&speedString);
    else
        strcpy(speedString, "--.-");

    if(mphSpeed > SPEED_RED_LINE)
    {
        spr.setTextColor(TFT_RED, TFT_BLACK);
    }
    else
    {
        spr.setTextColor(TFT_GREEN, TFT_BLACK);
    }

    if(speedString[0] == ' ')
        spr.print('0'); 
    else
        spr.print(speedString[0]);     

    spr.print(speedString[1]); 
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.print(speedString[2]); 
    spr.print(speedString[3]); 
}


void drawVolts()
{
    spr.fillScreen(TFT_BLACK);
    static char voltString[16] = {0};

    spr.setTextFont(7);
    spr.setTextSize(2);
    spr.setCursor(15, 15);
    WheelData* wheel = getWheelData();

    if(wheel->connected)
        dtostrf(wheel->voltage, 5, 1, (char*)&voltString);
    else
        strcpy(voltString, "---.-v");

    spr.setTextColor(TFT_GREEN, TFT_BLACK);    
    spr.print(voltString); 
}


void drawHudBattery()
{
    static char batteryPercentString[8] = {0};
    float battery_percent = get_board_battery_percentage();
    spr.fillScreen(TFT_BLACK);
    //spr.setCursor(15, 15);
    spr.setTextFont(2);
    spr.setTextSize(1);
    dtostrf(battery_percent, 4, 2, (char*)&batteryPercentString);  
    spr.setTextColor(TFT_YELLOW, TFT_BLACK);
    spr.println(batteryPercentString); 
}


void drawDutyCycle()
{ 
    spr.setCursor(0, 0); 
    spr.fillScreen(TFT_RED);
    WheelData* wheel = getWheelData();
    spr.setTextFont(7);
    spr.setTextSize(2);
    spr.setCursor(25, 20);  

    if(wheel->dutyCycle > 80)
    {
        spr.setTextColor(TFT_BLACK, TFT_RED);
    }
    else if(wheel->dutyCycle > 60)
    {
        spr.setTextColor(TFT_BLACK, TFT_YELLOW);
    }
    else
    {
        spr.setTextColor(TFT_GREEN, TFT_BLACK);
    }  

    if(!wheel->connected)
    {
        spr.print("---"); 
    }
    else
    {
        if(wheel->dutyCycle < 100)
            spr.print('0');
        if(wheel->dutyCycle < 10)
            spr.print('0');
        spr.print(wheel->dutyCycle); 
    }
}


void drawSpeedWithDutyBar()
{
    drawSpeed();
    drawDutyCycleBar();    
}


typedef void (*DrawModFunc)();

struct DISPLAY_MODE
{
    DrawModFunc drawFunc;
    char name[12];
};

DISPLAY_MODE displayModes[] = 
{
    {drawSpeedWithDutyBar, "MULTI"},
    {drawDutyCycle,        "PWM"},
    {drawVolts,            "VOLT"},
    {drawHudBattery,       "BATT"},
};

size_t currDisplayModeIdx = 0;
DISPLAY_MODE currentMode = displayModes[currDisplayModeIdx];

static void nextMode()
{
    //tft.fillScreen(TFT_BLACK);
    //spr.fillScreen(TFT_BLACK);
    size_t numMode = _countof(displayModes);
    currDisplayModeIdx++;
    if(currDisplayModeIdx >= numMode)
        currDisplayModeIdx = 0;

    currentMode = displayModes[currDisplayModeIdx];
    //displayMsg(currentMode.name, 1300);
}


void button_init()
{
    btn1.setLongClickTime(2000);
    btn2.setLongClickTime(2000);

    btn1.setClickHandler([](Button2 & b) 
    {
        Serial.println("Button1 pressed");
        nextMode();
    });

    btn2.setClickHandler([](Button2 & b) 
    {
        Serial.println("Button 2 pressed");
        nextMode();
    });

    btn1.setLongClickDetectedHandler([](Button2 & b) 
    {
        Serial.println("Button 1 long press");
        Serial.println("Power OFF");
        displayMsg("OFF", 1300);
        deep_sleep();
    });

    btn2.setLongClickDetectedHandler([](Button2 & b) 
    {
        Serial.println("Button 2 long press");
        Serial.println("Power OFF");
        displayMsg("OFF", 1300);
        deep_sleep();  
    });

    btn1.setDoubleClickHandler([](Button2 & b) 
    {
        Serial.println("Button 1 double click");       
    });

    btn2.setDoubleClickHandler([](Button2 & b) 
    {
        Serial.println("Button 2 double click");       
    });

    btn1.setTripleClickHandler([](Button2 & b) 
    {
        Serial.println("Button 1 thriple click");       
    });

    btn2.setTripleClickHandler([](Button2 & b) 
    {
        Serial.println("Button 2 thriple click");       
    });
}


void init_ui()
{
    // set up the screen
    tft.init();
    tft.setCursor(0, 0);
    tft.setRotation(3); // 1 or 3 to flip
    tft.fillScreen(TFT_BLACK);   

    spr.createSprite(IWIDTH, IHEIGHT);
    spr.fillScreen(TFT_BLACK);
    spr.setTextFont(7);
    spr.setTextSize(2);

    displayMsg("POWER", 1200);
  
    // setup buttons
    button_init();
}


void draw_ui()
{
    btn1.loop();
    btn2.loop();        
    //currentMode.drawFunc();
    drawSpeedWithDutyBar();
    spr.pushSprite(0, 0); // draw the sprite to the display   
}