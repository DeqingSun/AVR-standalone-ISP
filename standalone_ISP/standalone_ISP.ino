// Standalone AVR ISP programmer
// Feb 2015 by Deqing Sun
// August 2011 by Limor Fried / Ladyada / Adafruit
// Jan 2011 by Bill Westfield ("WestfW")
//
// this sketch allows an Arduino to program a flash program
// into any AVR if you can fit the HEX file into program memory
// No computer is necessary. Two LEDs for status notification
// Press button to program a new chip. Piezo beeper for error/success 
// This is ideal for very fast mass-programming of chips!
//
// It is based on AVRISP
//
// using the following pins:
// 10: slave reset
// 11: MOSI
// 12: MISO
// 13: SCK
//  9: 8 MHz clock output - connect this to the XTAL1 pin of the AVR
//     if you want to program a chip that requires a crystal without
//     soldering a crystal in
// ----------------------------------------------------------------------


#include "optiLoader.h"
#include "SPI.h"

// Global Variables
int pmode=0;
byte pageBuffer[128]; 		       /* One page of flash */


/*
 * Pins to target
 */
#define SCK 13
#define MISO 12
#define MOSI 11
#define RESET 10
#define CLOCK 9     // self-generate 8mhz clock - handy!

#define BUTTON A1
#define PIEZOPIN A3

#define LED_ERR 8
#define LED_PROGMODE A0

void setup () {
  Serial.begin(115200); 			/* Initialize serial for status msgs */
  Serial.println(F("\nAdaBootLoader Bootstrap programmer (originally OptiLoader Bill Westfield (WestfW))"));

  pinMode(PIEZOPIN, OUTPUT);

  pinMode(LED_PROGMODE, OUTPUT);
  pulse(LED_PROGMODE,2);
  pinMode(LED_ERR, OUTPUT);
  pulse(LED_ERR, 2);

  pinMode(BUTTON, INPUT);     // button for next programming
  digitalWrite(BUTTON, HIGH); // pullup

  pinMode(CLOCK, OUTPUT);
  // setup high freq PWM on pin 9 (timer 1)
  // 50% duty cycle -> 8 MHz
  OCR1A = 0;
  ICR1 = 1;
  // OC1A output, fast PWM
  TCCR1A = _BV(WGM11) | _BV(COM1A1);
  TCCR1B = _BV(WGM13) | _BV(WGM12) | _BV(CS10); // no clock prescale
}


void loop (void) {
  Serial.println(F("\nType 'G' or hit BUTTON for next chip"));
  while (1) {
    if  ((! digitalRead(BUTTON)) || (Serial.read() == 'G'))
      break;  
  }

  do{
    target_poweron(); 			/* Turn on target power */

    uint16_t signature;
    image_t *targetimage;

    if (! (signature = readSignature())){		// Figure out what kind of CPU
      error_no_fatal(F("Signature fail"));
      break;
    }
    if (! (targetimage = findImage(signature))){	// look for an image
      error_no_fatal(F("Image fail"));
      break;
    }

    eraseChip();

    if (! programFuses(targetimage->image_progfuses)){	// get fuses ready to program
      error_no_fatal(F("Programming Fuses fail"));
      break;
    }

    if (! verifyFuses(targetimage->image_progfuses, targetimage->fusemask) ) {
      error_no_fatal(F("Failed to verify fuses"));
      break;
    }

    end_pmode();
    start_pmode();    

    if (0&targetimage->image_calibration!=NULL){
      Serial.println(F("\nStart calibration, write calibration firmware"));
      byte *hextext = (byte *)pgm_read_word(&targetimage->image_calibration);  
      uint16_t pageaddr = 0;
      uint8_t pagesize = pgm_read_byte(&targetimage->image_pagesize);
      uint16_t chipsize = pgm_read_word(&targetimage->chipsize);

      while (pageaddr < chipsize) {
        byte *hextextpos = readImagePage (hextext, pageaddr, pagesize, pageBuffer);

        boolean blankpage = true;
        for (uint8_t i=0; i<pagesize; i++) {
          if (pageBuffer[i] != 0xFF) blankpage = false;
        }          
        if (! blankpage) {
          if (! flashPage(pageBuffer, pageaddr, pagesize)){	
            error_no_fatal(F("Flash programming failed"));
            break;
          }
        }
        hextext = hextextpos;
        pageaddr += pagesize;

        if (hextextpos == NULL) break;
      }

      break;



      end_pmode();
      start_pmode();    

    }
    else{
      Serial.println(F("\nNo need to calibrate"));
    }


    byte *hextext = (byte *)pgm_read_word(&targetimage->image_final);  
    uint16_t pageaddr = 0;
    uint8_t pagesize = pgm_read_byte(&targetimage->image_pagesize);
    uint16_t chipsize = pgm_read_word(&targetimage->chipsize);

    //Serial.println(chipsize, DEC);
    while (pageaddr < chipsize) {
      byte *hextextpos = readImagePage (hextext, pageaddr, pagesize, pageBuffer);

      boolean blankpage = true;
      for (uint8_t i=0; i<pagesize; i++) {
        if (pageBuffer[i] != 0xFF) blankpage = false;
      }          
      if (! blankpage) {
        if (! flashPage(pageBuffer, pageaddr, pagesize)){	
          error_no_fatal(F("Flash programming failed"));
          break;
        }
      }
      hextext = hextextpos;
      pageaddr += pagesize;

      if (hextextpos == NULL) break;
    }

    // Set fuses to 'final' state
    if (! programFuses(targetimage->image_normfuses)){
      error_no_fatal(F("Programming Fuses fail"));
      break;
    }

    end_pmode();
    start_pmode();

    Serial.println("\nVerifing flash...");
    if (! verifyImage((byte *)pgm_read_word(&targetimage->image_final)) ) {
      error_no_fatal(F("Failed to verify chip"));
      break;
    } 
    else {
      Serial.println(F("\tFlash verified correctly!"));
    }

    if (! verifyFuses(targetimage->image_normfuses, targetimage->fusemask) ) {
      error_no_fatal(F("Failed to verify fuses"));
      break;
    } 
    else {
      Serial.println(F("Fuses verified correctly!"));
    }

  }
  while (false);
  target_poweroff(); 			/* turn power off */
  tone(PIEZOPIN, 4000, 200);
}


