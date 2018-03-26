#include <TimeLib.h>
#include <Time.h>
#include <Button.h>
#include <EEPROM.h>
#include <OneWire.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>


// OLED 
const int rs = 13, rw = 12, en = 11, d4 = 7, d5 = 6, d6 = 5, d7 = 4;
LiquidCrystal_I2C lcd(0x20);

// one wire
OneWire  ds(10);	// on pin 10 (a 4.7K resistor is necessary)

// EEPROM
const byte Fahrenheit = 0, Celcius = 1;
int address = 0;	// start reading from the first byte (address 0) of the EEPROM
byte tMode;

// Buttons
const int tModeBtnPin = 2, stopWatchBtnPin = 3;   				
const bool pullUp = true, invert = true;
const int debounce = 20;

Button tModeBtn(tModeBtnPin, pullUp, invert, debounce);
Button stopWatchBtn(stopWatchBtnPin, pullUp, invert, debounce);

// Timer function
const byte running = 0, stopped = 1, reset = 2;
byte timerState = reset;
long lastWriteTime;

void setup()
{
	Serial.begin(9600);
	lcd.begin(20, 4);
	lcd.home();

	// read a byte from the current address of the EEPROM
	tMode = EEPROM.read(address);

	// attach interupts for reading the buttons immediately
	attachInterrupt(digitalPinToInterrupt(tModeBtnPin), SetTModeState, CHANGE);
	attachInterrupt(digitalPinToInterrupt(stopWatchBtnPin), SetTimerState, CHANGE);		
	
	// (re)set the time give the lastWrite Time a valid value
	setTime(0);
	lastWriteTime = now();
	
	PrintTime(lastWriteTime);

}

void loop()
{
	// update the timer if needed
	UpdateTimer();

	// update the temperature
	UpdateTemperature();
}

void SetTModeState()
{
	tModeBtn.read();      
	if (tModeBtn.wasPressed())
	{
		switch (tMode)
		{
		case Fahrenheit:
			Serial.println("Faherheit");
			EEPROM.write(address, Celcius);
			tMode = Celcius;
			break;
		case Celcius:
			Serial.println("Celcius");
			EEPROM.write(address, Fahrenheit);
			tMode = Fahrenheit;
			break;
		default:
			Serial.println("Default");
			EEPROM.write(address, Celcius);
			tMode = Celcius;
			break;
		}
	}	
}

void SetTimerState() 
{
	stopWatchBtn.read();
	if (stopWatchBtn.wasPressed())
	{
		switch (timerState)
		{
		case running:
			Serial.println("running");
			timerState = stopped;
			break;
		case stopped:
			Serial.println("stopped");
			timerState = reset;
			break;
		case reset:
			Serial.println("reset");
			timerState = running;
			lastWriteTime = -1;
			break;
		default:
			Serial.println("Default");
			timerState = running;
			break;
		}
	}
}


void UpdateTimer()
{
	if (timerState == running && lastWriteTime == -1)
	{
		setTime(0);
	}
	// update only once per second
	long time = now();
	if (timerState == running && (time - lastWriteTime >= 1))
	{		
		lastWriteTime = time;
		PrintTime(time);
	}
}

void UpdateTemperature()
{
	byte i;
	byte present = 0;
	byte type_s;
	byte data[12];
	byte addr[8];
	float celsius, fahrenheit;

	if (!ds.search(addr)) 
	{
		ds.reset_search();
		delay(250);
		return;
	}

	if (OneWire::crc8(addr, 7) != addr[7]) 
	{
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("CRC is not valid");
		delay(2500);
		return;
	}

	// the first ROM byte indicates which chip
	switch (addr[0]) 
	{
	case 0x10:	// Chip = DS18S20" or old DS1820
		type_s = 1;
		break;
	case 0x28:	// Chip = DS18B20
		type_s = 0;
		break;
	case 0x22:	// Chip = DS1822
		type_s = 0;
		break;
	default:
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.print("Device is not a");
		lcd.setCursor(0, 1);
		lcd.print("DS18x20 family");
		lcd.setCursor(0, 2);
		lcd.print("device!");
		delay(2500);
		return;
	}

	ds.reset();
	ds.select(addr);
	ds.write(0x44, 1);        // start conversion, with parasite power on at the end

	delay(1000);     // maybe 750ms is enough, maybe not
	// we might do a ds.depower() here, but the reset will take care of it.

	present = ds.reset();
	ds.select(addr);
	ds.write(0xBE);         // Read Scratchpad

	for (i = 0; i < 9; i++) 
	{           // we need 9 bytes
		data[i] = ds.read();
	}

	// Convert the data to actual temperature
	// because the result is a 16 bit signed integer, it should
	// be stored to an "int16_t" type, which is always 16 bits
	// even when compiled on a 32 bit processor.
	int16_t raw = (data[1] << 8) | data[0];
	if (type_s) 
	{
		raw = raw << 3; // 9 bit resolution default
		if (data[7] == 0x10) 
		{
			// "count remain" gives full 12 bit resolution
			raw = (raw & 0xFFF0) + 12 - data[6];
		}
	}
	else 
	{
		byte cfg = (data[4] & 0x60);
		// at lower res, the low bits are undefined, so let's zero them
		if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
		else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
		else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
		//// default is 12 bit resolution, 750 ms conversion time
	}
	celsius = (float)raw / 16.0;
	fahrenheit = celsius * 1.8 + 32.0;	

	int c;
	lcd.setCursor(0, 0);
	switch (tMode) 
	{
	case Fahrenheit:	
		
		if (celsius > 10)
			c = 15;
		else
			c = 14;
		lcd.print(fahrenheit);
		lcd.print(" Fahrenheit");
		
		//Serial.print(fahrenheit);
		//Serial.print(" Fahrenheit");
		break;
	case Celcius:
		if (celsius > 10)
			c = 12;
		else
			c = 11;
		lcd.print(celsius);
		lcd.print(" Celsius");

		//Serial.print(celsius);
		//Serial.print(" Celsius");
		break;
	default:
		SetTModeState();
		break;
	}

	// print blanks
	for (size_t i = c; i < 20; i++)
	{
		lcd.print(" ");
	}
}

void PrintTime(long val)
{
	int days = elapsedDays(val);
	int hours = numberOfHours(val);
	int minutes = numberOfMinutes(val);
	int seconds = numberOfSeconds(val);

	// digital clock display of current time
	lcd.setCursor(3, 4);
	lcd.print(days, DEC);
	printDigits(hours);
	printDigits(minutes);
	printDigits(seconds);
}

void printDigits(byte digits)
{
	// utility function for digital clock display: prints colon and leading 0
	lcd.print(":");
	if (digits < 10)
		lcd.print('0');
	lcd.print(digits, DEC);
}
