/*
 * Project Sensors-Test
 * Description:
 * Author:
 * Date:
 */

#include "thingspeak-webhooks.h"
#include "Particle.h"
#include "secrets.h"

ThingSpeakWebhooks thingspeak;
String csvData;
String time_format = "absolute";

const char *writeKey = WRITE_KEY;
const int channelID = CHANNEL_ID;

volatile unsigned long anemCount, rainCount;
uint64_t last_time, rainLastTime;
double windSpeed, windDirection, rainFall, temperature, humidity;

#define SAMPLE_DELAY (60)

// setup() runs once, when the device is first turned on.
void setup() 
{
  Serial.begin(9600);

  // Initialize global variables
  rainCount = 0;
  anemCount = 0;
  last_time = millis();
  rainLastTime = millis();
  windSpeed = 0.0;
  windDirection = 0.0;
  rainFall = 0.0;
  temperature = 0.0;
  humidity = 0.0;

  // Reserve space in memory for our data
  csvData.reserve(62);

  pinMode(2, INPUT_PULLUP); // Anemometer switch pin
  pinMode(3, INPUT_PULLUP); // Rain gauge switch pin

  attachInterrupt(2, ANEM_ISR, RISING);
  attachInterrupt(3, RAIN_ISR, FALLING);
}

void loop() 
{
  // Calculate and print wind speed & direction every 2s
  if(millis() - last_time >= (SAMPLE_DELAY * 1000))
  {
    // ===================================
    // Load in interrupt counters
    // ATOMIC BLOCK
    ATOMIC_BLOCK()
    {
      windSpeed = anemCount;
      rainFall = rainCount;

      anemCount = 0; // Reset counters
      rainCount = 0;
    }
    // ===================================
    
    // Calculate wind speed 
    windSpeed /= (2.0 * SAMPLE_DELAY); // Two pulses per rev, 2 sec sampling period
    windSpeed *= 1.492*1.15; // 1.492 mph = 1 pulse/second, 1.15 knots/mph

    // Calculate wind direction
    windDirection = lookupFromRaw(analogRead(A0));
    
    // Calculate rainfall ammount
    rainFall *= 0.011; // 0.011 inches of rain per click

    // Print Results
    Serial.print("Wind Speed: ");
    Serial.println(windSpeed);
  
    Serial.print("Direction: ");
    Serial.println(windDirection);

    Serial.print("Rain ammount: ");
    Serial.println(rainFall);

    // Build the data string
    csvData = 
      String(Time.now())+","+ // Current time
      minimiseNumericString(String::format("%.1f",(float)windDirection),1)+","+ // Wind direction
      minimiseNumericString(String::format("%.3f",(float)windSpeed),3)+","+ // Wind speed
      minimiseNumericString(String::format("%.2f",(float)humidity),2)+","+ // Humidity
      minimiseNumericString(String::format("%.2f",(float)temperature),2)+","+ // Air Temp
      minimiseNumericString(String::format("%.3f",(float)rainFall),3)+","+ // Rainfall
      +",,,,,,";

    Serial.println("Built data string");
    Serial.println(csvData);

    // Send data over particle's event system  
    if(waitFor(Particle.connected,20000))
    {
      thingspeak.TSBulkWriteCSV(String(channelID), String(writeKey), time_format, csvData);
      last_time = millis();
    }
    else
    {
      // Timeout
      Serial.println("No connection!");
    }

    Serial.println("===============================");
  } // End upload code

}

float lookupFromRaw(uint16_t adc)
{
    //Serial.println(adc);
    // The mechanism for reading the weathervane isn't arbitrary, but effectively, we just need to look up which of the 16 positions we're in.
    if (adc < 299) return (113);
    if (adc < 354) return (68);
    if (adc < 439) return (90);
    if (adc < 622) return (158); 
    if (adc < 859) return (135); 
    if (adc < 1064) return (203); 
    if (adc < 1387) return (180); 
    if (adc < 1735) return (23); 
    if (adc < 2122) return (45); 
    if (adc < 2459) return (248); 
    if (adc < 2666) return (225); 
    if (adc < 2977) return (338); 
    if (adc < 3277) return (0); 
    if (adc < 3430) return (293); 
    if (adc < 3665) return (315); 
    if (adc < 4000) return (270); 
    return (-1); // error, disconnected?
}

void ANEM_ISR()
{
  anemCount++;
}

void RAIN_ISR()
{
  if(millis() - rainLastTime >= 20)
  {
    rainCount++;
    rainLastTime = millis();
  }
}

//https://stackoverflow.com/questions/277772/avoid-trailing-zeroes-in-printf
String minimiseNumericString(String ss, int n) {
    int str_len = ss.length() + 1;
    char s[str_len];
    ss.toCharArray(s, str_len);

    //Serial.println(s);
    char *p;
    int count;

    p = strchr (s,'.');         // Find decimal point, if any.
    if (p != NULL) {
        count = n;              // Adjust for more or less decimals.
        while (count >= 0) {    // Maximum decimals allowed.
             count--;
             if (*p == '\0')    // If there's less than desired.
                 break;
             p++;               // Next character.
        }

        *p-- = '\0';            // Truncate string.
        while (*p == '0')       // Remove trailing zeros.
            *p-- = '\0';

        if (*p == '.') {        // If all decimals were zeros, remove ".".
            *p = '\0';
        }
    }
    return String(s);
}