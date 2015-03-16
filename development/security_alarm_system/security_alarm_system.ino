/*
 *  Copyright (C) 2015  Efstathios Chatzikyriakidis (contact@efxa.org)
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <FiniteStateMachine.h>
#include <ShiftRegLCD123.h>
#include <SoftwareSerial.h>
#include <SimpleGsm.h>
#include <LinkedList.h>
#include <SPI.h>
#include <SD.h>

const byte DEFAULT_SLAVE_SELECT_PIN = 10;
const byte SD_SHIELD_CHIP_SELECT_PIN = 8;
const byte SHIFT_REGISTER_CLOCK_PIN = 9;
const byte SHIFT_REGISTER_DATA_PIN = 6;
const byte GSM_RECEIVER_PIN = 2;
const byte GSM_TRANSMITTER_PIN = 3;
const byte GSM_POWER_PIN = 7;
const byte DOOR_SENSOR_PIN = A3;
const byte PIR_SENSOR_PIN = A1;
const byte SIREN_PIN = A0;

const unsigned long DELAY_TIME_OF_PIR_SENSOR_CALIBRATION = 40000; // 40 seconds
const unsigned long DELAY_TIME_OF_CALL_RINGING_DURATION = 10000; // 10 seconds
const unsigned long DELAY_TIME_BEFORE_ENABLING_ALARM = 120000; // 2 minutes
const unsigned long DELAY_TIME_OF_CRITICAL_SECTION = 15000; // 15 seconds
const unsigned long DELAY_TIME_OF_RINGING_SIREN = 900000; // 15 minutes
const unsigned long DELAY_TIME_OF_LCD_MESSAGE = 2000; // 2 seconds

String smsText, encryptionKey;

State &
getDisabledState ()
{
  static State * object = NULL;

  if (object == NULL)
  {
    object = new State (disabledEnterOperation, disabledUpdateOperation, disabledExitOperation);
  }

  return *object;
}

State &
getEnabledState ()
{
  static State * object = NULL;

  if (object == NULL)
  {
    object = new State (enabledEnterOperation, enabledUpdateOperation, NULL);
  }

  return *object;
}

State &
getPossibleThreatState ()
{
  static State * object = NULL;

  if (object == NULL)
  {
    object = new State (possibleThreatEnterOperation, possibleThreatUpdateOperation, NULL);
  }

  return *object;
}

State &
getRealThreatState ()
{
  static State * object = NULL;

  if (object == NULL)
  {
    object = new State (realThreatUpdateOperation);
  }

  return *object;
}

FSM &
getFsm ()
{
  static FSM * object = NULL;

  if (object == NULL)
  {
    object = new FSM (getDisabledState());
  }

  return *object;
}

ShiftRegLCD123 &
getLcd ()
{
  static ShiftRegLCD123 * object = NULL;

  if (object == NULL)
  {
    object = new ShiftRegLCD123 (SHIFT_REGISTER_DATA_PIN, SHIFT_REGISTER_CLOCK_PIN, SRLCD123);
  }

  return *object;
}

SimpleGsm &
getGsm ()
{
  static SimpleGsm * object = NULL;

  if (object == NULL)
  {
    object = new SimpleGsm (GSM_RECEIVER_PIN, GSM_TRANSMITTER_PIN, GSM_POWER_PIN);
  }

  return *object;
}

LinkedList <String> &
getRfids ()
{
  static LinkedList <String> * object = NULL;

  if (object == NULL)
  {
    object = new LinkedList <String> ();
  }

  return *object;
}

LinkedList <String> &
getMobiles ()
{
  static LinkedList <String> * object = NULL;

  if (object == NULL)
  {
    object = new LinkedList <String> ();
  }

  return *object;
}

void
disabledEnterOperation ()
{
  printStringWithoutDelay(F("Alarm disabled"));

  flushRfid();
}

void
disabledUpdateOperation ()
{
  if (isAuthenticated ())
  {
    getFsm().transitionTo(getEnabledState());
  }
}

void
disabledExitOperation ()
{
  printStringWithoutDelay(F("Enabling alarm"));

  delay (DELAY_TIME_BEFORE_ENABLING_ALARM);
}

void
enabledEnterOperation ()
{
  printStringWithoutDelay(F("Alarm enabled"));

  flushRfid();
}

void
enabledUpdateOperation ()
{
  if (pirSensorSensesMotion() || isDoorOpen ())
  {
    getFsm().transitionTo(getPossibleThreatState());
  }

  if (isAuthenticated ())
  {
    getFsm().transitionTo(getDisabledState());
  }
}

void
possibleThreatEnterOperation ()
{
  printStringWithoutDelay(F("Possible threat"));
}

void
possibleThreatUpdateOperation ()
{
  if (authenticatedOnDelay(DELAY_TIME_OF_CRITICAL_SECTION))
  {
    getFsm().transitionTo(getDisabledState());
  }
  else
  {
    getFsm().transitionTo(getRealThreatState());
  }
}

void
realThreatUpdateOperation ()
{
  sirenOn();

  printStringWithoutDelay(F("Notifying Users"));

  notifyUsers();

  printStringWithoutDelay(F("Waiting police"));

  authenticatedOnDelay(DELAY_TIME_OF_RINGING_SIREN);

  sirenOff();

  getFsm().transitionTo(getDisabledState());
}

bool
initializeSdCard ()
{
  pinMode (DEFAULT_SLAVE_SELECT_PIN, OUTPUT);

  pinMode (SD_SHIELD_CHIP_SELECT_PIN, OUTPUT);

  return SD.begin(SD_SHIELD_CHIP_SELECT_PIN);
}

void
initializePirSensor ()
{
  pinMode (PIR_SENSOR_PIN, INPUT);

  delay (DELAY_TIME_OF_PIR_SENSOR_CALIBRATION);
}

void
initializeDoorSensor ()
{
  pinMode (DOOR_SENSOR_PIN, INPUT_PULLUP);
}

void
initializeLcd ()
{
  const byte numberOfColumns = 16;

  const byte numberOfRows = 2;

  getLcd().begin(numberOfColumns, numberOfRows);

  getLcd().backlightOff();

  getLcd().clear();
}

void
initializeRfid()
{
  const long baudRate = 9600;

  Serial.begin (baudRate);

  flushRfid();
}

bool
initializeGsm()
{
  const long baudRate = 9600;

  const byte numberOfRetries = 10;

  if (!getGsm().begin(baudRate, numberOfRetries))
  {
    return false;
  }

  if (!getGsm().disableEcho())
  {
    return false;
  }

  if (!getGsm().setSmsTextMode())
  {
    return false;
  }

  return true;
}

void
initializeSiren ()
{
  pinMode (SIREN_PIN, OUTPUT);

  sirenOff ();
}

void
sirenOn ()
{
  digitalWrite(SIREN_PIN, HIGH);
}

void
sirenOff ()
{
  digitalWrite(SIREN_PIN, LOW);
}

bool
isDoorOpen ()
{
  return digitalRead(DOOR_SENSOR_PIN) == HIGH;
}

bool
pirSensorSensesMotion ()
{
  return digitalRead(PIR_SENSOR_PIN) == HIGH;
}

void
systemError (const __FlashStringHelper * string)
{
  printStringWithoutDelay(string);

  while (true) ;
}

void
printStringWithoutDelay (const __FlashStringHelper * string)
{
  getLcd().clear();

  getLcd().print(string);
}

void
printString (const __FlashStringHelper * string)
{
  printStringWithoutDelay(string);

  delay(DELAY_TIME_OF_LCD_MESSAGE);
}

void
flushRfid()
{
  while (Serial.available ())
  {
    Serial.read ();
  }
}

bool
authenticatedOnDelay (const unsigned long milliseconds)
{
  flushRfid();

  const unsigned long previousMillis = millis();

  while ((unsigned long) (millis() - previousMillis) < milliseconds)
  {
    if (isAuthenticated ())
    {
      return true;
    }
  }

  return false;
}

bool
isAuthenticated ()
{
  String rfidCode;

  return (rfidTagHandled(rfidCode) && rfidCodeExists(rfidCode));
}

bool
rfidTagHandled (String & rfidCode)
{
  const byte rfidCodeSize = 10;

  byte code[6], value, checkSum, bytesRead, tempByte;

  value = checkSum = bytesRead = tempByte = 0;

  char codeRead[rfidCodeSize + 1] = { '\0' };

  if (Serial.available () > 0)
  {
    if (0x02 == (value = Serial.read ()))
    {
      while (bytesRead < (rfidCodeSize + 2))
      {
        if (Serial.available () > 0)
        {
          value = Serial.read ();

          if ((0x0D == value) || (0x0A == value) || (0x03 == value) || (0x02 == value))
          {
            break;
          }

          if (bytesRead < rfidCodeSize)
          {
            codeRead[bytesRead] = value;
          }

          if ((value >= '0') && (value <= '9'))
          {
            value = value - '0';
          }
          else if ((value >= 'A') && (value <= 'F'))
          {
            value = 10 + value - 'A';
          }

          if (bytesRead & 1 == 1)
          {
            code[bytesRead >> 1] = (value | (tempByte << 4));

            if (bytesRead >> 1 != 5)
            {
              checkSum ^= code[bytesRead >> 1];
            }
          }
          else
          {
            tempByte = value;
          }

          bytesRead++;
        }
      }

      if (bytesRead == (rfidCodeSize + 2))
      {
        if (code[5] == checkSum)
        {
          rfidCode = String(codeRead);

          return true;
        }
      }
    }
  }

  return false;
}

bool
readRfids()
{
  getRfids().clear();

  File file = SD.open("rfids.txt");

  if (file)
  {
    while (file.available())
    {
      getRfids().add(file.readStringUntil('\n'));
    }

    file.close();

    return true;
  }

  return false;
}

bool
readSmsText()
{
  File file = SD.open("message.txt");

  if (file)
  {
    if (file.available())
    {
      smsText = file.readStringUntil('\n');
    }

    file.close();

    return true;
  }

  return false;
}

bool
readMobiles()
{
  getMobiles().clear();

  File file = SD.open("mobiles.txt");

  if (file)
  {
    while (file.available())
    {
      getMobiles().add(file.readStringUntil('\n'));
    }

    file.close();

    return true;
  }

  return false;
}

bool
rfidCodeExists (const String & rfidCode)
{
  const String cipherText = xorEncryption (encryptionKey, rfidCode);

  const byte numberOfRfids = getRfids().size();

  for (byte i = 0; i < numberOfRfids; i++)
  {
    const String rfid = getRfids().get(i);

    if (cipherText == rfid)
    {
      return true;
    }
  }

  return false;
}

void
notifyUsers ()
{
  const byte notificationsPerUser = 3;

  const byte numberOfMobiles = getMobiles().size();

  for (byte i = 0; i < notificationsPerUser; i++)
  {
    for (byte j = 0; j < numberOfMobiles; j++)
    {
      const String mobile = getMobiles().get(j);

      getGsm().missedCall(mobile, DELAY_TIME_OF_CALL_RINGING_DURATION);

      getGsm().sendSms(mobile, smsText);
    }
  }
}

void
readEncryptionKey ()
{
  while (!rfidTagHandled(encryptionKey)) ;
}

String
convertStringToHexadecimal (const char string[], const int length)
{
  const char symbols[] = "0123456789ABCDEF";

  String result;

  for (int i = 0; i < length; i++)
  {
    result += symbols[string[i] >> 4];

    result += symbols[string[i] & 0x0F];
  }

  return result;
}

String
xorEncryption (const String key, const String data)
{
  const int dataLength = data.length();

  const int bufferLength = dataLength + 1;

  char cipherText[bufferLength];

  data.toCharArray(cipherText, bufferLength);

  for (int i = 0; i < dataLength; i++)
  {
    cipherText[i] = cipherText[i] ^ key[i];
  }

  return convertStringToHexadecimal (cipherText, dataLength);
}

void
setup ()
{
  initializeSiren();

  initializeLcd();

  printString(F("Init door sensor"));

  initializeDoorSensor();

  printString(F("Init PIR sensor"));

  initializePirSensor();

  printString(F("Init GSM"));

  if (!initializeGsm())
  {
    systemError (F("GSM failed"));
  }

  printString(F("GSM OK"));

  printString(F("Init SD card"));

  if (!initializeSdCard())
  {
    systemError (F("SD card failed"));
  }

  printString(F("SD card OK"));

  printString(F("Read RFIDs"));

  if (!readRfids())
  {
    systemError(F("RFIDs failed"));
  }

  printString(F("RFIDs OK"));

  printString(F("Read mobiles"));

  if (!readMobiles())
  {
    systemError(F("Mobiles failed"));
  }

  printString(F("Mobiles OK"));

  printString(F("Read SMS text"));

  if (!readSmsText())
  {
    systemError(F("SMS text failed"));
  }

  printString(F("SMS text OK"));

  printString(F("Init RFID"));

  initializeRfid();

  printString(F("Read key"));

  readEncryptionKey();
}

void
loop ()
{
  getFsm().update();
}
