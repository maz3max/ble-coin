#define KX022_REG_XHPL 0x00	//R
#define KX022_REG_XHPH 0x01	//R
#define KX022_REG_YHPL 0x02	//R
#define KX022_REG_YHPH 0x03	//R
#define KX022_REG_ZHPL 0x04	//R
#define KX022_REG_ZHPH 0x05	//R
#define KX022_REG_XOUTL 0x06	//R
#define KX022_REG_XOUTH 0x07	//R
#define KX022_REG_YOUTL 0x08	//R
#define KX022_REG_YOUTH 0x09	//R
#define KX022_REG_ZOUTL 0x0A	//R
#define KX022_REG_ZOUTH 0x0B	//R
#define KX022_REG_COTR 0x0C	//R
#define KX022_REG_Who_AM_I 0x0F	//R/W
#define KX022_REG_TSCP 0x10	//R
#define KX022_REG_TSPP 0x11	//R
#define KX022_REG_INS1 0x12	//R
#define KX022_REG_INS2 0x13	//R
#define KX022_REG_INS3 0x14	//R
#define KX022_REG_STAT 0x15	//R
#define KX022_REG_INT_REL 0x17	//R
#define KX022_REG_CNTL1 0x18	//R/W
#define KX022_REG_CNTL2 0x19	//R/W
#define KX022_REG_CNTL3 0x1A	//R/W
#define KX022_REG_ODCNTL 0x1B	//R/W
#define KX022_REG_INC1 0x1C	//R/W
#define KX022_REG_INC2 0x1D	//R/W
#define KX022_REG_INC3 0x1E	//R/W
#define KX022_REG_INC4 0x1F	//R/W
#define KX022_REG_INC5 0x20	//R/W
#define KX022_REG_INC6 0x21	//R/W
#define KX022_REG_TILT_TIMER 0x22	//R/W
#define KX022_REG_WUFC 0x23	//R/W
#define KX022_REG_TDTRC 0x24	//R/W
#define KX022_REG_TDTC 0x25	//R/W
#define KX022_REG_TTH 0x26	//R/W
#define KX022_REG_TTL 0x27	//R/W
#define KX022_REG_FTD 0x28	//R/W
#define KX022_REG_STD 0x29	//R/W
#define KX022_REG_TLT 0x2A	//R/W
#define KX022_REG_TWS 0x2B	//R/W
#define KX022_REG_ATH 0x30	//R/W
#define KX022_REG_TILT_ANGLE_LL 0x32	//R/W
#define KX022_REG_TILT_ANGLE_HL 0x33	//R/W
#define KX022_REG_HYST_SET 0x34	//R/W
#define KX022_REG_LP_CNTL 0x35	//R/W
#define KX022_REG_BUF_CNTL1 0x3A	//R/W
#define KX022_REG_BUF_CNTL2 0x3B	//R/W
#define KX022_REG_BUF_STATUS_1 0x3C	//R
#define KX022_REG_BUF_STATUS_2 0x3D	//R
#define KX022_REG_BUF_CLEAR 0x3E	//W
#define KX022_REG_BUF_READ 0x3F	//R
#define KX022_REG_SELF_TEST 0x60	//R/W

// bits of KX022_REG_ODCNTL
#define KX022_BIT_OSA0 (1 << 0)
#define KX022_BIT_OSA1 (1 << 1)
#define KX022_BIT_OSA2 (1 << 2)
#define KX022_BIT_OSA3 (1 << 3)
#define KX022_BIT_LPRO (1 << 6)
#define KX022_BIT_IIR_BYPASS (1 << 7)

// bits of KX022_REG_CNTL1
#define KX022_BIT_TPE (1 << 0)
#define KX022_BIT_WUFE (1 << 1)
#define KX022_BIT_TDTE (1 << 2)
#define KX022_BIT_GSEL0 (1 << 3)
#define KX022_BIT_GSEL1 (1 << 4)
#define KX022_BIT_DRDYE (1 << 5)
#define KX022_BIT_RES (1 << 6)
#define KX022_BIT_PC1 (1 << 7)

// bits of KX022_REG_INC1
#define KX022_BIT_SPI3E (1 << 0)
#define KX022_BIT_STPOL (1 << 1)
#define KX022_BIT_IEL1 (1 << 3)
#define KX022_BIT_IEA1 (1 << 4)
#define KX022_BIT_IEN1 (1 << 5)

//bits of KX022_REG_INC2
#define KX022_BIT_ZPWUE (1 << 0)
#define KX022_BIT_ZNWUE (1 << 1)
#define KX022_BIT_YPWUE (1 << 2)
#define KX022_BIT_YNWUE (1 << 3)
#define KX022_BIT_XPWUE (1 << 4)
#define KX022_BIT_XNWUE (1 << 5)

//bits of KX022_REG_INC3
#define KX022_BIT_TFUM (1 << 0)
#define KX022_BIT_TFDM (1 << 1)
#define KX022_BIT_TUPM (1 << 2)
#define KX022_BIT_TDOM (1 << 3)
#define KX022_BIT_TRIM (1 << 4)
#define KX022_BIT_TLEM (1 << 5)

//bits of KX022_REG_INC4
#define KX022_BIT_TPI1 (1 << 0)
#define KX022_BIT_WUFI1 (1 << 1)
#define KX022_BIT_TDTI1 (1 << 2)
#define KX022_BIT_DRDYI1 (1 << 4)
#define KX022_BIT_WMI1 (1 << 5)
#define KX022_BIT_BFI1 (1 << 6)


#define KX022_WHO_AM_I_VAL 0x14
