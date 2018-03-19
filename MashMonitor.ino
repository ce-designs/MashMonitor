#include <EEPROM.h>
#include <LiquidCrystal.h>
#include <OneWire.h>


// liquid crystal
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// one wire
OneWire  ds(10);	// on pin 10 (a 4.7K resistor is necessary)

// EEPROM
const byte Fahrenheit = 0, Celcius = 1;
int address = 0;	// start reading from the first byte (address 0) of the EEPROM
byte tMode;

void setup()
{
	//Serial.begin(9600);
	lcd.begin(16, 2);

	// read a byte from the current address of the EEPROM
	tMode = EEPROM.read(address);
}

void loop()
{
	byte i;
	byte present = 0;
	byte type_s;
	byte data[12];
	byte addr[8];
	float celsius, fahrenheit;	

	if (!ds.search(addr)) {
		ds.reset_search();
		delay(250);
		return;
	}	

	if (OneWire::crc8(addr, 7) != addr[7]) {
		lcd.clear();
		lcd.setCursor(0, 0);
		lcd.println("CRC is not valid");
		delay(2500);
		return;
	}
	
	// the first ROM byte indicates which chip
	switch (addr[0]) {
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
		lcd.println("Not a DS18x20");
		lcd.setCursor(0, 1);
		lcd.println("family device!");
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

	for (i = 0; i < 9; i++) {           // we need 9 bytes
		data[i] = ds.read();
	}

	// Convert the data to actual temperature
	// because the result is a 16 bit signed integer, it should
	// be stored to an "int16_t" type, which is always 16 bits
	// even when compiled on a 32 bit processor.
	int16_t raw = (data[1] << 8) | data[0];
	if (type_s) {
		raw = raw << 3; // 9 bit resolution default
		if (data[7] == 0x10) {
			// "count remain" gives full 12 bit resolution
			raw = (raw & 0xFFF0) + 12 - data[6];
		}
	}
	else {
		byte cfg = (data[4] & 0x60);
		// at lower res, the low bits are undefined, so let's zero them
		if (cfg == 0x00) raw = raw & ~7;  // 9 bit resolution, 93.75 ms
		else if (cfg == 0x20) raw = raw & ~3; // 10 bit res, 187.5 ms
		else if (cfg == 0x40) raw = raw & ~1; // 11 bit res, 375 ms
		//// default is 12 bit resolution, 750 ms conversion time
	}
	celsius = (float)raw / 16.0;
	fahrenheit = celsius * 1.8 + 32.0;
	
	lcd.clear();
	lcd.setCursor(0, 0);
	switch (tMode) {
	case Fahrenheit:
		lcd.println(fahrenheit);
		lcd.println(" Fahrenheit");
		//Serial.print(fahrenheit);
		//Serial.println(" Fahrenheit");
		break;
	case Celcius:
		lcd.println(celsius);
		lcd.println(" Celsius");
		//Serial.print(celsius);
		//Serial.println(" Celsius");
		break;
	default:
		SetTModeDefault();
		break;
	}	
}

void SetTModeDefault()
{
	switch (tMode) {
	case Fahrenheit:
		EEPROM.write(address, Celcius);
		tMode = Celcius;
		break;
	case Celcius:
		EEPROM.write(address, Fahrenheit);
		tMode = Fahrenheit;
		break;
	default:
		EEPROM.write(address, Celcius);
		tMode = Celcius;
		break;
	}	
}
