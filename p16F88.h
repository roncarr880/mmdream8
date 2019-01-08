
#asm

        LIST
               radix  decimal
               errorlevel -302
               errorlevel -306

;**********************************************************************
	ORG		0x000			; processor reset vector
        pagesel         init
        call            init
        pagesel         main
  	goto		main			; go to beginning of program


	ORG		0x004			; interrupt vector location
	movwf		w_temp			; save off current W register contents
	movf		STATUS,w		; move status register into W register
	movwf		status_temp		; save off contents of STATUS register
	movf		PCLATH,w		; move pclath register into W register
	movwf		pclath_temp		; save off contents of PCLATH register
        movf            FSR,w
        movwf           fsr_temp

        pagesel _interrupt
        goto    _interrupt

;   restore code is in _interrupt routine


;   runtime code
; locate runtime at top of each page
; and restore org to page 0


          org 0x7eA

 ; -------------------- run time library  -------------------- */

        ; shifting, shift _temp with count in W
_rshift
        btfsc   STATUS,Z    
        return             ; ret if shift amount is zero
        bcf     STATUS,C   ; unsigned shift, mask bits
        rrf     _temp,F
        addlw   255        ; sub one
        goto    _rshift

_lshift
        btfsc   STATUS,Z    
        return             ; ret if shift amount is zero
        bcf     STATUS,C   ; arithmetic shift
        rlf     _temp,F
        addlw   255        ; sub one
        goto    _lshift

_eeadr                     ; set up EEDATA to contain desired data
        banksel EEADR
        movwf   EEADR      ; correct address in W
        bsf     STATUS,RP0
        bsf     EECON1,RD
        bcf     STATUS,RP0
        movf    EEDATA,W
        movwf   _eedata    ; in access ram
        return


          org 0xfeA
; shadow runtime in page1
        ; shifting, shift _temp with count in W
_rshift2
        btfsc   STATUS,Z    
        return             ; ret if shift amount is zero
        bcf     STATUS,C   ; unsigned shift, mask bits
        rrf     _temp,F
        addlw   255        ; sub one
        goto    _rshift

_lshift2
        btfsc   STATUS,Z    
        return             ; ret if shift amount is zero
        bcf     STATUS,C   ; arithmetic shift
        rlf     _temp,F
        addlw   255        ; sub one
        goto    _lshift

_eeadr2                     ; set up EEDATA to contain desired data
        banksel EEADR
        movwf   EEADR      ; correct address in W
        bsf     STATUS,RP0
        bsf     EECON1,RD
        bcf     STATUS,RP0
        movf    EEDATA,W
        movwf   _eedata    ; in access ram
        return

; signed arithmetic shift macros
ASR     MACRO   var1
        errorlevel +302    ; banksel elsewhere for some speed
        bcf     STATUS,C
        btfsc   var1,7     ; check sign
        bsf     STATUS,C
        rrf     var1,F
        errorlevel -302
        ENDM

ASRD    MACRO   varh,varl  ; Shift 16 bit
        errorlevel +302    ; banksel elsewhere for some speed
        bcf     STATUS,C
        btfsc   varh,7     ; check sign
        bsf     STATUS,C
        rrf     varh,F
        rrf     varl,F
        errorlevel -302
        ENDM

        

          org 14           ; rest of code starts in page 0
         

#endasm

/* declare registers for C and define device */

#pragma pic 0
/*;-----Bank0------------------*/
char INDF  ; /*           EQU  H'0000'    */
char TMR0   ; /*             EQU  H'0001'    */
char PCL     ; /*            EQU  H'0002'    */
char STATUS ; /*             EQU  H'0003'    */
char FSR   ; /*              EQU  H'0004'    */
char PORTA ; /*              EQU  H'0005'    */
char PORTB ; /*              EQU  H'0006'    */
#pragma pic 10
char PCLATH ; /*             EQU  H'000A'    */
char INTCON ; /*             EQU  H'000B'    */
char PIR1  ; /*              EQU  H'000C'    */
char PIR2  ; /*              EQU  H'000D'    */
char TMR1L ; /*              EQU  H'000E'    */
char TMR1H ; /*              EQU  H'000F'    */
char T1CON ; /*              EQU  H'0010'    */
char TMR2 ; /*               EQU  H'0011'    */
char T2CON ; /*              EQU  H'0012'    */
char SSPBUF ; /*             EQU  H'0013'    */
char SSPCON ; /*             EQU  H'0014'    */
char CCPR1L ; /*             EQU  H'0015'    */
char CCPR1H ; /*             EQU  H'0016'    */
char CCP1CON  ; /*           EQU  H'0017'    */
char RCSTA   ; /*            EQU  H'0018'    */
char TXREG  ; /*             EQU  H'0019'    */
char RCREG ; /*              EQU  H'001A'    */

/*;-----Bank1------------------*/
#pragma pic  129
char OPTION_REG ; /*         EQU  H'0081'    */
#pragma pic  133
char TRISA  ; /*             EQU  H'0085'    */
char TRISB  ; /*             EQU  H'0086'    */
#pragma pic  140
char PIE1  ; /*              EQU  H'008C'    */
char PIE2  ; /*              EQU  H'008D'    */
char PCON  ; /*              EQU  H'008E'    */
char OSCCON ; /*             EQU  H'008F'    */
char OSCTUNE ; /*            EQU  H'0090'    */
#pragma pic 146
char PR2   ; /*              EQU  H'0092'    */
/*char MSK              EQU  H'0093'*/
char SSPADD  ;        /*     EQU  H'0093'    */
/* char SSPMSK  ;             EQU  H'0093'    */
char SSPSTAT  ; /*           EQU  H'0094'    */
/*char WPU              EQU  H'0095'*/
char WPUB   ; /*             EQU  H'0095'    */
#pragma pic 152
char TXSTA ; /*              EQU  H'0098'    */
char SPBRG  ; /*             EQU  H'0099'    */
#pragma pic 155
char ANSEL;
char CMCON;
char CVRCON;

/*;-----Bank2------------------*/
/* TMR0 and PORTB are shadowed in this bank but ignored here */
#pragma pic  261
char WDTCON ;
#pragma pic  268
char EEDATA  ; /*            EQU  H'010C'    */
char EEADR   ; /*            EQU  H'010D'    */
char EEDATH  ; /*            EQU  H'010E'    */
char EEADRH  ; /*            EQU  H'010F'    */


/*;-----Bank3------------------*/
/* TRISB and OPTION_REG are shadowed in this bank but ignored here */
#pragma pic   396
char EECON1   ; /*           EQU  H'018C'    */
char EECON2   ; /*           EQU  H'018D'    */

#pragma eesize 256
#pragma banksize 128
#pragma access 112 127
#pragma access 0 0
#pragma access 2 4
#pragma access 10 11






