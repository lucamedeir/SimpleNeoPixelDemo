
/*
  This is an example of how simple driving a Neopixel can be
  This code is optimized for understandability and changability rather than raw speed
  More info at http://wp.josh.com/2014/05/11/ws2812-neopixels-made-easy/
*/

// Change this to be at least as long as your pixel string (too long will work fine, just be a little slower)

#define PIXELS 90*10  // Number of pixels in the string

// These values depend on which pin your string is connected to and what board you are using
// More info on how to find these at http://www.arduino.cc/en/Reference/PortManipulation

// These values are for the pin that connects to the Data Input pin on the LED strip. They correspond to...

// Arduino Yun:     Digital Pin 8
// DueMilinove/UNO: Digital Pin 12
// Arduino MeagL    PWM Pin 4

// You'll need to look up the port/bit combination for other boards.

// Note that you could also include the DigitalWriteFast header file to not need to to this lookup.

#define PIXEL_PORT  PORTB  // Port of the pin the pixels are connected to
#define PIXEL_DDR   DDRB   // Port of the pin the pixels are connected to
#define PIXEL_BIT   4      // Bit of the pin the pixels are connected to

// These are the timing constraints taken mostly from the WS2812 datasheets
// These are chosen to be conservative and avoid problems rather than for maximum throughput

#define T1H  900    // Width of a 1 bit in ns
#define T1L  600    // Width of a 1 bit in ns

#define T0H  400    // Width of a 0 bit in ns
#define T0L  900    // Width of a 0 bit in ns

// The reset gap can be 6000 ns, but depending on the LED strip it may have to be increased
// to values like 600000 ns. If it is too small, the pixels will show nothing most of the time.
#define RES 600000    // Width of the low gap between bits to cause a frame to latch

// Here are some convience defines for using nanoseconds specs to generate actual CPU delays

#define NS_PER_SEC (1000000000L)          // Note that this has to be SIGNED since we want to be able to check for negative values of derivatives

#define CYCLES_PER_SEC (F_CPU)

#define NS_PER_CYCLE ( NS_PER_SEC / CYCLES_PER_SEC )

#define NS_TO_CYCLES(n) ( (n) / NS_PER_CYCLE )

// Actually send a bit to the string. We must to drop to asm to enusre that the complier does
// not reorder things and make it so the delay happens in the wrong place.

inline void sendBit( bool bitVal ) {

  if (  bitVal ) {				// 0 bit

    asm volatile (
      "sbi %[port], %[bit] \n\t"				// Set the output bit
      ".rept %[onCycles] \n\t"                                // Execute NOPs to delay exactly the specified number of cycles
      "nop \n\t"
      ".endr \n\t"
      "cbi %[port], %[bit] \n\t"                              // Clear the output bit
      ".rept %[offCycles] \n\t"                               // Execute NOPs to delay exactly the specified number of cycles
      "nop \n\t"
      ".endr \n\t"
      ::
      [port]		"I" (_SFR_IO_ADDR(PIXEL_PORT)),
      [bit]		"I" (PIXEL_BIT),
      [onCycles]	"I" (NS_TO_CYCLES(T1H) - 2),		// 1-bit width less overhead  for the actual bit setting, note that this delay could be longer and everything would still work
      [offCycles] 	"I" (NS_TO_CYCLES(T1L) - 2)			// Minimum interbit delay. Note that we probably don't need this at all since the loop overhead will be enough, but here for correctness

    );

  } else {					// 1 bit

    // **************************************************************************
    // This line is really the only tight goldilocks timing in the whole program!
    // **************************************************************************


    asm volatile (
      "sbi %[port], %[bit] \n\t"				// Set the output bit
      ".rept %[onCycles] \n\t"				// Now timing actually matters. The 0-bit must be long enough to be detected but not too long or it will be a 1-bit
      "nop \n\t"                                              // Execute NOPs to delay exactly the specified number of cycles
      ".endr \n\t"
      "cbi %[port], %[bit] \n\t"                              // Clear the output bit
      ".rept %[offCycles] \n\t"                               // Execute NOPs to delay exactly the specified number of cycles
      "nop \n\t"
      ".endr \n\t"
      ::
      [port]		"I" (_SFR_IO_ADDR(PIXEL_PORT)),
      [bit]		"I" (PIXEL_BIT),
      [onCycles]	"I" (NS_TO_CYCLES(T0H) - 2),
      [offCycles]	"I" (NS_TO_CYCLES(T0L) - 2)

    );

  }

  // Note that the inter-bit gap can be as long as you want as long as it doesn't exceed the 5us reset timeout (which is A long time)
  // Here I have been generous and not tried to squeeze the gap tight but instead erred on the side of lots of extra time.
  // This has thenice side effect of avoid glitches on very long strings becuase


}


inline void sendByte( unsigned char byte ) {

  for ( unsigned char bit = 0 ; bit < 8 ; bit++ ) {

    sendBit( bitRead( byte , 7 ) );                // Neopixel wants bit in highest-to-lowest order
    // so send highest bit (bit #7 in an 8-bit byte since they start at 0)
    byte <<= 1;                                    // and then shift left so bit 6 moves into 7, 5 moves into 6, etc

  }
}

/*

  The following three functions are the public API:

  ledSetup() - set up the pin that is connected to the string. Call once at the begining of the program.
  sendPixel( r g , b ) - send a single pixel to the string. Call this once for each pixel in a frame.
  show() - show the recently sent pixel on the LEDs . Call once per frame.

*/


// Set the specified pin up as digital out

void ledsetup() {

  bitSet( PIXEL_DDR , PIXEL_BIT );

}

inline void sendPixel( unsigned char r, unsigned char g , unsigned char b )  {

  sendByte(g);          // Neopixel wants colors in green then red then blue order
  sendByte(r);
  sendByte(b);

}


// Just wait long enough without sending any bots to cause the pixels to latch and display the last sent frame

void show() {
  _delay_us( (RES / 1000UL) + 1);				// Round up since the delay must be _at_least_ this long (too short might not work, too long not a problem)
}




void clearLed() {
  cli();
  for (int k = 0; k < PIXELS; k++) {
    sendPixel(0, 0, 0);
  }
  sei();
  show();
}

void clickLed(unsigned char led) {
  sendPixel(10 * (led >> 7), 0, 0 );
  sendPixel(10 * ((led & B01000000 ) >> 6), 0, 0);
  sendPixel(10 * ((led & B00100000 ) >> 5), 0, 0);
  sendPixel(10 * ((led & B00010000 ) >> 4), 0, 0);
  sendPixel(10 * ((led & B00001000 ) >> 3), 0, 0);
  sendPixel(10 * ((led & B00000100 ) >> 2), 0, 0);
  sendPixel(10 * ((led & B00000010 ) >> 1), 0, 0);
  sendPixel(10 * (led & B00000001), 0, 0);
}


const unsigned char zero[10] = {0x1c,
                                0x3e,
                                0x47,
                                0xc3,
                                0xc3,
                                0xc3,
                                0xc3,
                                0xe2,
                                0x7c,
                                0x38
                               };

const unsigned char one[10] = {0xc,
                               0x1c,
                               0x3c,
                               0x7c,
                               0xc,
                               0xc,
                               0xc,
                               0xc,
                               0x1e,
                               0x3f
                              };


const unsigned char two[10] = {0x1c,
                               0x7e,
                               0xc3,
                               0xc6,
                               0xc,
                               0x18,
                               0x30,
                               0x60,
                               0xfe,
                               0xff
                              };


const unsigned char three[10] = {0x1c,
                                 0x7e,
                                 0x63,
                                 0x3,
                                 0xe,
                                 0xe,
                                 0x63,
                                 0x63,
                                 0x3e,
                                 0x1c
                                };


const unsigned char four[10] = {0xc,
                                0x1c,
                                0x2c,
                                0x4c,
                                0xfe,
                                0xc,
                                0xc,
                                0xc,
                                0x1e,
                                0x3f
                               };


const unsigned char five[10] = {0x3c,
                                0x7e,
                                0x60,
                                0x60,
                                0x78,
                                0x3e,
                                0x3,
                                0x63,
                                0x3e,
                                0x1c
                               };

const unsigned char six[10] = {0x1c,
                               0x3e,
                               0x63,
                               0x40,
                               0x70,
                               0x7e,
                               0x43,
                               0x63,
                               0x3e,
                               0x1c
                              };

const unsigned char seven[10] = {0x7f,
                                 0x7f,
                                 0x6,
                                 0xc,
                                 0x18,
                                 0x30,
                                 0x30,
                                 0x30,
                                 0x30,
                                 0x30
                                };

const unsigned char eight[10] = {0x3c,
                                 0x66,
                                 0xc7,
                                 0xe3,
                                 0x72,
                                 0x5e,
                                 0xc7,
                                 0xc3,
                                 0x66,
                                 0x3c
                                };


const unsigned char nine[10] = {0x1c,
                                0x3e,
                                0x63,
                                0x41,
                                0x63,
                                0x3f,
                                0x3,
                                0x63,
                                0x66,
                                0x3c
                               };


unsigned char reverseBits(unsigned char num) {

  int i = 7; //size of unsigned char -1, on most machine is 8bits
  unsigned char j = 0;
  unsigned char temp = 0;

  while (i >= 0) {
    temp |= ((num >> j) & 1) << i;
    i--;
    j++;
  }
  return (temp);

}

const unsigned int counter[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

unsigned char get_led( int counter, int index) {
  unsigned char led = zero[index];

  switch (counter) {
    case 0:
      led = zero[index];
      break;
    case 1:
      led =  one[index];
      break;
    case 2:
      led = two[index];
      break;
    case 3:
      led = three[index];
      break;
    case 4:
      led = four[index];
      break;
    case 5:
      led = five[index];
      break;
    case 6:
      led = six[index];
      break;
    case 7:
      led = seven[index];
      break;
    case 8:
      led = eight[index];
      break;
    case 9:
      led = nine[index];
  }

  return led;
}

void clickDigit(int j, unsigned char led) {

  if ( j % 2 == 0) {
    sendPixel(0, 0, 0);
    clickLed(led);
  } else {
    clickLed(reverseBits(led));
    sendPixel(0, 0, 0);
  }
}



unsigned char digits[10] = {9,8,7,6,5,4,3,2,1,0};


void clickNumber() {

  cli();

  for (int j = 0; j < 10; j++) {
    if (j % 2 == 1) {
      for (int i = 9; i >= 0; i--) {

        unsigned char led = get_led(digits[i], j);

        clickDigit(j, led);
      }
    } else {
      for (int i = 0; i < 10; i++) {

        unsigned char led = get_led(digits[i], j);

        clickDigit(j, led);
      }
    }

  }

  sei();
  show();
}

void prepare_number(unsigned long number) {
  digits[0] = (number / 1000000000L);
  digits[1] = (number / 100000000L) % 10;
  digits[2] = (number / 10000000L) % 10;
  digits[3] = (number / 1000000L) % 10;
  digits[4] = (number / 100000L) % 10;
  digits[5] = (number / 10000) % 10;
  digits[6] = (number / 1000) % 10;
  digits[7] = (number / 100) % 10;
  digits[8] = (number / 10) % 10;
  digits[9] = number % 10;

}


unsigned long timer = 999994000L;
unsigned long time_passed =0;
unsigned long dt_first = 0;
void setup() {
  ledsetup();
  dt_first = millis();

  prepare_number(timer);
  clickNumber();
}

#define THEATER_SPACING (PIXELS/20)

void theaterChase( unsigned char r , unsigned char g, unsigned char b, unsigned char wait ) {
  
  for (int j=0; j< 3 ; j++) {  
  
    for (int q=0; q < THEATER_SPACING ; q++) {
      
      unsigned int step=0;
      
      cli();
      
      for (int i=0; i < PIXELS ; i++) {
        
        if (step==q) {
          
          sendPixel( r , g , b );
          
        } else {
          
          sendPixel( 0 , 0 , 0 );
          
        }
        
        step++;
        
        if (step==THEATER_SPACING) step =0;
        
      }
      
      sei();
      
      show();
      delay(wait);
      
    }
    
  }
  
}
        


// I rewrite this one from scrtach to use high resolution for the color wheel to look nicer on a *much* bigger string
                                                                            
void rainbowCycle() {
  int r = random(0,10);
  int g = random(0,10);
  int b = random(0,10);
  for(int i =0; i < 900; i++) {
    sendPixel(r,g,b);
  }
}

void loop() {
  unsigned long dt = millis();

  if(timer > 1000000000L) {
    
      delay(100);
      rainbowCycle();
  }
  
  if (time_passed > 1000) {
    time_passed = 0;
    timer++;
    prepare_number(timer);
    clickNumber();
  }

  
    
    time_passed += dt-dt_first;
    
    dt_first = dt;

  
}
