/*
  Bit.krshr - DIY bit crusher for electric guitar
  Board: Arduino Nano-compatible (ELEGOO Nano V3, ATmega328P)

  Pins:
    A0 <- guitar signal, biased to about 2.5 V (reads about 512 at silence)
    A1 <- trimpot 1: bit depth   (8 bits ... 1 bit)
    A2 <- trimpot 2: sample rate (hold each sample for 1 ... 24 loop cycles)
    D3 -> audio output as fast PWM (62.5 kHz), smoothed by an RC filter
*/

const int GAIN = 16;        // digital gain: 1 = none, 16 = 16x (try 4-16)

long acc = 512L << 12;      // running estimate of the DC level, stored x4096
uint8_t held = 128;         // the sample sent to the output (128 = silence)
uint8_t bits = 8;           // current bit depth, 1-8 (from the pot on A1)
uint8_t holdN = 1;          // take a new sample every holdN loops (pot on A2)
uint8_t holdCount = 0;      // loops since the last new sample
uint16_t ctr = 0;           // loop counter, so the pots are read only now and then
bool readBitPot = true;     // alternates between reading A1 and A2

void setup() {
  pinMode(3, OUTPUT);       // D3 = OC2B, output channel B of Timer2

  // Timer2: Fast PWM on pin 3
  TCCR2A = _BV(COM2B1) | _BV(WGM21) | _BV(WGM20);
  // No prescaler: PWM frequency = 16 MHz / 256 = 62.5 kHz (above hearing range)
  TCCR2B = _BV(CS20);

  // Faster ADC: clock divider 64 instead of the default 128 (about 52 us per reading)
  ADCSRA = (ADCSRA & 0b11111000) | 0x06;
}

void loop() {
  // 1. Read the guitar signal and remove its DC level
  int raw = analogRead(A0);
  acc += raw - (acc >> 12);               // slowly follow the DC level (about 0.3 s)
  int x = raw - (int)(acc >> 12);         // x = audio only, centered around 0

  // 2. Sample rate reduction: take a new sample only every holdN loops
  if (holdCount == 0) {
    int s = (x * GAIN) / 4;               // amplify and scale from 10 bit to 8 bit
    s = constrain(s, -128, 127);          // clip, so the value cannot wrap around

    // 3. Bit depth reduction: round to the nearest of fewer levels
    uint8_t shift = 8 - bits;             // how many low bits to throw away
    if (shift > 0) {
      // add half a step, drop the low bits, shift back up
      s = ((s + (1 << (shift - 1))) >> shift) << shift;
      s = constrain(s, -128, 127);        // rounding can push 127 up to 128
    }
    held = s + 128;                       // shift to the PWM range 0-255
  }
  if (++holdCount >= holdN) holdCount = 0;

  OCR2B = held;                           // write the PWM duty cycle to pin 3

  // 4. Read one pot about every 30 ms (A1 and A2 take turns)
  if ((++ctr & 0x1FF) == 0) {
    int v = analogRead(readBitPot ? A1 : A2);
    if (readBitPot) bits  = map(v, 0, 1023, 8, 1);
    else            holdN = map(v, 0, 1023, 1, 24);
    readBitPot = !readBitPot;
    analogRead(A0);                       // dummy reading: clears charge left by the pot reading
  }
}
