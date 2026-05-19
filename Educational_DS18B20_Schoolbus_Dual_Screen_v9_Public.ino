//Author: baDubU, 2026
//** NOTE: I2C pins are A4->SDA, A5->SCL (Screens are highly recommended, but optional, as data could only be read from serial port )
//Max ds18b20 units on an Arduino Nano is rumored to be 5, but this code supports a maximum of 2
//use a 4.7K Ohm resistor between the therm leads -- 5V and Data (highly recommended, may not function properly without the resistor)
//Lights are placed on D9 through D12 and tied to GND (optional)
//Reset button is tied to RST and GND (optional)
//DS18B20 units are connected in parallel with data connected to D2
//Turbo mode switch is tied to pin 3 and GND (optional)
//Code also outputs an approximated CPU tempature, but it may be disabled in settings below
//Code is theorized as compatible with LCD2004 (20x4), instead of LCD1602 (16x2) but is untested
//Origional code was implemented in scraps of primed pine and shoe molding for the top curves, shaped into a schoolbus with LCD1602 for windows. 


#include <Wire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>

const int LED = 13;
const int ONE_WIRE_BUS = 2;
const int HEADLIGHT_ONE = 9;
const int HEADLIGHT_TWO = 10; 
const int TOPLIGHT_ONE = 11;
const int TOPLIGHT_TWO = 12;

//This version has suppressed debugging and a new format to accommodate Arduino IDE's serial grapher
//The serial grapher may be used to show a real-time graph
//This is primarily accomplished with use of ENABLE_SERIAL_DEBUG_MESSAGES = 0
const bool ENABLE_SERIAL_DEBUG_MESSAGES = false;

//THERM_PRECISION must be 9 - 12 -- see DallasTemperature.h documentation
//9-bit, 0.5°C, ~93.75 ms
//10-bit, 0.25°C, ~187.5 ms
//11-bit, 0.125°C, ~375 ms
//12-bit, 0.0625°C, ~750 ms

//TURBO MODE is desgined to fill up a serial plotter quickly, and lower the time to sample from the therms
const int TURBO_MODE_SWITCH_PIN = 3;
const int THERM_PRECISION_TURBO_MODE = 10;
const int THERM_PRECISION_DEFAULT = 12;

//this will change depending on if Turbo Mode is engaged
int THERM_PRECISION = 12;

//economy mode turns off the lights after a number of loops
//the bus is wired with a reset button tied to the RST pin, the lights can be turned back on by resetting the system
const unsigned long LED_ECONOMY_COUNTER_LOOP_MAX = 2000;

int turboModeEngaged = false;

const int SERIAL_OUTPUT_PRECISION = 4;
const int DISPLAY_OUTPUT_PRECISION = 2;

//The main loop delay value

const int MASTER_LOOP_DELAY_MILLISECONDS_DEFAULT = 2000;
const int MASTER_LOOP_DELAY_MILLISECONDS_TURBO_MODE = 800;
const int MASTER_LOOP_DELAY_MILLISECONDS_LIGHT_ON = 300;
int MASTER_LOOP_DELAY_MILLISECONDS = MASTER_LOOP_DELAY_MILLISECONDS_DEFAULT;

//add the CPU temp to the output serial messages
const bool DISPLAY_CPU_TEMP = true;

//default is 2000, being 2 seconds
const unsigned long STARTUP_DELAYS_FOR_READABILITY = 1000;
const unsigned long STARTUP_DELAY_LONG = 5000;

//A serial speed of 9600 is slow, but recommended in the case of using long USB wires which have noise
const int SERIAL_SPEED = 9600;

const int BLINK_DURATION_MS  = 500;   // how long LEDs stay off during a blink
const int BLINK_REPEAT = 3;           // how many times to blink
const String lineClear = "               "; 

//Change based on the type of LCD Screen. LCD2004 units have 20 chars and 4 rows
//This program is only designed for 16x2 or 20x4 LCD screens
const int LCD_CHARS = 16;
const int LCD_LINES = 2;
const int LCD_WRITE_LOOP_NUM = 2; 

//These are designed to reject false readings.
const float MAX_HIGH_TEMP = 196.60;
const float MAX_LOW_TEMP = -196.60;
int deviceCount = 0;

unsigned long loopCounter = 0;

//LCD Setup, Define I2C Address where the LCD is
//Addresses can be changed by soldering the A0, A1, and A2 pins -- see online documentation
#define I2C_ADDR1 0x27 
#define I2C_ADDR2 0x23  

LiquidCrystal_I2C lcd(I2C_ADDR1, LCD_CHARS, LCD_LINES);
LiquidCrystal_I2C lcd2(I2C_ADDR2, LCD_CHARS, LCD_LINES);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

class thermInstance 
{
  private:
    float highTemp = MAX_LOW_TEMP;
    float lowTemp = MAX_HIGH_TEMP;
    int indexNum = -1;
    float currentTemp = MAX_LOW_TEMP;

    float getTempFromSensorByIndex()
    {
      return sensors.getTempFByIndex(indexNum);
    }
    

  public:
    //Constructor
    thermInstance(int indexNumArgument) 
    {
      indexNum = indexNumArgument;
    }
    int getIndexNum()
    {
      return indexNum;
    }
    float getHighTemp()
    {
      return highTemp;
    }
    float getLowTemp()
    {
      return lowTemp; 
    }
    float getCurrentTemp()
    {
      return currentTemp;
    }
    void printAddressViaSerial(DeviceAddress deviceAddress)
    {
      for (uint8_t i = 0; i < 8; i++)
      {
        if (deviceAddress[i] < 16) Serial.print("0");
        Serial.print(deviceAddress[i], HEX);
      }
    }
    void printAddressViaSerial()
    {
      DeviceAddress tempDeviceAddress;
      sensors.getAddress(tempDeviceAddress, indexNum);
      printAddressViaSerial(tempDeviceAddress);
    }
    void refreshTempData()
    {
      currentTemp = getTempFromSensorByIndex();
           
      //store a new high temp and low temp 
      if( highTemp < currentTemp && currentTemp < MAX_HIGH_TEMP )
      {
        highTemp = currentTemp;
      }
      if( lowTemp > currentTemp && currentTemp > MAX_LOW_TEMP )
      {
        lowTemp = currentTemp;
      }
      
    }
    bool filterErrorHighReadings(float currentTemp)
    {
      return ( currentTemp >= MAX_HIGH_TEMP );
    }
    bool filterErrorLowReadings(float currentTemp)
    {
      return ( currentTemp <= MAX_LOW_TEMP );
    }
};

//manually define the therm instances -- let's not make an array of a custom class in arduino -- too complex
//assume we have two of them, expand if needed someday -- todo
thermInstance thermInstance0 = thermInstance(0);
thermInstance thermInstance1 = thermInstance(1);

//*****************************************************************************************

void lcd1PrintScreen( String line0, String line1)
{
  //writing the same message multiple times to the same LCD can prevent glitches
  for( byte i=0; i < LCD_WRITE_LOOP_NUM; i++ )
  {
    lcd.setCursor(0,0);
    lcd.print( line0 );
    lcd.setCursor(0,1);
    lcd.print( line1 );
  }
  delay(5);
}

void lcd2PrintScreen( String line0, String line1)
{
  for( byte i=0; i < LCD_WRITE_LOOP_NUM; i++ )
  {
    lcd2.setCursor(0,0);
    lcd2.print( line0 );
    lcd2.setCursor(0,1);
    lcd2.print( line1 );
  }
  delay(5);
}
void toggleLightState( bool state )
{
  digitalWrite(HEADLIGHT_ONE, state);
  digitalWrite(HEADLIGHT_TWO, state);
  digitalWrite(TOPLIGHT_ONE, state);
  digitalWrite(TOPLIGHT_TWO, state);
}
void blinkAllLEDs()
{
  for( int i = 0; i < BLINK_REPEAT; i++ )
  {
    toggleLightState( false );
    delay( BLINK_DURATION_MS );
    // restore to current state, not always ON
    toggleLightState( true );  

    delay(BLINK_DURATION_MS );
  }
}
float getComputeTempInF()
{
  long sum = 0;
  for (int i = 0; i < 8; i++) 
  {          // Average 8 readings
    ADMUX = _BV(REFS1) | _BV(REFS0) | _BV(MUX3);
    delay(10);
    ADCSRA |= _BV(ADSC);
    while (ADCSRA & _BV(ADSC));
    sum += ADC;
  }
  
  int adc = sum / 8;
  
  float tempC = (adc - 315.5) / 1.22;     // Your calibration
  float tempF = tempC * 9.0 / 5.0 + 32.0;
  
  //Serial.print("Raw ADC: ");
  //Serial.print(adc);
  //Serial.print("  →  ");
  //Serial.print(tempC, 1);
  //Serial.print(" °C / ");
  //Serial.print(tempF, 1);
  //Serial.println(" °F");

  return tempF;
}
void toggleTurboMode( bool state )
{
  if( state == true )
  {
    THERM_PRECISION = THERM_PRECISION_TURBO_MODE;
    MASTER_LOOP_DELAY_MILLISECONDS = MASTER_LOOP_DELAY_MILLISECONDS_TURBO_MODE;
    turboModeEngaged = true;
    if( ENABLE_SERIAL_DEBUG_MESSAGES ){ Serial.println( "Turbo Mode Engaged! Buckle Up!" );}
  }
  else
  {
    THERM_PRECISION = THERM_PRECISION_DEFAULT;
    MASTER_LOOP_DELAY_MILLISECONDS = MASTER_LOOP_DELAY_MILLISECONDS_DEFAULT;
    turboModeEngaged = false;
    if( ENABLE_SERIAL_DEBUG_MESSAGES ){ Serial.println( "Turbo Mode Disabled! Resuming defaults." );}
  }
  
  sensors.setResolution(THERM_PRECISION);
  sensors.requestTemperatures();   // force new conversion
  delay(200);                      // settle time (safe minimum)                      
}
bool readTurboModeSwitchDebounced()
{
  const int DEBOUNCE_READS = 8;          
  
  bool initialReading = (digitalRead(TURBO_MODE_SWITCH_PIN) == LOW);

  if( ENABLE_SERIAL_DEBUG_MESSAGES )
  {
    Serial.print( "Turbo bounce check:");
    Serial.print( String(initialReading));
    Serial.println();
  }
  
  
  // Quick consecutive reads with very short delay
  for(int i = 1; i < DEBOUNCE_READS; i++) 
  {
    delayMicroseconds(800);               // 800µs = almost no noticeable lag
    if ((digitalRead(TURBO_MODE_SWITCH_PIN) == LOW) != initialReading) 
    {
      return false;                       // unstable → ignore
    }
  }
  
  return initialReading;
}


//*****************************************************************************************
void setup(void) 
{ 
  Serial.begin(SERIAL_SPEED); 

  if( ENABLE_SERIAL_DEBUG_MESSAGES )
  {
    Serial.println( "System Online" );
  }

  //turn TURBO Mode off initially
  toggleTurboMode( false);
  pinMode(TURBO_MODE_SWITCH_PIN, INPUT_PULLUP);

  Wire.begin();
  sensors.begin(); 
  deviceCount = sensors.getDeviceCount();

  lcd.init();           
  lcd.backlight(); 
  lcd2.init();           
  lcd2.backlight();            
  lcd.home();
  lcd2.home();

    // Define pin #13 as output, for the LED
  pinMode( LED, OUTPUT );

  lcd.clear();
  lcd2.clear();

  //Fun output for show
  lcd1PrintScreen( "*** Science! ***", "With TEACHER!" );
  lcd2PrintScreen( "Buckle Up!", "   Bus Loading..." );

  if( ENABLE_SERIAL_DEBUG_MESSAGES )
  {
    Serial.println( "*** Science! ***"); 
    Serial.println( "With Mrs. Bomar!" );
    Serial.println( "Buckle Up!" );
    Serial.println( "  Bus Loading..." );
  }
  delay( STARTUP_DELAYS_FOR_READABILITY );

  //Enable the lights
  pinMode( HEADLIGHT_ONE, OUTPUT );
  pinMode( HEADLIGHT_TWO, OUTPUT );
  pinMode( TOPLIGHT_ONE,  OUTPUT );
  pinMode( TOPLIGHT_TWO,  OUTPUT );
  
  toggleLightState(true );
  blinkAllLEDs();
  lcd.clear();
  lcd2.clear(); 
  lcd1PrintScreen( "DS18B20", "Devices: " + String( deviceCount ));
  lcd2PrintScreen( "Turbo Switch", "Under Hood." );

  if( ENABLE_SERIAL_DEBUG_MESSAGES )
  {
    Serial.println( "DS18B20 Devices: " + String( deviceCount ));
    Serial.println( "Resolution: " + String(THERM_PRECISION)); 
  }
  delay(STARTUP_DELAYS_FOR_READABILITY);

  blinkAllLEDs();
  lcd.clear();
  lcd2.clear();
  lcd1PrintScreen( "USB Serial Data", String(SERIAL_SPEED) + " Baud"); 
  lcd2PrintScreen( "Transmission...", "ONLINE"); 
  delay(STARTUP_DELAYS_FOR_READABILITY);
    
  if (sensors.isParasitePowerMode())
  {
    if( ENABLE_SERIAL_DEBUG_MESSAGES )
    {
      Serial.println("Parasite Power: ON");
    }
    lcd1PrintScreen( "Parasite Power:", "ON");
    delay(STARTUP_DELAY_LONG);
  }  

  if( ENABLE_SERIAL_DEBUG_MESSAGES )
  {
    Serial.println( "*****Begin*****" );
  }

  //print the serial header to name the variables
  //a good collection script on a computer with a processor can evaluate lows and highs later, and add a timestamp

  Serial.print( "Temp_0,Temp_1" );
  if( DISPLAY_CPU_TEMP )
  {
     Serial.print( ",Temp_CPU");
  }
  Serial.println();
  

  blinkAllLEDs();
} 

//*****************************************************************************************
void loop(void) 
{ 

  bool turboModePinStableRead = readTurboModeSwitchDebounced();

  //turn on turbo mode
  if( turboModePinStableRead == true && turboModeEngaged == false )
  {
    if( ENABLE_SERIAL_DEBUG_MESSAGES )
    {
      Serial.println( "Turbo Mode Engaged!");
    }
    toggleTurboMode( true );

    lcd.clear();
    lcd2.clear();
    
    lcd1PrintScreen( "Turbo Mode:", "Engaged.");
    lcd2PrintScreen( "LET'S GO!", "****************" );

    delay(STARTUP_DELAY_LONG);
  }

  //turn off TURBO mode
  if( turboModePinStableRead == false && turboModeEngaged == true )
  {
    toggleTurboMode( true );
    if( ENABLE_SERIAL_DEBUG_MESSAGES )
    {
      Serial.println( "Turbo Mode Disabled!");
    }
    
    toggleTurboMode( false );
    lcd.clear();
    lcd2.clear();
    
    lcd1PrintScreen( "Turbo Mode:", "Disabled.");
    lcd2PrintScreen( "Normal Speed.", "Higher Accuracy!" );

     delay(STARTUP_DELAY_LONG);
  }
    
  
  digitalWrite( LED, LOW );
  delay(MASTER_LOOP_DELAY_MILLISECONDS);
  digitalWrite( LED, HIGH );
  delay(MASTER_LOOP_DELAY_MILLISECONDS_LIGHT_ON);   

  // Send the global command to get temperature readings
  sensors.requestTemperatures(); 

  if( deviceCount <= 0 )
  {
    if( ENABLE_SERIAL_DEBUG_MESSAGES )
    {
      Serial.println( "Error: 0 Sensors Found" );
    }
    lcd1PrintScreen( "Error:", "0 Sensors Found" );
    lcd2PrintScreen( "Error:", "0 Sensors Found" );
    delay(STARTUP_DELAYS_FOR_READABILITY);
    return;
  }
  else if( deviceCount > 2 )
  {
    if( ENABLE_SERIAL_DEBUG_MESSAGES )
    {
      Serial.println( "Error: Extra Sensors Unsupported" );
    }
    
    lcd1PrintScreen( "Error:", "Too Many Sensors" );
    lcd2PrintScreen( "Error:", "Too Many Sensors" );
    delay(STARTUP_DELAYS_FOR_READABILITY);
    return;
  }
  
  if( ENABLE_SERIAL_DEBUG_MESSAGES )
  {
    Serial.print( thermInstance0.getIndexNum() );
  }

  if( deviceCount >= 1 )
  {
    String outputLine1;
    String outputLine2;
    
    thermInstance0.refreshTempData();
    float currentTemp = thermInstance0.getCurrentTemp();
    
    if( thermInstance0.filterErrorLowReadings(currentTemp) || thermInstance0.filterErrorHighReadings(currentTemp) )
    {
      //error case
      outputLine1 = "L:" + String(thermInstance0.getLowTemp(), DISPLAY_OUTPUT_PRECISION) + " H:" + String(thermInstance0.getHighTemp(), DISPLAY_OUTPUT_PRECISION) + lineClear;
      outputLine2 = "* ERROR *" + lineClear;
      lcd1PrintScreen(outputLine1, outputLine2);
      
      //print nothing for the serial monitor
      Serial.print("");
    }
    else
    {
      //optimal case of no data read issues from the therms
      outputLine1 = "L:" + String(thermInstance0.getLowTemp(), DISPLAY_OUTPUT_PRECISION) + " H:" + String(thermInstance0.getHighTemp(), DISPLAY_OUTPUT_PRECISION) + lineClear;
      outputLine2 = "* " + String(currentTemp, DISPLAY_OUTPUT_PRECISION) + " F *    ";

      if( turboModeEngaged == true)
      {
        outputLine2 += "!";
      }
      
      outputLine2 += lineClear;
      lcd1PrintScreen(outputLine1, outputLine2);
      Serial.print(String(currentTemp, SERIAL_OUTPUT_PRECISION));
    }
    
  }
  else
  {
    lcd1PrintScreen("Error:", "Sensor 0 Missing");
  }
  
  //
  Serial.print(",");
  
  if( deviceCount == 2 )
  {
    
    String outputLine1;
    String outputLine2;
    
    thermInstance1.refreshTempData();
    float currentTemp = thermInstance1.getCurrentTemp();
    
    if( thermInstance1.filterErrorLowReadings(currentTemp) || thermInstance1.filterErrorHighReadings(currentTemp) )
    {
      //error case
      outputLine1 = "L:" + String(thermInstance1.getLowTemp(), DISPLAY_OUTPUT_PRECISION) + " H:" + String(thermInstance1.getHighTemp(), DISPLAY_OUTPUT_PRECISION) + lineClear;
      outputLine2 = "* ERROR *" + lineClear;
      lcd2PrintScreen(outputLine1, outputLine2);
      
      //print nothing for the serial monitor
      Serial.print("");
    }
    else
    {
      //optimal case of no data read issues from the therms
      outputLine1 = "L:" + String(thermInstance1.getLowTemp(), DISPLAY_OUTPUT_PRECISION) + " H:" + String(thermInstance1.getHighTemp(), DISPLAY_OUTPUT_PRECISION) + lineClear;
      outputLine2 = "* " + String(currentTemp, DISPLAY_OUTPUT_PRECISION) + " F *    ";

      if( turboModeEngaged )
      {
        outputLine2 += "!";
      }
      
      outputLine2 += lineClear;
      lcd2PrintScreen(outputLine1, outputLine2);
      Serial.print(String(currentTemp, SERIAL_OUTPUT_PRECISION));
    }
  }
  else
  {
    lcd2PrintScreen("Error:", "Sensor 1 Missing");
  }
  

  if(DISPLAY_CPU_TEMP)
  {
    Serial.print(",");
    Serial.print( String(getComputeTempInF(), SERIAL_OUTPUT_PRECISION) );
  }

  Serial.println( );

  //activate economy mode here, it will only run once
  if( loopCounter == LED_ECONOMY_COUNTER_LOOP_MAX )
  {
    lcd.clear();
    lcd2.clear();
    
    blinkAllLEDs();
    
    //duplicate on purpose, stylistic
    lcd1PrintScreen("Economy Mode", "Activated.");
    lcd2PrintScreen("Economy Mode", "Activated.");
    if( ENABLE_SERIAL_DEBUG_MESSAGES ){ Serial.println( "Economy Mode Activated."); }
    
    delay(STARTUP_DELAY_LONG);
    
    toggleLightState( false );
    lcd.noBacklight();
    lcd2.noBacklight();

  }
  
  //turn the lights off if we hit economy mode
  loopCounter++;

}
