#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <curses.h>
#include <termios.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>


#define DEFAULT_FTP_PORT 21

extern int h_errno;

char user[64]; //用户名
char passwd[64];  //密码

//ftp address
struct sockaddr_in ftp_server, local_host;

//主机信息
struct hostent * server_hostent;

int sock_control;

//ftp mode, 0 is PORT, 1 is PASV
int mode = 1;

static struct termios stored_settings;

//获取用户名和密码
void echo_off(void){
	struct termios new_settings;

	tcgetattr(0, &stored_settings);

	new_settings = stored_settings;
	new_settings.c_lflag &= (~ECHO);

	tcsetattr(0, TCSANOW, &new_settings);
	return;
}

void echo_on(void){
	tcsetattr(0, TCSANOW, &stored_settings);
	return;
}

//报错
void cmd_err_exit(char * err_msg, int err_code){
	printf("%s\n", err_msg);
	exit(err_code);
}

int fill_host_addr(char * host_ip_addr, struct sockaddr_in * host, int port){
	if(port <= 0 || port > 65535){
		return 254;
	}

	bzero(host, sizeof(struct sockaddr_in));

	host -> sin_family = AF_INET;

	if(inet_addr(host_ip_addr) != -1){
		host -> sin_addr.s_addr = inet_addr(host_ip_addr);
	}
	else{
		if((server_hostent = gethostbyname(host_ip_addr)) != 0){
			memcpy(&host -> sin_addr, server_hostent -> h_addr, sizeof(host -> sin_addr));
		}
		else
			return 253;
	}

	host -> sin_port = htons(port);
	return 1;
}

//连接到服务器
int xconnect(struct sockaddr_in * s_addr, int type){
	struct timeval outtime;
	int set;
	int s = socket(AF_INET, SOCK_STREAM, 0);
	if(s < 0){
		cmd_err_exit("Create socket error!", 249);
	}

	if(type == 1){
		outtime.tv_sec = 0;
		outtime.tv_usec = 300000;
	}
	else{
		outtime.tv_sec = 5;
		outtime.tv_usec = 0;
	}

	set = setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &outtime, sizeof(outtime));

	if(set != 0){
		printf("Set socket %s errno:%d\n", strerror(errno), errno);
		cmd_err_exit("set socket", 1);
	}

	//连接到服务器，如果连接成功，connect函数返回0，如果不成功，返回-1。
	if(connect(s, (struct sockaddr *)s_addr, sizeof(struct sockaddr_in)) < 0){
		printf("Can't connect to server %s, prot %d\n", inet_ntoa(s_addr -> sin_addr), ntohs(ftp_server.sin_port));
		exit(252);
	}
	return s;
}

//向服务器发送客户端的命令的函数。
int ftp_send_cmd(const char *s1, const char *s2, int sock_fd){
	char send_buf[256];
	int send_err, len;

	if(s1){
		strcpy(send_buf, s1);
		if(s2){
			strcat(send_buf, s2);
			strcat(send_buf, "\r\n");
			len = strlen(send_buf);
			send_err = send(sock_fd, send_buf, len, 0);
		}
		else{
			strcat(send_buf, "\r\n");
			len = strlen(send_buf);
			send_err = send(sock_fd, send_buf, len, 0);
		}
	}

	if(send_err < 0){
		printf("Send() error!\n");
	}
	return send_err;
}

//接收服务器返回信息
int ftp_get_reply(int sock_fd){
	static int reply_code = 0, count = 0;
	char rcv_buf[512];
	count = read(sock_fd, rcv_buf, 510);
	if(count > 0){
		reply_code = atoi(rcv_buf);
	}
	else{
		return 0;
	}

	while(1){
		if(count <= 0){
			break;
		}

		rcv_buf[count] = '\0';
		printf("%s\n", rcv_buf);
		count = read(sock_fd, rcv_buf, 510);
	}
	return reply_code;
}



int get_port(){
	char port_respond[512];

	char *buf_ptr;

	int count, port_num;

	ftp_send_cmd("PASV", NULL, sock_control);

	count = read(sock_control, port_respond, 510);

	if(count <= 0){
		return 0;
	}

	port_respond[count] = '\0';
	if(atoi(port_respond) == 227){
		buf_ptr = strrchr(port_respond, ',');
		port_num = atoi(buf_ptr + 1);

		*buf_ptr = '\0';

		buf_ptr = strrchr(port_respond, ',');
		port_num += atoi(buf_ptr + 1) * 256;
		return port_num;
	}

	return 0;
}


int rand_local_port(){
	int local_port;

	srand((unsigned)time(NULL));
	local_port = rand() % 10000 + 1025;
	return local_port;
}


//连接服务器数据流
int xconnect_ftpdata(){

	//PASV
	if(mode){
		int data_port = get_port();
		if(data_port != 0)
			ftp_server.sin_port = htons(data_port);

		return(xconnect(&ftp_server, 0));
	}
	//PORT
	else{
		int client_port, get_sock, opt, set;

		char cmd_buf[32];

		struct timeval outtime;

		struct sockaddr_in local;

		char local_ip[24];
		char *ip_1, *ip_2, *ip_3, *ip_4;
		int addr_len = sizeof(struct sockaddr);
		client_port = rand_local_port();
		get_sock = socket(AF_INET, SOCK_STREAM, 0);

		if(get_sock < 0){
			cmd_err_exit("socket()", 1);
		}



		outtime.tv_sec = 7;
		outtime.tv_usec = 0;

		opt = SO_REUSEADDR;

		set = setsockopt(get_sock, SOL_SOCKET, SO_RCVTIMEO, &outtime, sizeof(outtime));

		if(set != 0){
			printf("Set socket %s errno:%d\n", strerror(errno), errno);
			cmd_err_exit("set socket", 1);
		}

		set = setsockopt(get_sock, SOL_SOCKET, SO_REUSEADDR, &opt,sizeof(opt));
		if(set !=0)
		{
			printf("set socket %s errno:%d\n",strerror(errno),errno);
			cmd_err_exit("set socket", 1);
		}

		bzero(&local_host, sizeof(local_host));

		local_host.sin_family = AF_INET;
		local_host.sin_port = htons(client_port);;

		local_host.sin_addr.s_addr = htonl(INADDR_ANY);

		bzero(&local, sizeof(struct sockaddr));

		while(1){
			set = bind(get_sock, (struct sockaddr *)& local_host, sizeof(local_host));

			if(set != 0 && errno == 11){
				client_port = rand_local_port();
				continue;
			}

			set = listen(get_sock, 1);
			if(set != 0 && errno == 11){
				cmd_err_exit("listen()", 1);
			}

			//获取ip地址
			if(getsockname(sock_control, (struct sockaddr *)&local, (socklen_t *)&addr_len) < 0)
				return -1;

			snprintf(local_ip, sizeof(local_ip), inet_ntoa(local.sin_addr));

			local_ip[strlen(local_ip)] = '\0';

			ip_1 = local_ip;

			ip_2 = strchr(local_ip, '.');

			*ip_2 = '\0';

			ip_2++;

			ip_3 = strchr(ip_2, '.');

			*ip_3 = '\0';

			ip_3++;

			ip_4 = strchr(ip_3, '.');

			*ip_4 = '\0';

			ip_4++;

			snprintf(cmd_buf, sizeof(cmd_buf), "PORT %s, %s, %s, %s, %d, %d", ip_1, ip_2, ip_3, ip_4, client_port >> 8, client_port & 0xff);
			ftp_send_cmd(cmd_buf, NULL, sock_control);

			if(ftp_get_reply(sock_control) != 200){
				printf("Can not use PORT mode! Please use \"mode\" change to PASV mode.\n");
				return -1;
			}
			else
				return get_sock;

		}
	}
}


//显示服务器当前目录下文件
void ftp_list(){
	int i = 0, new_sock;
	int set = sizeof(local_host);

	int list_sock_data = xconnect_ftpdata();

	if(list_sock_data < 0){
		ftp_get_reply(sock_control);

		printf("Create data sock error!\n");

		return;
	}

	ftp_get_reply(sock_control);

	ftp_send_cmd("LIST", NULL, sock_control);

	ftp_get_reply(sock_control);

	if(mode)
		//PASV
		ftp_get_reply(list_sock_data);
	else{
		//PORT
		while(i < 3){
			new_sock = accept(list_sock_data, (struct sockaddr *)& local_host, (socklen_t *)& set);
			if(new_sock == -1){
				printf("Accept return:%s errno:%d\n", strerror(errno), errno);
				i++;
				continue;
			}
			else
				break;
		}

		if(new_sock == -1){
			printf("Sorry, you can not use PORT mode. There is something wrong when the server connect to you.\n");
			return;
		}

		ftp_get_reply(new_sock);
		close(new_sock);
	}


	close(list_sock_data);
	ftp_get_reply(sock_control);
}


//读取客户端命令后的文件名字
void ftp_cmd_filename(char * user_cmd, char * src_file, char * dst_file){
	int length, flag = 0;

	int i = 0, j = 0;

	char *cmd_src;
	cmd_src = strchr(user_cmd, ' ');
	if(cmd_src == NULL){
		printf("Command error!\n");
		return;
	}

	else{
		while(*cmd_src == ' ')
			cmd_src++;
	}

	if(cmd_src == NULL || cmd_src == '\0'){
		printf("Command error!\n");
	}
	else{
		length = strlen(cmd_src);

		while(i <= length){
			if((*(cmd_src + i)) != ' ' && (*(cmd_src + i)) != '\\'){
				if(flag == 0){
					src_file[j] = *(cmd_src + i);
				}
				else{
					dst_file[j] = *(cmd_src + i);
				}

				j++;
			}

			if((*(cmd_src + i)) == '\\' && (*(cmd_src + i + 1)) != ' '){
				if(flag == 0)
					src_file[j] = *(cmd_src + i);
				else
					dst_file[j] = *(cmd_src + i);
				j++;
			}

			if((*(cmd_src + i)) == ' ' && (*(cmd_src + i - 1)) != '\\'){
				
				src_file[j] = '\0';
				flag = 1;
				j = 0;
			}

			if((*(cmd_src + i)) == '\\' && (*(cmd_src + i + 1)) == ' '){
				if(flag == 0)
					src_file[j] = ' ';
				else
					dst_file[j] = ' ';
				j++;
			}
			i++;
		}
	}
	if(flag == 0)
			strcpy(dst_file, src_file);
		else
			dst_file[j] = '\0';

}

//get命令
void ftp_get(char * user_cmd){
	int get_sock, set, new_sock, i = 0;

	char src_file[512];
	char dst_file[512];
	char rcv_buf[512];
	char cover_flag[3];
	struct stat file_info;
	int local_file;
	int count = 0;

	ftp_cmd_filename(user_cmd, src_file, dst_file);

	ftp_send_cmd("SIZE ",src_file, sock_control);

	if(ftp_get_reply(sock_control) != 213){
		printf("SIZE error!\n");
		return;
	}


	if(!stat(dst_file, &file_info)){
		printf("local file %s exists: %d bytes\n", dst_file, (int)file_info.st_size);
		printf("Do you want to cover it? [y/n]");

		fgets(cover_flag, sizeof(cover_flag), stdin);

		fflush(stdin);

		if(cover_flag[0] != 'y'){
			printf("Get file %s aborted.\n", src_file);
			return;
		}
	}

	local_file = open(dst_file, O_CREAT|O_TRUNC|O_WRONLY);

	if(local_file < 0){
		printf("Create local file %s error!\n", dst_file);
		return;
	}

	get_sock = xconnect_ftpdata();

	if(get_sock < 0){
		printf("Socket error!\n");
		return;
	}

	set = sizeof(local_host);

	ftp_send_cmd("TYPE I", NULL, sock_control);
	ftp_get_reply(sock_control);

	ftp_send_cmd("RETR", src_file, sock_control);

	//PORT
	if(!mode){
		while(i < 3){
			new_sock = accept(get_sock, (struct sockaddr *)&local_host, (socklen_t *)&set);
			if(new_sock == -1){
				printf("Accept return:%s errno:%d\n", strerror(errno), errno);
				i++;
				continue;
			}
			else
				break;
		}

		if(new_sock == -1){
			printf("Sorry, you can not use PORT mdoe. There is something wrong when the server connect to you.\n");

			return;
		}

		ftp_get_reply(sock_control);

		while(1){
			printf("loop \n");
			count = read(new_sock, rcv_buf, sizeof(rcv_buf));

			if(count <= 0)
				break;

			else{
				write(local_file, rcv_buf, count);
			}
		}

		close(local_file);

		close(get_sock);

		close(new_sock);


		ftp_get_reply(sock_control);
	}
	else {
		ftp_get_reply(sock_control);

		while(1){
			count = read(get_sock, rcv_buf, sizeof(rcv_buf));
			if(count <= 0)
				break;
			else
				write(local_file, rcv_buf, count);
		}

		close(local_file);

		close(get_sock);

		ftp_get_reply(sock_control);
	}

	if(!chmod(src_file, 0644)){
		printf("Chmod %s to 0644!\n", dst_file);
		return;
	}
	else
		printf("Chmod %s to 0644 error!\n", dst_file);

	ftp_get_reply(sock_control);
}

//put命令
void ftp_put(char * user_cmd){
	char src_file[512];
	char dst_file[512];
	char send_buf[512];
	struct stat file_info;

	int local_file;

	int file_put_sock, new_sock, count = 0, i = 0;

	int set = sizeof(local_host);

	ftp_cmd_filename(user_cmd, src_file, dst_file);

	if(stat(src_file, &file_info) < 0){
		printf("Local file %s doesn't exist!\n", src_file);
		return;
	}

	local_file = open(src_file, O_RDONLY);

	if(local_file < 0){
		printf("Open local file %s error!\n", dst_file);
		return;
	}

	file_put_sock = xconnect_ftpdata();

	if(file_put_sock < 0){
		ftp_get_reply(sock_control);

		printf("Create data sock error!\n");

		return;
	}


	ftp_send_cmd("STOR ", dst_file, sock_control);

	ftp_get_reply(sock_control);

	ftp_send_cmd("TYPE I", NULL, sock_control);

	ftp_get_reply(sock_control);

	//PORT

	if(!mode){

		while(i < 3){
			new_sock = accept(file_put_sock, (struct sockaddr *)&local_host, (socklen_t *)&set);

			if(new_sock == -1){
				printf("accept return:%s errno:%d\n", strerror(errno), errno);
				i++;
				continue;
			}
			else
				break;
		}

		if(new_sock == -1){
			printf("Sorry, you can not use PORT mode, There is someting wrong when the server connect to you.\n");
			return;
		}

		while(1){
			count = read(local_file, send_buf, sizeof(send_buf));

			if(count <= 0)
				break;
			else 
				write(new_sock, send_buf, sizeof(send_buf));
		}

		close(local_file);

		close(file_put_sock);

		close(new_sock);
	}
	else{

		while(1){
			count = read(local_file, send_buf, sizeof(send_buf));
			if(count <= 0)
				break;

			else
				write(file_put_sock, send_buf, count);
		}

		close(local_file);
		close(file_put_sock);
	}

	ftp_get_reply(sock_control);
}

//quit命令
void ftp_quit(){
	ftp_send_cmd("QUIT", NULL, sock_control);

	ftp_get_reply(sock_control);

	close(sock_control);
}

//终止与服务器的连接
void close_cli(){
	ftp_send_cmd("CLOSE", NULL, sock_control);
	ftp_get_reply(sock_control);
}

//PWD命令
void ftp_pwd(){
	ftp_send_cmd("PWD", NULL, sock_control);

	ftp_get_reply(sock_control);
}

//cd命令
void ftp_cd(char * user_cmd){
	char *cmd = strchr(user_cmd, ' ');

	char path[1024];

	if(cmd == NULL){
		printf("Command error!\n");
		return;
	}
	else{
		while(*cmd == ' ')
			cmd++;
	}

	if(cmd == NULL || cmd == '\0'){
		printf("Command error!\n");
		return;
	}
	else{
		strncpy(path, cmd, strlen(cmd));
		path[strlen(cmd)] = '\0';

		ftp_send_cmd("CWD", path, sock_control);

		ftp_get_reply(sock_control);
	}
}


void del(char * user_cmd){
	char *cmd = strchr(user_cmd, ' ');

	char filename[1024];

	if(cmd == NULL){
		printf("Command error!\n");
		return;
	}

	else{
		while(*cmd == ' ')
			cmd++;
	}

	if(cmd == NULL || cmd == '\0'){
		printf("Command error!\n");
		return;
	}

	else{
		strncpy(filename, cmd, strlen(cmd));
		filename[strlen(cmd)] = '\0';

		ftp_send_cmd("DELE", filename, sock_control);

		ftp_get_reply(sock_control);
	}
}

void mkdir_srv(char *user_cmd){
	char *cmd = strchr(user_cmd,' ');
	char path[1024];
	if(cmd == NULL){
		printf("Command error!\n");
		return;
	}
	else{
		while(*cmd == ' ')
			cmd ++;
	}
	if(cmd == NULL||cmd == '\0'){
		printf("Command error!\n");
		return;
	}
	else
	{
		strncpy(path, cmd, strlen(cmd));
		path[strlen(cmd)]='\0';
		ftp_send_cmd("MKD",path, sock_control);
		ftp_get_reply(sock_control);
	}
}


void rmdir_srv(char *user_cmd){
	char *cmd = strchr(user_cmd, ' ');

	char path[1024];

	if(cmd == NULL){
		printf("Command error!\n");
		return;
	}
	else{
		while(*cmd == ' ')
			cmd ++;
	}
	if(cmd == NULL || cmd == '\0'){
		printf("Command error!\n");
		return;
	}

	else{
		strncpy(path, cmd, strlen(cmd));

		path[strlen(cmd)] = '\0';

		ftp_send_cmd("RMD", path, sock_control);

		ftp_get_reply(sock_control);
	}
}


void local_list(){
	DIR * dp;

	struct dirent * dirp;

	if((dp = opendir("./")) == NULL){
		printf("Opendir() error!\n");
		return;
	}

	printf("Local file list:\n");

	while((dirp = readdir(dp)) != NULL){
		if(strcmp(dirp -> d_name, ".") == 0 || strcmp(dirp -> d_name, "..") == 0){
			continue;
		}

		printf("%s\n", dirp -> d_name);
	}
}


void local_pwd(){
	char curr_dir[512];

	int size = sizeof(curr_dir);

	if(getcwd(curr_dir, size) == NULL)
		printf("Getcwd failed\n");
	else
		printf("Current local directory: %s\n", curr_dir);
}


void local_cd(char * user_cmd){
	char *cmd = strchr(user_cmd, ' ');

	char path[1024];

	if(cmd == NULL){
		printf("Command error!\n");
		return;
	}

	else{
		while(*cmd == ' ')
			cmd++;
	}

	if(cmd == NULL || cmd == '\0'){
		printf("Command error!\n");
		return;
	}

	else{
		strncpy(path, cmd, strlen(cmd));

		path[strlen(cmd)] = '\0';

		if(chdir(path) < 0)
			printf("Local: chdir to %s error!\n", path);
		else
			printf("Local: chdir to %s\n", path);
	}
}



void show_help(){
	printf("\033[32mhelp\033[0m\t--print this command list\n");
	printf("\033[32mopen\033[0m\t--open the server\n");
	printf("\033[32mclose\033[0m\t--close the connection with the server\n");
	printf("\033[32mmkdir\033[0m\t--make new dir on the ftp server\n");
	printf("\033[32mrmdir\033[0m\t--delete the dir on the ftp server\n");
	printf("\033[32mdele\033[0m\t--delete the file on the ftp server\n");
	printf("\033[32mpwd\033[0m\t--print the current directory of server\n");
	printf("\033[32mls\033[0m\t--list the files and directoris in current directory of server\n");
	printf("\033[32mcd [directory]\033[0m\n\t--enter  of server\n");
	printf("\033[32mmode\033[0m\n\t--change current mode, PORT or PASV\n");
	printf("\033[32mput [local_file] \033[0m\n\t--send [local_file] to server as \n");
	printf("\tif  isn't given, it will be the same with [local_file] \n");
	printf("\tif there is any \' \' in , write like this \'\\ \'\n");
	printf("\033[32mget [remote file] \033[0m\n\t--get [remote file] to local host as\n");
	printf("\tif  isn't given, it will be the same with [remote_file] \n");
	printf("\tif there is any \' \' in , write like this \'\\ \'\n");
	printf("\033[32mlpwd\033[0m\t--print the current directory of local host\n");
	printf("\033[32mlls\033[0m\t--list the files and directoris in current directory of local host\n");
	printf("\033[32mlcd [directory]\033[0m\n\t--enter  of localhost\n");
	printf("\033[32mbye\033[0m\t--quit this ftp client program\n");
}


void get_user(){
	char read_buf[64];
	printf("User(Press for anonymous): ");
	fgets(read_buf, sizeof(read_buf), stdin);

	if(read_buf[0] == '\n')
		strncpy(user, "anonymous", 9);
	else
		strncpy(user, read_buf, strlen(read_buf) - 1);
}


void get_pass(){
	char read_buf[64];

	printf("Password(Press for anonymous): ");
	echo_off();

	fgets(read_buf, sizeof(read_buf), stdin);

	if(read_buf[0] == '\n')
		strncpy(passwd, "anonymous", 9);
	else
		strncpy(passwd, read_buf, strlen(read_buf) - 1);

	echo_on();

	printf("\n");
}


int ftp_login(){
	int err;

	get_user();

	if(ftp_send_cmd("USER ", user, sock_control) < 0)
		cmd_err_exit("Can not send message", 1);

	err = ftp_get_reply(sock_control);

	if(err == 331){
		get_pass();

		if(ftp_send_cmd("PASS ", passwd, sock_control) <= 0)
			cmd_err_exit("Can not send message", 1);

		else
			err = ftp_get_reply(sock_control);

		if(err == 230)
			return 1;
		else if(err == 531)
			return 1;
		else{
			printf("Password error!\n");
			return 0;
		}
	}

	else{
		printf("User error!\n");
		return 0;
	}
}



int ftp_user_cmd(char * user_cmd){

	if(!strncmp(user_cmd,"ls",2))
		return 1;
	if(!strncmp(user_cmd,"pwd",3))
		return 2;
	if(!strncmp(user_cmd,"cd ",3))
		return 3;
	if(!strncmp(user_cmd,"put ",4))
		return 4;
	if(!strncmp(user_cmd,"get ",4))
		return 5;
	if(!strncmp(user_cmd,"bye",3))
		return 6;
	if(!strncmp(user_cmd,"mode",4))
		return 7;
	if(!strncmp(user_cmd,"lls",3))
		return 11;
	if(!strncmp(user_cmd,"lpwd",4))
		return 12;
	if(!strncmp(user_cmd,"lcd ",4))
		return 13;

	if(!strncmp(user_cmd,"open",4))
		return 15;
	if(!strncmp(user_cmd,"close",5))
		return 16;
	if(!strncmp(user_cmd,"mkdir",5))
		return 17;
	if(!strncmp(user_cmd,"rmdir",5))
		return 18;
	if(!strncmp(user_cmd,"dele",4))
		return 19;


	return -1;
}



void open_srv(){
	char user_cmd[1024];

	int cmd_flag;

	while(1){
		printf("ftp_cli>");

		fgets(user_cmd, 510, stdin);

		fflush(stdin);

		if(user_cmd[0] == '\n')
			continue;

		user_cmd[strlen(user_cmd) - 1] = '\0';

		cmd_flag = ftp_user_cmd(user_cmd);

		if(cmd_flag == 15){
			char *cmd = strchr(user_cmd, ' ');

			char dress_ftp[1024];

			if(cmd == NULL){
				printf("Command error!\n");
				show_help();
				return;
			}
			else{
				while(*cmd == ' ')
					cmd++;
			}

			if(cmd == NULL || cmd == '\0'){
				printf("Command error!\n");
				return 0;
			}
			else{
				char *dr = "127.0.0.1";
				strncpy(dress_ftp, cmd, strlen(cmd));

				dress_ftp[strlen(cmd)] = '\0';

				printf("%s", dress_ftp);

				if(1){
					printf("Connect successed!\n");
					start_ftp_cmd(dr, DEFAULT_FTP_PORT);
				}
				else{
					printf("Inviable Server Dress!\n");
				}
			}
		}

		else{
			switch(cmd_flag){
				case 11:
					local_list();
					memset(user_cmd, '\0', sizeof(user_cmd));
					break;

				case 12:
					local_pwd();
					memset(user_cmd, '\0', sizeof(user_cmd));
					break;

				case 13:
					local_cd(user_cmd);
					memset(user_cmd, '\0', sizeof(user_cmd));
					break;


				case 6:
					printf("BYE TO THE BEST FTP!\n");
					exit(0);
					break;

				default:
					printf("Command error!\n");
					show_help();
					memset(user_cmd, '\0', sizeof(user_cmd));
					break;
			}
		}
	}
}



int start_ftp_cmd(char * host_ip_addr, int port){
	int err;

	int cmd_flag;

	char user_cmd[1024];

	err = fill_host_addr(host_ip_addr, &ftp_server, port);

	if(err == 254)
		cmd_err_exit("Invalid port!", 254);
	if(err == 253)
		cmd_err_exit("Invalid server address!", 253);

	sock_control = xconnect(&ftp_server, 1);

	if((err = ftp_get_reply(sock_control)) != 220)
		cmd_err_exit("Connect error!", 220);

	do{
		err = ftp_login();
	}while(err != 1);

	while(1){
		printf("ftp_cli>");

		fgets(user_cmd, 510, stdin);

		fflush(stdin);

		if(user_cmd[0] == '\n')
			continue;

		user_cmd[strlen(user_cmd) - 1] = '\0';

		cmd_flag = ftp_user_cmd(user_cmd);

		switch(cmd_flag)
		{
			case 1:
				ftp_list();
				memset(user_cmd, '\0',sizeof(user_cmd));
				break;
			case 2:
				ftp_pwd();
				memset(user_cmd, '\0',sizeof(user_cmd));
				break;
			case 3:
				ftp_cd(user_cmd);
				memset(user_cmd, '\0',sizeof(user_cmd));
				break;
			case 4:
				ftp_put(user_cmd);
				memset(user_cmd, '\0',sizeof(user_cmd));
				break;
			case 5:
				ftp_get(user_cmd);
				memset(user_cmd, '\0',sizeof(user_cmd));
				break;
			case 6:
				ftp_quit();
				printf("BYE TO THE BEST FTP!\n");
				exit(0);
			case 7:
				mode = (mode + 1)%2;
				if(mode)
					printf("change mode to PASV\n");
				else
					printf("change mode to PORT\n");
				break;
			case 11:
				local_list();
				memset(user_cmd, '\0',sizeof(user_cmd));
				break;
			case 12:
				local_pwd();
				memset(user_cmd, '\0',sizeof(user_cmd));
				break;
			case 13:
				local_cd(user_cmd);
				memset(user_cmd, '\0',sizeof(user_cmd));
				break;

			case 16:		
				close_cli();
				memset(user_cmd,'\0',sizeof(user_cmd));
				open_srv();
				break;
			case 17:
				mkdir_srv(user_cmd);
				memset(user_cmd,'\0',sizeof(user_cmd));
				break;
			case 18:
				rmdir_srv(user_cmd);
				memset(user_cmd,'\0',sizeof(user_cmd));
				break;
			case 19:
				del(user_cmd);
				memset(user_cmd,'\0',sizeof(user_cmd));
				break;
			default:
				show_help();
				memset(user_cmd, '\0',sizeof(user_cmd));
				break;
		}
	}
	return 1;
}


void open_ftpsrv(){
	char user_cmd[1024];

	int cmd_flag;

	while(1){
		printf("ftp_cli>");

		fgets(user_cmd, 510, stdin);

		fflush(stdin);

		if(user_cmd[0] == '\n')
			continue;

		user_cmd[strlen(user_cmd) - 1] = '\0';

		cmd_flag = ftp_user_cmd(user_cmd);

		if(cmd_flag == 15){
			char *cmd = strchr(user_cmd, ' ');
			char dress_ftp[1024];
			if(cmd == NULL){
				printf("command error!\n");
				show_help();
				return;
			}

			else{

				while(*cmd == ' ')
					cmd++;
			}

			if(cmd == NULL || cmd == '\0'){
				printf("Command error!\n");
				return;
			}

			else{

				char *dr = "127.0.0.1";

				strncpy(dress_ftp, cmd, strlen(cmd));

				dress_ftp[strlen(cmd)] = '\0';

				printf("%s", dress_ftp);

				if(1){
					printf("Connect successed!\n");

					start_ftp_cmd(dr, DEFAULT_FTP_PORT);
				}
				else{
					printf("Inviable server address!\n");
				}
			}
		}

		else{
			switch(cmd_flag)
			{
				case 11:
					local_list();
					memset(user_cmd,'\0',sizeof(user_cmd));
					break;
				case 12:
					local_pwd();
					memset(user_cmd,'\0',sizeof(user_cmd));
					break;
				case 13:
					local_cd(user_cmd);
					memset(user_cmd,'\0',sizeof(user_cmd));
					break;
				case 6://quit
					printf("BYE TO WEILIQI FTP!\n");
					exit(0);
					break;
				default:
					printf("Command error!\n");
					show_help();
					memset(user_cmd,'\0',sizeof(user_cmd));
					break;
			}
		}

	}

}


int main(int argc, char * argv[]){
	if(argc == 1){
		printf("Hello! Welcome to The best FTP!\n");

		open_ftpsrv();
	}

	else{
		if(argc == 2 || argc == 3){
			if(argv[2] == NULL)
				start_ftp_cmd(argv[1], DEFAULT_FTP_PORT);

			else
				start_ftp_cmd(argv[1], atol(argv[2]));
		}
		else{
			printf("Input Inviable!\n");
			exit(-1);
		}
	}

	return 1;
	
}

