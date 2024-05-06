; NIBTOOLS - main floppy routines
;
; Some of this code may have still have roots in Datel Burstnibbler, but not much 
; is the same anymore 
;
; This file is a modified version that removes the parallel port requirement and
; replaces it with SRQ support by Arnd Menge
;
; It will later be cleaned up, as there is dupe code here for now, including unused 
; 1541 code
;
; General description of routines
; ===============================
; This code is uploaded to the drive and then executed. It can be at most
; 1 KB ($300 - $700). The main_loop reads commands and executes them by
; a direct RTS. Each command is at least 5 bytes: a 4-byte header and then
; the command byte itself. A table at the end of this code links routines
; to command bytes.
;
; There are two forms of IO: interlocked (send_byte) and handshaked
; (read_gcr_1). Both use the parallel port for the actual byte transfer,
; but signal differently via the IEC lines. Interlocked IO involves toggling
; the ATN (host) and DATA lines (drive). It allows both sides to be sure
; their timing is correct. The host sets ATN to indicate it is ready for IO,
; then the drive releases DATA when it is ready. After the data byte is done
; (send or receive), the sender toggles its line. (The CPU releases ATN or
; the drive acquires DATA, depending on who was sending). Then the sequence
; can continue. This method is reliable but slightly slower than handshaked.
; It is used for commands and their status return value because some can
; take a long time (e.g., a seek) and the other side has to wait.
;
; Handshaked transfers only give one-way notification. The sending side
; toggles DATA when a byte is ready. Each edge (0->1 or 1->0) indicates
; another byte is ready. The other side just has to be fast enough to keep up.
; This is used for the high-speed transfer where bytes are ready quickly.
;
; After receiving a command, the drive indicates it is executing it by
; sending an interlocked byte to the host. This byte is not interpreted, but
; allows the host to wait for it. If the command was a parallel read or write,
; the host needs to be immediately ready to start transferring bytes via
; handshaked IO. Thus, it should not receive the ack byte until it is entering
; its tight IO loop.

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
; read out track without waiting for sync
; 1571-SRQ version without parallel port.
; best drive speed is 300.00 rpm
;
; Copyright 2011-2015 Arnd Menge
;
; THIS ROUTINE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND
; AND/OR FITNESS FOR A PARTICULAR PURPOSE. USE AT YOUR OWN RISK.
;
; Thanks to Nate Lawson, Pete Rittwage and Wolfgang Moser.

_read_track:
        JSR  _send_byte_SRQ_on_send ; send data byte to host

        LDY  #$20
        STY  $60
        LDY  #$00                 ; Read NIB_TRACK_LENGTH = 0x2000 bytes

        LDA  #$04
_rt_L1:
        BIT  $1800
        BEQ  _rt_L1               ; wait for SLOW CLK = 1

        BIT  $180F
        BPL  _read_1
        BIT  $180F
        BPL  _read_1
        BIT  $180F
        BPL  _read_1
_rt_L2:
        BIT  $180F
        BPL  _read_1
_rt_L3:
        BIT  $180F
        BPL  _read_1
_rt_L4:
        BIT  $180F
        BPL  _read_1
_rt_L5:
        BIT  $180F
        BPL  _read_1
_rt_L6:
        BIT  $180F
        BPL  _read_1
        LDX  #$FF
        BIT  $180F
        BPL  _read_1
        BIT  $180F
        BPL  _read_1
        BIT  $180F
        BPL  _read_1
        BIT  $180F
        BPL  _read_1
        STX  CIA_SDR              ; send byte
        INY                       ; update byte count
        BEQ  _rts1
        LDX  $60
        JMP  _rtp1
_rts1:
        DEC  $60
_rtp1:
        BNE  _rt_L2               ; loop while count < 0x2000
        BEQ  _read_end
        NOP                       ; timing
        NOP
        JMP  _rt_L4
        NOP                       ; timing
        JMP  _rt_L5
        NOP                       ; timing
        NOP
        JMP  _rt_L6
        JMP  _rt_L6               ; timing
_read_1:
        LDX  $1C01                ; read data byte
        INY                       ; update byte count
        BNE  _rts2
        DEC  $60
        JMP  _rts3
_rts2:
        NOP                       ; timing
        NOP
        BIT  $EA
_rts3:
        NOP                       ; timing
        NOP
        BIT  $EA
        STX  CIA_SDR              ; send byte
        BIT  $EA                  ; timing
        LDX  $60
_rtp2:
        BNE  _rt_L2               ; loop while count < 0x2000
        BEQ  _read_end
        NOP                       ; timing
        NOP
        JMP  _rt_L2
        NOP                       ; timing
        JMP  _rt_L3
        NOP                       ; timing
        NOP
        JMP  _rt_L4
        JMP  _rt_L4               ; timing
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
; step motor to destination halftrack
_step_dest:
        JSR  _read_byte           ; read byte from parallel data port
_step_dest_internal:
        LDX  #$01                 ; step value: step up
        CMP  $c2                  ; compare with current track (CARRY!!!)
        BEQ  _step_dest_end       ; destination track == current -> RTS
        PHA                       ; push destination track
        SBC  $c2                  ; calculate track difference
        BPL  _step_up             ; destination track > current ->
        EOR  #$ff                 ; else negate track difference
        LDX  #$ff                 ; step value: step down
_step_up:
        TAY                       ; # of tracks to step
_step_loop:
        TXA                       ; step value
        CLC                       ;
        ADC  $1c00                ;
        AND  #$03                 ;
        STA  $c0                  ; temp store
        LDA  $1c00                ;
        AND  #$fc                 ; mask off stepper bits
        ORA  $c0                  ;
        STA  $1c00                ; perform half step
        LDA  #$04                 ;
        STA  $c1                  ;
        LDA  #$00                 ; busy wait $0400 times
        STA  $c0                  ;
_stepL1:
;	NOP			; ** PAGE BOUNDARY ADJUST**
        DEC  $c0                  ;
        BNE  _stepL1              ;
        DEC  $c1                  ;
        BNE  _stepL1              ;
        DEY                       ;
        BNE  _step_loop           ; repeat for # of halftracks
        PLA                       ; pull destination track
        STA  $c2                  ; current track = destination
_step_dest_end:
        RTS
;----------------------------------------
; Density Scan for current track
_scan_density:
        LDX  #$05                 ;
        LDY  #$00                 ;
        STX  $C0                  ; Max timeout count
_scL1:
        STY  $c3,X                ; reset bit-rate statistic
        DEX                       ;
        BPL  _scL1                ;
_sc_retry:
        DEC  $C0                  ; limit number of timeouts
        BEQ  _scExit             ;
        CLV                       ;
_scW1:
        BVC  _scW1                ; wait for GCR byte
        CLV                       ;
        LDA  $1c01                ; read GCR byte
        PHA                       ;
        PLA                       ; (busy wait timing)
        PHA                       ;
        PLA                       ;
_scL2:
        NOP                       ;
        BVS  _scJ1                ;
        BVS  _scJ2                ;
        BVS  _scJ3                ; measure bit-rate between bytes
        BVS  _scJ4                ;
        BVS  _scJ5                ;
        BVS  _scJ6                ;
        BNE  _sc_retry            ; -> time too long, retry with next pair
_scJ1:
        LDX  #$00                 ; bit-rate = 0
        BEQ  _scJ7                ;
_scJ2:
        LDX  #$01                 ; bit-rate = 1
        BNE  _scJ7                ;
_scJ3:
        LDX  #$02                 ; bit-rate = 2
        BNE  _scJ7                ;
_scJ4:
        LDX  #$03                 ; bit-rate = 3
        BNE  _scJ7                ;
_scJ5:
        LDX  #$04                 ; bit-rate = 4
        BNE  _scJ7                ;
_scJ6:
        LDX  #$05                 ; bit-rate = 5
        BNE  _scJ7                ;
_scJ7:
        CLV                       ;
        ; INC  $00c3,X            ; adjust statistic for bit-rate X
.byte $fe,$c3,$00                 ; INC  $00c3,X (not supported by C64asm)
        INY                       ;
        BPL  _scL2                ;
_scExit:
        LDY  #$00                 ;
_scL3:
        LDA  $00c4,Y              ; transfer density statistic 1-4 to C64
        JSR  _send_byte           ; parallel-send data byte to C64
        INY                       ;
        CPY  #$04                 ;
        BNE  _scL3                ;

        LDY  #$00                 ;
        RTS                       ;

;----------------------------------------
; adjust routines to density value

_timing_table:
.byte $02,$07,$0B,$10

_adjust_density:
        JSR  _read_byte
        TAX
        LDA  _timing_table,X
        STA  _rtp1+1
        STA  _rtp2+1

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
; detect 'killer tracks' (all SYNC)
_detect_killer:
	LDX  #$80	;   	
	STY  $c0		;  y=0

_dkL1:
        LDA  $1c00                ; wait for SYNC
        BPL  _dk_sync             ; if SYNC found, check for 'killer track'
        DEY                       ;
        BNE  _dkL1                ; wait max. $8000 times for at least one SYNC
        DEX                       ;
        BNE  _dkL1                ;
        LDY  #$40                 ; track doesn't contain SYNC
        RTS                       ; -> $40 = track has no SYNC

_dk_sync:                         ; try to read some bytes within $10000 cycles
        LDX  #$00                 ;
        LDA  $1c01                ; read GCR byte
        CLV                       ;
_dkL2:
        DEY                       ;
        BNE  _dkWait              ; wait max $10000 times
        DEX                       ;
        BEQ  _dk_killer           ; timeout, not enough bytes found ->
_dkWait:
        BVC  _dkL2                ;
        CLV                       ;
        DEC  $c0                  ; check for at least $c0 bytes in track
        BNE  _dkWait              ;
        LDY  #$00                 ; track contains something
        RTS                       ; -> $00 = track OK

_dk_killer:
        LDY  #$80                 ; track only contains sync
        RTS                       ; -> $80 = killer track (too many syncs)
        
;----------------------------------------
; read out track from MARKER BYTE
_read_from_mark:
        JSR  _read_byte           ; read byte from parallel data port
        STA  _marker+1            ; MARKER BYTE
        JSR  _send_byte           ; parallel-send data byte to C64
        LDA  #$ff                 ;
        STA  $1800                ; send handshake
        LDX  #$20                 ;
        STX  $c0                  ; read $2000 GCR bytes

_rfm1:
        CLV                      	 ;
_rfm2:
        BVC  _rfm1	              ;
        LDX  $1c01                ; read GCR byte
_marker:
        CPX  #$37                 ; check for MARKER BYTE
        BNE  _rfm1              ; wrong header mark, repeat
        JMP  _read_track             ; -> read out track


;----------------------------------------
; send a short sync to all tracks on a disk
; Pete Rittwage 3/7/2010
_align_disk:
        JSR  _read_byte           ; read byte from parallel data port
        STA  _delay_loop+1         ; can change delay loop by 10ms
      
        LDA  #$ce
        STA  $1c0c
        DEC  $1c03                ; CA data direction head (0->$ff: write)
        
        LDA #$52  	 ; track 41	
       	STA $cf

;--------------------------------------
; this simple sweep aligns small syncs 20ms off from each other, not real great 
;--------------------------------------
 _admain:
	JSR _step_dest_internal
	LDA  #$ff                 ;
        STA  $1c01                ; write $ff byte (Sync mark)
_adL1:
        BVC  _adL1                ;
        STA  $1c01                ; write $ff byte (Sync mark)
_adL2:
        BVC  _adL2                ;

_delay_loop:
     	DEC $cf
	DEC $cf
	LDA $cf
	CMP #$01
	BNE _admain 
	
        LDA  #$ee
        STA  $1c0c
        INC  $1c03                ; CA data direction head ($ff->$0: read)
        RTS


;----------------------------------------
; completely fill a track with given byte
; used for unformat/kill

_fill_track:
        LDA  #$ce
        STA  $1c0c
        DEC  $1c03                ; CA data direction head (0->$ff: write)

	JSR  _read_byte           ; read byte from parallel data port
        LDX  #$20                 ;
        STA  $1c01                ; send byte to head
_ftL1:
        BVC  _ftL1                ;
        CLV                       ;
        INY                       ; write $2000 ($20 x $100) times
        BNE  _ftL1                ;
        DEX                       ;
        BNE  _ftL1                ;

        LDA  #$ee
        STA  $1c0c
        INC $1C03
        RTS
       
       
;----------------------------------------
; write a track, can wait for IHS or sync before start.
; 1571-SRQ version without parallel port.
;
; Copyright 2011-2015 Arnd Menge
;
; Update 17-05-2015: Workaround for potential CIA 6526 ICR bug.
;                    Failure still possible if unlucky.
;                    Replacement by 2MHz 8520/8521 recommended.
;
; THIS ROUTINE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND
; AND/OR FITNESS FOR A PARTICULAR PURPOSE. USE AT YOUR OWN RISK.

_timing_table2:
.byte $00,$03,$07,$0C

_write_track:
        JSR  _enable_SRQ_read
        JSR  _read_byte_SRQ       ; read byte via SRQ
        STA  $60                  ; wait for IHS? (0=YES)
        JSR  _read_byte_SRQ       ; read byte via SRQ
        STA  $61                  ; wait for SYNC? (0=NO)
 
        LDA  #$60                 ;  based on density
        AND  $1C00		  
        LSR
        LSR
        LSR
        LSR
        LSR
        TAX
        LDA  _timing_table2,X
        STA  _wts_rtp+1           ; set routine timing

_wts_wait4srq:
        BIT  CIA_ICR
        BPL  _wts_wait4srq        ; wait for first SRQ data byte (start signal)

;wait for ihs?
        LDA  $60                  ; wait for IHS? (0=YES)
        BNE  _wts_skip_ihs
        JSR  _1571_ihs_wait_hole
        
_wts_skip_ihs:
        LDA  $61                  ; wait for SYNC? (0=NO)
        BEQ  _wts_start

_wts_wait4sync:
        BIT  $1C00                ; wait for sync
        BMI  _wts_wait4sync

_wts_start:
        LDA  #$08                 ; first handshake
        LDX  #$CE
        DEC  $1C03                ; set port A to output
        STX  $1C0C                ; switch to write
        JMP  _wts_write

_wts_L1:
        NOP                       ; timing
        NOP
        NOP
_wts_L2:
        NOP                       ; timing
        NOP
        NOP
_wts_L3:
        NOP                       ; timing
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP
        NOP
        BIT  CIA_ICR              ; check for SRQ byte-ready
        BPL  _wts_exit            ; branch to exit

_wts_write:
        STA  $1800                ; send handshake
        LDX  CIA_SDR              ; read data byte
_wts_br1:
        BIT  $180F
        BMI  _wts_br1             ; wait for byte-ready
        STX  $1C01                ; write data byte to disk
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
_wts_br2:
        BIT  $180F
        BMI  _wts_br2             ; wait for byte-ready
        STY  $1C01                ; write byte
_wts_br3:
        BIT  $180F
        BMI  _wts_br3             ; wait for byte-ready
        STA  $1C0C                ; switch to read
        INC  $1C03                ; set port A to input
        JSR  _disable_SRQ_read
        RTS

;----------------------------------------
; read $1c00 motor/head status
_read_1c00:
        LDY  $1c00
        RTS

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
_perform_ui:
        LDA  #$12                 ;
        STA  $22                  ; current track = 18
        JMP  $eb22                ; UI command (?)


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
; measure destination track length
_measure_trk_len:
        LDX  #$20
        LDA  #$ce
        STA  $1c0c
        DEC  $1c03                ; CA data direction head (0->$ff: write)

        LDA  #$55                 ;
        STA  $1c01                ; write $55 byte
_mtL1:
        BVC  _mtL1                ;
        CLV                       ;
        INY                       ; write $2000 times
        BNE  _mtL1                ;
        DEX                       ;
        BNE  _mtL1                ;

        LDA  #$ff                 ;
        STA  $1c01                ; write $ff byte (Sync mark)
_mtL2:
        BVC  _mtL2                ;
        CLV                       ;
        INX                       ; write 5 times (short Sync)
        CPX  #$05                 ;
        BNE  _mtL2                ;

        LDA  #$ee
        STA  $1c0c
        STY  $1c03                ; CA data direction head ($ff->0: read)
_mtJ1:
        LDA  $1c00                ;
        BPL  _mt_end              ; 1st time: Sync, 2nd time: no Sync ->

_mtL3:
        BVC  _mtJ1                ; if no more bytes available ->
        CLV                       ;
        INX                       ; X/Y = counter: GCR bytes in one spin
        BNE  _mtL3                ;
        INY                       ;
        BNE  _mtL3                ;
_mt_end:
        TXA                       ; (0) : Track 'too long'
        JMP  _send_byte           ; parallel-send data byte to C64
        

;----------------------------------------
; setup 1571 index hole sensor and wait for the hole to appear
;-----------------------------------------        
_1571_ihs_wait_hole:
        LDA  #$10                 ; send L1 command to WD177x so we can query status
        STA  $2000               

        LDX #$20                  ; we do this here to satisfy 16/32 cycle wait
_ihsr_busywait:
        DEX
        BNE _ihsr_busywait;
        
        LDA #$02                  ; index hole is bit 1 in WD177x status register
;--------------------------------------------------------------
;wait for it to pass or start at beginning?  What did TRACE devices do? 
;--------------------------------------------------------------
_ihsr_wait1:
	BIT  $2000               ; check ihs
	BNE  _ihsr_wait1       ; wait until dark
_ihsr_wait2:
	BIT  $2000               ; check ihs
	BEQ  _ihsr_wait2       ; wait until light
	RTS 
        
;----------------------------------------
; setup SC index hole sensor and wait for the hole to appear
;-----------------------------------------
_sc_ihs_wait_hole:
        PHA
        LDA  $1c02                ; prep for IHS reading
        AND  #$f7
        STA  $1c02                ; set PB3 (ACT) to be an input (0)
;--------------------------------------------------------------
;wait for it to pass or start at beginning?  What did TRACE devices do? 
;--------------------------------------------------------------
_sc_ihs_wait1:
        LDA  $1c00                ; read from PB3 (IHS)
        AND  #$08                 ;
        BEQ  _sc_ihs_wait1             ; wait until dark
_sc_ihs_wait2:
        LDA  $1c00                ; read from PB3 (IHS)
        AND  #$08                 ;
        BNE  _sc_ihs_wait2             ; wait until we hit the index hole
        PLA
        RTS

_sc_ihs_cleanup:
        LDA  $1c02                ; clean up from IHS reading
        ORA  #$08
        STA  $1c02                ; set PB3 (ACT) to be an output again
        RTS


;----------------------------------------
; Command Jump table, return value: Y
_command_table:
.byte <(_step_dest-1),>(_step_dest-1)             ; <0> step motor to destination halftrack
.byte <(_set_1c00-1),>(_set_1c00-1)               ; <1> set $1c00 bits (head/motor)
.byte <(_perform_ui-1),>(_perform_ui-1)           ; <2> track $22 = 17, UI command: $eb22
.byte <(_read_track-1),>(_read_track-1)           ; <3> read out track w/out waiting for Sync
.byte <(_read_track-1),>(_read_track-1)           ; <4> read out track after Sync
.byte <(_read_track-1),>(_read_track-1)           ; <5> read out track after IHS
.byte <(_adjust_density-1),>(_adjust_density-1)   ; <6> adjust read routines to density value
.byte <(_detect_killer-1),>(_detect_killer-1)     ; <7> detect 'killer tracks'
.byte <(_scan_density-1),>(_scan_density-1)       ; <8> perform Density Scan
.byte <(_read_1c00-1),>(_read_1c00-1)             ; <9> read $1c00 motor/head status
.byte <(_send_count-1),>(_send_count-1)           ; <a> send 0,1,2,...,$ff bytes to C64
.byte <(_write_track-1),>(_write_track-1)         ; <b> write a track on destination
.byte <(_measure_trk_len-1),>(_measure_trk_len-1) ; <c> measure destination track length
.byte <(_align_disk-1),>(_align_disk-1)           ; <d> align sync on all tracks
.byte <(_verify_code-1),>(_verify_code-1)         ; <e> send floppy side code back to PC
.byte <(_fill_track-1),>(_fill_track-1)           ; <f> zero out (unformat) a track
.byte <(_read_from_mark-1),>(_read_from_mark-1)	; read out track from MARKER BYTE

_command_header:
.byte $ff,$aa,$55,$00                             ; command header code (reverse order)
