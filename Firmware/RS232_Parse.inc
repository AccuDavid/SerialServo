;=========================================================================================
;
;    Filename:      RS232_Parse.inc
;    Date:          5/18/2018
;    File Version:  1.0d4
;
;    Author:        David M. Flynn
;    Company:       Oxford V.U.E., Inc.
;    E-Mail:        dflynn@oxfordvue.com
;
;=========================================================================================
; Routines:
;
; RS232_Parse	Call from main loop
; RS232_ClearTXData	Clear the TX data, FSR1
; RS232_Send	Send the data at TX_data, FSR0, FSR1
;
;=========================================================================================
; Notes:
;
; History:
; 1.0d4  5/18/2018	Added RS232_ClearTXData, RS232_Send.
; 1.0d3  5/17/2018	Used Timer1Lo for RXTimeout.
; 1.0d2  4/26/2018	Comments, used in robot arm project.
; 1.0d1  1/12/2016	Modified for I2C to RS-232 8 byte packets.
; 1.0d1  11/9/2015	Adapted for QD Tester
; 1.0    12/6/2013	First Rev'd version
;
; RS232_Parse	(X/0) Parse any bytes received by the serial port
; Packet format: (FF FF) SA0 (SA1) DA0 (DA1) D0 D1 D2 D3 D4 D5 D6 D7 (CS)
;  FF = Sync byte, SAn = Source Address, DA = Destination Address,
;       Dn = Data, CS Check Sum (xor of Address and Data bytes)
;  SA must be RS232_RAddr ( the boss )
;  DA must be RS232_MyAddr ( this program )
;  If the checksum matches put the data bytes in the RX_Data buffer,
;   set RXDataValidFlag and RXDataIsNew.
;=========================================================================================
;	constant RP_LongAddr=1		;16 bit addresses
; default to a 6 byte packet SA DA D0 D1 D2 D3
;
	ifndef UseRS232SyncBytes
	constant UseRS232SyncBytes=0
	endif
;
	ifndef RP_LongAddr
	constant RP_LongAddr=0
	endif
;
	ifndef RP_DataBytes
	constant RP_DataBytes=4
	endif
;
	ifndef UseRS232Chksum
	constant UseRS232Chksum=0
	endif
;
	constant RXTimeout=5		;50 mS max between incoming bytes
;=========================================================================================
; Bank0 variable used by RS232_Parse:
;
;	cblock
;	RX_ParseFlags
;	RX_Flags
;	RX_DataCount
;	RX_CSUM
;	RX_TempData:RP_DataBytes
;	RX_Data:RP_DataBytes
;	TX_Data:RP_DataBytes
;	endc
;RX_ParseFlags Bits
;#Define	SyncByte1RXd	RX_ParseFlags,0
;#Define	SyncByte2RXd	RX_ParseFlags,1
;#Define	SourceAddLoRXd	RX_ParseFlags,2
;#Define	SourceAddHiRXd	RX_ParseFlags,3
;#Define	DestAddLoRXd	RX_ParseFlags,4
;#Define	DestAddHiRXd	RX_ParseFlags,5
;#Define	AllDataRXd	RX_ParseFlags,6
;
;RX_Flags Bits
;#Define	RXDataValidFlag	RX_Flags,0
;#Define	RXDataIsNew	RX_Flags,1
;
;
; *** put in main loop
;	CALL	RS232_Parse
; if RXDataValidFlag is set data may be used
;
;=========================================================================================
; Parse any bytes received by the serial port
;
; Receive 0xFF,0xFF,SA0,(SA1),DA0,(DA1),Cmd,Data0,Data1,Data2,CS
;
;
; Entry: none
; Exit: none
; Ram Used: Param78, FSR
; Calls (1+0): GetSerIn
;
RS232_Parse	movf	Timer1Lo,F
	SKPZ
	bra	RS232_Parse_Timing
;timed out
	clrf	RX_ParseFlags
;
RS232_Parse_Timing	CALL	GetSerIn
	BTFSS	Param78,0	;Got a byte?
	RETURN		; No
;
	MOVWF	Param78	;Data Byte RXd
	movlw	RXTimeout
	movwf	Timer1Lo
; Command inturpreter
	if UseRS232SyncBytes
;
	MOVF	RX_ParseFlags,F
	SKPZ		;Beginning new packet?
	BRA	RS232_Parse_Sync2	; No
	MOVLW	0xFF
	SUBWF	Param78,W
	SKPNZ		;Sync byte OK?
	BSF	SyncByte1RXd	; Yes
	return
;
RS232_Parse_Sync2	btfsc	SyncByte2RXd
	bra	RS232_Parse_SALo
	movlw	0xFF
	subwf	Param78,W
	SKPZ
	goto	RS232_Parse_Error
	bsf	SyncByte2RXd
	clrf	RX_CSUM
	clrf	RX_DataCount
	return
;
	endif
;
; LSB of Source Address
RS232_Parse_SALo	btfsc	SourceAddLoRXd
	bra	RS232_Parse_SALo_end
	movlw	low RS232_RAddr
	subwf	Param78,W
	SKPZ
	goto	RS232_Parse_Error
	bsf	SourceAddLoRXd
	movf	Param78,W
	xorwf	RX_CSUM,F
	return
RS232_Parse_SALo_end
;
; MSB of Source Address
	if RP_LongAddr
RS232_Parse_SAHi	btfsc	SourceAddHiRXd
	bra	RS232_Parse_SAHi_end
	movlw	high RS232_RAddr
	subwf	Param78,W
	SKPZ
	goto	RS232_Parse_Error
	bsf	SourceAddHiRXd
	movf	Param78,W
	xorwf	RX_CSUM,F
	return
RS232_Parse_SAHi_end
	endif
;
; LSB of Destination Address
RS232_Parse_DALo	btfsc	DestAddLoRXd
	bra	RS232_Parse_DALo_End
	movlw	low RS232_MyAddr
	subwf	Param78,W
	SKPZ
	goto	RS232_Parse_Error
	bsf	DestAddLoRXd
	movf	Param78,W
	xorwf	RX_CSUM,F
	return
RS232_Parse_DALo_End
;
; MSB of Destination Address
	if RP_LongAddr
RS232_Parse_DAHi	btfsc	DestAddHiRXd
	bra	RS232_Parse_DAHi_end
	movlw	high RS232_MyAddr
	subwf	Param78,W
	SKPZ
	goto	RS232_Parse_Error
	bsf	DestAddHiRXd
	movf	Param78,W
	xorwf	RX_CSUM,F
	return
RS232_Parse_DAHi_end
	endif
;
RS232_Parse_Data	btfsc	AllDataRXd
	bra	RS232_Parse_Data_end
	movf	RX_DataCount,W
	addlw	low RX_TempData
	movwf	FSR0L
	movlw	high RX_TempData
	movwf	FSR0H
	movf	Param78,W
	xorwf	RX_CSUM,F
	movwf	INDF0
	incf	RX_DataCount,F
	movlw	RP_DataBytes
	subwf	RX_DataCount,W
	SKPZ
	return
	bsf	AllDataRXd
;
	if UseRS232Chksum
	return
	endif
RS232_Parse_Data_end
;
	if UseRS232Chksum
; checksum
	movf	Param78,W
	subwf	RX_CSUM,W
	SKPZ		;Checksum matches?
	bra	RS232_Parse_Error	; No
;
	endif
;
	movlw	low RX_TempData
	movwf	FSR0L
	movlw	high RX_TempData
	movwf	FSR0H
;
	movlw	low RX_Data
	movwf	FSR1L
	movlw	high RX_Data
	movwf	FSR1H
;
	movlw	RP_DataBytes
	movwf	Param78
RS232_Parse_L1	moviw	FSR0++
	movwi	FSR1++
	decfsz	Param78,F
	bra	RS232_Parse_L1
;
	bsf	RXDataValidFlag
	bsf	RXDataIsNew
	clrf	RX_ParseFlags
	return
;
;
; We didn't get the sync byte we were expecting so restart the parser
RS232_Parse_Error	CLRF	RX_ParseFlags
	GOTO	RS232_Parse
;
;=========================================================================================
; Clear the TX data, unused bytes are sent as 0x00
; Entry: none
; Exit: Bank 0
; Ram used: Param78, FSR1
; Calls: none
RS232_ClearTXData	movlw	0	;bank 0
	movlw	RP_DataBytes
	movwf	Param78
	movlw	low TX_Data
	movwf	FSR1L
	movlw	high TX_Data
	movwf	FSR1H
	movlw	0x00
RS232_ClearTXData_L1	movwi	FSR1++
	decfsz	Param78,F
	bra	RS232_ClearTXData_L1
	return
;
;=========================================================================================
; Send a data packet
;
;Ram used: Param78,Param79,Param7A,FSR0,FSR1
;Calls: (1+0) StoreSerOut
;
RS232_Send	movlb	0	;bank 0
;
	if UseRS232SyncBytes
	movlw	0xFF
	call	StoreSerOut
	movlw	0xFF
	call	StoreSerOut
	endif
;
	clrf	Param7A	;CSum
	movlw	low RS232_MyAddr	;source address
	xorwf	Param7A,F
	call	StoreSerOut
;
	if RP_LongAddr
	movlw	high RS232_MyAddr	;source address
	xorwf	Param7A,F
	call	StoreSerOut
	endif
;
	movlw	low RS232_RAddr	;destination address
	xorwf	Param7A,F
	call	StoreSerOut
;
	if RP_LongAddr
	movlw	high RS232_RAddr	;destination address
	xorwf	Param7A,F
	call	StoreSerOut
	endif
;
	movlw	RP_DataBytes
	movwf	Param79
	movlw	low TX_Data
	movwf	FSR1L
	movlw	high TX_Data
	movwf	FSR1H
;
RS232_Send_L1	moviw	FSR1++
	xorwf	Param7A,F
	call	StoreSerOut
	decfsz	Param79,F
	bra	RS232_Send_L1
;
	if UseRS232Chksum
	movf	Param7A,W
	call	StoreSerOut
	endif
;
	goto	RS232_ClearTXData
;
;=========================================================================================
;






















