#include "power.h"
#include "ui.h"
#include <arduino.h>
#include <esp_adc_cal.h> // to measure battery level


#define REFERANCE_PIN 14

void init_power()
{
    print_wakeup_reason();
    
    // Setup waking from sleep
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_1, 0);

    //setup pins to read the battery level of this board
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize((adc_unit_t)ADC_UNIT_1, 
                                                            (adc_atten_t)ADC_ATTEN_DB_2_5, 
                                                            (adc_bits_width_t)ADC_WIDTH_BIT_12, 
                                                            1100, 
                                                            &adc_chars);
    pinMode(REFERANCE_PIN, OUTPUT); 
}


float get_board_battery_percentage()
{
    static float battery_voltage = 0.0f;
    static float battery_percent = 0.0f;
    static unsigned long checkBatteryFreq = 2000; 

    static unsigned long lastBattCheck = 0;
    unsigned long currentTime = millis();
    float diff = (currentTime - lastBattCheck);
    if(diff > checkBatteryFreq)
    {
        lastBattCheck = currentTime;
        Serial.println("Rechecking bat");

        digitalWrite(REFERANCE_PIN, HIGH);
        delay(1);
        float adc_calibrate_factor = 0.447f;
        float measurement = (float) analogRead(34);
        battery_voltage = (measurement / 4095.0) * 7.26;
        battery_voltage += adc_calibrate_factor;
        digitalWrite(REFERANCE_PIN, LOW);
        battery_percent = (battery_voltage / 4.2f) * 100.0f;
        if(battery_percent > 100.0f)  // the ADC is not good, so fudge the battery level as not to report over 100%
            battery_percent = 100.0f;
        //Serial.print("battery_voltage: ");
        //Serial.print(battery_voltage);
        //Serial.print("   battery_percent: ");
        //Serial.println(battery_percent);
    }
    return battery_percent;
}


void deep_sleep()
{
    Serial.println("Shutting down...");
    esp_deep_sleep_start(); 
}


void print_wakeup_reason()
{
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}
