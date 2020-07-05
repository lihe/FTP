all:ftpclient ftpserver
ftpclient:ftp_client.c ftp.h
	gcc  ftp_client.c -o ftp_cli
ftpserver:ftp_server.c ftp.h
	gcc  ftp_server.c -o ftp_srv	-lpthread
clean:
	rm -f ftp_cli ftp_srv

