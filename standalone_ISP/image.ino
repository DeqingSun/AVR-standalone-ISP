
image_t PROGMEM image_328 = {
    {"test.hex"},
    {"attiny13A"},
    0x9007,				/* Signature bytes for 328P */
    {0x3F, 0xFF, 0xDA, 0x05},            // pre program fuses (prot/lock, low, high, ext)
    {0x0F, 0x0, 0x0, 0x0},            // post program fuses
    {0x3F, 0xFF, 0xFF, 0x07},           // fuse mask
    1024,     // size of chip flash in bytes
    32,   // size in bytes of flash page
    {
    
    }
};


/*
 * Table of defined images
 */
image_t *images[] = {
  &image_328,
};

uint8_t NUMIMAGES = sizeof(images)/sizeof(images[0]);
