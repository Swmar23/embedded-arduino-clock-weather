#include <Wire.h>
#include <math.h>
#include <TimerOne.h>
#include <IRremote.hpp>
#include <SoftwareSerial.h>
#include "DS1307.h"
#include "Seeed_BME280.h"
#include "rgb_lcd.h"
#include "TM1637.h"

#define rxPin 10
#define txPin 11
#define CLK 2//pins definitions for TM1637 and can be changed to other ports
#define DIO 3

SoftwareSerial HC5(rxPin, txPin);
char appData;  

int8_t TimeDisp[] = {0x02, 0x01, 0x03, 0x07};
TM1637 tm1637(CLK, DIO);

rgb_lcd lcd;
BME280 bme280;
DS1307 clock;
long prevTime;
long prevTimeFourMinuteMeasure;
long prevTimeHourlyMeasure;
long currTime;
int8_t screenMode = 0;
/*
  0 - overall
  1 - temp avg
  2 - hum avg
  3 - pres avg

  10 - date and time selection
*/
int8_t time[] = {0, 0, 0, 0};

uint8_t selectionStage = 0;
/*
  0 - none
  1 - select year
  2 - select month
  3 - select day
  4 - select hour
  5 - select minute
*/

uint16_t selectedYear = 20;
uint8_t selectedMonth = 0;
uint8_t selectedDay = 0;
int8_t selectedHour = -1;
int8_t selectedMinute = -1;

uint16_t humidityHour[15];
uint16_t humidityDay[24];
uint8_t totalHourMeasures = 0;
uint8_t totalDayMeasures = 0;
int8_t hourMeasuresIndex = -1;
int8_t dayMeasuresIndex = -1;
int8_t prevDayMeasuresIndex = -1;

int16_t temperatureHour[15];
int16_t temperatureDay[24];

uint16_t pressureHour[15];
uint16_t pressureDay[24];

uint8_t screenBrightness = 2;

// uint16_t previousCommand;
// long timeGotSignal;
// bool readyToCommand = true;

float defaultElevation = 118.0;


// bool canReceiveSignal = true;
// long IRCooldown;

/*
  REMOTE COMMANDS
  CH-     69
  CH      70
  CH+     71
  |<<     68
  >>|     64
  >||     67
  -       7
  +       21
  EQ      9
  0       22
  100+    25
  200+    13
  1       12
  2       24
  3       94
  4       8
  5       28
  6       90
  7       66
  8       82
  9       74
*/

const uint8_t RECV_PIN = 8;

void setup()
{
  HC5.begin(9600);
  Serial.begin(9600);
  pinMode(RECV_PIN, INPUT);
  IrReceiver.begin(RECV_PIN, ENABLE_LED_FEEDBACK);
  if(!bme280.init()){
    Serial.println("Device error!");
  }
  currTime = millis();
  clock.begin();
  // clock.fillByYMD(2000,1,1);//Jan 19,2013
  // clock.fillByHMS(0,0,0);//15:28 30"
  // clock.fillDayOfWeek(MON);//Saturday
  // clock.setTime();//write time to the RTC chip
  lcd.begin(16, 2);
  tm1637.set(screenBrightness);
  tm1637.init();
  tm1637.point(POINT_ON);
  randomSeed(analogRead(0));
}

void loop() {

  //Bluetooth communication through UART
  HC5.listen();
  while (HC5.available()) {   // if HC5 sends something then read
    appData = HC5.read();
    Serial.write(appData);
    switch (appData) {
      case 'A':
        sendNow();
        break;
      case 'B':
        sendAvg1H();
        break;
      case 'C':
        sendAvg24H();
        break;
      case 'D':
        if (screenBrightness > 0) {
          screenBrightness--;
          tm1637.set(screenBrightness);
        }
        break;
      case 'E':
        if (screenBrightness < 3) {
          screenBrightness++;
          tm1637.set(screenBrightness);
        }
        break;

      case 'F':
        uint8_t randNumber = random(1, 7);
        char buf[2];
        sprintf(buf, "%d\n", randNumber);        
        HC5.write(buf);
        break;
    }
  }
  
  currTime = millis();

  //Update LCD and 4-digit display
  if ( abs(currTime - prevTime) > 1000) {
    prevTime = currTime;
    clock.getTime();

    TimeDisp[0] = clock.hour / 10;
    TimeDisp[1] = clock.hour % 10;
    TimeDisp[2] = clock.minute / 10;
    TimeDisp[3] = clock.minute % 10;
    tm1637.display(TimeDisp);
    switch (screenMode) {
      case 0:
        displayZero();
        break;
      case 1:
        displayOne();
        break;
      case 2:
        displayTwo();
        break;
      case 3:
        displayThree();
        break;
      case 10:
        displayTen();
        break;
    }
    
  }

  //MEASURING DATA FOR STATS
  if ( abs(currTime - prevTimeFourMinuteMeasure) > 240000) {
    // measure data
    prevTimeFourMinuteMeasure = currTime;
    measureDataHourly();
  }

  if ( abs(currTime - prevTimeHourlyMeasure) > 3600000) {
    //measure data
    prevTimeHourlyMeasure = currTime;
    measureDataDaily();
  }


  //Reading IR Remote commands
  while (IrReceiver.decode()) {

    Serial.println(IrReceiver.decodedIRData.command); // Print "old" raw data
    //lower BRIGHTNESS
    if (IrReceiver.decodedIRData.command == 7) {
      if (screenBrightness > 0) {
        screenBrightness--;
        tm1637.set(screenBrightness);
      }
    }
    //increase BRIGHTNESS
    if (IrReceiver.decodedIRData.command == 21) {
      if (screenBrightness < 3) {
        screenBrightness++;
        tm1637.set(screenBrightness);
      }
    }

    //standby mode - maybe to do later?
    // if (IrReceiver.decodedIRData.command == 67) {
    //   attachInterrupt(RECV_PIN, wakeUp, LOW);
    //   Serial.print("SLEEPY");
    //   delay(50);
    //   LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
    //   screenMode = -1;
    // }


    if (screenMode == 0 || screenMode == 1 || screenMode == 2 || screenMode == 3) {
      switch (IrReceiver.decodedIRData.command) {
        case 9:
          lcd.clear();
          selectionStage = 1;
          screenMode = 10;
          break;
        case 69:
          screenMode--;
          if (screenMode < 0) screenMode = 3;
          break;
        case 70:
          screenMode = 0;
          break;
        case 71:
          screenMode++;
          if(screenMode > 3)  screenMode = 0;    
          break;               
      }
    } else if (screenMode == 10) {
      if (IrReceiver.decodedIRData.command == 9) {
        lcd.clear();
        screenMode = 0;
        selectedYear = 20;
        selectedMonth = 0;
        selectedDay = 0;
        selectedHour = -1;
        selectedMinute = -1;
      }
      if (selectionStage == 1) { 
        switch (IrReceiver.decodedIRData.command) {
          case 12:
            updateYear(1);
            break;
          case 24:
            updateYear(2);
            break;
          case 94:
            updateYear(3);
            break;
          case 8:
            updateYear(4);
            break;
          case 28:
            updateYear(5);
            break;
          case 90:
            updateYear(6);
            break;
          case 66:
            updateYear(7);
            break;
          case 82:
            updateYear(8);
            break;
          case 74:
            updateYear(9);
            break;
          case 22:
            updateYear(0);
            break;            
        }
        if (IrReceiver.decodedIRData.command == 68) {
          selectedYear = ((selectedYear / 10) >= 200 ? (selectedYear / 10) : 20);
        }
        if (IrReceiver.decodedIRData.command == 64) {
          if (selectedYear >= 2000 && selectedYear < 2100) {
            lcd.clear();
            selectionStage++;
          }
        }
      } else if (selectionStage == 2) {
        switch (IrReceiver.decodedIRData.command) {
          case 12:
            updateMonth(1);
            break;
          case 24:
            updateMonth(2);
            break;
          case 94:
            updateMonth(3);
            break;
          case 8:
            updateMonth(4);
            break;
          case 28:
            updateMonth(5);
            break;
          case 90:
            updateMonth(6);
            break;
          case 66:
            updateMonth(7);
            break;
          case 82:
            updateMonth(8);
            break;
          case 74:
            updateMonth(9);
            break;
          case 22:
            updateMonth(0);
            break;            
        }
        if (IrReceiver.decodedIRData.command == 68) {
          selectedMonth /= 10;
        }
        if (IrReceiver.decodedIRData.command == 64) {
          if (selectedMonth > 0 && selectedMonth <= 12) {
            lcd.clear();
            selectionStage++;
          }
        }
      }  else if (selectionStage == 3) {
        switch (IrReceiver.decodedIRData.command) {
          case 12:
            updateDay(1);
            break;
          case 24:
            updateDay(2);
            break;
          case 94:
            updateDay(3);
            break;
          case 8:
            updateDay(4);
            break;
          case 28:
            updateDay(5);
            break;
          case 90:
            updateDay(6);
            break;
          case 66:
            updateDay(7);
            break;
          case 82:
            updateDay(8);
            break;
          case 74:
            updateDay(9);
            break;
          case 22:
            updateDay(0);
            break;            
        }
        if (IrReceiver.decodedIRData.command == 68) {
          selectedDay /= 10;
        }
        if (IrReceiver.decodedIRData.command == 64) {
          if (selectedDay > 0 && selectedDay <= 31) {
            lcd.clear();
            selectionStage++;
          }
        }
      }  else if (selectionStage == 4) {
        switch (IrReceiver.decodedIRData.command) {
          case 12:
            updateHour(1);
            break;
          case 24:
            updateHour(2);
            break;
          case 94:
            updateHour(3);
            break;
          case 8:
            updateHour(4);
            break;
          case 28:
            updateHour(5);
            break;
          case 90:
            updateHour(6);
            break;
          case 66:
            updateHour(7);
            break;
          case 82:
            updateHour(8);
            break;
          case 74:
            updateHour(9);
            break;
          case 22:
            updateHour(0);
            break;            
        }
        if (IrReceiver.decodedIRData.command == 68 && selectedHour >= 0) {
          selectedHour /= 10;
          if (selectedHour == 0) selectedHour = -1;
        }
        if (IrReceiver.decodedIRData.command == 64) {
          if (selectedHour >= 0 && selectedHour <= 23) {
            lcd.clear();
            selectionStage++;
          }
        }
      }  else if (selectionStage == 5) {
        switch (IrReceiver.decodedIRData.command) {
          case 12:
            updateMinute(1);
            break;
          case 24:
            updateMinute(2);
            break;
          case 94:
            updateMinute(3);
            break;
          case 8:
            updateMinute(4);
            break;
          case 28:
            updateMinute(5);
            break;
          case 90:
            updateMinute(6);
            break;
          case 66:
            updateMinute(7);
            break;
          case 82:
            updateMinute(8);
            break;
          case 74:
            updateMinute(9);
            break;
          case 22:
            updateMinute(0);
            break;            
        }
        if (IrReceiver.decodedIRData.command == 68 && selectedMinute >= 0) {
          selectedMinute /= 10;
          if (selectedMinute == 0) selectedMinute = -1;
        }
        if (IrReceiver.decodedIRData.command == 64) {
          if (selectedMinute >= 0 && selectedMinute <= 59) {
            lcd.clear();
            clock.fillByYMD(selectedYear, selectedMonth, selectedDay);
            clock.fillByHMS(((uint8_t) selectedHour), ((uint8_t) selectedMinute), 0);
            clock.fillDayOfWeek(SAT);
            clock.setTime();
            screenMode = 0;
          }
        }
      }
    }
    delay(50);
    IrReceiver.printIRResultShort(&Serial); // Print complete received data in one line
    IrReceiver.printIRSendUsage(&Serial);   // Print the statement required to send this data
    IrReceiver.resume(); // Enable receiving of the next value
  }
}

float getPressureAtSeaLevel(float pressure, float temperature, float elevation) {
  float factor = 1.0 - (0.0065 * elevation)/(temperature + 0.0065 * elevation + 273.15);
  factor = pow(factor, -5.257);
  return (factor * pressure);
}

void displayZero() {
  lcd.setCursor(0, 0);
  if (clock.dayOfMonth < 10) lcd.print("0");
  lcd.print(clock.dayOfMonth);
  lcd.print(".");
  if (clock.month < 10) lcd.print("0");
  lcd.print(clock.month);
  lcd.print(".");
  lcd.print(clock.year+2000);
  lcd.print("  ");
  lcd.print((int) round(bme280.getHumidity()));
  lcd.println(" %");
  lcd.setCursor(0, 1);
  //get and print temperatures
  float temp = bme280.getTemperature();
  lcd.print(temp, 1);
  lcd.print(" C  ");//The unit for  Celsius because original arduino don't support speical symbols
  float pressure = bme280.getPressure() / 100.0;
  //get and print atmospheric pressure data
  lcd.print((int) (round(getPressureAtSeaLevel(pressure, temp, defaultElevation))));      
  lcd.print(" hPa");
}

void displayOne() {
  // lcd.clear();
  lcd.setCursor(0, 0);
  float temp = 0.0;
  lcd.print("AVG 1H: ");
  if (totalHourMeasures == 15) {
    for (int i = 0; i < 15; i++) {
      temp += temperatureHour[i];
    }
    temp /= 150.0;
    lcd.print(temp, 1);
    lcd.print(" C    ");
  } else {
    lcd.print("NO-DATA ");
  }
  temp = 0.0;
  lcd.setCursor(0, 1);
  lcd.print("24H: ");
  if (totalDayMeasures == 24) {
    for (int i = 0; i < 24; i++) {
      temp += temperatureDay[i];
    }
    temp /= 240.0;
    lcd.print(temp, 1);
    lcd.print(" C     ");
  } else {
    lcd.print("NO-DATA      ");
  }
}

void displayTwo() {
  // lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("AVG 1H: ");
  float humidityAvg = 0.0;
  if (totalHourMeasures == 15) {
    for (int i = 0; i < 15; i++) {
      humidityAvg += humidityHour[i];
    }
    humidityAvg /= 150.0;
    lcd.print(humidityAvg, 1);
    lcd.print(" %    ");
  } else {
    lcd.print("NO-DATA ");
  }
  lcd.setCursor(0, 1);
  lcd.print("24H: ");
  humidityAvg = 0.0;
  if (totalDayMeasures == 24) {
    for (int i = 0; i < 24; i++) {
      humidityAvg += (float) humidityDay[i];
    }
    humidityAvg /= 240.0;
    lcd.print(humidityAvg, 1);
    lcd.print(" %     ");
  } else {
    lcd.print("NO-DATA     ");
  }
}

void displayThree() {
  // lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("AVG 1H: ");
  float pressureAvg = 0.0;
  if (totalHourMeasures == 15) {
    for (int i = 0; i < 15; i++) {
      pressureAvg += (float) pressureHour[i];
    }
    pressureAvg /= 150.0;
    lcd.print(pressureAvg, 1);
    lcd.print(" hPa  ");
  } else {
    lcd.print("NO-DATA ");
  }
  lcd.setCursor(0, 1);
  lcd.print("24H: ");
  pressureAvg = 0.0;
    if (totalDayMeasures == 24) {
      for (int i = 0; i < 24; i++) {
        pressureAvg += (float) pressureDay[i];
      }
      pressureAvg /= 240.0;
      lcd.print(pressureAvg, 1);
      lcd.print(" hPa    ");
    } else {
      lcd.print("NO-DATA    ");
    }
}

void displayTen() {
  if (selectionStage == 1) {
    lcd.setCursor(0, 0);
    lcd.print("ENTER YEAR: ");
    if (selectedYear != 0) {
      lcd.print(selectedYear);
      if(selectedYear < 1000) lcd.print("_");
      if(selectedYear < 100) lcd.print("_");
    }
  } else if (selectionStage == 2) {
    lcd.setCursor(0, 0);
    lcd.print("ENTER MONTH: ");
    if (selectedMonth != 0) {
      lcd.print(selectedMonth);
    } else {
      lcd.print("_ ");
    }
    lcd.print("  ");
  } else if (selectionStage == 3) {
    lcd.setCursor(0, 0);
    lcd.print("ENTER DAY: ");
    if (selectedDay != 0) {
      lcd.print(selectedDay);
    } else {
      lcd.print("_ ");
    }
    lcd.print("  ");
  } else if (selectionStage == 4) {
    lcd.setCursor(0, 0);
    lcd.print("ENTER HOUR: ");
    if (selectedHour != -1) {
      lcd.print(selectedHour);
    } else {
      lcd.print("_ ");
    }
    lcd.print("  ");
  } else if (selectionStage == 5) {
    lcd.setCursor(0, 0);
    lcd.print("ENTER MINUTE: ");
    if (selectedMinute != -1) {
      lcd.print(selectedMinute);
    } else {
      lcd.print("_ ");
    }
    lcd.print("  ");
  } 
}

void updateYear(uint8_t digit) {
  if (selectedYear < 1000) {
    selectedYear *= 10;
    selectedYear += digit;
  }
}

void updateMonth(uint8_t digit) {
  if (selectedMonth == 0) {
    selectedMonth += digit;
  } else if (selectedMonth == 1 && (digit == 0 || digit == 1 || digit == 2) ) {
    selectedMonth *= 10;
    selectedMonth += digit;
  }
}

void updateDay(uint8_t digit) {
  if (selectedDay == 0) {
    selectedDay += digit;
  } else if (selectedDay == 1 || (selectedDay == 2 && (selectedMonth != 2 || (digit <= 8 || selectedYear % 4 == 0))) || 
              (selectedDay == 3 && selectedMonth != 2 && (digit == 0 || (digit == 1 && (selectedMonth == 1 || selectedMonth == 3 || selectedMonth == 5 || selectedMonth == 7 || selectedMonth == 8 || selectedMonth == 10 || selectedMonth == 12))))) {
    selectedDay *= 10;
    selectedDay += digit;
  }
}

void updateHour(uint8_t digit) {
  if (selectedHour == -1) {
    selectedHour = digit;
  } else if (selectedHour * 10 + digit < 24) {
    selectedHour *= 10;
    selectedHour += digit;
  }
}

void updateMinute(uint8_t digit) {
  if (selectedMinute == -1) {
    selectedMinute = digit;
  } else if (selectedMinute * 10 + digit < 60) {
    selectedMinute *= 10;
    selectedMinute += digit;
  }
}

void measureDataDaily() {
  dayMeasuresIndex++;
  if (dayMeasuresIndex == 24) dayMeasuresIndex = 0;
  humidityDay[dayMeasuresIndex] = ((int) round(bme280.getHumidity() * 10.0));
  float temp = bme280.getTemperature();
  float pressure = bme280.getPressure() / 100.0;
  pressureDay[dayMeasuresIndex] = (uint16_t) round(getPressureAtSeaLevel(pressure, temp, defaultElevation) * 10.0);
  temperatureDay[dayMeasuresIndex] = ((int16_t) round(temp * 10.0));
  if (totalDayMeasures < 24) totalDayMeasures++;
}

void measureDataHourly() {
  hourMeasuresIndex++;
  if (hourMeasuresIndex == 15) hourMeasuresIndex = 0;
  humidityHour[hourMeasuresIndex] = ((int) round(bme280.getHumidity() * 10.0));
  float temp = bme280.getTemperature();
  float pressure = bme280.getPressure() / 100.0;
  pressureHour[hourMeasuresIndex] = ((uint16_t) round(getPressureAtSeaLevel(pressure, temp, defaultElevation) * 10.0));
  temperatureHour[hourMeasuresIndex] = ((int16_t) round(temp * 10.0));
  if (totalHourMeasures < 15) totalHourMeasures++;
}

void sendNow() {
  char buffer[20];
  float temp = bme280.getTemperature();
  float pressure = bme280.getPressure() / 100.0;
  int humidity = round(bme280.getHumidity());
  int tempInt = temp;
  int tempFrac = (int)(round(temp*10.0))%10;


  sprintf(buffer, "CURRENT DATA:\n");
  HC5.write(buffer);

  sprintf(buffer, "%d %%\n", humidity);
  HC5.write(buffer);

  sprintf(buffer, "%d.%d C\n", tempInt, tempFrac);
  HC5.write(buffer);

  sprintf(buffer, "%d hPa\n", ((int) (round(getPressureAtSeaLevel(pressure, temp, defaultElevation)))));
  HC5.write(buffer);
}

void sendAvg1H() {
  char buffer[20];
  float avg = 0.0;
  sprintf(buffer, "AVG DATA 1H:\n");
  HC5.write(buffer);
  if (totalHourMeasures == 15) {
    for (int i = 0; i < 15; i++) {
      avg += (float) humidityHour[i];
    }
    avg /= 150.0;
    int avgInt = avg;
    int avgFrac = ((int)round(avg*10.0))%10;
    sprintf(buffer, "%d.%d %%\n", avgInt, avgFrac);
    HC5.write(buffer);
    avg = 0.0;
    for (int i = 0; i < 15; i++) {
      avg += (float) temperatureHour[i];
    }
    avg /= 150.0;
    avgInt = avg;
    avgFrac = ((int)round(avg*10.0))%10;
    sprintf(buffer, "%d.%d C\n", avgInt, avgFrac);
    HC5.write(buffer);
    avg = 0.0;
    for (int i = 0; i < 15; i++) {
      avg += (float) pressureHour[i];
    }
    avg /= 150.0;
    avgInt = avg;
    avgFrac = ((int)round(avg*10.0))%10;
    sprintf(buffer, "%d.%d hPa\n", avgInt, avgFrac);
    HC5.write(buffer);
  } else {
    HC5.write("NO-DATA\n");
  }
}

void sendAvg24H() {
  char buffer[20];
  float avg = 0.0;
  sprintf(buffer, "AVG DATA 24H:\n");
  HC5.write(buffer);
  if (totalDayMeasures == 24) {
    for (int i = 0; i < 24; i++) {
      avg += (float) humidityDay[i];
    }
    avg /= 240.0;
    int avgInt = avg;
    int avgFrac = ((int)round(avg*10.0))%10;
    sprintf(buffer, "%d.%d %%\n", avgInt, avgFrac);
    HC5.write(buffer);
    avg = 0.0;
    for (int i = 0; i < 24; i++) {
      avg += (float) temperatureDay[i];
    }
    avg /= 240.0;
    avgInt = avg;
    avgFrac = ((int)round(avg*10.0))%10;
    sprintf(buffer, "%d.%d C\n", avgInt, avgFrac);
    HC5.write(buffer);
    avg = 0.0;
    for (int i = 0; i < 24; i++) {
      avg += (float) pressureDay[i];
    }
    avg /= 240.0;
    avgInt = avg;
    avgFrac = ((int)round(avg*10.0))%10;
    sprintf(buffer, "%d.%d hPa\n", avgInt, avgFrac);
    HC5.write(buffer);
  } else {
    HC5.write("NO-DATA\n");
  }
}