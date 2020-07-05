#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include "ftp.h"
#include <arpa/inet.h>
#include <time.h>


#define FPT_SERVER_PORT 21
#define MAX_INFO 1024
#define DIR_INFO 100
#define MSG_INFO 100
#define LISTEN_QENU 5


// 定义权限用户名及密码
const char default_user[] = "root";  
const char default_pass[] = "1234";  

// 定义匿名用户名及密码
const char anony_user[] = "anonymous";  
const char anony_pass[] = "anonymous";

int ftp_server_sock;  // socket函数返回值
int ftp_data_sock;
char client_Control_Info[MAX_INFO];
char client_Data_Info[MAX_INFO];

char format_client_Info[MAX_INFO];	

int translate_data_mode = FILE_TRANS_MODE_ASIC;

void *Handle_Client_Request(void* arg);


struct ARG{
    int client_sock;
    struct sockaddr_in client;
};


void do_client_work(int client_sock, struct sockaddr_in client);
int login(int client_sock);
void handle_cwd(int client_sock);
void handle_list(int client_sock);
void handle_pasv(int client_sock, struct sockaddr_in client);
void handle_file(int client_sock);
void handle_del(int client_sock);
void handle_mkd(int client_sock);
void handle_rmd(int client_sock);


struct sockaddr_in create_date_sock();
void send_client_info(int client_sock, char* info, int length);
int recv_client_info(int client_sock);


//处理客户需求，从客户端连接成功开始到结束服务。其中调用do_client_work(info->client_sock,info->client)函数与客户端交互。
void *Handle_Client_Request(void *arg){
    struct ARG*info;
    info = (struct ARG*)arg;

    printf("You got a connection from %s\n", inet_ntoa(info->client.sin_addr));

    do_client_work(info -> client_sock, info -> client);

    close(info -> client_sock);

    pthread_exit(NULL);
}


// 处理FTP的各种命令
void do_client_work(int client_sock, struct  sockaddr_in client){
    int login_flag;
    login_flag = login(client_sock);

    while(recv_client_info(client_sock) && login_flag == 1){

        if((strncmp("quit", client_Control_Info, 4) == 0) || (strncmp("QUIT", client_Control_Info, 4) == 0)){

            send_client_info(client_sock, serverInfo221, strlen(serverInfo221));
            break;
        }
        else if((strncmp("close", client_Control_Info, 5) == 0) || (strncmp("CLOSE", client_Control_Info, 5) == 0)){
            printf("Client quit!\n");
            shutdown(client_sock, SHUT_WR);  // 断开输出流
        }

        else if((strncmp("pwd", client_Control_Info, 3) == 0) || (strncmp("PWD", client_Control_Info, 3) == 0)){
            char pwd_info[MSG_INFO];
            char tmp_dir[DIR_INFO];

            snprintf(pwd_info, MSG_INFO, "257 \"%s\" is current location. \r\n", getcwd(tmp_dir, DIR_INFO));
            send_client_info(client_sock, pwd_info, strlen(pwd_info));
        }
        else if((strncmp("cwd", client_Control_Info, 3) == 0) || (strncmp("CWD", client_Control_Info, 3) == 0)){
            handle_cwd(client_sock);
        }
        else if((strncmp("mkd", client_Control_Info, 3) == 0) || (strncmp("MKD", client_Control_Info, 3) == 0)){
            handle_mkd(client_sock);
        }
        else if((strncmp("rmd", client_Control_Info, 3) == 0) || (strncmp("RMD", client_Control_Info, 3) == 0)){
            handle_rmd(client_sock);
        }
        else if((strncmp("dele", client_Control_Info, 4) == 0) || (strncmp("DELE", client_Control_Info, 4) == 0)){
            handle_del(client_sock);
        }
        else if((strncmp("pasv", client_Control_Info, 4) == 0) || (strncmp("PASV", client_Control_Info, 4) == 0)){
            handle_pasv(client_sock, client);
        }

        else if((strncmp("list", client_Control_Info, 4) == 0) || (strncmp("LIST", client_Control_Info, 4) == 0)){
            handle_list(client_sock);
            send_client_info(client_sock, serverInfo226, strlen(serverInfo226));

        }
        else if((strncmp("type", client_Control_Info, 4) == 0) || (strncmp("TYPE", client_Control_Info, 4) == 0)){
            if((strncmp("type I", client_Control_Info, 6) == 0) ||(strncmp("TYPE I", client_Control_Info, 6) == 0)){
                translate_data_mode = FILE_TRANS_MODE_BIN;
            }
            send_client_info(client_sock, serverInfo200, strlen(serverInfo200));
        }
        else if((strncmp("retr", client_Control_Info, 4) == 0)||(strncmp("RETR", client_Control_Info, 4) == 0)){
            handle_file(client_sock);
            send_client_info(client_sock, serverInfo226, strlen(serverInfo226));
        }
        else if(strncmp("stor", client_Control_Info, 4) == 0||(strncmp("STOR", client_Control_Info, 4) == 0)){
            handle_file(client_sock);
            send_client_info(client_sock,serverInfo226, strlen(serverInfo226));
        }
        else if(strncmp("syst", client_Control_Info, 4) == 0||(strncmp("SYST", client_Control_Info, 4) == 0)){
            send_client_info(client_sock, serverInfo215, strlen(serverInfo215));
        }
        else if(strncmp("size", client_Control_Info, 4) == 0||(strncmp("SIZE", client_Control_Info, 4) == 0)){
            send_client_info(client_sock, serverInfo213, strlen(serverInfo213));
        }
        else if(strncmp("feat", client_Control_Info, 4) == 0||(strncmp("FEAT", client_Control_Info, 4) == 0)){
            send_client_info(client_sock, serverInfo211, strlen(serverInfo211));
        }
        else if(strncmp("rest", client_Control_Info, 4) == 0||(strncmp("REST", client_Control_Info, 4) == 0)){
            send_client_info(client_sock, serverInfo350, strlen(serverInfo350));
        }
        else {
            send_client_info(client_sock, serverInfo, strlen(serverInfo));
        }
    }

    while(recv_client_info(client_sock) && (login_flag == 2)){
        
        
        if((strncmp("quit", client_Control_Info, 4) == 0)||(strncmp("QUIT", client_Control_Info, 4) ==0)){
            send_client_info(client_sock, serverInfo221, strlen(serverInfo221));
            break;
        }
        else if((strncmp("close",client_Control_Info, 5) == 0)||(strncmp("CLOSE",client_Control_Info, 5) == 0)){
            printf("Client Quit!\n");
            shutdown(client_sock, SHUT_WR); // 断开输出流
        }

        else if((strncmp("pwd", client_Control_Info, 3) == 0)||(strncmp("PWD", client_Control_Info, 3) == 0)){
            char pwd_info[MSG_INFO];
            char tmp_dir[DIR_INFO];
            snprintf(pwd_info, MSG_INFO, "257 \"%s\" is current location.\r\n", getcwd(tmp_dir, DIR_INFO));
            send_client_info(client_sock, pwd_info, strlen(pwd_info));
        }
        else if((strncmp("cwd", client_Control_Info, 3) == 0)||(strncmp("CWD", client_Control_Info, 3) == 0)){
            handle_cwd(client_sock);
        }
        else if((strncmp("pasv", client_Control_Info, 4) == 0)||(strncmp("PASV", client_Control_Info, 4)== 0)){
            handle_pasv(client_sock,client);
        }
        else if((strncmp("list", client_Control_Info, 4) == 0)||(strncmp("LIST", client_Control_Info, 4)== 0)){
            handle_list(client_sock);
            send_client_info(client_sock, serverInfo226, strlen(serverInfo226));
        }
        else if((strncmp("type", client_Control_Info, 4) == 0)||(strncmp("TYPE", client_Control_Info, 4)== 0)){
            if((strncmp("type I", client_Control_Info, 6) == 0)||(strncmp("TYPE I", client_Control_Info, 6) == 0)) {
                translate_data_mode = FILE_TRANS_MODE_BIN;
            }
            send_client_info(client_sock, serverInfo200, strlen(serverInfo200));
        }
        else if((strncmp("retr", client_Control_Info, 4) == 0)||(strncmp("RETR", client_Control_Info, 4)== 0)){
            handle_file(client_sock);
            send_client_info(client_sock,serverInfo226, strlen(serverInfo226));
        }
        else if((strncmp("syst", client_Control_Info, 4) == 0)||(strncmp("SYST", client_Control_Info, 4)== 0)){
            send_client_info(client_sock, serverInfo215, strlen(serverInfo215));
        }
        else if((strncmp("size", client_Control_Info, 4) == 0)||(strncmp("SIZE", client_Control_Info, 4)== 0)){
            send_client_info(client_sock, serverInfo213, strlen(serverInfo213));
        }
        else if((strncmp("feat", client_Control_Info, 4) == 0)||(strncmp("FEAT", client_Control_Info, 4)== 0)){
            send_client_info(client_sock, serverInfo211, strlen(serverInfo211));
        }
        else if((strncmp("rest", client_Control_Info, 4) == 0)||(strncmp("REST", client_Control_Info, 4)== 0)){
            send_client_info(client_sock, serverInfo350, strlen(serverInfo350));
        }
        else{
            send_client_info(client_sock, serverInfo, strlen(serverInfo));
        }
    }
}


//登陆函数。处理客户端登录请求，与已定义的权限用户密码进行匹配，成功则为ROOT用户，失败则匹配匿名用户，匿名用户只能进行简单服务。
int login(int client_sock){

	send_client_info(client_sock, serverInfo220, strlen(serverInfo220));

	while(1){
		if(recv_client_info(client_sock) == 2) // user
			break;
		else 
			send_client_info(client_sock, serverInfo, strlen(serverInfo));
	}

	int flag = 0;
	int i = 0;
	int length = strlen(client_Control_Info);
	
	//将指令格式化
	for(i = 5; i < length; i++){
		format_client_Info[i - 5] = client_Control_Info[i];
	}

	format_client_Info[i - 7] = "\0";
	if(strncmp(format_client_Info, default_user, 4) == 0){
		flag = 1;
	}

	if(strncmp(format_client_Info, anony_user, 9) == 0){
		flag = 2;
	}

	send_client_info(client_sock, serverInfo331, strlen(serverInfo331));

	recv_client_info(client_sock); //password

	length = strlen(client_Control_Info);


	//格式化命令
	for(i = 5; i < length; i++){
		format_client_Info[i - 5] = client_Control_Info[i];
	}

	format_client_Info[i - 7] = "\0";


	if(strncmp(format_client_Info, default_pass, 4) == 0 && flag == 1){
		send_client_info(client_sock, serverInfo230, strlen(serverInfo230));
		return 1;
	}

	else if(strncmp(format_client_Info, anony_pass, 9) == 0 && flag == 2){
		send_client_info(client_sock, serverInfo531, strlen(serverInfo531));
		return 2;
	}

	else{
		send_client_info(client_sock, serverInfo530, strlen(serverInfo530));
		return 0;
	}
}    

//处理转换目录功能。
void handle_cwd(int client_sock){
	char cwd_info[MSG_INFO];
	char tmp_dir[DIR_INFO];
	char client_dir[DIR_INFO];

	char t_dir[DIR_INFO];

	int dirlength = -1;

	int length = strlen(client_Control_Info);

	int i = 0;

	for(i = 4; i < length; i++){
		format_client_Info[i - 4] = client_Control_Info[i];
	}

	format_client_Info[i - 6] = "\0";

	if(strncmp(getcwd(t_dir, DIR_INFO), format_client_Info, strlen(getcwd(t_dir, DIR_INFO)) - 10) != 0){
		getcwd(client_dir, DIR_INFO);
		dirlength = strlen(client_dir);
		client_dir[dirlength] = '/';
	}

	for(i = 4; i < length; i ++){
		client_dir[dirlength + i - 3] = client_Control_Info[i];
	}

	client_dir[dirlength + i - 5] = '\0';

	//当转换目录存在则把当前目录更改为转换目录，否则报错。
	if(chdir(client_dir) >= 0){
		snprintf(cwd_info, MSG_INFO, "257 \"%s\" is current location.\r\n", getcwd(tmp_dir, DIR_INFO));
		send_client_info(client_sock, cwd_info, strlen(cwd_info));
	}
	else{
		snprintf(cwd_info, MSG_INFO, "550 %s :%s\r\n", client_dir, strerror(errno));
		perror("chdir():");
		send_client_info(client_sock, cwd_info, strlen(cwd_info));
	}
}    


//处理删除目录功能。
void handle_rmd(int client_sock){ 

	char rmd_info[MSG_INFO];
	char tmp_dir[DIR_INFO];
	char client_dir[DIR_INFO];

	char t_dir[DIR_INFO];

	int dirlength = -1;

	int length = strlen(client_Control_Info);

	int i = 0;

	for(i = 4; i < length; i++){
		format_client_Info[i - 4] = client_Control_Info[i];
	}

	format_client_Info[i - 6] = '\0';

	if(strncmp(getcwd(t_dir, DIR_INFO), format_client_Info, strlen(getcwd(t_dir, DIR_INFO))-10) != 0){
		getcwd(client_dir, DIR_INFO);
		dirlength = strlen(client_dir);
		client_dir[dirlength] = '/';
	}

	for (i = 4; i < length; i++){
		client_dir[dirlength + i - 3] = client_Control_Info[i];
	}

	client_dir[dirlength + i - 5] = '\0';

	//client_dir目录，成功输出消息，否则报错。
	if(rmdir(client_dir) >= 0){
		printf("\"%s\" is deleted successfully.\r\n", client_dir);
		send_client_info(client_sock, rmd_info, strlen(rmd_info));
	}
	else{
		snprintf(rmd_info, MSG_INFO, "550 %s :%s\r\n", client_dir, strerror(errno));

		perror("rmdir():");
		send_client_info(client_sock, rmd_info, strlen(rmd_info));
	}
}


//处理创建目录功能
void handle_mkd(int client_sock){  

	char mkd_info[MSG_INFO];
	char tmp_dir[DIR_INFO];
	char client_dir[DIR_INFO];

	char t_dir[DIR_INFO];

	int dirlength = -1;

	int length = strlen(client_Control_Info);
	int i = 0;
	for(i = 4; i < length; i++)
		format_client_Info[i - 4] = client_Control_Info[i];

	format_client_Info[i - 6] = '\0';

	if(strncmp(getcwd(t_dir, DIR_INFO), format_client_Info, strlen(getcwd(t_dir, DIR_INFO)) - 10) != 0){
		getcwd(client_dir, DIR_INFO);
		dirlength = strlen(client_dir);
		client_dir[dirlength] = '/';
	}

	for(i = 4; i < length; i++){
		client_dir[dirlength + i - 3] = client_Control_Info[i];
	}


	if(mkdir(client_dir, 0644) >= 0){ 
		printf("\"%s\" is create successfully.\r\n", client_dir);

		send_client_info(client_sock, mkd_info, strlen(mkd_info));
	}
	else{
		snprintf(mkd_info, MSG_INFO, "550 %s : %s\r\n", client_dir, strerror(errno));
		perror("mkdir():");
		send_client_info(client_sock, mkd_info, strlen(mkd_info));
	}
}

//处理list命令。
void handle_list(int client_sock)
{
        send_client_info(client_sock, serverInfo150, strlen(serverInfo150));
   
   	int t_data_sock;
	struct sockaddr_in client;
	int sin_size=sizeof(struct sockaddr_in);
	if((t_data_sock=accept(ftp_data_sock,(struct sockaddr *)&client,&sin_size))==-1)
	{
		perror("accept error");
		return;
	}

	FILE *pipe_fp;
	char t_dir[DIR_INFO];
	char list_cmd_info[DIR_INFO];
	snprintf(list_cmd_info, DIR_INFO, "ls -l %s", getcwd(t_dir, DIR_INFO));
	
   	if ((pipe_fp = popen(list_cmd_info, "r")) == NULL)
    	{
		printf("pipe open error in cmd_list\n");
		return ;
    	}
   	printf("pipe open successfully!, cmd is %s\n", list_cmd_info);

	char t_char;
   	while ((t_char = fgetc(pipe_fp)) != EOF)
    	{
		printf("%c", t_char);
		write(t_data_sock, &t_char, 1);
    	}
   	pclose(pipe_fp);
   	printf("close pipe successfully!\n");	
   	close(t_data_sock);	
	printf("%s close data successfully!\n",serverInfo226);	
	close(ftp_data_sock);	
}

//处理被动连接过程
void handle_pasv(int client_sock, struct sockaddr_in client){
	char pasv_msg[MSG_INFO];
	char port_str[8];
	char addr_info_str[30];

	struct sockaddr_in user_data_addr;

	user_data_addr = create_date_sock();


	int tmp_port1;
	int tmp_port2;

	tmp_port1 = ntohs(user_data_addr.sin_port) / 256;
	tmp_port2 = ntohs(user_data_addr.sin_port) % 256;

	long ipNum = inet_addr(inet_ntoa(client.sin_addr));

	//IP address
	snprintf(addr_info_str, sizeof(addr_info_str), "%ld,%ld,%ld,%ld,", ipNum&0xff, ipNum>>8&0xff, ipNum>>16&0xff, ipNum>>24&0xff);
	
	snprintf(port_str, sizeof(port_str), "%d,%d", tmp_port1, tmp_port2);
	
	strcat(addr_info_str, port_str);

	snprintf(pasv_msg, MSG_INFO, "227 Entering Passive Mode (%s).\r\n", addr_info_str);

	send_client_info(client_sock, pasv_msg, strlen(pasv_msg));


}


//)创建数据sock。
struct sockaddr_in create_date_sock(){
	int t_client_sock;

	struct sockaddr_in t_data_addr;

	t_client_sock = socket(AF_INET, SOCK_STREAM, 0);

	if(t_client_sock < 0){
		printf("Creating data socket error!\n");
		return;
	}

	srand((int)time(0));

	int a = rand() % 1000 + 1025;

	bzero(&t_data_addr, sizeof(t_data_addr));

	t_data_addr.sin_family = AF_INET;

	t_data_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	t_data_addr.sin_port = htons(a);

	if(bind(t_client_sock, (struct sockaddr *)&t_data_addr, sizeof(struct sockaddr)) < 0){
		printf("Bind error in create data socket:%s\n", strerror(errno));
		return;
	}

	listen(t_client_sock, LISTEN_QENU);

	ftp_data_sock = t_client_sock;

	return t_data_addr;
}

// 处理文件类命令
void handle_file(int client_sock){

	send_client_info(client_sock, serverInfo150, strlen(serverInfo150));

	int t_data_sock;

	struct sockaddr_in client;

	int sin_size = sizeof(struct sockaddr_in);
	
	if((t_data_sock = accept(ftp_data_sock, (struct sockaddr *)&client, &sin_size)) == -1){
		perror("Accept error");
		return;
	}

	int i = 0;

	int length = strlen(client_Control_Info);

	//格式化命令
	for(i = 5; i < length; i++){
		format_client_Info[i - 5] = client_Control_Info[i];
	}

	format_client_Info[i - 7] = '\0';


	FILE* fp;

	int file_fd;

	int n;

	char t_dir[DIR_INFO];

	char file_info[DIR_INFO];

	snprintf(file_info, DIR_INFO, "%s/%s", getcwd(t_dir, DIR_INFO), format_client_Info);

	char file_mode[3];


	if((strncmp("retr", client_Control_Info, 4) == 0) || (strncmp("RETR", client_Control_Info, 4) == 0)){
		//打开文件方式
		file_mode[0] = 'r';
		file_mode[1] = 'b';
		file_mode[2] = '\0';
	}
	else{
		file_mode[0] = 'a';
		file_mode[1] = 'b';
		file_mode[2] = '\0';
	}

	if(strncmp(getcwd(t_dir, DIR_INFO), format_client_Info, strlen(getcwd(t_dir, DIR_INFO)) - 1) == 0){
		//打开文件
		fp = fopen(format_client_Info, file_mode); 
	}
	else{
		fp = fopen(file_info, file_mode);
	}

	if(fp == NULL){
		printf("Open file error: %s\r\n", strerror(errno));

		char cwd_info[MSG_INFO];

		snprintf(cwd_info, MSG_INFO, "550 %s : %s\r\n", format_client_Info, strerror(errno));
		send_client_info(client_sock, cwd_info,strlen(cwd_info));
		close(t_data_sock);

		close(ftp_data_sock);
		return;
	}

	int cmd_sock = fileno(fp); //获取文件描述词

	memset(client_Data_Info, 0, MSG_INFO);

	//下载文件
	if((strncmp("retr", client_Control_Info, 4) == 0) || (strncmp("RETR", client_Control_Info, 4) == 0)){
		while((n = read(cmd_sock, client_Data_Info, MSG_INFO)) > 0){
			if(write(t_data_sock, client_Data_Info, n) != n){
				printf("Retr transfer error!\n");
				return;
			}
		}
	}

	//上传文件
	else{
		while((n = read(t_data_sock, client_Data_Info, MAX_INFO)) > 0){
			if(write(cmd_sock, client_Data_Info, n) != n){
				printf("Stor transfer error!\n");
				return;
			}
		}
	}


	fclose(fp);

	close(t_data_sock);
	close(ftp_data_sock);
}


//处理删除目录功能。
void handle_del(int client_sock){

	char del_info[MSG_INFO];

	char tmp_file[DIR_INFO];
	char client_dir[DIR_INFO];

	char t_dir[DIR_INFO];

	int dirlength = -1;
	int length = strlen(client_Control_Info);
	int i = 0;
	for(i = 4; i < length; i++){
		format_client_Info[i - 4] = client_Control_Info[i];
	}

	format_client_Info[i - 6] = '\0';

	if(strncmp(getcwd(t_dir, DIR_INFO), format_client_Info, strlen(getcwd(t_dir, DIR_INFO)) - 10) != 0){
		getcwd(client_dir, DIR_INFO);
		dirlength = strlen(client_dir);

		client_dir[dirlength] = '/';
	}


	for(i = 4; i < length; i++){
		client_dir[dirlength + i - 3] = client_Control_Info[i];
	}

	client_dir[dirlength + i - 5] = '\0';

	//删除文件
	if(unlink(client_dir) >= 0){
		printf("\"%s\" is deleted successfully.\r\n", client_dir);

		send_client_info(client_sock, del_info, strlen(del_info));
	}
	else{
		snprintf(del_info, MSG_INFO, "550 %s : %s \r\n", client_dir, strerror(errno));
		perror("unlink():");
		send_client_info(client_sock, del_info, strlen(del_info));
	}
	
}



//发送客户端信息。
void send_client_info(int client_sock, char* info, int length){
	int len;
	if((len = send(client_sock, info, length, 0)) < 0){
		perror("Send info error!");
		return;
	}
}

//接收客户端信息。
int recv_client_info(int client_sock){
	int num;
	if((num = recv(client_sock, client_Control_Info, MSG_INFO, 0)) < 0){
		perror("Receive info error!");
		return;
	}

	client_Control_Info[num] = '\0';

	printf("Client %ld Message: %s\n", pthread_self(), client_Control_Info);
	if((strncmp("USER", client_Control_Info, 4) == 0) || (strncmp("user", client_Control_Info, 4) == 0)){
		return 2;
	}
	return 1;
}


int main(int argc, char* argv[]){

    // 子线程
    pthread_t thread;
    struct ARG arg;
    struct sockaddr_in server;


    if((ftp_server_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("Creating socket failed");
        exit(1);
    }

    // int opt = 1;  // 允许在bind()过程中本地地址可重复使用
    // if(setsockopt(ftp_server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0){
    //  printf("setsockopt");
    //     exit(1);
    // }
  
    int opt=SO_REUSEADDR;
    setsockopt(ftp_server_sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));

    bzero(&server, sizeof(server));  // 将server清零
    server.sin_family = AF_INET;  //与socket第一个参数保持一致
    server.sin_port = htons(FPT_SERVER_PORT);  //设置端口号，FTP服务的端口号为21
    server.sin_addr.s_addr = htonl(INADDR_ANY);  //设置IP地址


    // bind() 函数将套接字与特定的IP地址和端口绑定起来
    if(bind(ftp_server_sock, (struct sockaddr *)&server, sizeof(struct sockaddr)) == -1){
        perror("Bind error!");
        exit(1);
    }

    //等待参数 socket 连线
    if(listen(ftp_server_sock, LISTEN_QENU) == -1){
        perror("Listen error!");
        exit(1);
    }

    int ftp_client_sock;
    struct sockaddr_in client;
    int sin_size = sizeof(struct sockaddr_in);

    while(1){
        if((ftp_client_sock = accept(ftp_server_sock, (struct sockaddr *)&client, &sin_size)) == -1){
            perror("Accept error!");
            exit(1);
        }

        arg.client_sock = ftp_client_sock;

        memcpy((void*)&arg.client, &client, sizeof(client));

        if(pthread_create(&thread, NULL, Handle_Client_Request, (void*)&arg)){
            perror("Thread create error!");
            exit(1);
        }
    }

    close(ftp_server_sock);

}