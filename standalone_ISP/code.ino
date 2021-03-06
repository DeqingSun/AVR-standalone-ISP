#include "optiLoader.h"
//#define VERBOSE 1
/*
 * Bootload images.
 * These are the intel Hex files produced by the optiboot makefile,
 * with a small amount of automatic editing to turn them into C strings,
 * and a header attched to identify them
 */

extern image_t *images[];
extern uint8_t NUMIMAGES;

/*
 * readSignature
 * read the bottom two signature bytes (if possible) and return them
 * Note that the highest signature byte is the same over all AVRs so we skip it
 */

uint16_t readSignature (void)
{
  SPI.setClockDivider(CLOCKSPEED_FUSES); 

  uint16_t target_type = 0;
  Serial.print(F("\nReading signature:"));

  target_type = spi_transaction(0x30, 0x00, 0x01, 0x00);
  target_type <<= 8;
  target_type |= spi_transaction(0x30, 0x00, 0x02, 0x00);

  Serial.println(target_type, HEX);
  if (target_type == 0 || target_type == 0xFFFF) {
    if (target_type == 0) {
      Serial.println(F("  (no target attached?)"));
    }
  }
  return target_type;
}

/*
 * findImage
 *
 * given 'signature' loaded with the relevant part of the device signature,
 * search the hex images that we have programmed in flash, looking for one
 * that matches.
 */
image_t *findImage (uint16_t signature)
{
  image_t *ip;
  Serial.println(F("Searching for image..."));

  for (byte i=0; i < NUMIMAGES; i++) {
    ip = images[i];

    if (ip && (pgm_read_word(&ip->image_chipsig) == signature)) {
      Serial.print(F("  Found \""));
      flashprint(&ip->image_name[0]);
      Serial.print(F("\" for "));
      flashprint(&ip->image_chipname[0]);
      Serial.println();

      return ip;
    }
  }
  Serial.println(F(" Not Found"));
  return 0;
}

/*
 * programmingFuses
 * Program the fuse/lock bits
 */
boolean programFuses (const byte *fuses)
{
  SPI.setClockDivider(CLOCKSPEED_FUSES); 

  byte f;
  Serial.print(F("\nSetting fuses"));

  f = pgm_read_byte(&fuses[FUSE_PROT]);
  if (f) {
    Serial.print(F("\n  Set Lock Fuse to: "));
    Serial.print(f, HEX);
    Serial.print(F(" -> "));
    busyWait();
    Serial.print(spi_transaction(0xAC, 0xE0, 0x00, f), HEX);
  }
  f = pgm_read_byte(&fuses[FUSE_LOW]);
  if (f) {
    Serial.print(F("  Set Low Fuse to: "));
    Serial.print(f, HEX);
    Serial.print(F(" -> "));
    busyWait();
    Serial.print(spi_transaction(0xAC, 0xA0, 0x00, f), HEX);
  }
  f = pgm_read_byte(&fuses[FUSE_HIGH]);
  if (f) {
    Serial.print(F("  Set High Fuse to: "));
    Serial.print(f, HEX);
    Serial.print(F(" -> "));
    busyWait();
    Serial.print(spi_transaction(0xAC, 0xA8, 0x00, f), HEX);
  }
  f = pgm_read_byte(&fuses[FUSE_EXT]);
  if (f) {
    Serial.print(F("  Set Ext Fuse to: "));
    Serial.print(f, HEX);
    Serial.print(F(" -> "));
    busyWait();
    Serial.print(spi_transaction(0xAC, 0xA4, 0x00, f), HEX);
  }
  Serial.println();
  return true; 			/* */
}

/*
 * verifyFuses
 * Verifies a fuse set
 */
boolean verifyFuses (const byte *fuses, const byte *fusemask)
{
  SPI.setClockDivider(CLOCKSPEED_FUSES); 
  byte f;
  Serial.println(F("Verifying fuses..."));
  f = pgm_read_byte(&fuses[FUSE_PROT]);
  if (f) {
    busyWait();
    uint8_t readfuse = spi_transaction(0x58, 0x00, 0x00, 0x00);  // lock fuse
    readfuse &= pgm_read_byte(&fusemask[FUSE_PROT]);
    Serial.print(F("\tLock Fuse: ")); 
    Serial.print(f, HEX);  
    Serial.print(F(" is ")); 
    Serial.print(readfuse, HEX);
    if (readfuse != f) 
      return false;
  }
  f = pgm_read_byte(&fuses[FUSE_LOW]);
  if (f) {
    busyWait();
    uint8_t readfuse = spi_transaction(0x50, 0x00, 0x00, 0x00);  // low fuse
    Serial.print(F("\tLow Fuse: 0x"));  
    Serial.print(f, HEX); 
    Serial.print(F(" is 0x")); 
    Serial.print(readfuse, HEX);
    readfuse &= pgm_read_byte(&fusemask[FUSE_LOW]);
    if (readfuse != f) 
      return false;
  }
  f = pgm_read_byte(&fuses[FUSE_HIGH]);
  if (f) {
    busyWait();
    uint8_t readfuse = spi_transaction(0x58, 0x08, 0x00, 0x00);  // high fuse
    readfuse &= pgm_read_byte(&fusemask[FUSE_HIGH]);
    Serial.print(F("\tHigh Fuse: 0x"));  
    Serial.print(f, HEX); 
    Serial.print(F(" is 0x"));  
    Serial.print(readfuse, HEX);
    if (readfuse != f) 
      return false;
  }
  f = pgm_read_byte(&fuses[FUSE_EXT]);
  if (f) {
    busyWait();
    uint8_t readfuse = spi_transaction(0x50, 0x08, 0x00, 0x00);  // ext fuse
    readfuse &= pgm_read_byte(&fusemask[FUSE_EXT]);
    Serial.print(F("\tExt Fuse: 0x")); 
    Serial.print(f, HEX); 
    Serial.print(F(" is 0x")); 
    Serial.print(readfuse, HEX);
    if (readfuse != f) 
      return false;
  }
  Serial.println();
  return true; 			/* */
}



/*
 * readImagePage
 *
 * Read a page of intel hex image from a string in pgm memory.
 */

// Returns number of bytes decoded
byte * readImagePage (byte *hextext, uint16_t pageaddr, uint8_t pagesize, byte *page)
{

  boolean firstline = true;
  uint16_t len;
  uint8_t page_idx = 0;
  byte *beginning = hextext;
  boolean endoffile = false;

  byte b, cksum = 0;

  //Serial.print("page size = "); Serial.println(pagesize, DEC);

  // 'empty' the page by filling it with 0xFF's
  for (uint8_t i=0; i<pagesize; i++)
    page[i] = 0xFF;

  while (1) {
    uint16_t lineaddr;

    // read one line!
    if (pgm_read_byte(hextext++) != ':') {
      error(F("No colon?"));
      break;
    }
    // Read the byte count into 'len'
    len = hexton(pgm_read_byte(hextext++));
    len = (len<<4) + hexton(pgm_read_byte(hextext++));
    cksum = len;

    // read high address byte
    b = hexton(pgm_read_byte(hextext++));  
    b = (b<<4) + hexton(pgm_read_byte(hextext++));
    cksum += b;
    lineaddr = b;

    // read low address byte
    b = hexton(pgm_read_byte(hextext++)); 
    b = (b<<4) + hexton(pgm_read_byte(hextext++));
    cksum += b;
    lineaddr = (lineaddr << 8) + b;

    if (lineaddr >= (pageaddr + pagesize)) {
      return beginning;
    }

    b = hexton(pgm_read_byte(hextext++)); // record type 
    b = (b<<4) + hexton(pgm_read_byte(hextext++));
    cksum += b;
    //Serial.print("Record type "); Serial.println(b, HEX);
    if (b == 0x1) { 
      // end record!
      endoffile = true;
      break;
    } 
#if VERBOSE
    Serial.print(F("\nLine address =  0x")); 
    Serial.println(lineaddr, HEX);      
    Serial.print(F("Page address =  0x")); 
    Serial.println(pageaddr, HEX);      
#endif
    for (byte i=0; i < len; i++) {
      // read 'n' bytes
      b = hexton(pgm_read_byte(hextext++));
      b = (b<<4) + hexton(pgm_read_byte(hextext++));

      cksum += b;
#if VERBOSE
      Serial.print(b, HEX);
      Serial.write(' ');
#endif

      page[page_idx] = b;
      page_idx++;

      if (page_idx > pagesize) {
        error(F("Too much code"));
        break;
      }
    }
    b = hexton(pgm_read_byte(hextext++));  // chxsum
    b = (b<<4) + hexton(pgm_read_byte(hextext++));
    cksum += b;
    if (cksum != 0) {
      error(F("Bad checksum: "));
      Serial.print(cksum, HEX);
    }
    if (pgm_read_byte(hextext++) != '\n') {
      error(F("No end of line"));
      break;
    }
#if VERBOSE
    Serial.println();
    Serial.println(page_idx, DEC);
#endif
    if (page_idx == pagesize) 
      break;
  }
#if VERBOSE
  Serial.print(F("\n  Total bytes read: "));
  Serial.println(page_idx, DEC);
#endif
  if (!endoffile){
    return hextext;
  }
  else{
    return NULL;
  }
}

// Send one byte to the page buffer on the chip
void flashWord (uint8_t hilo, uint16_t addr, uint8_t data) {
#if VERBOSE
  Serial.print(data, HEX);  
  Serial.print(':');
  Serial.print(spi_transaction(0x40+8*hilo,  addr>>8 & 0xFF, addr & 0xFF, data), HEX);
  Serial.print(F(" "));
#else
  spi_transaction(0x40+8*hilo, addr>>8 & 0xFF, addr & 0xFF, data);
#endif
}

// Basically, write the pagebuff (with pagesize bytes in it) into page $pageaddr
boolean flashPage (byte *pagebuff, uint16_t pageaddr, uint8_t pagesize) {  
  SPI.setClockDivider(CLOCKSPEED_FLASH); 
  busyWait();

  Serial.print(F("Flashing page ")); 
  Serial.println(pageaddr, HEX);
  for (uint16_t i=0; i < pagesize/2; i++) {

#if VERBOSE
    Serial.print(pagebuff[2*i], HEX); 
    Serial.print(' ');
    Serial.print(pagebuff[2*i+1], HEX); 
    Serial.print(' ');
    if ( i % 16 == 15) Serial.println();
#endif

    flashWord(LOW, i, pagebuff[2*i]);
    flashWord(HIGH, i, pagebuff[2*i+1]);
  }

  // page addr is in bytes, byt we need to convert to words (/2)
  pageaddr = (pageaddr/2) & (~(((pagesize/2)-1)));

  uint16_t commitreply = spi_transaction(0x4C, (pageaddr >> 8) & 0xFF, pageaddr & 0xFF, 0);

  Serial.print(F("  Commit Page: 0x"));  
  Serial.print(pageaddr, HEX);
  Serial.print(F(" -> 0x")); 
  Serial.println(commitreply, HEX);
  if (commitreply != pageaddr) 
    return false;

  busyWait();

  return true;
}

unsigned char readByteEEPROM(unsigned int addr) {
  unsigned char value = spi_transaction(0xA0, (addr >> 8) & 0x1F, addr & 0xFF, 0x00) & 0xFF;
  return value;
}

boolean programEEPROM(const unsigned char *imagedata, int pagesize, int imagesize) {
  for(int idx = 0; idx < imagesize; idx += pagesize) {
    for(int i = 0; (i < pagesize) && (idx + i < imagesize); i++) {
      spi_transaction(0xC1, 0x00, i, pgm_read_byte(&imagedata[idx + i]));
    }
    spi_transaction(0xC2, idx >> 8, idx & 0xFC, 0x00);
    busyWait();
  }
  return true;
}

boolean verifyEEPROM(const unsigned char *imagedata, int imagesize) {
  for(int idx = 0; idx < imagesize; idx++) {
    byte b = pgm_read_byte(&imagedata[idx]);
    if(b != spi_transaction(0xA0, idx >> 8, idx & 0xFF, 0x00) & 0xFF) {
        Serial.print("EEPROM verification error at address 0x"); Serial.print(idx, HEX);
        Serial.print(" Should be 0x"); Serial.print(b, HEX); Serial.print(" not 0x");
        Serial.println((spi_transaction(0xA0, idx >> 8, idx & 0xFF, 0x00) & 0xFF), HEX);
        return false;      
    }
  }
  return true;
}

// verifyImage does a byte-by-byte verify of the flash hex against the chip
// Thankfully this does not have to be done by pages!
// returns true if the image is the same as the hextext, returns false on any error
boolean verifyImage (byte *hextext)  {
  uint16_t address = 0;

  SPI.setClockDivider(CLOCKSPEED_FLASH); 

  uint16_t len;
  byte b, cksum = 0;

  while (1) {
    uint16_t lineaddr;

    // read one line!
    if (pgm_read_byte(hextext++) != ':') {
      error(F("No colon"));
      return false;
    }
    len = hexton(pgm_read_byte(hextext++));
    len = (len<<4) + hexton(pgm_read_byte(hextext++));
    cksum = len;

    b = hexton(pgm_read_byte(hextext++)); // record type 
    b = (b<<4) + hexton(pgm_read_byte(hextext++));
    cksum += b;
    lineaddr = b;
    b = hexton(pgm_read_byte(hextext++)); // record type
    b = (b<<4) + hexton(pgm_read_byte(hextext++));
    cksum += b;
    lineaddr = (lineaddr << 8) + b;

    b = hexton(pgm_read_byte(hextext++)); // record type 
    b = (b<<4) + hexton(pgm_read_byte(hextext++));
    cksum += b;

    //Serial.print("Record type "); Serial.println(b, HEX);
    if (b == 0x1) { 
      // end record!
      break;
    } 

    for (byte i=0; i < len; i++) {
      // read 'n' bytes
      b = hexton(pgm_read_byte(hextext++));
      b = (b<<4) + hexton(pgm_read_byte(hextext++));
      cksum += b;

#if VERBOSE
      Serial.print(F("$"));
      Serial.print(lineaddr, HEX);
      Serial.print(F(":0x"));
      Serial.print(b, HEX);
      Serial.print(F(" ? "));
#endif

      // verify this byte!
      if (!verify_byte(lineaddr,b)) return false;
      lineaddr++;  
    }

    b = hexton(pgm_read_byte(hextext++));  // chxsum
    b = (b<<4) + hexton(pgm_read_byte(hextext++));
    cksum += b;
    if (cksum != 0) {
      error(F("Bad checksum: "));
      Serial.print(cksum, HEX);
      return false;
    }
    if (pgm_read_byte(hextext++) != '\n') {
      error(F("No end of line"));
      return false;
    }
  }
  return true;
}

boolean verifyImage_with_osccal (byte *hextext, unsigned int pos, unsigned char value)  {
  uint16_t address = 0;

  SPI.setClockDivider(CLOCKSPEED_FLASH); 

  uint16_t len;
  byte b,data,cksum = 0;

  while (1) {
    uint16_t lineaddr;

    // read one line!
    if (pgm_read_byte(hextext++) != ':') {
      error(F("No colon"));
      return false;
    }
    len = hexton(pgm_read_byte(hextext++));
    len = (len<<4) + hexton(pgm_read_byte(hextext++));
    cksum = len;

    b = hexton(pgm_read_byte(hextext++)); // record type 
    b = (b<<4) + hexton(pgm_read_byte(hextext++));
    cksum += b;
    lineaddr = b;
    b = hexton(pgm_read_byte(hextext++)); // record type
    b = (b<<4) + hexton(pgm_read_byte(hextext++));
    cksum += b;
    lineaddr = (lineaddr << 8) + b;

    b = hexton(pgm_read_byte(hextext++)); // record type 
    b = (b<<4) + hexton(pgm_read_byte(hextext++));
    cksum += b;

    //Serial.print("Record type "); Serial.println(b, HEX);
    if (b == 0x1) { 
      // end record!
      break;
    } 

    for (byte i=0; i < len; i++) {
      // read 'n' bytes
      b = hexton(pgm_read_byte(hextext++));
      b = (b<<4) + hexton(pgm_read_byte(hextext++));
      cksum += b;

#if VERBOSE
      Serial.print(F("$"));
      Serial.print(lineaddr, HEX);
      Serial.print(F(":0x"));
      Serial.print(b, HEX);
      Serial.print(F(" ? "));
#endif

      // verify this byte!
      if (lineaddr!=pos){
        if (!verify_byte(lineaddr,b)) return false;
      }
      lineaddr++;  
    }

    b = hexton(pgm_read_byte(hextext++));  // chxsum
    b = (b<<4) + hexton(pgm_read_byte(hextext++));
    cksum += b;
    if (cksum != 0) {
      error(F("Bad checksum: "));
      Serial.print(cksum, HEX);
      return false;
    }
    if (pgm_read_byte(hextext++) != '\n') {
      error(F("No end of line"));
      return false;
    }
  }
  
  //check osccal seperately
  if (!verify_byte(pos,value)) return false;
  
  return true;
}

boolean verify_byte(uint16_t addr,uint8_t value){
  uint8_t data;
  //check osccal seperately
  if (addr % 2) {
    // for 'high' bytes:
    data = (spi_transaction(0x28, addr >> 9, addr / 2, 0) & 0xFF);
  }else{
    // for 'low bytes'
    data = (spi_transaction(0x20, addr >> 9, addr / 2, 0) & 0xFF);
  }
  if (value != data) {
    Serial.print(F("verification error at address 0x")); 
    Serial.print(addr, HEX);
    Serial.print(F(" Should be 0x")); 
    Serial.print(value, HEX); 
    Serial.print(F(" not 0x"));
    Serial.println(data, HEX);
    return false;
  }  
  return true;
}


// Send the erase command, then busy wait until the chip is erased

void eraseChip(void) {
  SPI.setClockDivider(CLOCKSPEED_FUSES); 

  spi_transaction(0xAC, 0x80, 0, 0);	// chip erase    
  busyWait();
}

// Simply polls the chip until it is not busy any more - for erasing and programming
void busyWait(void)  {
  byte busybit;
  do {
    busybit = spi_transaction(0xF0, 0x0, 0x0, 0x0);
    //Serial.print(busybit, HEX);
  } 
  while (busybit & 0x01);
}


/*
 * Functions specific to ISP programming of an AVR
 */
uint16_t spi_transaction (uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  uint8_t n, m;
  SPI.transfer(a); 
  n = SPI.transfer(b);
  //if (n != a) error = -1;
  m = SPI.transfer(c);
  return 0xFFFFFF & ((n<<16)+(m<<8) + SPI.transfer(d));
}




