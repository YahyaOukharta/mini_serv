#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

int extract_message(char **buf, char **msg)
{
	char *newbuf;
	int i;

	free(*msg);
	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(sizeof(*newbuf) * (strlen(*buf + i + 1) + 1) + 1, 1);
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

int sockfd;
int fd_to_id[1025] = {-1};
int last_id = 0;
char buf[1024][2000000];

void error(char *s)
{
	write(2, s, strlen(s));
	close(sockfd);
	exit(1);
}

void broadcast(int from, char *str){
	for (int fd = 0; fd < 1025; fd++){
		if (fd == from || fd_to_id[fd] == -1) continue;
		send(fd, str,strlen(str),0);
	}
}

int main(int ac, char **av){

	if (ac != 2)
	{
		error("Wrong number of arguments\n");
	}
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
	{
		error("Fatal error\n");
	}

	int connfd;
	struct sockaddr_in servaddr;
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); // 127.0.0.1
	servaddr.sin_port = htons(atoi(av[1]));

	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0)
	{
		error("Fatal error\n");
	}
	if (listen(sockfd, 10) != 0)
	{
		error("Fatal error\n");
	}
	for (int fd = 0; fd < 1025; fd++){
		fd_to_id[fd] = -1;
	}
	fd_set master_rd, working_rd;
	bzero(&master_rd, sizeof(master_rd));
	FD_SET(sockfd, &master_rd);
	struct timeval timeout = {1, 0};
	int maxfd = sockfd;
	while (1)
	{
		working_rd = master_rd;
		int r = select(maxfd + 1, &working_rd, NULL, NULL, &timeout);

		for (int fd = 0; fd <= maxfd && r; fd++)
		{
			if (FD_ISSET(fd, &working_rd))
			{
				r--;
				if (fd == sockfd)
				{
					connfd = accept(sockfd, 0, 0);
					if (connfd == -1)
					{
						error("Fatal error\n");
					}

					FD_SET(connfd, &master_rd);
					fd_to_id[connfd] = last_id++;
					bzero(&buf[connfd], sizeof(buf[connfd]));

					char tmp[200000] = {0};
					sprintf(tmp, "server: client %d just arrived\n",fd_to_id[connfd]);
					broadcast(connfd,tmp);

					if (connfd > maxfd)
						maxfd = connfd;
				}
				else
				{
					int rd;
					while (
						(rd = recv(fd, buf[fd] + strlen(buf[fd]), 1000,0)) == 1000 
						&& buf[fd][strlen(buf[fd]) - 1] != '\n'
					);

					if (rd <= 0)
					{
						char tmp[200000] = {0};
						sprintf(tmp, "server: client %d just left\n",fd_to_id[fd]);
						broadcast(fd,tmp);

						fd_to_id[fd] = -1;
						FD_CLR(fd, &master_rd);
						while (!FD_ISSET(maxfd, &master_rd))
							maxfd--;
					}
					else
					{
						char *msg = 0;
						char *b = calloc(sizeof(buf[fd]),1);
						strcpy(b, buf[fd]);
						bzero(&buf[fd], sizeof(buf[fd]));
						while(extract_message(&b,&msg)){
							char tmp[200000] = {0};
							sprintf(tmp, "client %d: %s",fd_to_id[fd],msg);
							broadcast(fd,tmp);
						}
						strcpy(buf[fd],b);
						free(b);
					}
				}
			}
		}
	}

	return (0);
}
