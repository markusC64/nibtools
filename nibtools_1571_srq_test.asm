; NIBTOOLS - SRQ Test routines

        CIA_BASE  = $4000
        CIA_TA_LO = CIA_BASE+4
        CIA_TA_HI = CIA_BASE+5
        CIA_SDR   = CIA_BASE+$c
        CIA_ICR   = CIA_BASE+$d
        CIA_CRA   = CIA_BASE+$e

.org $300

_flop_main:
        SEI
        LDA  #$ee
        STA  $1c0c
        LDA  #$0b
        STA  $180c
        LDA  $1c00                ;
        AND  #$f3                 ; motor off, LED off
        STA  $1c00                ;
        LDA  #$24                 ;
        STA  $c2                  ; current halftrack = 36

_main_loop:
        LDX  #$45                 ;
        TXS                       ; reset stack
        TYA                       ; return value from last call
        JSR  _send_byte           ; Send byte to ack to the host that
                                  ; we are now in the main loop and
                                  ; ready for commands.
        LDA  #>(_main_loop-1)
        PHA                       ; set RTS to main_loop
        LDA  #<(_main_loop-1)
        PHA
        JSR  _read_command
        ASL                       ; * 2 for 16 bit index
        TAX
        LDA  _command_table+1,X
        PHA
        LDA  _command_table,X
        PHA
        RTS                       ; -> to command function


;----------------------------------------
; read from RAM at $600-$6FF, send to host

_read_data:
        JSR  _send_byte_SRQ_on_send ; send data byte to host

	LDY  #$00
        LDA  #$04
_rt_L1:
        BIT  $1800
        BEQ  _rt_L1               ; wait for SLOW CLK = 1

_read_1:
        LDX  $0600,Y              ; read data byte
        PHA
        PLA
        PHA                       ; timing
        PLA
        BIT  $EA
        NOP
        STX  CIA_SDR              ; send byte

; stuff that may affect timing removed here
	INY
	BNE  _rt_L1

_read_end:
        NOP                       ; timing
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP
        LDY  #$00
        JSR  _disable_SRQ_send
        RTS

;----------------------------------------
; write data from host to RAM

_timing_table2:
.byte $00,$03,$07,$0C

_write_data:
        JSR  _enable_SRQ_read

        LDA  #$60		  ; based on density
        AND  $1C00		  
        LSR
        LSR
        LSR
        LSR
        LSR
        TAX
        LDA  _timing_table2,X
        STA  _wts_rtp+1           ; set routine timing

	LDY  #$00
	
_wts_wait4srq:
        BIT  CIA_ICR
        BPL  _wts_wait4srq        ; wait for first SRQ data byte (start signal)

_wts_start:
        LDA  #$08                 ; first handshake
        JMP  _wts_write

_wts_L1:
        BIT  CIA_ICR              ; check for SRQ byte-ready
        BMI  _wts_write
_wts_L2:
        BIT  CIA_ICR              ; check for SRQ byte-ready
        BMI  _wts_write
_wts_L3:
        BIT  CIA_ICR              ; check for SRQ byte-ready
        BMI  _wts_write
        BIT  CIA_ICR              ; check for SRQ byte-ready
        BMI  _wts_write
        BIT  CIA_ICR              ; check for SRQ byte-ready
        BMI  _wts_write
        BIT  CIA_ICR              ; check for SRQ byte-ready
        BPL  _wts_exit            ; branch to exit

_wts_write:
	STA  $1800                ; send handshake
        TAX
        LDA  CIA_SDR              ; read data byte
        STA  $0600,Y              ; write data byte to RAM
        INY
        TXA
        EOR  #$08                 ; toggle handshake
_wts_rtp:
        BPL  _wts_L1
        JMP  _wts_L1              ; timing
        NOP                       ; timing
        JMP  _wts_L2
        NOP                       ; timing
        NOP
        JMP  _wts_L3
        JMP  _wts_L3              ; timing

_wts_exit:
        LDY  #$00
        LDA  #$EE

        JSR  _disable_SRQ_read
        RTS

;----------------------------------------
; set $1c00 bits (head/motor)
_set_1c00:
        JSR  _read_byte           ; read byte from parallel data port
        STA  $c0                  ; $1c00 mask
        JSR  _read_byte           ; read byte from parallel data port
        STA  $c1                  ; new bit value for $1c00
        LDA  $1c00                ;
        AND  $c0                  ; mask off $1c00 bits
        ORA  $c1                  ; set new $1c00 bits
        STA  $1c00                ;
        RTS                       ;
        
;----------------------------------------
_perform_ui:
        LDA  #$12                 ;
        STA  $22                  ; current track = 18
        JMP  $eb22                ; UI command (?)

; ----------------------------------------
; SRQ-send data byte to host

_send_byte:
_send_byte_SRQ_on_send_off:
        JSR  _enable_SRQ_send
        JSR  _send_byte_SRQ
        JSR  _disable_SRQ_send
        RTS
_send_byte_SRQ_on_send:
        JSR  _enable_SRQ_send
        JSR  _send_byte_SRQ
        RTS
_send_byte_SRQ:
_sbs_L1:
        BIT  $1800
        BPL  _sbs_L1              ; wait for ATN IN = 1
        PHA
        LDA  #$18
        STA  $1800                ; handshake: SLOW CLK = 1 (ack ATN, DTA released)
        PLA
        STA  CIA_SDR              ; send data byte
_sbs_L2:
        BIT  $1800
        BMI  _sbs_L2              ; wait for ATN IN = 0
        LDA  #$00
        STA  $1800                ; handshake: SLOW CLK = 0 (ack ATN, DTA released)
        RTS

_enable_SRQ_send:                 ; enable 1571 SRQ mode for sending data byte to host
        PHA
        LDA  $180F
        STA  $96
        ORA  #$22
        STA  $180F                ; enable 2MHz, SER_DIR out
        LDA  #$01
        STA  CIA_TA_LO
        LDA  #$00
        STA  CIA_TA_HI            ; set timer A latches
        LDA  CIA_CRA
        STA  $97
        LDA  #$55                 ; start continuous timer for 62.5 KB/s output
        STA  CIA_CRA
        PLA
        RTS

_disable_SRQ_send:                ; disable 1571 SRQ mode for sending data byte to host
        LDA  $96
        STA  $180F                ; restore original value
        LDA  $97
        STA  CIA_CRA              ; restore original value
        RTS

;----------------------------------------
; read 1 byte with 4 byte command header
_read_command:
        LDY  #$04                 ; command header is 4 bytes long
_rcL1:
        JSR  _read_byte           ; read byte from parallel data port
        CMP  _command_header-1,Y  ; check with command header sequence:
        BNE  _read_command        ; $00,$55,$aa,$ff
        DEY                       ;
        BNE  _rcL1                ;
        JMP  _read_byte           ; read byte from parallel data port

;----------------------------------------
; SRQ-receive data byte from host

_read_byte:
_read_byte_SRQ_on_read_off:
        JSR  _enable_SRQ_read
        JSR  _read_byte_SRQ
        JSR  _disable_SRQ_read
        RTS
_read_byte_SRQ:
_rbs_L1:
        BIT  $1800
        BPL  _rbs_L1              ; wait for ATN IN = 1
        LDA  #$18
        STA  $1800                ; handshake: SLOW CLK = 1 (ack ATN, DTA released)
_rbs_L2:
        BIT  CIA_ICR
        BPL  _rbs_L2              ; wait for SRQ byte
        LDA  CIA_SDR              ; read data byte
        PHA
_rbs_L3:
        BIT  $1800
        BMI  _rbs_L3              ; wait for ATN IN = 0
        LDA  #$00
        STA  $1800                ; handshake: SLOW CLK = 0 (ack ATN, DTA released)
        PLA
        RTS

_enable_SRQ_read:                 ; enable 1571 SRQ mode for reading data byte from host
        LDA  $180F
        STA  $96
        ORA  #$20
        AND  #$FD
        STA  $180F                ; enable 2MHz, SER_DIR in
        JSR _enable_SRQ_IR        ; only SRQ byte-ready activates IR bit (MSB)
        RTS

_disable_SRQ_read:                ; disable 1571 SRQ mode for reading data byte from host
        PHA
        JSR _disable_SRQ_IR       ; clear CIA_ICR MASK
        LDA  $96
        STA  $180F                ; restore original value
        PLA
        RTS

_enable_SRQ_IR:                   ; Only SRQ byte-ready activates IR bit (MSB)
        LDA  #$1F                 ; 1Fh=0001.1111b
        STA  CIA_ICR              ; set IRQ mask: no IRQ activates IR bit (MSB)
        LDA  #$88                 ; 88h=1000.1000b
        STA  CIA_ICR              ; set IRQ mask: only SRQ byte-ready activates IR bit (MSB)
        BIT  CIA_ICR              ; clear pending IRQs
        RTS

_disable_SRQ_IR:                  ; Clear CIA_ICR MASK
        LDA  #$1F                 ; 1Fh=0001.1111b
        STA  CIA_ICR              ; set IRQ mask: no IRQ activates IR bit (MSB)
        RTS

;----------------------------------------
; send parallel port test sequence (0,1,2,...,$ff bytes) to C64
_send_count:
        TYA                       ;
        JSR  _send_byte           ; parallel-send data byte to C64
        INY                       ;  (send 0,1,2,...,$ff)
        BNE  _send_count          ;
        RTS                       ;

;----------------------------------------
_verify_code:
        LDY  #$00
        STY  $c0
        LDA  #$03
        STA  $c1
_verify_L1:
        LDA  ($c0),Y
        JSR  _send_byte           ; parallel-send data byte to C64
        INY
        BNE  _verify_L1           ;
        INC  $c1
        LDA  $c1
        CMP  #$08
        BNE  _verify_L1
        RTS
       

;----------------------------------------
; Command Jump table, return value: Y
_command_table:
.byte 0,0                                         
.byte <(_set_1c00-1),>(_set_1c00-1)               ; <1> set $1c00 bits (head/motor)
.byte <(_perform_ui-1),>(_perform_ui-1)           ; <2> track $22 = 17, UI command: $eb22
.byte 0,0                                         
.byte <(_read_data-1),>(_read_data-1)             ; <4> read RAM to host
.byte 0,0                                    
.byte 0,0  
.byte 0,0   
.byte 0,0   
.byte 0,0   
.byte <(_send_count-1),>(_send_count-1)           ; <a> send 0,1,2,...,$ff bytes to C64
.byte <(_write_data-1),>(_write_data-1)           ; <b> write RAM from host
.byte 0,0   
.byte 0,0   
.byte <(_verify_code-1),>(_verify_code-1)         ; <e> send floppy side code back to PC
.byte 0,0   

_command_header:
.byte $ff,$aa,$55,$00                             ; command header code (reverse order)
