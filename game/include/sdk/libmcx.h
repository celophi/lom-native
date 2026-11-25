#ifndef SDK_LIBMCX_H
#define SDK_LIBMCX_H

#define McxSyncRun 0
#define McxSyncNone (-1)
#define McxSyncFin 1

#define McxErrSuccess 0
#define McxErrNoCard 1
#define McxErrInvalid 2
#define McxErrNewCard 3

#define MCX_SYNC_WAIT 0

#define MCX_COMMAND_ISSUED 1

void McxStartCom(void);
int McxSync(int mode, long* cmd, long* result);
int McxGetTime(int port, unsigned char* time);
int McxCardType(int port);
int McxHideTrans(int port);
int McxExecFlag(int port, int block, int exec);

#endif
