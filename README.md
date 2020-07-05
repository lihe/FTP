# FTP

此FTP遵守rfc959标准，其目录在ftp.h文件中。

rfc959标准请参考rfc959.pdf

## 运行流程：

### 编译
Linux环境下使用make命令进行编译，生成ftp_cli和ftp_srv可执行文件

### 运行服务器 
./ftp_srv

### 运行客户端 
./ftp_cli

open 127.0.0.1

或 ./ftp_cli 127.0.0.1 21 (ip地址 端口号）

接着输入用户名和密码即可（登录名为root，密码为1234）

登录成功后输入help命令即可查看各种功能。

## P.S.
打赏一下，生活更好。

![](https://cdn.jsdelivr.net/gh/lihe/Pic/img/20200606003724.jpg)
