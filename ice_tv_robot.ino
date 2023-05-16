// ESP32_LEDMatrix_I2S
//
// Example sketch which shows how to display a 64x64
// animated GIF image stored in FLASH memory
// on a 64x64 LED matrix

// Written for the I2S DMA Matrix Library for the ESP32
// https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA

// To display a GIF from memory, a single callback function
// must be provided - GIFDRAW
// This function is called after each scan line is decoded
// and is passed the 8-bit pixels, RGB565 palette and info
// about how and where to display the line. The palette entries
// can be in little-endian or big-endian order; this is specified
// in the begin() method.
//
// The AnimatedGIF class doesn't allocate or free any memory, but the
// instance data occupies about 22.5K of RAM.
//

// This reference doesn't work on Windows anyways
// #include "../../test_images/homer_tiny.h"

// If it doesn't work:
// - Comment it out
// - Create a new tab (little arrow top right)
// - Call it "homer_tiny.h"
// - copy in this text: https://raw.githubusercontent.com/bitbank2/AnimatedGIF/master/test_images/homer_tiny.h
// - uncomment the following

#include "happy.h"
#include "angry.h"
#include "sad.h"

// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

#include <AnimatedGIF.h>

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
// This is the library for interfacing with the display

// Can be installed from the library manager (Search for "ESP32 MATRIX DMA")
// https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA

// -------------------------------------
// -------   Matrix Config   ------
// -------------------------------------

const int panelResX = 64;  // Number of pixels wide of each INDIVIDUAL panel module.
const int panelResY = 64;  // Number of pixels tall of each INDIVIDUAL panel module.
const int panel_chain = 1; // Total number of panels chained one to another

// See the "displaySetup" method for more display config options

#define R1 25
#define G1 26
#define BL1 27
#define R2 14
#define G2 12
#define BL2 13
#define CH_A 23
#define CH_B 22
#define CH_C 5
#define CH_D 17
#define CH_E 32 // assign to any available pin if using two panels or 64x64 panels with 1/32 scan
#define CLK 16
#define LAT 4
#define OE 15

//------------------------------------------------------------------------------------------------------------------

MatrixPanel_I2S_DMA *dma_display = nullptr;
AnimatedGIF gif;

const int sensorPin = 2;

#include "pitches.h"
#define BUZZZER_PIN 8

int melody[] = {
    NOTE_C4, NOTE_G3, NOTE_G3, NOTE_A3, NOTE_G3, 0, NOTE_B3, NOTE_C4};

int noteDurations[] = {
    4, 8, 8, 4, 4, 4, 4, 4};
int playing = 0;
void tone(byte pin, int freq)
{
  ledcSetup(0, 2000, 8);  // setup beeper
  ledcAttachPin(pin, 0);  // attach beeper
  ledcWriteTone(0, freq); // play tone
  playing = pin;          // store pin
}
void noTone(byte pin)
{
  tone(pin, 0);
}
void playMelody()
{
  tone(BUZZZER_PIN, NOTE_B4);
  delay(1000);
  noTone(BUZZZER_PIN);
  // for (int thisNote = 0; thisNote < 8; thisNote++)
  // {
  //   int noteDuration = 1000 / noteDurations[thisNote];
  //   tone(BUZZZER_PIN, melody[thisNote]);

  //   int pauseBetweenNotes = noteDuration * 1.30;
  //   delay(pauseBetweenNotes);
  //   noTone(BUZZZER_PIN);
  // }
}

// Draw a line of image directly on the LCD
void GIFDraw(GIFDRAW *pDraw)
{
  uint8_t *s;
  uint16_t *d, *usPalette, usTemp[320];
  int x, y, iWidth;

  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y; // current line

  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) // restore to background color
  {
    for (x = 0; x < iWidth; x++)
    {
      if (s[x] == pDraw->ucTransparent)
        s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }
  // Apply the new pixels to the main image
  if (pDraw->ucHasTransparency) // if transparency used
  {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    int x, iCount;
    pEnd = s + pDraw->iWidth;
    x = 0;
    iCount = 0; // count non-transparent pixels
    while (x < pDraw->iWidth)
    {
      c = ucTransparent - 1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd)
      {
        c = *s++;
        if (c == ucTransparent) // done, stop
        {
          s--; // back up to treat it like transparent
        }
        else // opaque
        {
          *d++ = usPalette[c];
          iCount++;
        }
      }           // while looking for opaque pixels
      if (iCount) // any opaque pixels?
      {
        for (int xOffset = 0; xOffset < iCount; xOffset++)
        {
          dma_display->drawPixel(x + xOffset, y, usTemp[xOffset]);
        }
        x += iCount;
        iCount = 0;
      }
      // no, look for a run of transparent pixels
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd)
      {
        c = *s++;
        if (c == ucTransparent)
          iCount++;
        else
          s--;
      }
      if (iCount)
      {
        x += iCount; // skip these
        iCount = 0;
      }
    }
  }
  else
  {
    s = pDraw->pPixels;
    // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
    for (x = 0; x < pDraw->iWidth; x++)
    {
      dma_display->drawPixel(x, y, usPalette[*s++]);
    }
  }
} /* GIFDraw() */

void displaySetup()
{
  HUB75_I2S_CFG mxconfig;
  mxconfig.mx_height = 64;   // we have 64 pix heigh panels
  mxconfig.chain_length = 1; // we have 2 panels chained
  mxconfig.gpio.e = CH_E;    // we MUST assign pin e to some free pin on a board to drive 64 pix height panels with 1/32 scan
  mxconfig.gpio.d = CH_D;
  mxconfig.gpio.c = CH_C;
  mxconfig.gpio.b = CH_B;
  mxconfig.gpio.a = CH_A;
  mxconfig.gpio.lat = LAT;
  mxconfig.gpio.oe = OE;
  mxconfig.gpio.clk = CLK;
  mxconfig.gpio.r1 = R1;
  mxconfig.gpio.g1 = G1;
  mxconfig.gpio.b1 = BL1;
  mxconfig.gpio.r2 = R2;
  mxconfig.gpio.g2 = G2;
  mxconfig.gpio.b2 = BL2;

  // Some matrix panels use different ICs for driving them and some of them have strange quirks.
  // If the display is not working right, try this.
  //  mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
}

//----------------------------------------
const int trigPin = 19;
const int echoPin = 18;
#define SOUND_SPEED 0.034
#define FAR 70
#define CLOSE 10
long duration;
float distanceCm;
int humeur = 2;
int five_range[5] = {2, 2, 2, 2, 2};
int current_range_index = 0;

int which_range()
{
  // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);

  // Calculate the distance
  distanceCm = duration * SOUND_SPEED / 2;

  int range = 2;

  if (distanceCm < CLOSE)
  {
    range = 0;
  }
  else if (distanceCm < FAR)
  {
    range = 1;
  }

  return range;
}

int most_freq_of_array(int arr[])
{
  int freq[3] = {0, 0, 0};
  for (int i = 0; i < 5; i++)
  {
    freq[arr[i]]++;
  }
  int max = 0;
  int max_index = 0;
  for (int i = 0; i < 3; i++)
  {
    if (freq[i] > max)
    {
      max = freq[i];
      max_index = i;
    }
  }
  return max_index;
}

void setup()
{
  Serial.begin(115200);

  displaySetup();
  dma_display->fillScreen(dma_display->color565(0, 0, 0));
  gif.begin(LITTLE_ENDIAN_PIXELS);

  pinMode(sensorPin, INPUT);
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT);  // Sets the echoPin as an Input
}

void loop()
{
  switch (humeur)
  {
  case 0:

    if (gif.open((uint8_t *)angry_smol_short, sizeof(angry_smol_short), GIFDraw))
    {
      while (gif.playFrame(true, NULL))
      {
        if (current_range_index == 5)
        {
          humeur = most_freq_of_array(five_range);
          if (humeur != 0)
          {
            break;
          }
          current_range_index = 0;
        }
        else
        {
          five_range[current_range_index] = which_range();
          current_range_index++;
        }
      }
    }
    gif.close();
  case 1:
    if (gif.open((uint8_t *)happy_smol_short, sizeof(happy_smol_short), GIFDraw))
    {
      while (gif.playFrame(true, NULL))
      {
        if (current_range_index == 5)
        {
          humeur = most_freq_of_array(five_range);
          if (humeur != 1)
          {
            break;
          }
          current_range_index = 0;
        }
        else
        {
          five_range[current_range_index] = which_range();
          current_range_index++;
        }
      }
    }
    gif.close();
  case 2:
    if (gif.open((uint8_t *)sad_smol_long, sizeof(sad_smol_long), GIFDraw))
    {
      while (gif.playFrame(true, NULL))
      {
        if (current_range_index == 5)
        {
          humeur = most_freq_of_array(five_range);
          if (humeur != 2)
          {
            break;
          }
          current_range_index = 0;
        }
        else
        {
          five_range[current_range_index] = which_range();
          current_range_index++;
        }
      }
    }
    gif.close();
  }
}
