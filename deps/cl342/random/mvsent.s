**********************************************************************
*
* DESCRIPTION
* -----------
*   THIS MODULE GATHERS DATA TO PROVIDE ENTROPY FOR THE CRYPTLIB
*   RNDMVS.C MODULE.
*
*   ENTROPY IS GATHERED BY OBTAINING THE TOD VALUE INTERMIXED WITH
*   TASKS THAT PASS CONTROL BACK TO MVS TO PROCESS.  ENTROPY IS
*   INTRODUCED FROM THE FACT THAT THE AMOUNT OF TIME MVS TAKES TO
*   PROCESS THE REQUESTS IS UNKNOWN AND DEPENDENT ON FACTORS NOT IN
*   OUR CONTROL.
*
* INPUT
* -----
*   R1 = PARM ADDRESS
*        +0 = LENGTH OF INPUT BUFFER.  MUST BE IN THE RANGE OF 1 TO
*             2,000,000
*        +4 = ADDRESS OF BUFFER TO PLACE THE RANDOM DATA.
*
* OUTPUT
* ------
*   R15 = RETURN CODE
*         0 = SUCCESSFUL
*         1 = PARAMETER LIST ERROR
*         2 = O/S MACRO ERROR
*
* ENVIRONMENT
* -----------
*   ENVIRONMENT:       LANGUAGE ENVIRONMENT
*   AUTHORIZATION:     PROBLEM STATE
*   CROSS MEMORY MODE: PASN=HASN=SASN
*   AMODE:             31
*   RMODE:             31
*   INTERUPT STATUS:   EXTERNAL INTERUPTS
*   LOCKS:             NONE
*
**********************************************************************
MVSENT   CEEENTRY MAIN=NO,AUTO=LDSSZ
         USING LDS,CEEDSA
**********************************************************************
*
* LOAD INPUT PARAMETERS.
*
**********************************************************************
         L     R3,0(,R1)               LOAD LENGTH
         LTR   R3,R3                   ZERO?
         BNH   EXIT01                  YES
         C     R3,=F'2000000'          MORE THAN MAX BYTES ?
         BH    EXIT01                  YES, IS ERROR
         ST    R3,BUFLEN               SAVE BUFFER LENGTH
*
         ICM   R3,B'1111',4(R1)        LOAD ADR OF BUFFER
         BZ    EXIT01                  IMPROPER PARAMETER LIST
         ST    R3,BUFADR               SAVE BUFFER ADDRESS
**********************************************************************
*
* MAIN FUNCTION.
*
* OBTAIN THE TOD VALUE INTERMIXED WITH TASKS THAT PASS CONTROL
* BACK TO MVS TO PROCESS.  ENTROPY IS INTRODUCED FROM THE FACT THAT
* THE AMOUNT OF TIME MVS TAKES TO PROCESS THE REQUESTS IS UNKNOWN
* AND DEPENDENT ON FACTORS NOT IN OUR CONTROL.
**********************************************************************
*
* EACH LOOP GATHERS 8 BYTES OF DATA.
*
ENLOOP   DS    0H
         LA    R2,WBUF                 LOAD ADR OF WORKING BUFFER
*
         MVC   PLIST(#ATTACH),$ATTACH  INIT PARM LIST
         LA    R4,0                    LOAD NULL
         ST    R4,STECB                ZERO ECB
         LA    R4,STECB                LOAD A(ECB)
         ATTACH ECB=(R4),                                              +
               SF=(E,PLIST)
         LTR   R15,R15                 SUCCESSFUL?
         BNZ   EXIT02                  FAILED
         ST    R1,STTCB                SAVE ADR OF TCB
*
         STCK  TOD                     OBTAIN TOD VALUE
         MVC   0(1,R2),TODB7           SAVE BYTE 7
         LA    R2,1(,R2)               INCR ADR
*
         WAIT  ECB=(R4)                WAIT FOR TASK TO COMPLETE
         LA    R4,STTCB                LOAD A TCB
         DETACH (R4)                   DETACK TASK
*
         STCK  TOD                     OBTAIN TOD VALUE
         MVC   0(1,R2),TODB7           SAVE BYTE 7
         LA    R2,1(,R2)               INCR ADR
*
         GETMAIN RC,LV=50000,LOC=(31,31)
         LTR   R15,R15                 SUCCESSFUL?
         BNZ   EXIT02                  NO, EXIT
         LR    R4,R1
*
         STCK  TOD                     OBTAIN TOD VALUE
         MVC   0(1,R2),TODB7           SAVE BYTE 7
         LA    R2,1(,R2)               INCR ADR
*
         MVC   PLIST(#GQSCAN),$GQSCAN  INIT PARM LIST
         GQSCAN AREA=((R4),50000),                                     +
               MF=(E,PLIST)
*
         STCK  TOD                     OBTAIN TOD VALUE
         MVC   0(1,R2),TODB7           SAVE BYTE 7
         LA    R2,1(,R2)               INCR ADR
*
         FREEMAIN RU,LV=50000,A=(R4)
*
         STCK  TOD                     OBTAIN TOD VALUE
         MVC   0(1,R2),TODB7           SAVE BYTE 7
         LA    R2,1(,R2)               INCR ADR
*
         LOAD  EP=A@#$#@Z,ERRET=NOTFND
NOTFND   DS    0H
*
         STCK  TOD                     OBTAIN TOD VALUE
         MVC   0(1,R2),TODB7           SAVE BYTE 7
         LA    R2,1(,R2)               INCR ADR
*
         MVC   PLIST(#ENQ),$ENQ        INIT PARM LIST
         ENQ   MF=(E,PLIST)
         MVC   PLIST(#DEQ),$DEQ        INIT PARM LIST
         DEQ   MF=(E,PLIST)
*
         STCK  TOD                     OBTAIN TOD VALUE
         MVC   0(1,R2),TODB7           SAVE BYTE 7
         LA    R2,1(,R2)               INCR ADR
*
         MVC   PLIST(#STIMERM),$STIMERM INIT PARM LIST
         LA    R4,TIMERID              LOAD ADR OF TIME ID FIELD
         XC    TIMEECB,TIMEECB         CLEAR ECB
         LA    R1,TIMEECB
         ST    R1,TIMEECB@             SAVE ADR ECB PARM
         STIMERM SET,                                                  +
               WAIT=NO,                                                +
               EXIT=TIMEEXIT,                                          +
               PARM=TIMEECB@,                                          +
               TUINTVL=WAITTIME,                                       +
               ID=(R4),                                                +
               MF=(E,PLIST)
         LTR   R15,R15                 SUCCESSFUL?
         BNZ   EXIT02                  NO, EXIT
         LA    R4,TIMEECB              LOAD ADR OF ECB
         WAIT  ECB=(R4)                WAIT FOR TIMER TO COMPLETE
*
         STCK  TOD                     OBTAIN TOD VALUE
         MVC   0(1,R2),TODB7           SAVE BYTE 7
         LA    R2,1(,R2)               INCR ADR
*
*        COPY THE DATA TO THE USER BUFFER.
*
         LA    R1,WBUF                 LOAD ADR OF BUFFER
         SR    R2,R1                   CALC LEN OF DATA
         L     R3,BUFLEN               LOAD REMAINING BUF LEN
         CR    R2,R3                   HOW MUCH ROOM LEFT?
         BL    COPYDATA                ITS ENOUGH ROOM
         LR    R2,R3                   SET LEN TO REMAINING ROOM
*
COPYDATA DS    0H
         L     R4,BUFADR               LOAD ADR OF CALLER'S BUF
         LR    R5,R2                   LOAD LEN OF DATA
         LA    R6,WBUF                 LOAD ADR OF DATA
         LR    R7,R5                   LOAD LEN OF DATA
         MVCL  R4,R6
*
         ST    R4,BUFADR               SAVE OFFSET INTO BUF
         L     R3,BUFLEN               LOAD REMAINING LEN OF BUF
         SR    R3,R2                   CALC NEW REMAINING LEN
         ST    R3,BUFLEN               SAVE NEW REMAINING LEN
*
         LTR   R3,R3                   END OF BUF
         BH    ENLOOP                  NO, CONTINUE
*
         B     EXIT00                  EXIT
*
**********************************************************************
*
* RETURN TO CALLER.
*
**********************************************************************
EXIT01   DS    0H
         LA    R15,1
         B     EXIT
EXIT02   DS    0H
         LA    R15,2
         B     EXIT
EXIT00   DS    0H
         LA    R15,0
EXIT     DS    0H
         CEETERM RC=(R15)
**********************************************************************
*
*  CONSTANTS
*
**********************************************************************
PPA      CEEPPA ,
*
WAITTIME DC    F'2'                         ~(2*26) MICROSECONDS
QNAME    DC    C'CRYPTLIB'                  ENQ QNAME
RNAME    DC    C'RANDOM'                    ENQ RNAME
*
$ATTACH  ATTACH EP=IEFBR14,                                            +
               DPMOD=-255,                                             +
               SF=L
#ATTACH  EQU   *-$ATTACH
*
$GQSCAN  GQSCAN SCOPE=ALL,                                             +
               XSYS=YES,                                               +
               MF=L
#GQSCAN  EQU   *-$GQSCAN
*
$ENQ     ENQ   (QNAME,RNAME,S,,SYSTEMS),MF=L
#ENQ     EQU   *-$ENQ
*
$DEQ     DEQ   (QNAME,RNAME,,SYSTEMS),MF=L
#DEQ     EQU   *-$ENQ
*
$STIMERM STIMERM SET,MF=L
#STIMERM EQU   *-$STIMERM
*
         LTORG ,
         YREGS ,
*
**********************************************************************
*
*  STIMERM ASYNC EXIT
*
**********************************************************************
         DROP
TIMEEXIT DS    0H
         USING TIMEEXIT,R15
         L     R2,4(,R1)               LOAD ADR OF ECB
         POST  (R2)                    POST ECB
         BR    R14                     RETURN TO CALLER
         DROP  R15
*
**********************************************************************
*
*  DYNAMIC STORAGE AREA
*
**********************************************************************
LDS      DSECT ,
         DS    XL(CEEDSASZ)                 DSA STORAGE
*
BUFADR   DS    A                            CALLER'S BUFFER ADR
BUFLEN   DS    H                            CALLER'S BUFFER LEN
*
TOD      DS    0D                           TIME-OF-DAY VALUE
TODB1    DS    B
TODB2    DS    B
TODB3    DS    B
TODB4    DS    B
TODB5    DS    B
TODB6    DS    B
TODB7    DS    B
TODB8    DS    B
*
STECB    DS    F                            SUBTASK ECB
STTCB    DS    A                            SUBTASK ADR
TIMERID  DS    F                            STIMERM TIMER ID
TIMEECB  DS    F                            STIMERM ECB
TIMEECB@ DS    A                            ADR OF STIMERM ECB
PLIST    DS    XL256                        GENERAL PARM LIST
WBUF     DS    CL100                        WORKING BUFFER
*
         DS    0D                           END LDS ON DOUBLE WORD
LDSSZ    EQU   *-LDS
*
**********************************************************************
*
*  MAPPING DSECTS
*
**********************************************************************
         CEECAA ,                      LE COMMON ANCHOR AREA
         CEEDSA ,                      LE DYNAMIC STORAGE AREA
*
         END    MVSENT
