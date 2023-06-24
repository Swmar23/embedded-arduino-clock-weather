# embedded-arduino-clock-weather
Small indoor weather station project with clock using Arduino Uno.

Main features: 
* measuring atmospheric pressure (reduced to sea level), humidity and temperature,
* collecting data and displaying them (average from 1 hour and 24 hours),
* displaying date and time,
* responsive design using IR remote,
* Bluetooth mode for remote access to the gathered data.

### Components
Components used in project:
* Arduino UNO with [Grove BaseShield v2](https://wiki.seeedstudio.com/Base_Shield_V2/) extension,
* [Grove BME280](https://wiki.seeedstudio.com/Grove-Barometer_Sensor-BME280/#getting-started) humidity, pressure and temperature sensor,
* [Grove DS1307](https://wiki.seeedstudio.com/Grove-RTC/) Real Time Clock,
* [Grove - 16x2 LCD](https://wiki.seeedstudio.com/Grove-16x2_LCD_Series/),
* [Grove - 4-Digit Display](https://wiki.seeedstudio.com/Grove-4-Digit_Display/),
* IR Signal Receiver 1838T,
* Bluetooth Module HC-05.

Following diagram showcases connection between components.
![components diagram](/img/Embedded_Systems_Project.drawio.png)

### Display Modes

Implemented display modes for LCD display:
* current date, temperature, pressure and humidity,
* average temperature, air pressure and humidity from last 1 hour,
* average temperature, air pressure and humidity from last 24 hours,
* setting date and time mode.

### Communication via Bluetooth 

Main controller is communicating with HC-05 module using UART. Bluetooth module is sending to main controller simple requests recieved from external device. Here are message codes used in project:
| Request code | Description                                                            |
|--------------|------------------------------------------------------------------------|
|       A      | Get current temperature, humidity and air pressure.                    |
|       B      | Get average temperature, humidity and air pressure from last 1 hour.   |
|       C      | Get average temperature, humidity and air pressure from last 24 hours. |
|       D      | Lower 4-digit Display brightness.                                      |
|       E      | Increase 4-digit Display brightness.                                   |
|       F      | "Dice request" - get a random integer between 1 and 6.                 |

### What more?
Here are some ideas that could be added to this project:

* sleep mode or standby mode (gather data in background with less energy consumption),
* alarm clock - adding a buzzer and a mode to set alarm clock using f.ex. IR remote,
* storing gathered data on some external drive (maybe SD card?)