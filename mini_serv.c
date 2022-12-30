#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
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

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void error(char *str){
	write(2,str,strlen(str));
	exit(1);
}

int last_id = 0;
int ids[1025];
char *buf[1025];
char rd_buf[4096 * 10];
char wr_buf[1024];
fd_set master_rd,working_rd;
int fd_max;

void broadcast(int from, char *str){
	for (int fd = 0; fd < 1025; fd++){
		if (fd == from || ids[fd] == -1) continue;
		send(fd, str,strlen(str),0);
	}
}
int main(int ac, char **av){
	if (ac != 2)
	{
		error( "Wrong number of arguments\n");
	}
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("Fatal error\n");
		
	struct sockaddr_in	servaddr;

	bzero(&servaddr, sizeof(servaddr));

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1]));

	if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)))
		error("Fatal error\n");
	if (listen(sockfd, 0))
		error("Fatal error\n");
	for(int i = 0; i < 1025; i++){
		ids[i] = -1;
		buf[i] = 0;
	}
	fd_max = sockfd;
	FD_ZERO(&master_rd);
	FD_SET(sockfd, &master_rd);

	while (1)
	{

		working_rd = master_rd;

		int ret = select(fd_max + 1, &working_rd, NULL, NULL, NULL);
		if (ret < 0)
			error("Fatal error\n");

		for(int fd = 0; fd <= fd_max && ret; fd++){
			if(FD_ISSET(fd, &working_rd)){
				ret--;
				if (fd == sockfd){
					int conn = accept(sockfd,NULL,NULL);
					if(conn < 0)
						error("Fatal error\n");
					
					ids[conn] = last_id++;
					sprintf(wr_buf, "server: client %d just arrived\n",ids[conn]);
					broadcast(conn, wr_buf);

					FD_SET(conn, &master_rd);
					fd_max = conn > fd_max ? conn : fd_max;
					break;
				}
				else{
					int ret = recv(fd, rd_buf, sizeof(rd_buf) - 1, 0);
					if (ret <= 0){
						sprintf(wr_buf, "server: client %d just left\n",ids[fd]);
						broadcast(fd, wr_buf);
						free(buf[fd]);
						buf[fd] = 0;
						ids[fd] = -1;
						FD_CLR(fd, &master_rd);
						while(!FD_ISSET(fd_max,&master_rd))
							fd_max--;
						break;
					}
					rd_buf[ret] = 0;
					buf[fd] = str_join(buf[fd],rd_buf);
					char *msg;
					while (extract_message(&buf[fd],&msg)){
						sprintf(wr_buf, "client %d: ",ids[fd]);
						broadcast(fd, wr_buf);
						broadcast(fd, msg);
						free(msg);
					}
				}
			}
		}
	}
}
