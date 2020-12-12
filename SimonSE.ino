#include <avr/sleep.h>
#include <util/delay_basic.h>
#include <avr/eeprom.h>

struct DATA {
  uint8_t hsLevel;
  uint16_t hsSeed;
  uint8_t lastLevel;
  uint16_t lastSeed;
} data;


const uint8_t buttons[4] = {
  0b00001010, 0b00000110, 0b00000011, 0b00010010
};
const uint8_t tones[4] = {
  239, 179, 143, 119
};

volatile uint16_t time;
uint16_t ctx;
uint16_t seed;
//uint8_t maxLvl = 0;
volatile uint8_t nrot = 8;
uint8_t lastKey;
uint8_t lvl = 0;

uint8_t simple_random4() {
  // ctx = ctx * 1103515245 + 12345; // too big for ATtiny13
  ctx = 2053 * ctx + 13849;
  uint8_t temp = ctx ^ (ctx >> 8); // XOR two bytes
  temp ^= (temp >> 4); // XOR two nibbles
  return (temp ^ (temp >> 2)) & 0b00000011; // XOR two pairs of bits and return remainder after division by 4
}

void sleepNow() {
  PORTB = 0b00000000; // disable all pull-up resistors
  cli(); // disable all interrupts
  WDTCR = 0; // turn off the Watchdog timer
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_cpu();
}

void play(uint8_t i, uint16_t t = 45000) {
  PORTB = 0b00000000;  // set all button pins low or disable pull-up resistors
  DDRB = buttons[i]; // set speaker and #i button pin as output
  OCR0A = tones[i];
  OCR0B = tones[i] >> 1;
  TCCR0B = (1 << WGM02) | (1 << CS01); // prescaler /8
  _delay_loop_2(t);
  TCCR0B = 0b00000000; // no clock source (Timer0 stopped)
  DDRB = 0b00000000;
  PORTB = 0b00011101;
}


void gameOver() {
  for (uint8_t i = 0; i < 4; i++) {
    play(3 - i, 25000);
  }
  if (lvl > data.hsLevel) {
//    eeprom_write_byte((uint8_t*) 0, ~lvl); // write best score
//    eeprom_write_byte((uint8_t*) 1, (seed >> 8)); // write high byte of seed
//    eeprom_write_byte((uint8_t*) 2, (seed & 0b11111111)); // write low byte of seed
    //eeprom_write_word((uint16_t*) 1, seed); // write seed
    for (uint8_t i = 0; i < 3; i++) { // play best score melody
      levelUp();
    }
  }
  eeprom_write_block((void*)&data,0,sizeof(data));
  sleepNow();
}

void levelUp() {
  for (uint8_t i = 0; i < 4; i++) {
    play(i, 25000);
  }
}

ISR(WDT_vect) {
  time++; // increase each 16 ms
  if (nrot) { // random seed generation
    nrot--;
    seed = (seed << 1) ^ TCNT0;
  }
}

int main() {
  PORTB = 0b00011101; // enable pull-up resistors on 4 game buttons

  eeprom_read_block((void*)&data,0,sizeof(data));

//
//  ADCSRA |= (1 << ADEN); // enable ADC
//  ADCSRA |= (1 << ADSC); // start the conversion on unconnected ADC0 (ADMUX = 0b00000000 by default)
//  // ADCSRA = (1 << ADEN) | (1 << ADSC); // enable ADC and start the conversion on unconnected ADC0 (ADMUX = 0b00000000 by default)
//  while (ADCSRA & (1 << ADSC)); // ADSC is cleared when the conversion finishes
//  seed = ADCL; // set seed to lower ADC byte
//  ADCSRA = 0b00000000; // turn off ADC


  WDTCR = (1 << WDTIE); // start watchdog timer with 16ms prescaller (interrupt mode)
  sei(); // global interrupt enable
  TCCR0B = (1 << CS00); // Timer0 in normal mode (no prescaler)
  
  while (nrot); // repeat for fist 8 WDT interrupts to shuffle the seed
//  eeprom_write_byte((uint8_t*) 3, (seed >> 8)); // write high byte of seed
//  eeprom_write_byte((uint8_t*) 4, (seed & 0b11111111)); // write low byte of seed
  

  TCCR0A = (1 << COM0B1) | (0 << COM0B0) | (0 << WGM01)  | (1 << WGM00); // set Timer0 to phase correct PWM


//  maxLvl = ~eeprom_read_byte((uint8_t*) 0); // read best score from eeprom

  switch (PINB & 0b00011101) {
    case 0b00010101: // red button is pressed during reset
//      eeprom_write_byte((uint8_t*) 0, 255); // reset best score
      data.hsLevel = 0;
      break;
    case 0b00001101: // green button is pressed during reset
      lvl = 255; // play random tones in an infinite loop
      break;
    case 0b00011001: // orange button is pressed during reset
      lvl = data.hsLevel; // start from max level and load seed from eeprom (no break here)
    case 0b00011100: // yellow button is pressed during reset
      seed = data.hsSeed;
//      seed = (((uint16_t) eeprom_read_byte((uint8_t*) 1)) << 8) | eeprom_read_byte((uint8_t*) 2);  // load seed from eeprom but start from first level
      break;
  }



  while(1) {
    ctx = seed;
    for (uint8_t cnt = 0; cnt <= lvl; cnt++) { // never ends if lvl == 255
      _delay_loop_2(4400 + 489088 / (8 + lvl));
      play(simple_random4());
    }
    time = 0;
    lastKey = 5;
    ctx = seed;    
    for (uint8_t cnt = 0; cnt <= lvl; cnt++) {
      bool pressed = false;
      while (!pressed) {
        for (uint8_t i = 0; i < 4; i++) {
          if (!(PINB & buttons[i] & 0b00011101)) {
            if (time > 1 || i != lastKey) {
              play(i);
              pressed = true;
              uint8_t correct = simple_random4();
              if (i != correct) {
                for (uint8_t j = 0; j < 3; j++) {
                  _delay_loop_2(10000);
                  play(correct, 20000);
                }
                _delay_loop_2(65536);
                gameOver();
              }
              time = 0;
              lastKey = i;
              break;
            }
            time = 0;
          }
        }
        if (time > 4000) {
          gameOver();
        }
      } // while(!pressed)
    } // for cnt <= lvl
    _delay_loop_2(65536);
    
    if (lvl < 254) {
      lvl++;
      levelUp(); // animation for completed level
      _delay_loop_2(45000);
    }
    else { // special animation for highest allowable (255th) level
      levelUp();
      gameOver(); // then turn off
    }
  } // while(1)
  

  sleepNow();
}
