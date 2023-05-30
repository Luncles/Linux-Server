/* ************************************************************************
> File Name:     init_socket.h
> Author:        Luncles
> 功能：         
> Created Time:  Tue 23 May 2023 09:47:39 PM CST
> Description:   
 ************************************************************************/

void InitSocketAddress(struct sockaddr_in &address, const char *ip, const char *port);
int SetNonblocking(int fd);
void addfd(int &epollfd, int &fd);