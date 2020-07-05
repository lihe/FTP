#ifndef __FTP_H_
#define __FTP_H_

#define COMMAND_NUM 7

#define FILE_TRANS_MODE_ASIC 0
#define FILE_TRANS_MODE_BIN 1

char clientCommand[COMMAND_NUM][20]={{"pwd"}, {"quit"}, {'?'}, {"ls"}, {"cd"}, {"get"}, {"put"}};

char serverInfo220[]="220 myFTP Server ready...\r\n";
char serverInfo230[]="230 User logged in, proceed.\r\n";
char serverInfo331[]="331 User name okay, need password.\r\n";
char serverInfo221[]="221 Goodbye!\r\n";
char serverInfo150[]="150 File status okay; about to open data connection.\r\n";
char serverInfo226[]="226 Closing data connection.\r\n";
char serverInfo200[]="200 Command okay.\r\n";
char serverInfo215[]="215 Unix Type FC5.\r\n";
char serverInfo213[]="213 File status.\r\n";
char serverInfo211[]="211 System status, or system help reply.\r\n";
char serverInfo350[]="350 Requested file action pending further information.\r\n";
char serverInfo530[]="530 Not logged in.\r\n";
char serverInfo531[]="531 Not root client. Anonymous client.\r\n";
char serverInfo[]="202 Command not implemented, superfluous at this site.\r\n";

#endif