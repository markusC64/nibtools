; NIBTOOLS - main floppy routines
;
; Some of this code may have still have roots in Datel Burstnibbler, but not much 
; is the same anymore 
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

.ifndef DRIVE
        .error "DRIVE must be defined as 1541 or 1571"
.elseif DRIVE = 1541
        PP_BASE = $1801
.elseif DRIVE = 1571
        PP_BASE = $4001
.else
        .error "DRIVE must be 1541 or 1571"
.endif

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
; read out track after 1541/1571 (SC+ compatible) IHS without waiting for Sync
; best drive speed is 300.00 rpm

_read_after_ihs4:
        JSR  _send_byte           ; parallel-send data byte to C64
        LDA  #$FF
        STA  $1800	              ; send handshake
        LDX  #$20	              ; read $2000 GCR bytes
        STX  $C0
        STA  $C1                  ; last handshake, timing (2->3 cpu cycles)
        LDX  #$FF

        LDA  #$08
_ihs4_L1:
        BIT  $1C00                ; make sure we really start at the beginning of index hole
        BNE  _ihs4_L1
_ihs4_L2:
        BIT  $1C00
        BEQ  _ihs4_L2
_ihs4_L3:
        BIT  $1C00
        BNE  _ihs4_L3
        BMI  _ihs4_L4

        BIT  $1C01
        CLV
        LDA  $C1
_rtp6:
        BNE  _read_gcr_loop

_ihs4_L4:
        BIT  $1C01
        CLV
        LDA  #$FF
        BVS  _read_gcr_1
        BVS  _read_gcr_1
        BVS  _read_gcr_1
        BVS  _read_gcr_1
_rtp5:
        BNE  _read_gcr_loop

_read_gcr_loop:
        BVS  _read_gcr_1
        BVS  _read_gcr_1
        BVS  _read_gcr_1
        BVS  _read_gcr_1
        BVS  _read_gcr_1
        BVS  _read_gcr_1
        BVS  _read_gcr_1
        LDX  #$FF
        BVS  _read_gcr_1
        EOR  #$FF
        BVS  _read_gcr_2
        STX  PP_BASE              ; parallel port
        BVS  _read_gcr_2
        STA  $1701,X              ; handshake
        DEY
_rtp4:
        BNE  _read_gcr_loop
        DEC  $C0
        BEQ  _read_track_end
_rtp3:
        BVC  _read_gcr_loop
_read_gcr_1:
        EOR  #$FF
_read_gcr_2:
        LDX  $1C01                ; read GCR byte
        CLV
        STX  PP_BASE              ; parallel port
        STA  $1800                ; handshake
        DEY
_rtp2:
        BNE  _read_gcr_loop
        DEC  $C0
_rtp1:
        BNE  _read_gcr_loop
_read_track_end:
        STY  $1800
        RTS

;----------------------------------------
; adjust routines to density value
_adjust_density:
        JSR  _read_byte
        CLC
        ADC  #$17
        STA  _rtp1+1
        ADC  #$02
        STA  _rtp2+1
        ADC  #$15
        STA  _rtp3+1
        ADC  #$04
        STA  _rtp4+1
        ADC  #$1D
        STA  _rtp5+1
        CLC
        ADC  #$18
        STA  _rtp6+1

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
; read memory location, by Arnd

_read_mem:
        JSR  _read_byte           ; read byte from parallel data port
        STA  $c0                  ; lo adress
        JSR  _read_byte           ; read byte from parallel data port
        STA  $c1                  ; hi address
        LDY  #$00                 ;
        LDA  ($C0),Y              ; load memory
        TAY                       ; return in Y
        RTS                       ;

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
; read $1c00 motor/head status
_read_1c00:
        LDY  $1c00
        RTS

; ----------------------------------------
_send_byte:
        JMP  _send_byte_1         ; parallel-send data byte to C64

;----------------------------------------
_send_byte_1:
        LDX  #$ff                 ;
        STX  PP_BASE+2            ; data direction port A = output
_sbJ1:
        LDX  #$10                 ;
_sbL1:
        BIT  $1800                ; wait for ATN IN = 1
        BPL  _sbL1                ;
        STA  PP_BASE              ; PA, port A (8 bit parallel data)
        STX  $1800                ; handshake: ATN OUT = 1
        DEX                       ;
_sbL2:
        BIT  $1800                ;
        BMI  _sbL2                ; wait for ATN IN = 0
        STX  $1800                ; ATN OUT = 0
        RTS                       ;

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
_read_byte:
        JMP  _read_byte_1         ; read byte from parallel data port

;----------------------------------------
        LDX  #$00
        STX  $b80c
        STX  $b808
        LDX  #$04
        STX  $b80c
        BNE  _rbJ1

_read_byte_1:
        LDX  #$00                 ;
        STX  PP_BASE+2            ; data direction port A = input
_rbJ1:
        LDX  #$10                 ;
_rbL1:
        BIT  $1800                ; wait for ATN IN = 1
        BPL  _rbL1                ;
        STX  $1800                ; handshake: ATN OUT = 1
        DEX                       ;
_rbL2:
        BIT  $1800                ;
        BMI  _rbL2                ; wait for ATN IN = 0
        LDA  PP_BASE              ; PA, port A (8 bit parallel data)
        STX  $1800                ; ATN OUT = 0
        RTS                       ;

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
; Deep Bitrate Analysis, by Arnd
; requires 1541/1571 SC+ compatible IHS
; best drive speed is 300.00 rpm

_dbr_analysis:
        JSR  _send_byte           ; parallel-send data byte
        LDA  #$FF                 ;
        STA  $1800                ; send handshake

        LDX #$00
        LDY #$00                  ; X=Y=0
        STY $CC
        STX $CD

        LDA #$08
_da_L1:                           ; make sure we *really* start at the beginning of index hole
        BIT $1C00
        BNE _da_L1
_da_L2:
        BIT $1C00
        BEQ _da_L2
_da_L3:
        BIT $1C00
        BNE _da_L3

_da_L4:
        BIT $1C00
        BNE _da_L6
        BPL _da_L4
_da_L5:
        BIT $1C00
        BNE _da_L7
        BMI _da_L5
        INY                       ; Y++
        BNE _da_L4
        INX                       ; X++
        BNE _da_L4
_da_L6:
        BIT $1C00
        BEQ _da_L8
        BPL _da_L6
_da_L7:
        BIT $1C00
        BEQ _da_L8
        BMI _da_L7
        INY                       ; Y++
        BNE _da_L6
        INX                       ; X++
        BNE _da_L6
_da_L8:
        LDA #$FF
        CPX #$00                  ; check X=0
        BNE _da_L9
        CPY #$00                  ; check Y=0
        BNE _da_L10
        JMP _da_LExit             ; >> exit if X=Y=0
_da_L9:
        STX $CD                   ; save X (hi #syncs)
        STY $CC                   ; save Y (lo #syncs)
        TAY
_da_L10:
        BIT $1C00
        BMI _da_L10               ; wait for sync
_da_L11:
        CLV

_da_L12:
        BVC _da_L12               ; wait for byte-ready
        CLV                       ;
        CMP ($FF,X)               ;
        CMP ($FF,X)               ; busy wait timing
        NOP                       ;
        NOP                       ;
_da_L13:
        NOP                       ;
        NOP                       ;
        BVS _da_L14               ;
        BVS _da_L15               ;
        BVS _da_L16               ;
        BVS _da_L17               ; measure bitrate between bytes
        BVS _da_L18               ;
        BVS _da_L19               ;
        LDX #$FF                  ; timeout
        EOR #$FF
        STX PP_BASE               ; parallel port
        STA $1800                 ; handshake
        DEY                       ; Y--
        BNE _da_L11
        JMP _da_LExit             ; >> exit

_da_L14:
        CLV
        LDX #$05
        EOR #$FF
        STX PP_BASE               ; parallel port
        STA $1800                 ; handshake
        JMP _da_L13

_da_L15:
        CLV
        LDX #$04
        EOR #$FF
        STX PP_BASE               ; parallel port
        STA $1800                 ; handshake
        JMP _da_L13

_da_L16:
        CLV
        LDX #$03
        EOR #$FF
        STX PP_BASE               ; parallel port
        STA $1800                 ; handshake
        JMP _da_L13

_da_L17:
        CLV
        LDX #$02
        EOR #$FF
        STX PP_BASE               ; parallel port
        STA $1800                 ; handshake
        JMP _da_L13

_da_L18:
        CLV
        LDX #$01
        EOR #$FF
        STX PP_BASE               ; parallel port
        STA $1800                 ; handshake
        JMP _da_L13

_da_L19:
        CLV
        LDX #$00
        EOR #$FF
        STX PP_BASE               ; parallel port
        STA $1800                 ; handshake
        JMP _da_L13

_da_LExit:
        LDX #$55                  ; end of transmission
        EOR #$FF
        STX PP_BASE               ; send $55 to parallel port
        STA $1800                 ; handshake
        NOP
        CMP ($FF,X)
        CMP ($FF,X)
        CMP ($FF,X)
        NOP
        STY $1800
        RTS
        
;----------------------------------------
; 1541/1571 IHS handling (SC+ compatible), by Arnd

_ihs_on:
        LDA #$F7
        AND $1C02
        STA $1C02
        RTS

_ihs_off:
        LDA #$08
        ORA $1C02
        STA $1C02
        RTS

_ihs_present2:                    ; return value:
        LDA #$08                  ;   A=0: IHS found
        BIT $1C02                 ;   A=$10: IHS disabled (must enable first!)
        BEQ _ihL1                 ;   A=8: hole not detected,
        ASL A                     ;        or IHS not working,
        BNE _ihL3                 ;        or IHS not present.
_ihL1:
        LDX #$7F
_ihL2:
        AND $1C00
        BEQ _ihL3
        DEY
        BNE _ihL2
        DEX
        BNE _ihL2
_ihL3:
        TAY
        RTS

_ihs_present:
        JSR _send_byte            ; parallel-send data byte
        LDA #$FF
        STA $1800                 ; send handshake
        LDX #$20
        STX $CD                   ; $2000 bytes
_ihM1:
        NOP
        NOP
        CMP $CD
_ihM2:
        CMP $CD
        NOP
        NOP

        PHA                       ; return value:
        LDA #$08                  ; A=0: IHS found
        BIT $1C02                 ;   A=$10: IHS disabled (must enable first!)
        BEQ _ihM3                 ;   A=8: hole not detected,
        ASL A                     ;        or IHS not working,
        JMP _ihM4                 ;        or IHS not present.
_ihM3:
        AND $1C00
_ihM4:
        TAX
        PLA
        EOR #$FF
        STX PP_BASE               ; parallel port
        STA $1800                 ; handshake
        DEY                       ; total byte counter (lo)
        BNE _ihM1
        DEC $CD                   ; total byte counter (hi)
        BNE _ihM2

        CMP ($FF,X)
        CMP ($FF,X)
        CMP ($FF,X)
        STY $1800
        RTS

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
.byte <(_step_dest-1),>(_step_dest-1)             ; <0> step motor to destination halftrack
.byte <(_set_1c00-1),>(_set_1c00-1)               ; <1> set $1c00 bits (head/motor)
.byte <(_perform_ui-1),>(_perform_ui-1)           ; <2> track $22 = 17, UI command: $eb22
.byte 0,0                                         ; <3> read out track w/out waiting for Sync
.byte 0,0                                         ; <4> read out track after Sync
.byte 0,0                                         ; <5> read out track after IHS
.byte <(_adjust_density-1),>(_adjust_density-1)   ; <6> adjust read routines to density value
.byte <(_detect_killer-1),>(_detect_killer-1)     ; <7> detect 'killer tracks'
.byte <(_scan_density-1),>(_scan_density-1)       ; <8> perform Density Scan
.byte <(_read_1c00-1),>(_read_1c00-1)             ; <9> read $1c00 motor/head status
.byte <(_send_count-1),>(_send_count-1)           ; <a> send 0,1,2,...,$ff bytes to C64
.byte 0,0                                         ; <b> write a track on destination
.byte 0,0                                         ; <c> measure destination track length
.byte 0,0                                         ; <d> align sync on all tracks
.byte <(_verify_code-1),>(_verify_code-1)         ; <e> send floppy side code back to PC
.byte 0,0                                         ; <f> zero out (unformat) a track
.byte <(_ihs_on-1),>(_ihs_on-1)                    ; <10> turn 1541/1571 (SC+ compatible) IHS on
.byte <(_ihs_off-1),>(_ihs_off-1)                  ; <11> turn 1541/1571 (SC+ compatible) IHS off
.byte <(_ihs_present2-1),>(_ihs_present2-1)        ; <12> is 1541/1571 (SC+ compatible) IHS present?
.byte <(_ihs_present-1),>(_ihs_present-1)          ; <13> is 1541/1571 (SC+ compatible) IHS present?
.byte <(_dbr_analysis-1),>(_dbr_analysis-1)        ; <14> deep bitrate analysis
.byte <(_read_mem-1),>(_read_mem-1)                ; <15> read memory location
.byte <(_read_after_ihs4-1),>(_read_after_ihs4-1)  ; <16> read out track after 1541/1571 (SC+ compatible) IHS w/out waiting for Sync


_command_header:
.byte $ff,$aa,$55,$00                             ; command header code (reverse order)
