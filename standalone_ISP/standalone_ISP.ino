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

#define DEBUG_PIN 2

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

  pinMode(DEBUG_PIN,OUTPUT);
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
    unsigned char osscal_value=0xFF;

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

    if (targetimage->image_calibration!=NULL){
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

      end_pmode();
      start_pmode();    

      Serial.println("\nVerifing flash...");
      if (! verifyImage((byte *)pgm_read_word(&targetimage->image_calibration)) ) {
        error_no_fatal(F("Failed to verify chip"));
        break;
      } 
      else {
        Serial.println(F("\tFlash verified correctly!"));
      }

      end_pmode();

      // do calibation here
      pinMode(RESET, OUTPUT);
      digitalWrite(RESET, LOW);  //hold RST
      delay(50);
      pinMode(MISO, INPUT);
      pinMode(MOSI, INPUT); 
      digitalWrite(MOSI, HIGH);  //pull UP mosi
      digitalWrite(MISO, HIGH);  //pull UP miso, suppress noise
      digitalWrite(SCK, HIGH);  //pull UP sck, suppress noise
      //let MOSI act as OC2A, 16E6/(2*244)=32787
      TCCR2A=(0b01<<COM2A0)|(0b10<<WGM20); //TOGGLE MOSI on CTC      
      TCCR2B=(0b001<<CS20);
      OCR2A=243;
      pinMode(MOSI, OUTPUT);
      delay(50);
      pinMode(RESET, INPUT);  //release RST

        unsigned char edge_count=0;
      {  //waiting for calibration response
        unsigned long millis_begin=millis();
        unsigned long millis_phase=millis_begin;
        boolean cali_finished=false;
        boolean cali_last=HIGH;
        while (!cali_finished){
          unsigned long millis_now=millis();
          if ((millis_now-millis_begin)>600){
            cali_finished=true;
          }
          else{
            boolean cali_input=digitalRead(MISO);
            if (cali_input!=cali_last){
              edge_count++;
              cali_last=cali_input;
              if (edge_count>=8){
                cali_finished=true;
              }
            }
          }
        }
        Serial.print('\n');
        Serial.print(edge_count);
        Serial.println(F(" edge received."));
      }
      TCCR2A=0;
      TCCR2B=0;
      pinMode(MOSI, INPUT);
      digitalWrite(MOSI, LOW);  
      digitalWrite(MISO, LOW);  
      digitalWrite(SCK, LOW);  
      // calibration over

      if (edge_count>=8){
        Serial.println(F("\tCalibrated successfully!"));
      }
      else{
        error_no_fatal(F("Failed to calibrate chip"));
        break;
      }

      Serial.println(F("\nReadback OSCCAL from EEPROM"));

      start_pmode();    
      osscal_value=readByteEEPROM(pgm_read_byte(&targetimage->osccal_eeprom_pos));
      end_pmode();
      if (osscal_value!=0xFF){
        Serial.print(F("\nOsccal is 0x"));
        Serial.print(osscal_value,HEX);
        Serial.println(F(" from EEPROM."));
      }
      else{
        error_no_fatal(F("Calibrate failed, got 0xFF"));
        break;
      }
      Serial.println(F("\nErase flash again."));
      start_pmode();  
      eraseChip();  
    }
    else{
      Serial.println(F("\nNo need to calibrate"));
    }

    byte *hextext = (byte *)pgm_read_word(&targetimage->image_final);  
    uint16_t pageaddr = 0;
    uint8_t pagesize = pgm_read_byte(&targetimage->image_pagesize);
    uint16_t chipsize = pgm_read_word(&targetimage->chipsize);

    uint16_t calibration_positon = 0xFFFF;  //
    if (osscal_value!=0xff){
      calibration_positon = pgm_read_word(&targetimage->osccal_flash_pos); 
    }

    //Serial.println(chipsize, DEC);
    while (pageaddr < chipsize) {
      byte *hextextpos = readImagePage (hextext, pageaddr, pagesize, pageBuffer);

      boolean blankpage = true;
      for (uint8_t i=0; i<pagesize; i++) {
        if (pageBuffer[i] != 0xFF) blankpage = false;
      }

      if (calibration_positon!=0xFFFF && calibration_positon>=(pageaddr) && calibration_positon<((pageaddr+pagesize))){
        pageBuffer[calibration_positon&(pagesize-1)]=osscal_value;
        calibration_positon=0xFFFF;
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

    if (calibration_positon!=0xFFFF){  //flash osccal separately
      for (uint8_t i=0; i<pagesize; i++) {
        pageBuffer[i] = 0xFF;
      }
      pageBuffer[calibration_positon&(pagesize-1)]=osscal_value;
      if (! flashPage(pageBuffer, calibration_positon&(~(pagesize-1)), pagesize)){	
        error_no_fatal(F("Flash programming failed"));
        break;
      }
    }

    // Set fuses to 'final' state
    if (! programFuses(targetimage->image_normfuses)){
      error_no_fatal(F("Programming Fuses fail"));
      break;
    }

    end_pmode();
    start_pmode();

    Serial.println("\nVerifing flash...");
    if (! verifyImage_with_osccal((byte *)pgm_read_word(&targetimage->image_final),pgm_read_word(&targetimage->osccal_flash_pos),osscal_value) ) {
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

