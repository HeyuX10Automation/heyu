
/* Bitfields for DigiMax 210 */

#define PARMASK     0x0f0000ff   /* Parity bits (to be discarded) */
#define STATMASK    0x30000000   /* Input Status field */
#define TCURRMASK   0x00ff0000   /* Current Temperature field */
#define TSETPMASK   0x00003f00   /* Temperature Setpoint field */
#define HEATMASK    0x00004000   /* Heat mode bit (0 = heat, 1 = cool) */
#define ONOFFMASK   0x03000000   /* Stored On/Off Status field */
#define UNDEFMASK   0xc0008000   /* Undefined Status bits */
#define VALMASK     0x00000001   /* Valid stored data (temp) bit */
#define SETPFLAG    0x00000002   /* Valid stored setpoint bit */
#define TCURRSHIFT  16
#define TSETPSHIFT   8
#define STATSHIFT   28
#define ONOFFSHIFT  24
#define HEATSHIFT   14

/* Is it necessary to swap On/Off when the DigiMax is operated  */
/* in cool mode (for agreement with On/Off PLC code transmitted */
/* by the base station) ?  YES|NO                               */
#define DMXCOOLSWAP    YES

