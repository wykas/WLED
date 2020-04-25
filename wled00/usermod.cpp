#include <Arduino.h>
#include "wled.h"
#include "FX.h"


long wipeState = 0; //0: inactive 1: wiping 2: solid
long scanState = 0; //0: inactive 1: scanning 2: solid
unsigned long timeStaticStart = 0;
uint16_t wipeOffDelay = 0;
uint16_t scannerSegment = 0;
long briBW = bri; //brightness before wipe
bool doWipe = false;
bool doScan = false;
bool isScanreverse = false;
bool isWipeReverse = false;
long scanNum = 0;
long scanTotal = 0;
byte scanPrevColMain[4];
uint32_t scanPreviousColor = 0;

uint32_t getRandomColor() {
  return strip.color_wheel(strip.get_random_wheel_index(random8()));
}

bool isScannerFinished()
{
  WS2812FX::segment& seg = strip.getSegment(scannerSegment);
  WS2812FX::segment_runtime segRunt = strip.getSegmentRuntime();
  uint32_t nowUp = millis(); // Be aware, millis() rolls over every 49 days
  uint32_t now = nowUp + strip.timebase;
  uint16_t counter = now * ((seg.speed >> 2) +8);      
  uint16_t index = counter * seg.virtualLength()  >> 16;

  if (index < seg.length() - 1)
  {
    return false;        
  }
  return true;
}

void startWipe()
{
  briBW = bri; //turn on  
  bri=255;  
  transitionDelayTemp = 0; //no transition
  effectCurrent = FX_MODE_COLOR_WIPE;
  resetTimebase(); //make sure wipe starts from beginning

  //set wipe direction
  WS2812FX::Segment& seg = strip.getSegment(0);  
  seg.setOption(1, isWipeReverse);

  colorUpdated(3);
}

void startScanner()
{   
  WS2812FX::Segment& seg = strip.getSegment(scannerSegment);   
  transitionDelayTemp = 3000; //no transition
  if (scannerSegment > 1)
  {
    strip.setMode(scannerSegment, FX_MODE_LARSON_SCANNER);
    scanPreviousColor = seg.colors[0];
    seg.colors[1] = scanPreviousColor;
    seg.colors[0] = getRandomColor();   
  }
  else
  {    
    scanPreviousColor = strip.getColor();
    strip.setColor(1,scanPreviousColor);
    strip.setColor(0,getRandomColor());
    effectCurrent = FX_MODE_LARSON_SCANNER;
    
   }
  
  //seg.setOption(0,true); //select segment
  resetTimebase(); //make sure wipe starts from beginning
    
  seg.setOption(1, isScanreverse);
  scanNum = 0;    
  colorUpdated(3);
}


void stopScan()
{
  WS2812FX::Segment& seg = strip.getSegment(scannerSegment);
      if (scannerSegment > 1)
      {
        seg.colors[0] = scanPreviousColor;
        seg.colors[1] = 0;          
        strip.setMode(scannerSegment, FX_MODE_STATIC);    
      }
      else
      {
        strip.setColor(0,scanPreviousColor);
        strip.setColor(1,0);
        effectCurrent = FX_MODE_STATIC;
      }
      scanNum = 0;
      scanTotal = 0;         
      doScan = false;
      colorUpdated(3);
}

void turnOff()
{
  transitionDelayTemp = 3000; //fade out slowly
  bri = briBW;
  effectCurrent = FX_MODE_STATIC;  
  wipeState = 0;  
  doWipe = false;
  colorUpdated(3);  
}

//gets called once at boot. Do all initialization that doesn't depend on network here
void userSetup()
{
  //setup PIR sensor here, if needed
}

//gets called every time WiFi is (re-)connected. Initialize own network interfaces here
void userConnected()
{

}

void processWipe() {
  if (!doWipe) 
  {
    return;
  }

  if (wipeState == 0) {
    startWipe();
    wipeState = 1;
  } else if (wipeState == 1) { //wiping
    uint32_t cycleTime = 360 + (255 - effectSpeed)*75; //this is how long one wipe takes (minus 25 ms to make sure we switch in time)
    if (millis() + strip.timebase > (cycleTime - 25)) { //wipe complete
      effectCurrent = FX_MODE_STATIC;
      timeStaticStart = millis();
      colorUpdated(3);
      wipeState = 2;
    }
  } else if (wipeState == 2) { //static
    if (wipeOffDelay > 0) //if U1 is not set, the light will stay on until second PIR or external command is triggered
    {
      if (millis() - timeStaticStart > wipeOffDelay*1000) wipeState = 3;
    }
    else
    {
      wipeState = 3;
    }    
  } else if (wipeState == 3) { //switch to wipe off
    turnOff();
  }
}
  
void processScan() {
  if (!doScan)
  {
    return;
  }
  if (scanState == 0) 
  {      
    startScanner();
    scanState = 1;
  } else if (scanState == 1) { //scanner
    if (isScannerFinished())
    {            
      scanState = 3;
    }
  } else if (scanState == 3) {

    if (scanNum < scanTotal)
    {
      scanNum++;
      scanState = 1;
      resetTimebase();
    }
    else
    {
      stopScan();      
    }
  }
}


//loop. You can use "if (WLED_CONNECTED)" to check for successful connection
void userLoop()
{
  if (userVar0 == 1 || userVar0 == 2)
  {
    if (doScan)
    {
      stopScan();    
    }
    doWipe = true;
    wipeState = 0;
    wipeOffDelay = userVar1;  
    isWipeReverse = userVar0 = 2;      
  } 
  else if (userVar0 >= 3 && userVar0 <= 6) 
  {    
    if (!doWipe && !doScan)
    {
      doScan = true;
      scanState = 0;
      scanTotal = userVar0 >= 5 ? 1 : 0;    
      scannerSegment = userVar1;   
      isScanreverse = userVar0 == 4 || userVar0 == 6; 
    }
  }   
  else if (userVar0 == 7)
  {    
    WS2812FX::Segment& seg = strip.getSegment(userVar1);
    transitionDelayTemp = 0;
    seg.colors[0] = getRandomColor();
    userVar0 = 0;
    userVar1 = 0;
    colorUpdated(3);
  }
  else if (userVar0 == 0) 
  {
    //wipeState = 0; //reset for next time
    //scanState = 0;    
  } 
   
  userVar0 = 0;
  userVar1 = 0;    

  processWipe();   
  processScan();
  
}


