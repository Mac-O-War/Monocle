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


static bool showFPS = false;


static inline float kilometers2miles(float kilometers)
{
    return kilometers * 0.621371f;
}


/*
 * the float libs are limited on this platform
 * sprintf and dtostrf won't do leading zeros.
 * So this function replaces spaces for zeros.
 * It doesn't handle negative numbers
*/
static void spacesToZeros(char* charStr) 
{
    size_t len = strlen(charStr);
    for (int i = 0; i < len; i++)
    {
        if (charStr[i] == ' ')
            charStr[i] = '0';
        else
            break;
    }
}


/* 
 * there's a bug in spr.fillScreen() where only half the screen 
 * is cleared. I think it happens because this code rotates the 
 * screen. This function is the workaround. It just draws a 
 * rectangle over the screen
*/
static inline void clearDisplay()
{
    spr.fillRect(0, 0, IWIDTH, IHEIGHT, TFT_BLACK);
}


static float calcFPS() 
{
    static unsigned long lastTime = 0;
    unsigned long currentTime = millis();
    float ups = 1000.00 / (currentTime - lastTime);
    lastTime = currentTime;
    return ups;
}


static void drawFPS()
{
    float fps = calcFPS();
    spr.setTextFont(2);
    spr.setTextSize(1);
    spr.setTextDatum(BR_DATUM);
    spr.setTextColor(TFT_YELLOW, TFT_BLACK);
    spr.drawFloat(fps, 1, IWIDTH - 12, IHEIGHT - 8); 
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


static void drawModeFooter(const char* str)
{
    spr.setTextFont(4);
    spr.setTextSize(1);
    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.drawString(str, IWIDTH/2, IHEIGHT/2 + 41); 
}


// This can be drawn over any other display
static void drawDutyCycleBar()
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
    spr.drawRect(0, IHEIGHT - barHeight, IWIDTH, barHeight, TFT_WHITE); // draw boarder

    // Only draw the 'PWM' label if it won't be in the 
    // way of the pwm bar.
    if( (wheel->dutyCycle < 35) && (wheel->dutyPeak < 35) )
        drawModeFooter("PWM");    
}



static void drawVerticalSpeedUnits(const char* units)
{
    spr.setTextFont(4);
    spr.setTextSize(1);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(TFT_RED, TFT_BLACK);
    spr.drawChar(units[0], IWIDTH - 18, 15); 
    spr.drawChar(units[1], IWIDTH - 18, 40);
    spr.drawChar(units[2], IWIDTH - 18, 65);
}


static void drawSpeedNumber()
{  
    char speedString[8];
    spr.setTextFont(7);
    spr.setTextSize(2);
    spr.setCursor(0, 0);
    WheelData* wheel = getWheelData();
    float mphSpeed = kilometers2miles(wheel->speed);
    dtostrf(mphSpeed, 4, 1, (char*)&speedString);
    spacesToZeros(speedString);

    if(mphSpeed > SPEED_RED_LINE)
    {
        spr.setTextColor(TFT_RED, TFT_BLACK);
    }
    else
    {
        spr.setTextColor(TFT_GREEN, TFT_BLACK);
    }

    spr.print(speedString[0]); 
    spr.print(speedString[1]); 
    spr.setTextColor(TFT_WHITE, TFT_BLACK);
    spr.print(speedString[2]); 
    spr.print(speedString[3]);

    drawVerticalSpeedUnits("MPH");
    //drawVerticalSpeedUnits("KPH");   
}


static void drawSpeed()
{  
    drawSpeedNumber();
    drawModeFooter("Speed");
}


static void drawConnecting()
{
    clearDisplay();
    spr.setTextFont(4);
    spr.setTextSize(2);
    spr.setTextDatum(MC_DATUM);
    spr.drawString("*", IWIDTH/2, IHEIGHT/2); 
}


static void drawVolts()
{    
    char voltString[8];
    WheelData* wheel = getWheelData();
    dtostrf(wheel->voltage, 5, 1, (char*)&voltString);    
    spacesToZeros(voltString);

    clearDisplay();

    spr.setTextFont(8);
    spr.setTextDatum(BC_DATUM);
    spr.setTextSize(1);
    spr.setTextColor(TFT_ORANGE, TFT_BLACK); 
    spr.drawString(voltString,  IWIDTH/2, IHEIGHT/2 + 20);  

    drawModeFooter("Volts"); 
}


static void drawHudBattery()
{
    static char batteryPercentString[8] = {0};
    float battery_percent = get_board_battery_percentage();
    
    char battStr[8] = {0};
    dtostrf(battery_percent, 3, 0, (char*)&battStr);
    
    clearDisplay();

    spr.setTextFont(8);
    spr.setTextSize(1);
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(TFT_BLUE, TFT_BLACK);
    spr.drawString(battStr, IWIDTH/2, IHEIGHT/2 - 15);  

    spr.setTextFont(4);
    spr.setTextSize(1);
    spr.setTextDatum(MC_DATUM);
    spr.drawString("%", IWIDTH - 18, IHEIGHT/2 + 15); 

    drawModeFooter("Display Battery"); 
}


static void drawDutyCycle()
{ 
    WheelData* wheel = getWheelData();
    
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

    char dutyString[4] = {0};
    sprintf(dutyString, "%03d", wheel->dutyCycle);
    
    spr.setTextFont(7);
    spr.setTextSize(2);
    spr.setTextDatum(MC_DATUM);
    spr.drawString(dutyString, IWIDTH/2, IHEIGHT/2 - 15);  

    spr.setTextFont(4);
    spr.setTextSize(1);
    spr.setTextDatum(MC_DATUM);
    spr.drawString("%", IWIDTH - 18, IHEIGHT/2 + 25); 

    drawModeFooter("Duty Cycle");   
}



static void drawSpeedWithDutyBar()
{
    drawSpeedNumber();
    drawDutyCycleBar();    
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
        Serial.println("Toggling FPS");
        showFPS = !showFPS;       
    });

    btn2.setTripleClickHandler([](Button2 & b) 
    {
        Serial.println("Button 2 triple click");       
    });
}


void init_ui()
{
    // set up the screen
    tft.init();
    tft.setCursor(0, 0);
    tft.setRotation(3); // 1 or 3 to flip depending on how it's mounted
    tft.fillScreen(TFT_BLACK);   

    // the sprite is used for double buffering
    spr.createSprite(IWIDTH, IHEIGHT);
    spr.setCursor(0, 0);    
    spr.fillRect(0, 0, IWIDTH, IHEIGHT, TFT_BLACK);
    spr.pushSprite(0, 0);
    spr.setTextFont(7);
    spr.setTextSize(2);

    // tell the user the display is working
    displayMsg("POWER", 0);
  
    // setup buttons
    button_init();
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
    {drawSpeed,            "SPEED"},
    {drawDutyCycle,        "PWM"},
    {drawVolts,            "VOLT"},
    {drawHudBattery,       "BATT"},
};

DISPLAY_MODE disconnectedMode = {drawConnecting, "Connecting"};

size_t currDisplayModeIdx = 0;
DISPLAY_MODE currentMode = disconnectedMode;


void nextMode()
{
    clearDisplay();
    size_t numMode = _countof(displayModes);
    currDisplayModeIdx++;
    if(currDisplayModeIdx >= numMode)
        currDisplayModeIdx = 0;

    currentMode = displayModes[currDisplayModeIdx];
}


static void checkWheelConnection()
{
    if(getWheelData()->connected)
        currentMode = displayModes[currDisplayModeIdx];
    else 
        currentMode = disconnectedMode;
}



void draw_ui()
{    
    btn1.loop();
    btn2.loop();

    checkWheelConnection(); 

    currentMode.drawFunc();

    if(showFPS)
        drawFPS();

    spr.pushSprite(0, 0); // draw the sprite to the display     
}



