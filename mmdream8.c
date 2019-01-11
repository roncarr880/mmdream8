

/*
   Mini Mizer Dream Receiver inspired radio, plus tx
   AD9850 DDS daughter board from the Pic-El as vfo
   Pic 16F88 controller
   Nokia display
   Diode bridge TR switch
   JFET balanced 1st mixer
   1.8432 IF frequency, single crystal filter
   MC1350  IF amp
   2 Diode balanced 2nd mixer
   LM386 audio

   The dds word calculation uses pre-calculated dds values for each decade and they
   are added to find the total dds word.  From there any offsets needed are added or subtracted.
   The pre-calculated values are stored as program literals.  A little more code, less ram/eeprom needed.

*/

#asm

     LIST      p=16F88              ; list directive to define processor
     #INCLUDE <P16F88.INC>          ; processor specific variable definitions


     __CONFIG    _CONFIG1, _CP_OFF & _CCP1_RB0 & _DEBUG_OFF & _WRT_PROTECT_OFF & _CPD_OFF & _LVP_OFF & _BODEN_ON & _MCLR_ON & _PWRTE_ON & _WDT_OFF & _INTRC_IO
     __CONFIG    _CONFIG2, _IESO_OFF & _FCMEN_OFF

   ;  !!! config changes needed, _mclr_off
 
; Available Data Memory divided into Bank 0 through Bank 3.  Each Bank contains
; Special Function Registers and General Purpose Registers at the locations 
; below:  
;
;           SFR           GPR               SHARED GPR's
; Bank 0    0x00-0x1F     0x20-0x6F         0x70-0x7F    
; Bank 1    0x80-0x9F     0xA0-0xEF         0xF0-0xFF  
; Bank 2    0x100-0x10F   0x110-0x16F       0x170-0x17F
; Bank 3    0x180-0x18F   0x190-0x1EF       0x1F0-0x1FF
;
;------------------------------------------------------------------------------


#endasm

#define LSB 0
#define USB 1
  /* lcd command bit masks on portb */
#define LCD_CS  0x08
#define LCD_DC  0x20
  /* some bits to pass to the dds calculation */
  /* if NEW is true, then need to do all the calculations, else load the saved base word */
  /* and add the IF offset for RX or the tx offset for TX */
#define NEW    1
#define TX     2
#define RX     4

#define DDS_LD 0x40

  /* encoder users */
#define FREQ 0
#define VOLUME 1
#define KEY_SPEED 2
  /* timer 1 */
#define T1ON  1
#define T1OFF  0
#define T1LOW   0x18
#define T1HIGH  0xfc
  /* switch states */
#define IDLE 0
#define ARM  1
#define DARM 2
#define DONE 3
#define TAP  4
#define DTAP 5
  /* screen rows ( + 0 to 13 columns in lcd_goto row,col) */
#define ROW0 0
#define ROW1 16
#define ROW2 32
#define ROW3 48
#define ROW4 64
#define ROW5 80

#define CONTRAST 70


/* include the SFR's as C chars and the runtime library */
#include  "p16f88.h"


/* eeprom   ( uses the extern keyword ) */
/* extern char EEPROM[2] = {0xff,0xff};  access entire eeprom as an array, or declare other variables */
extern char numbers[] = {
 0x3E, 0x51, 0x49, 0x45, 0x3E,  
 0x00, 0x42, 0x7F, 0x40, 0x00,
 0x42, 0x61, 0x51, 0x49, 0x46,
 0x21, 0x41, 0x45, 0x4B, 0x31,
 0x18, 0x14, 0x12, 0x7F, 0x10,
 0x27, 0x45, 0x45, 0x45, 0x39,
 0x3C, 0x4A, 0x49, 0x49, 0x30,
 0x01, 0x71, 0x09, 0x05, 0x03,
 0x36, 0x49, 0x49, 0x49, 0x36,
 0x06, 0x49, 0x49, 0x29, 0x1E
};
extern char letters[] = {
 0x7C, 0x12, 0x11, 0x12, 0x7C,
 0x7F, 0x49, 0x49, 0x49, 0x36,
 0x3E, 0x41, 0x41, 0x41, 0x22,
 0x7F, 0x41, 0x41, 0x22, 0x1C,
 0x7F, 0x49, 0x49, 0x49, 0x41,
 0x7F, 0x09, 0x09, 0x09, 0x01,
 0x3E, 0x41, 0x49, 0x49, 0x7A,
 0x7F, 0x08, 0x08, 0x08, 0x7F,
 0x00, 0x41, 0x7F, 0x41, 0x00,
 0x20, 0x40, 0x41, 0x3F, 0x01,
 0x7F, 0x08, 0x14, 0x22, 0x41,
 0x7F, 0x40, 0x40, 0x40, 0x40,
 0x7F, 0x02, 0x0C, 0x02, 0x7F,
 0x7F, 0x04, 0x08, 0x10, 0x7F,
 0x3E, 0x41, 0x41, 0x41, 0x3E,
 0x7F, 0x09, 0x09, 0x09, 0x06,
 0x3E, 0x41, 0x51, 0x21, 0x5E,
 0x7F, 0x09, 0x19, 0x29, 0x46,
 0x46, 0x49, 0x49, 0x49, 0x31,
 0x01, 0x01, 0x7F, 0x01, 0x01,
 0x3F, 0x40, 0x40, 0x40, 0x3F,
 0x1F, 0x20, 0x40, 0x20, 0x1F,
 0x3F, 0x40, 0x38, 0x40, 0x3F,
 0x63, 0x14, 0x08, 0x14, 0x63,
 0x07, 0x08, 0x70, 0x08, 0x07,
 0x61, 0x51, 0x49, 0x45, 0x43
};                             /* 180 locations used, 76 left */


/* access ram  - 16 locations, use wisely */
#pragma pic 112

char w_temp;    
char status_temp;
char pclath_temp;
char _temp;     /* temp location for subtracts and shifts */
char _temp2;    /* for interrupt subs and shifts */
char _eedata;   /* eedata in access ram */
char fsr_temp;
/* 9 more locations for access ram */
char ticks;     /* timer tick in interrupt */



/* bank 0 ram - 80 locations */
/* any arrays need to be in page 0 or page 1 as the IRP bit is not updated */
#pragma pic  32
char s_porta;      /* shadow ports to avoid rmw issues, in same bank as the ports */
char s_portb;      /* but watch out if need to set pins in an interrupt */
char sstate[4];    /* state of each switch */
char sticks;       /* switch timer */
char press;        /* count of switch pressed time */
char nopress;      /* count of switch up time */

/* bank 1 ram - 80 bytes */
/* try using more of bank 1 and see how banksel looks between statics in bank3 */
#pragma pic  160
char freq[7];   /* frequency in sort of bcd form but one byte for each digit, hz ignored */
                /* offset 0 is 10meg, 1 is 1 meg, 2 is 100k, 3 is 10k, 4 is 1k, 5 is 100 hz, 6 is 10 hz */
                /* IF dds word will be added or subtracted to get the dds vfo value */
                /* freq is in a form to directly display to the nokia screen */

               /* dds word before IF or tx offset has been added, saved so don't need to recalc */
char dds_base0,dds_base1,dds_base2,dds_base3;

char acc0,acc1,acc2,acc3;   /* 32 bit working registers */
char arg0,arg1,arg2,arg3;    
char sideband;  
/* 20 used */

char tune_digit;       /* digit we are tuning */


/* bank 2 ram - no arrays - 96 locations -  */
#pragma pic 272
char encoder_user;

/* bank 3 ram - no arrays - 96 locations - function args and statics are also here */
#pragma pic 400

/*  ********
#pragma shortcall   - use for functions that do not call other pages
#pragma longcall    - use for functions that call other pages
                    - do not allow code to pass over the page boundary
********  */

#pragma shortcall

main(){       /* any page */
static char t;

   encoder_user = FREQ;

for(;;){    /* loop main */

   t = encoder(); 
   if( t ){                     
      switch( encoder_user ){
      case FREQ:
         no_interrupts(); 
         if( t == 1 ) inc_freq(tune_digit);   /* recursive, no interrupts because of 8 level stack */
         else dec_freq(tune_digit);
         interrupts();
         vfo_update(NEW+RX);   /* side effect, if sweeping vfo on TX, you return to rx mode */
         display_freq();
      break;
      }
   }  /* end encoder true */

   switches();    /* run the switch state code */
                  /* act on any switch presses */
   tune_digit_ch();
   sideband_change();
   freq_volume();          /* encoder functions */
   rit_key_speed();        /* encoder functions */ 

}
}

tune_digit_ch(){
static char update;

  update = 0;
  if( sstate[3] < TAP ) return;
  if( sstate[3] == TAP ){
     if( ++tune_digit > 6 ) tune_digit = 1;   /* skip 10mhz tuning, 1mhz is quick enough for qsy */
  }
  else{    /* it is a double tap */
     if( --tune_digit == 0 ) tune_digit = 6;
  }

  if( tune_digit == 5 ) freq[6] = 0 , update = 1;       /* 100hz steps, no tens */
  if( tune_digit == 4 ){
     freq[5] = 0;       /* 1 k steps, no 100's or tens */
     freq[6] = 0;
     update = 1;
  }
  if( update ){
     vfo_update(NEW+RX);
     display_freq();
  }

  status_line();
  sstate[3] = DONE;
}

sideband_change(){

   if( sstate[2] < TAP ) return;
   sideband ^= 1;                /* just toggle tap or dtap */
                                 /* consider altering freq to keep same 800hz tone unchanged */
   vfo_update();
   status_line():
   sstate[2] = DONE;
}

freq_volume(){

   if( sstate[1] < TAP ) return;

   sstate[1] = DONE:
}

rit_key_speed(){

   if( sstate[0] < TAP ) return;

   sstate[0] = DONE;
}

lcd_putch(char val){    /* write uppercase letters to display */
static char i;


   if( val >= '0' && val <= '9' ){    /* call different function if its a number */
      lcd_putchn( val );
      return;
   }   
   if( val < 'A' || val > 'Z' ) return;
   val -= 'A';
   i = 0;
   while( val-- ) i += 5;   /* get index to start of the letter */
   lcd_data(0);
   for( val = 0; val < 5; ++val ){
     lcd_data( letters[i++] );
   }
}


lcd_putchn( char val ){
static char i;

   val -= '0';
   i = 0;
   while( val-- ) i += 5;   /* get index to start of the font */
   lcd_data(0);
   for( val = 0; val < 5; ++val ){
     lcd_data( numbers[i++] );
   }

}

display_freq(){   /* top row, 5 big numbers and 3 small */
static char i,c;
static char j,k;

   lcd_goto(ROW0);   /* print the top part */
   for( i = 0; i < 5; ++i ){  /* each decade */
      c = freq[i];
      j = 0;
      while(c--) j += 5;   /* index into the numbers fonts */
      lcd_data(0);         /* zero space column left out of the fonts to save eeprom space */
      lcd_data(0);
      for( k = 0; k < 5; ++k ){   /* 5 real columns of the 6 column font */
         c = numbers[j++];
         c = top_font(c);
         if( i == 0 && j < 6 ){   /* blank leading zero */
            lcd_data(0);
            lcd_data(0);
         }
         else{
            lcd_data(c);
            lcd_data(c);
         }
      }
   }

   lcd_goto(ROW1);   /* print the bottom part, lcd row 1 */
   for( i = 0; i < 5; ++i ){  /* each decade */
      c = freq[i];
      j = 0;
      while(c--) j += 5;   /* index into the numbers fonts */
      lcd_data(0);
      lcd_data(0);
      for( k = 0; k < 5; ++k ){
         c = numbers[j++];
         c = bot_font(c);
         if( i == 0 && j < 6 ){   /* blank leading zero */
            lcd_data(0);
            lcd_data(0);
         }
         else{
            lcd_data(c);
            lcd_data(c);
         }
      }
   }

   /* print last 3 digits in regular size, last digit is always a zero */
   lcd_goto(11);
   for( i = 5; i < 7; ++i ){
      c = freq[i];
      j = 0;
      while(c--) j += 5;
      for( k = 0; k < 5; ++k ){
         lcd_data( numbers[j++] );
      }
      lcd_data(0);   /* move characters back one column by printing space at the end */
   }

  /* lcd_data(0); */
   for( k = 0; k < 5; ++k ) lcd_data( numbers[k] );   /* print last digit as a zero */ 
  

}


char top_font( char c ){   /* double the font size, two vertical bits for one */
static char r;

   /* the top part is the lower 4 bits */
    r = 0;
    if( c & 1 ) r+= 3;
    if( c & 2 ) r+= 0xc;
    if( c & 4 ) r+= 0x30;
    if( c & 8 ) r+= 0xc0;
    return r;
}

char bot_font( char c ){
static char r;

   /* the bottom part is the upper 4 bits */
    r = 0;
    if( c & 16 ) r+= 3;
    if( c & 32 ) r+= 0xc;
    if( c & 64 ) r+= 0x30;
    if( c & 128 ) r+= 0xc0;
    return r;
}


status_line(){   /* print on line 3 */
static char i;
static char j;
static char f;

   lcd_goto(ROW2);
   for( i = 0; i < 5; ++i ){
      f = ( i == tune_digit ) ? 0x83:0x80;
      lcd_data(0x80);
      lcd_data(0x80);
      for( j = 2; j < 12; ++j ) lcd_data( f );
   }
   for( j = 0; j < 3; ++j ) lcd_data(0);
   if( sideband == USB ) lcd_putch( 'U' );
   else lcd_putch( 'L' );
   lcd_putch( 'S' );
   lcd_putch( 'B' );
}


   #asm
   ; removing the extra C overhead to speed this function up

spi_send

   banksel SSPBUF
   movwf   SSPBUF
   nop            ; 4us delay
   nop
   nop
   nop
   return         ; 2 us delay and caller will suffer a banksel before data strobe or releasing CS
                  ; so fastest would be a 10us delay with 2us safety margin
   #endasm

lcd_command( char data ){

   /* d/c low cs low, call spi, d/c and cs high */
   PORTB = s_portb = s_portb & ( 0xff ^ ( LCD_DC + LCD_CS));
   spi_send( data );
   PORTB = s_portb = s_portb | (LCD_DC + LCD_CS) );
}

lcd_data( char data ){

    /* cs low, spi, cs high */
    PORTB = s_portb = s_portb & ( 0xff ^ LCD_CS );
    spi_send( data );
    PORTB = s_portb = s_portb | LCD_CS;    /* set up for releasing CS */
}

lcd_goto( char row_col ){   /* 6 rows, 14 columns in nibbles */
static char row;
static char col;

   row = row_col >> 4;
   if( row >= 6 ) row = 5;
   lcd_command( 0x40 + row );

   row = row_col & 15;        /* get a count */
   col = 0;
   while( row-- ) col += 6;   /* character based adressing instead of graphic */
   lcd_command( 0x80 + col );

}

clock_dds(){
   /* data to load is in the accumulator */

   spi_send(bitrev(acc0));    /* lsb bytes and bits first */
   spi_send(bitrev(acc1));
   spi_send(bitrev(acc2));
   spi_send(bitrev(acc3));
   #asm
     nop
     nop
     nop
   #endasm
   spi_send(0);

   /* load pin is toggled elsewhere */
}


char bitrev( char data ){    /* reverse the bits in a byte */

  #asm
    clrw
    btfsc  _bitrev,7
    iorlw  0x01
    btfsc  _bitrev,6
    iorlw  0x02
    btfsc  _bitrev,5
    iorlw  0x04
    btfsc  _bitrev,4
    iorlw  0x08
    btfsc  _bitrev,3
    iorlw  0x10
    btfsc  _bitrev,2
    iorlw  0x20
    btfsc  _bitrev,1
    iorlw  0x40
    btfsc  _bitrev,0
    iorlw  0x80
    movwf  _bitrev
  #endasm

    return data;
}

char dadd(){    /* 32 bit add */

#asm 
     banksel arg0
     movf    arg0,W
     addwf   acc0,F
     movlw   1
     btfsc   STATUS,C
     addwf   acc1,F
     btfsc   STATUS,C
     addwf   acc2,F
     btfsc   STATUS,C
     addwf   acc3,F


     movf    arg1,W
     addwf   acc1,F
     movlw   1
     btfsc   STATUS,C
     addwf   acc2,F
     btfsc   STATUS,C
     addwf   acc3,F

     movf    arg2,W
     addwf   acc2,F
     movlw   1
     btfsc   STATUS,C
     addwf   acc3,F

     movf    arg3,W
     addwf   acc2,F
     
   #endasm

}

char dsub(){    /* 32 bit sub */

   #asm

     banksel arg0
     movf    arg0,W
     subwf   acc0,F     ; borrow is carry 0
     movlw   1
     btfss   STATUS,C   ; propagate borrow to the 3 upper bytes
     subwf   acc1,F
     btfss   STATUS,C
     subwf   acc2,F
     btfss   STATUS,C
     subwf   acc3,F

     movf    arg1,W
     subwf   acc1,F
     movlw   1
     btfss   STATUS,C
     subwf   acc2,F
     btfss   STATUS,C
     subwf   acc3,F

     movf    arg2,W
     subwf   acc2,F
     movlw   1
     btfss   STATUS,C
     subwf   acc3,F

     movf    arg3,W
     subwf   acc2,F
     
   #endasm

}



/*   freq control words by decade for 100mhz clock */
/*
extern char M10[4]= {0x19,0x99,0x99,0x9a};  
extern char M1[4]= {0x02,0x8f,0x5c,0x29};   
extern char K100[4]= {0x0,0x41,0x89,0x37};
extern char K10[4]= {0x0,0x06,0x8d,0xb9};
extern char K1[4]= {0x0,0x0,0xa7,0xc6};
extern char H100[4]= {0x0,0x0,0x10,0xc7};
extern char H10[4]= {0x0,0x0,0x01,0xad};
*/

/*  functions to calculate the dds word to load */
add_10M(char count){       /* dds words from program literals, add to accumulator */

   /* load the arg register with magic numbers for 10 meg */
   arg0 = 0x9a;  arg1 = 0x99;  arg2 = 0x99; arg3 = 0x19;
   while(count--) dadd();

}

add_1M(char count){       /* dds words from program literals, add to accumulator */

   arg3 = 0x02;  arg2 = 0x8f;  arg1 = 0x5c;  arg0 = 0x29;
   while(count--) dadd();
}

add_100K(char count){       /* dds words from program literals, add to accumulator */

   arg3 = 0x00;  arg2 = 0x41;  arg1 = 0x89;  arg0 = 0x37;
   while(count--) dadd();

}
add_10K(char count){       /* dds words from program literals, add to accumulator */

   arg3 = 0x00;  arg2 = 0x06;  arg1 = 0x8d;  arg0 = 0xb9;
   while(count--) dadd();

}
add_1K(char count){       /* dds words from program literals, add to accumulator */

   arg3 = 0x00;  arg2 = 0x00;  arg1 = 0xa7;  arg0 = 0xc6;
   while(count--) dadd();

}
add_100(char count){       /* dds words from program literals, add to accumulator */

   arg3 = 0x00;  arg2 = 0x00;  arg1 = 0x10;  arg0 = 0xc7;
   while(count--) dadd();

}
add_10(char count){       /* dds words from program literals, add to accumulator */

   arg3 = 0x00;  arg2 = 0x00;  arg1 = 0x01;  arg0 = 0xad;
   while(count--) dadd();

}

add_sub_IF(){        /* add or sub the dds IF depending upon sideband, for rx */
                     /* sideband heard will depend upon if bfo is high or low side */
                     /* if incorrect swap if statements to USB ???, we will figure it out */
                     /* looks like was usb for high side in previous radio */

   arg3 = 0x04;  arg2 = 0xb7;  arg1 = 0xf5; arg0 = 0xa5;
   if( sideband == USB ) dadd();
   else dsub();

}

add_sub_offset(){   /* add or sub 800hz tx offset depending upon sideband, for tx */

    arg3 = 0x00; arg2 = 0x00;  arg1 = 0x86;  arg0 = 0x38;
    if( sideband == USB ) dadd():   /* tx lower or higher */
    else dsub();
}

calc_dds_base(){   /* add everything up and save it when frequency changes */

   zacc();
   add_10M( freq[0] );
   add_1M( freq[1] );
   add_100K( freq[2] );
   add_10K( freq[3] );
   add_1K( freq[4] );
   add_100( freq[5] );
   add_10(freq[6] );

   dds_base0 = acc0;
   dds_base1 = acc1;
   dds_base2 = acc2;
   dds_base3 = acc3;
}

vfo_update( char function ){    /* functions NEW, RX, TX */

   if( function & NEW ) calc_dds_base();
   else{
      acc0 = dds_base0;
      acc1 = dds_base1;
      acc2 = dds_base2;
      acc3 = dds_base3;
   }
   if( (function & RX) ){
      if( s_porta & DDS_LD ){    /* we are transmitting, so turn it off */
         PORTA = s_porta = s_porta & ( 0xff ^ DDS_LD );
         delay(10);             /* let tx decay on freq, how long should this be? 10ms */
      }         
      add_sub_IF();
   }
   if( function & TX ) add_sub_offset();   /* 800 hz */

   clock_dds(); 
  
   /* set the dds load strobe */
   PORTA = s_porta = s_porta | DDS_LD;

   if( function & RX ){
    /* clear the strobe, left high on TX as it also keys the transmitter */
      PORTA = s_porta = s_porta & ( 0xff ^ DDS_LD );
   }
}


zacc(){     /* zero the accumulator */
   
   acc3 = acc2 = acc1 = acc0 = 0;

}

inc_freq( char i ){    /* increment the frequency array by step size, try recursion for carry */
                       /* only 8 stack levels, must be called from main with ints disabled */
   if( i > 6 ) return;
   freq[i] += 1;
   if( freq[i] == 10 ){
      freq[i] = 0;
      inc_freq( i-1 );
   }
}

dec_freq( char i ){    /* must be called from main with ints disabled */

   if( i > 6 ) return;
   freq[i] -= 1;
   if( freq[i] == 255 ){
      freq[i] = 9;
      dec_freq(i-1);
   }
}

delay( char t ){        /* delay something close to 1ms per count */
static char time_sink;

   do{
      time_sink = 198;
      do{ } while( --time_sink ); 
   } while( --t );
}

interrupts(){

   #asm
      bsf   INTCON,GIE   
   #endasm
}

no_interrupts(){

   #asm
      bcf   INTCON,GIE
      btfsc INTCON,GIE    ;see AN576
      goto $-2
   #endasm

}

#pragma longcall

/* 
   can use long call stub functions here to call page 1 from page 0
   without the need to always use long calls 
   example(){
     lc_example();    call into another code page
   }
*/

char encoder(){
   return ( lc_encoder() );
}

switches(){         /* use some page 2 program memory */
   lc_switches();
}

/* end of page 0 program section */


#asm
           org  0x800
#endasm

/********************  start of page 1 code section *************************/

/*
#pragma shortcall   can use shortcalls in this page if the functions do not call over page boundary
#pragma longcall    or specify longcall
*/

#pragma longcall

/*
   lc_example(){
      code here, called from page 0
   }
*/

init(){       /* any page, called once from reset */
static char i;

/* enable clock at 4 or 8 meg */
   OSCCON= 0x60;   /* 60 is 4 meg internal clock or 70 for 8 meg */
   EECON1 = 0;     /* does the free bit sometime power up as 1 */


/* init variables */
   freq[0] = 1;
   freq[1] = 0;
   freq[2] = 1;
   freq[3] = 1;
   freq[4] = 6;
   freq[5] = 0;
   freq[6] = 0;
   tune_digit = 1;        /* tuning mhz digit for quick qsy */
   sideband = USB;
   for( i = 0; i < 4; ++i ) sstate[i] = 0;
   sticks = ticks;
   press = nopress = 0;

/* init pins */
   /* reset LCD before setting up SPI */
   PORTB = 0xfc;                /* 0b11111100 */
   TRISB = 0xc0;                /* 0b11000000 reset low, bit 1 */
   delay(10);
   s_portb = PORTB = 0xfe;        /* 0b11111110; */
   TRISB = 0xc2;                /* 0b11000010; reset pin will become unused SDI input ( with pullup resistor ) */
                                /*  sw1,sw2,LCD d/c,spi_clk, LCD CS,spi_data,LCD_reset/spi_data_in,PWM_agc */
                                /*  sw3 is a diode OR of sw1 and sw2. it looks like both sw1 and sw2 were pressed */
   
   ANSEL = 0x04;     /* one analog pin RA2 */
   PORTA = 0;
   TRISA = 0x3f;     /* 0011 1111 */
                     /* sidetone, tx-ld strobe,sw4,dit, dah ,audio,encoder b,encoder a */

/* init DDS, clock and load to get into serial mode, twice just in case */

   PORTB = 0xfe - 0x10;     /* clock low */
   PORTB = 0xfe;         /* clock high */
   PORTA = DDS_LD;            /* load pulse */
   PORTA = 0;        
   PORTB = 0xfe - 0x10;     /* clock low */
   PORTB = 0xfe;         /* clock high */
   PORTA = DDS_LD;            /* load pulse */
   PORTA = 0;        

   PORTB = s_portb = 0xfe;
   PORTA = s_porta = 0;

/* set up SPI */
   SSPSTAT = 0x00;      /* default is zero, different clock to data timing with CKE 0x40 set? */
   SSPCON  = 0x30;      /* need clock idle high for correct data timing to clock rising edge */

/* init LCD */
   lcd_init(); 

/* init PWM , uses timer2*/

/* load the default DDS words */
   vfo_update(NEW+RX);
   display_freq();
   status_line();

/* start timer1 interrupts */
/* start timer */
   TMR1H = T1HIGH;     /* with 4 meg clock, interrupt at 1ms rate */
   TMR1L = T1LOW;
   T1CON = T1ON;

/*  enable interrupts */
   PIE1 = 1;       /* timer 1 interrupt */
   INTCON = 0xc0; 

   lc_encoder();    /* init some of the static vars in encoder read */

}



/* any page. Save _temp if using any calls to other functions */
_interrupt(){  /* status has been saved */

   T1CON= T1OFF:         /* 1 ms timer rate */
   TMR1H = T1HIGH;  
   TMR1L += (T1LOW + 6 + 2);   /*  adjust for timer off time */
   T1CON= T1ON;  
   ++ticks;              /* count milliseconds */


/* restore status */

#asm
       banksel    PIR1                       ; clear timer 1 interrupt flag
       bcf        PIR1,TMR1IF

        movf            fsr_temp,w
        movwf           FSR
	movf		pclath_temp,w		; retrieve copy of PCLATH register
	movwf		PCLATH			; restore pre-isr PCLATH register contents	
	movf		status_temp,w		; retrieve copy of STATUS register
	movwf		STATUS			; restore pre-isr STATUS register contents
	swapf		w_temp,f
	swapf		w_temp,w		; restore pre-isr W register contents
#endasm
}


lcd_init( ){
static char i;
   
   lcd_command( 0x21 );              /* extended mode */
   lcd_command( 0x80 + CONTRAST );   /* v op */
   lcd_command( 6 );                 /* temp comp */
   lcd_command( 0x13 );              /* bias */
   lcd_command( 0x20 );              /* out of extended mode */
   lcd_goto( 0 );

   for( i = 0; i < 200; ++i ) lcd_data( 0 );   /* clear ram 504 locations */
   for( i = 0; i < 200; ++i ) lcd_data( 0 );
   for( i = 0; i < 104; ++i ) lcd_data( 0 );

   lcd_command( 0x8 + 0x4 );         /* display on normal mode */

}


char lc_encoder(){   /* read encoder, return 1, 0, or -1( 255 ) */
  
static char mod;     /* encoder is divided by 4 because it has detents */
static char dir;     /* need same direction as last time, effective debounce */
static char last;    /* save the previous reading */
static char new;     /* this reading */
static char b;

   new = PORTA & 3;  
   if( new == last ) return 0;       /* no change */

   b = ( (last << 1) ^ new ) & 2 );  /* direction 2 or 0 from xor of last shifted and new data */
   last = new;
   if( b != dir ){
      dir = b;
      return 0;      /* require two clicks in the same direction */
   }
   mod = (mod + 1) & 3;      /* divide clicks by 4 */
   if( mod != 3 ) return 0;

   if( dir == 2 ) return 255;   /* swap return values if it works backwards */
   else return 1;
}


lc_switches(){       /* run the switch state machine */
static char i,j;
static char sw;
static char s;

   if( sticks == ticks ) return;   /* not time yet */
   ++sticks;
   
   /* get the switch readings, low active but invert bits */
   sw = ((PORTB & 0xc0) >> 4) ^ 0x0c;                 
   if( sw == 0x0c ) sw = 2;     /* virtual switch 3, via diodes OR */
   /* !!! if( (PORTA & 0x20) == 0 ) sw |= 1; */  /* last switch is on mclr pin */

   if( sw ) ++press, nopress = 0;       /* only acting on one switch at a time */
   else ++nopress, press = 0;           /* so these simple vars work for all of them */

   /* run the state machine for the 4 switches in a loop */
   for( i = 0, j = 1; i < 4; ++i ){
      s = sstate[i];
      switch(s){
         case DONE:  if( nopress >= 100 ) s = IDLE;  break;
         case IDLE:  if( ( j & sw ) && press >= 30 ) s = ARM;  break; /* pressed */
         case ARM:   if( nopress >= 30 ) s = DARM; break;             /* it will be a tap or double tap */
         case DARM:
            if( nopress >= 240 ) s = TAP;
            if( press >= 30 )    s = DTAP;
         break;
      }      
      sstate[i] = s; 
      j <<= 1;
   }

}
