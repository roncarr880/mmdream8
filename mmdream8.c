

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
   The pre-calculated values are stored as program literals.  A little more code, less ram needed.

*/

#asm

     LIST      p=16F88              ; list directive to define processor
     #INCLUDE <P16F88.INC>          ; processor specific variable definitions


     __CONFIG    _CONFIG1, _CP_OFF & _CCP1_RB0 & _DEBUG_OFF & _WRT_PROTECT_OFF & _CPD_OFF & _LVP_OFF & _BODEN_OFF & _MCLR_ON & _PWRTE_ON & _WDT_OFF & _INTRC_IO
     __CONFIG    _CONFIG2, _IESO_OFF & _FCMEN_OFF


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

/* include the SFR's as C chars and the runtime library */
#include  "p16f88.h"


/* eeprom   ( uses the extern keyword ) */
/* extern char EEPROM[2] = {0xff,0xff};  access entire eeprom as an array, or declare other variables */
extern char fonts[5] = {0x46, 0x49, 0x49, 0x49, 0x31};  /*letter S, row of zeros removed */
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



/* bank 0 ram - 80 locations */
/* any arrays need to be in page 0 or page 1 as the IRP bit is not updated */
#pragma pic  32


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


/* bank 2 ram - no arrays - 96 locations -  */
#pragma pic 272
char tune;       /* digit we are tuning */
char porta;      /* shadow ports to avoid rmw issues */
char portb;      /* but watch out if need to set pins in an interrupt */

/* bank 3 ram - no arrays - 96 locations - function args and statics are also here */
#pragma pic 400

/*  ********
#pragma shortcall   - use for functions that do not call other pages
#pragma longcall    - use for functions that call other pages
                    - do not allow code to pass over the page boundary
********  */

#pragma shortcall

main(){       /* any page */

  /* just test the lcd */
  lcd_goto( 0 );
  test_font();
  lcd_goto( 1*16 + 2 );
  test_font(); test_font();
  lcd_goto( 2*16 + 4 );
  test_font(); test_font(); test_font();
  lcd_goto( 3*16 + 6 );
  test_font(); test_font(); test_font(); test_font();
  lcd_goto( 4*16 + 8 );
  test_font(); test_font(); test_font(); test_font(); test_font();
  lcd_goto( 5*16 + 10 );
  test_font(); test_font(); test_font(); test_font(); test_font();

for(;;){    /* loop main */



}
}

test_font(){    /* just a test function */
static char i;

   lcd_data(0);  /* letter spacing */
   for( i = 0; i < 5; ++i ) lcd_data( fonts[i] ); 

}

display_freq(){   /* top row, 5 big numbers and 3 small */
static char i,c;
static char j,k;

   lcd_goto(0);   /* print the top part */
   for( i = 0; i < 5; ++i ){  /* each decade */
      c = freq[i];
      j = 0;
      while(c--) j += 5;   /* index into the numbers fonts */
      lcd_data(0);  lcd_data(0);
      for( k = 0; k < 5; ++k ){
         c = numbers[j++];
         c = top_font(c);
         lcd_data(c); lcd_data(c);
      }
   }

   lcd_goto(16);   /* print the bottom part, lcd row 1 */
   for( i = 0; i < 5; ++i ){  /* each decade */
      c = freq[i];
      j = 0;
      while(c--) j += 5;   /* index into the numbers fonts */
      lcd_data(0);  lcd_data(0);
      for( k = 0; k < 5; ++k ){
         c = numbers[j++];
         c = bot_font(c);
         lcd_data(c); lcd_data(c);
      }
   }

   /* print last 3 digits in regular size, last digit is always a zero */
   lcd_goto(11);
   for( i = 5; i < 7; ++i ){
      c = freq[i];
      j = 0;
      while(c--) j += 5;
      lcd_data(0);
      for( k = 0; k < 5; ++k ){
         lcd_data( numbers[j++] );
      }
   }

   lcd_data(0);
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



spi_send( char data ){   /* waiting for done at end so can keep cs and other control signals active */

   #asm
     banksel PIR1
     bcf  PIR1,SSPIF      ; read only bit but must be cleared in software ???
   #endasm

   SSPBUF = data;

   #asm
spi_send2
     banksel PIR1
     btfss PIR1,SSPIF
     goto spi_send2
   #endasm
}

lcd_command( char data ){

   /* d/c low, call lcd_data, d/c high */
   portb = portb & ( 0xff ^ LCD_DC );
   PORTB = PORTB;
   lcd_data( data );
   portb = portb | LCD_DC;
   PORTB = portb;
}

lcd_data( char data ){

    /* cs low, spi, cs high */
    portb = portb & ( 0xff ^ LCD_CS );
    PORTB = portb;
    spi_send( data );
    portb = portb | LCD_CS;
    PORTB = portb;
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
  ;   bcf     STATUS,C   not needed as the add will set or clear the bit
     movf    arg0,W
     addwf   acc0,F
     clrw
     btfsc   STATUS,C   ; propagate carry to the 3 upper bytes
     movlw    1
     addwf   acc1,F
     clrw
     btfsc   STATUS,C
     movlw   1
     addwf   acc2,F
     clrw
     btfsc   STATUS,C
     movlw   1
     addwf   acc3,F

  ;   bcf     STATUS,C
     movf    arg1,W
     addwf   acc1,F
     clrw
     btfsc   STATUS,C
     movlw   1
     addwf   acc2,F
     clrw
     btfsc   STATUS,C
     movlw   1
     addwf   acc3,F

  ;   bcf     STATUS,C
     movf    arg2,W
     addwf   acc2,F
     clrw
     btfsc   STATUS,C
     movlw   1
     addwf   acc3,F

  ;   bcf     STATUS,C
     movf    arg3,W
     addwf   acc2,F
     
   #endasm

}

char dsub(){    /* 32 bit sub */

   #asm
   ;  bcf     STATUS,C  must be holdover from 6502 coding
     movf    arg0,W
     subwf   acc0,F     ; borrow is carry 0
     clrw
     btfss   STATUS,C   ; propagate borrow to the 3 upper bytes
     movlw    1
     subwf   acc1,F
     clrw
     btfss   STATUS,C
     movlw   1
     subwf   acc2,F
     clrw
     btfss   STATUS,C
     movlw   1
     subwf   acc3,F

  ;   bcf     STATUS,C   ; 
     movf    arg1,W
     subwf   acc1,F
     clrw
     btfss   STATUS,C
     movlw   1
     subwf   acc2,F
     clrw
     btfss   STATUS,C
     movlw   1
     subwf   acc3,F

  ;   bcf     STATUS,C
     movf    arg2,W
     subwf   acc2,F
     clrw
     btfss   STATUS,C
     movlw   1
     subwf   acc3,F

  ;   bcf     STATUS,C
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

zacc(){     /* zero the accumulator */
   
   acc3 = acc2 = acc1 = acc0 = 0;

}

inc_freq( char i ){    /* increment the frequency array by step size, try recursion for carry */
                       /* only 8 stack levels, must be called from main with ints disabled */
   freq[i] += 1;
   if( freq[i] == 10 && i != 0 ){
      freq[i] = 0;
      inc_freq( i-1 );
   }
}

dec_freq( char i ){    /* must be called from main with ints disabled */

   freq[i] -= 1;
   if( freq[i] == 255 && i != 0 ){
      freq[i] = 9;
      dec_freq(i-1);
   }
}

#pragma longcall

/* 
   can use long call stub functions here to call page 1 from page 0
   without the need to always use long calls 
   example(){
     lc_example();    call into another code page
   }
*/

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

/* enable clock at 8 meg */
   OSCCON= 0x72;        /* 8 meg internal clock or 70 */


/* init variables */
   freq[0] = 1;
   freq[1] = 0;    /* somewhere in 10 mhz */
   freq[2] = 1;
   tune = 0;       /* tuning mhz digit */

/* init pins */
   portb = PORTB = 0xea;        /* 0b11101010; */
   TRISB = 0xc2;                /* 0b11000010; */

/* init DDS, clock and load to get into serial mode */

/* set up SPI */
   SSPSTAT = 0;      /* default is zero */
   SSPCON  = 0x20;   /* enable spi master mode at 2 meg clocks, or 1 meg if at 4 mhz clock */

/* init LCD */
   lcd_init( 65 );

/* init PWM */


}



/* any page. Save _temp if using any calls to other functions */
_interrupt(){  /* status has been saved */


/* restore status */
#asm
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


lcd_init( char contrast ){
static char i;
    /* reset pulse from  ? */
   
   lcd_command( 0x21 );              /* extended mode */
   lcd_command( 0x80 + contrast );   /* v op */
   lcd_command( 6 );                 /* temp comp */
   lcd_command( 0x13 );              /* bias */
   lcd_command( 0x20 );              /* out of extended mode */
   lcd_goto( 0 );

   for( i = 0; i < 200; ++i ) lcd_data( 0 );   /* clear ram 504 locations */
   for( i = 0; i < 200; ++i ) lcd_data( 0 );
   for( i = 0; i < 104; ++i ) lcd_data( 0 );

   lcd_command( 0x8 + 0x4 );         /* display normal */

}
