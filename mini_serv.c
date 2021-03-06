#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>

typedef struct		s_client {
	int             fd, id;
	struct s_client *next;
	char            cl_buff[42 * 4096];
}					t_client;

t_client            *g_clients;
fd_set              curr_sock, cpy_read, cpy_write;
int                 sock_fd, g_id = 0;
char                str[42*4096], tmp[42*4096], buff[42*4096 + 42];

void fatal() {
	write(2, "Fatal error\n", strlen("Fatal error\n"));
	close(sock_fd);
	exit(1);
}

void send_all(int fd, char *str_req) {
	for (t_client *it = g_clients; it; it = it->next)
		if (FD_ISSET(it->fd, &cpy_write) && it->fd != fd)
			send(it->fd, str_req, strlen(str_req), 0);
}

char *get_buff(int fd) {
	for (t_client *it = g_clients; it; it = it->next)
		if (it->fd == fd)
			return (it->cl_buff);
	return (0);
}

int get_id(int fd) {
	for (t_client *it = g_clients; it; it = it->next)
		if (it->fd == fd)
			return (it->id);
	return (-1);
}

int get_max_fd() {
	int	max = sock_fd;

	for (t_client *it = g_clients; it; it = it->next)
		max = max < it->fd ? it->fd: max;
	return (max);
}

void ex_msg(int fd) {
	int j = 0, i = -1;

	while (get_buff(fd)[++i]) {
		tmp[j++] = get_buff(fd)[i];
		if (get_buff(fd)[i] == '\n') {
			sprintf(buff, "client %d: %s", get_id(fd), tmp);
			send_all(fd, buff);
			j = 0;
			bzero(&tmp, strlen(tmp));
			bzero(&buff, strlen(buff));
		}
	}
	strcpy(get_buff(fd), &get_buff(fd)[i]);
}

int add_client_to_list(int fd) {
	t_client	*it = g_clients, *new;

	if (!(new = calloc(1, sizeof(t_client))))
		fatal();
	new->id = g_id++;
	new->fd = fd;
	bzero(&new->cl_buff, sizeof(new->cl_buff));
	if (!g_clients) {
		g_clients = new;
	} else {
		while (it->next)
			it = it->next;
		it->next = new;
	}
	return (new->id);
}

int rm_client(int fd) {
	t_client	*it = g_clients, *del;
	int			id = get_id(fd);

	if (it->fd == fd) {
		g_clients = it->next;
		free(it);
	} else {
		while (it->next && it->next->fd != fd)
			it = it->next;
		del = it->next;
		it->next = it->next->next;
		free(del);
	}
	return (id);
}

int main(int ac, char **av) {
	if (ac != 2) {
		write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
		exit(1);
	}

	struct sockaddr_in	servaddr;
	uint16_t			port = atoi(av[1]);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = 127 | (1 << 24);
	servaddr.sin_port = port >> 8 | port << 8;

	if ((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		fatal();
	if (bind(sock_fd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
		fatal();
	if (listen(sock_fd, 0) < 0)
		fatal();

	FD_ZERO(&curr_sock);
	FD_SET(sock_fd, &curr_sock);
	bzero(&tmp, sizeof(tmp));
	bzero(&buff, sizeof(buff));
	bzero(&str, sizeof(str));

	while (420) {
		cpy_write = cpy_read = curr_sock;
		if (select(get_max_fd() + 1, &cpy_read, &cpy_write, NULL, NULL) < 0)
			continue ;
		for (int fd = 0; fd <= get_max_fd(); fd++) {
			if (FD_ISSET(fd, &cpy_read)) {
				if (fd == sock_fd) {
					struct sockaddr_in	clientaddr;
					unsigned int len; 
					int client_fd;

					if ((client_fd = accept(sock_fd, (struct sockaddr *)&clientaddr, &len)) < 0)
						continue ;
					sprintf(buff, "server: client %d just arrived\n", add_client_to_list(client_fd));
					send_all(client_fd, buff);
					FD_SET(client_fd, &curr_sock);
                    bzero(&buff, sizeof(buff));
					break ;
				} else {
					if (recv(fd, str, 42 * 4096, 0) <= 0) {
						sprintf(buff, "server: client %d just left\n", rm_client(fd));
						send_all(fd, buff);
						bzero(&buff, sizeof(buff));
						FD_CLR(fd, &curr_sock);
						close(fd);
						break ;
					} 
					else {
						strcat(get_buff(fd), str);
						bzero(&str, sizeof(str));
						if (strstr(get_buff(fd), "\n"))
							ex_msg(fd);
						break ;
					}
				}
			}
		}
	}
	return (0);
}
