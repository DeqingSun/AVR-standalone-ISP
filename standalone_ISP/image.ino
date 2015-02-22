
image_t PROGMEM image_328 = {
  {
    "test.hex"  }
  ,
  {
    "attiny13A"  }
  ,
  0x9007,				/* Signature bytes for 328P */
  {
    0x03, 0x6A, 0x1F, 0x00  }
  ,            // pre program fuses (prot/lock, low, high, ext)
  {
    0x0F, 0x0, 0x0, 0x0  }
  ,            // post program fuses
  {
    0x03, 0xFF, 0x1F, 0x00  }
  ,           // fuse mask
  1024,     // size of chip flash in bytes
  32,   // size in bytes of flash page
  {
    ":1000000009C00EC00DC00CC00BC00AC009C008C09A\n"
      ":1000100007C006C011241FBECFE9CDBF02D010C05B\n"
      ":10002000EFCFBC9A90E12FEB34ED41E0215030400E\n"
      ":100030004040E1F700C0000088B3892788BBF3CFB8\n"
      ":04004000F894FFCF62\n"
      ":00000001FF\n"
  }
};


/*
 * Table of defined images
 */
image_t *images[] = {
  &image_328,
};

uint8_t NUMIMAGES = sizeof(images)/sizeof(images[0]);

