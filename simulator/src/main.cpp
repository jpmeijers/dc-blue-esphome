#include <Arduino.h>

#define PIN 32
#define BREAK_DELAY_MS 33
#define BIT_DELAY_US 900
#define FRAME_SPACING_MS 10000

void writeBreak();
void writeBit(bool bit);
void writeEnd();

void setup()
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN, HIGH);
  delay(FRAME_SPACING_MS);
}

void loop()
{
  bool message[] = {1, 
  0, 0, 1, 0, 
  1, 1, 0, 0, 
  0, 0, 1, 0, 
  0, 1, 0, 0, 
  0, 0, 1, 0, 
  0, 1, 0};

  writeBreak();
  for (int i = 0; i < sizeof(message); i++)
  {
    writeBit(message[i]);
  }
  writeEnd();

  delay(FRAME_SPACING_MS);
}

void writeBreak()
{
  digitalWrite(PIN, LOW);
  delay(BREAK_DELAY_MS);
}

void writeEnd()
{
  digitalWrite(PIN, HIGH);
}

void writeBit(bool bit)
{
  if (bit)
  {
    digitalWrite(PIN, HIGH);
  }
  else
  {
    digitalWrite(PIN, LOW);
  }
  delayMicroseconds(BIT_DELAY_US);
}