#include "sipc_common.h"

#define VERSION		"00.04"

struct port_list_entry {
	unsigned int port;
	TAILQ_ENTRY(port_list_entry) entries;
};

TAILQ_HEAD(port_list, port_list_entry);

struct title_list_entry {
	char *title;
	struct port_list port_list;
	TAILQ_ENTRY(title_list_entry) entries;
};

TAILQ_HEAD(title_list, title_list_entry);

struct orphan_list_entry {
	char *title;
	char *data;
	struct port_list port_list;
	TAILQ_ENTRY(orphan_list_entry) entries;
};

TAILQ_HEAD(orphan_list, orphan_list_entry);

static struct title_list title_list;
static struct orphan_list orphan_list;
static bool available_port_map[BACKLOG] = {0};

static struct option parameters[] = {
	{ "help",				no_argument,		0,	'h'	},
	{ "version",			no_argument,		0,	'v'	},
	{ NULL,					0,					0, 	0 	},
};

static void print_help_exit (char *arg)
{
	if (!arg) {
		return;
	}
	printf("\n%s help:\n\n", arg);

	printf("--version:\t('v')\n\t\treturns version\n\n");

	exit(OK);
}

static struct orphan_list_entry *find_entry_in_orphan_list(char *title, struct orphan_list *orphan_list)
{
	struct orphan_list_entry *entry = NULL;

	if (!title || !orphan_list) {
		errorf("args cannot be NULL\n");
		return NULL;
	}

	TAILQ_FOREACH(entry, orphan_list, entries) {
		if (entry->title && strcmp(title, entry->title) == 0) {
			return entry;
		}
	}

	return NULL;
}

static struct title_list_entry *find_entry_in_title_list(char *title, struct title_list *title_list)
{
	struct title_list_entry *entry = NULL;

	if (!title || !title_list) {
		errorf("args cannot be NULL\n");
		return NULL;
	}

	TAILQ_FOREACH(entry, title_list, entries) {
		if (entry->title && strcmp(title, entry->title) == 0) {
			return entry;
		}
	}

	return NULL;
}

static bool is_port_int_the_list(unsigned int port, struct port_list *port_list)
{
	struct port_list_entry *entry = NULL;

	if (!port_list || port < STARTING_PORT || port > STARTING_PORT + BACKLOG) {
		errorf("args cannot be NULL\n");
		return false;
	}

	TAILQ_FOREACH(entry, port_list, entries) {
		if (entry->port == port) {
			return true;
		}
	}

	return false;
}

static int add_new_entry_to_the_port_list(unsigned int port, struct port_list *port_list)
{
	struct port_list_entry *entry = NULL;

	if (!port_list || port < STARTING_PORT || port > STARTING_PORT + BACKLOG) {
		errorf("args cannot be NULL\n");
		return NOK;
	}

	entry = (struct port_list_entry *)calloc(1, sizeof(struct port_list_entry));
	if (!entry) {
		errorf("data is null\n");
		return NOK;
	}

	entry->port = port;

	TAILQ_INSERT_HEAD(port_list, entry, entries);

	return OK;
}

static int add_new_entry_to_title_list(char *title, unsigned int port, struct title_list *title_list)
{
	struct title_list_entry *entry = NULL;

	if (!title || port < STARTING_PORT || port > STARTING_PORT + BACKLOG || !title_list) {
		errorf("args cannot be NULL\n");
		return NOK;
	}

	entry = (struct title_list_entry *)calloc(1, sizeof(struct title_list_entry));
	if (!entry) {
		errorf("data is null\n");
		return NOK;
	}

	entry->title = calloc(strlen(title) + 1, sizeof(char));
	if (!entry->title) {
		errorf("calloc fail\n");
		FREE(entry);
		return NOK;
	}

	strncpy(entry->title, title, strlen(title));
	TAILQ_INIT(&(entry->port_list));

	if (add_new_entry_to_the_port_list(port, &(entry->port_list)) == NOK) {
		errorf("add_new_entry_to_the_port_list() failed\n");
		FREE(entry->title);
		FREE(entry);
		return NOK;
	}

	TAILQ_INSERT_HEAD(title_list, entry, entries);

	return OK;
}

static int add_port_title_couple(char *title, unsigned int port, struct title_list *title_list)
{
	struct title_list_entry *entry = NULL;

	if (!title || port < STARTING_PORT || port > STARTING_PORT + BACKLOG || !title_list) {
		errorf("args cannot be NULL\n");
		return NOK;
	}

	if ((entry = find_entry_in_title_list(title, title_list)) == NULL) {
		debugf("add new entry to the title list\n");
		return add_new_entry_to_title_list(title, port, title_list);
	}

	if (is_port_int_the_list(port, &(entry->port_list)) == false) {
		if (add_new_entry_to_the_port_list(port, &(entry->port_list)) == NOK) {
			errorf("add_new_entry_to_the_port_list() failed\n");
			return NOK;
		}
		debugf("port %d is registered for the title '%s'\n", port, title);
	} else {
		debugf("port %d is already registered for the title '%s'\n", port, title);
	}

	return OK;
}

static void port_data_structure_destroy(struct port_list *port_list)
{
	struct port_list_entry *entry1 = NULL;
	struct port_list_entry *entry2 = NULL;

	if (!port_list) {
		errorf("args cannot be NULL\n");
		return;
	}

	if (TAILQ_EMPTY(port_list)) {
		return;
	}

	entry1 = TAILQ_FIRST(port_list);
	while (entry1 != NULL) {
		entry2 = TAILQ_NEXT(entry1, entries);
		FREE(entry1);
		entry1 = entry2;
	}

	TAILQ_INIT(port_list); 
}

static int remove_port_from_orphan(char *title, unsigned int port, struct orphan_list *orphan_list)
{
	struct orphan_list_entry *entry = NULL;
	struct port_list_entry *pentry = NULL;

	if (!title || port < STARTING_PORT || port > STARTING_PORT + BACKLOG || !orphan_list) {
		errorf("args cannot be NULL\n");
		return NOK;
	}

	if ((entry = find_entry_in_orphan_list(title, orphan_list)) == NULL) {
		return OK;
	}

	if (TAILQ_EMPTY(&(entry->port_list))) {
		return OK;
	}

	TAILQ_FOREACH(pentry, &(entry->port_list), entries) {
		if (pentry && port == pentry->port) {
			TAILQ_REMOVE(&(entry->port_list), pentry, entries);
			FREE(pentry);
			break;
		}
	}

	if (entry && (&(entry->port_list)) && TAILQ_EMPTY(&(entry->port_list))) {
		TAILQ_INIT(&(entry->port_list));
	}

	return OK;
}

static int remove_port_from_title(char *title, unsigned int port, struct title_list *title_list)
{
	struct title_list_entry *entry = NULL;
	struct port_list_entry *pentry = NULL;

	if (!title || port < STARTING_PORT || port > STARTING_PORT + BACKLOG || !title_list) {
		errorf("args cannot be NULL\n");
		return NOK;
	}

	if ((entry = find_entry_in_title_list(title, title_list)) == NULL) {
		return OK;
	}

	if (TAILQ_EMPTY(&(entry->port_list))) {
		return OK;
	}

	TAILQ_FOREACH(pentry, &(entry->port_list), entries) {
		if (pentry && port == pentry->port) {
			TAILQ_REMOVE(&(entry->port_list), pentry, entries);
			FREE(pentry);
			break;
		}
	}

	if (entry && (&(entry->port_list)) && TAILQ_EMPTY(&(entry->port_list))) {
		TAILQ_INIT(&(entry->port_list));
	}

	return OK;
}

static int remove_port_from_all_title(unsigned int port, struct title_list *title_list)
{
	struct title_list_entry *entry = NULL;
	struct port_list_entry *pentry = NULL;

	if (port < STARTING_PORT || port > STARTING_PORT + BACKLOG || !title_list) {
		errorf("args cannot be NULL\n");
		return NOK;
	}

	if (TAILQ_EMPTY(title_list)) {
		return OK;
	}

	TAILQ_FOREACH(entry, title_list, entries) {
		if (!entry || !(&(entry->port_list)) || TAILQ_EMPTY(&(entry->port_list))) {
			break;
		}
		TAILQ_FOREACH(pentry, &(entry->port_list), entries) {
			if (!pentry) {
				continue;
			}
			if (port == pentry->port) {
				TAILQ_REMOVE(&(entry->port_list), pentry, entries);
				FREE(pentry);
				break;
			}
		}
		if (entry && (&(entry->port_list)) && TAILQ_EMPTY(&(entry->port_list))) {
			TAILQ_INIT(&(entry->port_list));
		}
	}

	return OK;
}

static int sipc_send_packet_daemon(struct _packet *packet, int fd)
{
	if (!packet || !packet->title || fd < 0) {
		errorf("args cannot be NULL\n");
		return NOK;
	}

	errno = 0;
	if (send(fd, &packet->packet_type, sizeof(packet->packet_type), MSG_NOSIGNAL) == -1) {
		errorf("send() failed with %d: %s\n", errno, strerror(errno));
		return NOK;
	}

	errno = 0;
	if (send(fd, &packet->title_size, sizeof(packet->title_size), MSG_NOSIGNAL) == -1) {
		errorf("send() failed with %d: %s\n", errno, strerror(errno));
		return NOK;
	}

	errno = 0;
	if (packet->title_size && send(fd, packet->title, packet->title_size, MSG_NOSIGNAL) == -1) {
		errorf("send() failed with %d: %s\n", errno, strerror(errno));
		return NOK;
	}

	errno = 0;
	if (send(fd, &packet->payload_size, sizeof(packet->payload_size), MSG_NOSIGNAL) == -1) {
		errorf("send() failed with %d: %s\n", errno, strerror(errno));
		return NOK;
	}

	errno = 0;
	if (packet->payload_size && packet->payload && send(fd, packet->payload, packet->payload_size, MSG_NOSIGNAL) == -1) {
		errorf("send() failed with %d: %s\n", errno, strerror(errno));
		return NOK;
	}

	return OK;
}

static int sipc_send_daemon(char *title, enum _packet_type packet_type, void *data, unsigned int len, unsigned int _port)
{
	int ret = OK;
	int fd = 0;
	struct sockaddr_storage address;
	struct _packet packet;

	if (!title) {
		errorf("title cannot be NULL\n");
		goto fail;
	}

	memset((void *)&address, 0, sizeof(struct sockaddr_in6));
	memset(&packet, 0, sizeof(struct _packet));

	if (sipc_buf_to_sockstorage(IPV6_LOOPBACK_ADDR, _port, &address) == NOK) {
		errorf("sipc_buf_to_sockstorage() failed\n");
		return NOK;
	}

	fd = sipc_socket_open_use_buf(IPV6_LOOPBACK_ADDR, SOCK_STREAM, 0);
	if (fd == -1) {
		errorf("socket() failed with %d: %s\n", errno, strerror(errno));
		return NOK;
	}

	if (sipc_connect_socket(fd, (struct sockaddr*)&address) < 0) {
		errorf("connect() failed with %d: %s\n", errno, strerror(errno));
		goto fail;
	}

	packet.title = strdup(title);
	if (!packet.title) {
		errorf("strdup failed\n");
		goto fail;
	}
	packet.title_size = strlen(title) + 1;
	packet.packet_type = (unsigned char)packet_type;
	packet.payload_size = 0;
	packet.payload = NULL;

	if (data && len) {
		packet.payload = (void *)calloc(1, len + 1);
		if (!packet.payload) {
			errorf("calloc failed\n");
			goto fail;
		}
		strncpy(packet.payload, data, len);
		packet.payload_size = len + 1;

		debugf("send data '%s' to port '%d'\n", packet.payload, _port);
	}

	if (sipc_send_packet_daemon(&packet, fd) == NOK) {
		errorf("sipc_send_packet_daemon() failed with %d: %s\n", errno, strerror(errno));
		goto fail;
	}

	goto out;

fail:
	ret = NOK;

out:
	if (fd) {
		close(fd);
	}
	FREE(packet.title);
	FREE(packet.payload);

	return ret;
}

static int send_data_to_all_title(char *title, char *data, struct title_list *title_list)
{
	struct title_list_entry *entry = NULL;
	struct port_list_entry *pentry = NULL;

	if (!title || !title_list || !data) {
		errorf("args cannot be NULL\n");
		return NOK;
	}

	if ((entry = find_entry_in_title_list(title, title_list)) == NULL) {
		return NOK;
	}

	TAILQ_FOREACH(pentry, &(entry->port_list), entries) {
		if (sipc_send_daemon(title, SENDATA, (void *)data, strlen(data), pentry->port) == NOK) {
			errorf("sipc_send() failed\n");
		}
	}

	return OK;
}

static int add_data_to_orphan_list(char *title, char *data, struct orphan_list *orphan_list)
{
	int ret = OK;
	struct orphan_list_entry *entry = NULL;

	if (!orphan_list || !title || !data) {
		errorf("args cannot be NULL\n");
		return NOK;
	}

	entry = (struct orphan_list_entry *)calloc(1, sizeof(struct orphan_list_entry));
	if (!entry) {
		errorf("data is null\n");
		return NOK;
	}

	entry->data = strdup(data);
	if (!entry->data) {
		goto fail;
	}

	entry->title = strdup(title);
	if (!entry->title) {
		errorf("strdup failed\n");
		goto fail;
	}

	TAILQ_INIT(&(entry->port_list));

	TAILQ_INSERT_HEAD(orphan_list, entry, entries);

	goto success;

fail:
	ret = NOK;

	FREE(entry->data);
	FREE(entry->title);
	FREE(entry);

success:

	return ret;
}

static int send_orphan_data_first(char *title, unsigned int port, struct orphan_list *orphan_list)
{
	bool should_send = true;
	struct orphan_list_entry *entry = NULL;
	struct port_list_entry *pentry = NULL;

	if (!title || port < STARTING_PORT || port > STARTING_PORT + BACKLOG || !orphan_list) {
		errorf("args cannot be NULL\n");
		return NOK;
	}

	TAILQ_FOREACH(entry, orphan_list, entries) {
		if (entry->title && strcmp(title, entry->title) == 0) {
			if (entry->data) {
				should_send = true;
				TAILQ_FOREACH(pentry, &(entry->port_list), entries) {
					if (should_send && pentry && port == pentry->port) {
						should_send = false;
					}
					debugf("port %d is '%d'\n", i++, pentry->port);
				}
				if (should_send) {
					if (sipc_send_daemon(title, SENDATA, (void *)entry->data, strlen(entry->data), port) == NOK) {
						errorf("sipc_send() failed\n");
						return NOK;
					}
					debugf("orphan data '%s' send for the title '%s' to the port '%d'\n", entry->data, entry->title, port);
					if (add_new_entry_to_the_port_list(port, &(entry->port_list)) == NOK) {
						errorf("add_new_entry_to_the_port_list() failed\n");
						return NOK;
					}
				}
			}
		}
	}

	return OK;
}

static int sipc_packet_handler_daemon(struct _packet *packet, unsigned int port, struct title_list *title_list, struct orphan_list *orphan_list, bool *available_ports)
{
	int ret = OK;
	unsigned int lport = 0;
	char *ptr = NULL;

	if (!packet) {
		errorf("args cannot be NULL\n");
		goto fail;
	}
	
	debugf("incoming packet type '%s'\n", packet_type_beautiy((enum _packet_type)packet->packet_type));

	switch (packet->packet_type) {
		case REGISTER:
			debugf("try to add '%d' port for the title '%s'\n", port, packet->title);
			if (add_port_title_couple(packet->title, port, title_list) == NOK) {
				errorf("add_port_title_couple() failed\n");
				goto fail;
			}
			debugf("title '%s' newly added, send orphan data first\n",  packet->title);
			if (send_orphan_data_first(packet->title, port, orphan_list) == NOK) {
				errorf("send_orphan_data_first() failed\n");
				goto fail;
			}
			available_ports[port - STARTING_PORT] = true;
			break;
		case UNREGISTER:
			if (!packet->payload) {
				errorf("paload i null\n");
				goto fail;
			}

			lport = strtoul(packet->payload, &ptr, 10);
			debugf("try to remove '%d' port for the title '%s'\n", lport, packet->title);

			if (!lport || lport < STARTING_PORT || lport > STARTING_PORT + BACKLOG) {
				debugf("port is incorrect, continue sliently\n");
				break;
			}

			if (remove_port_from_title(packet->title, lport, title_list) == NOK) {
				errorf("remove_port_from_title() failed\n");
				goto fail;
			}
			available_ports[lport - STARTING_PORT] = false;

			if (remove_port_from_orphan(packet->title, lport, orphan_list) == NOK) {
				errorf("remove_port_from_orphan() failed\n");
				goto fail;
			}
			break;
		case UNREGISTER_ALL:
			if (!packet->payload) {
				errorf("paload i null\n");
				goto fail;
			}

			lport = strtoul(packet->payload, &ptr, 10);
			debugf("remove '%d' port from all titles\n",  lport);

			if (!lport || lport < STARTING_PORT || lport > STARTING_PORT + BACKLOG) {
				debugf("port is incorrect, continue sliently\n");
				break;
			}

			if (remove_port_from_all_title(lport, title_list) == NOK) {
				errorf("remove_port_from_all_title() failed\n");
				goto fail;
			}
			available_ports[lport - STARTING_PORT] = false;
			break;
		case SENDATA:
			debugf("send data '%s' to title '%s'\n",  packet->payload, packet->title);
			if (send_data_to_all_title(packet->title, packet->payload, title_list) == NOK) {
				debugf("send_data_to_all_title() failed\n");
				if (add_data_to_orphan_list(packet->title, packet->payload, orphan_list) == NOK) {
					errorf("add_data_to_orphan() failed\n");
					goto fail;
				} else {
					debugf("data added to the orphan list\n");
				}
			}
			break;
		default:
			break;
	}

	goto out;

fail:
	ret = NOK;

out:
	return ret;
}

static unsigned int next_available_port(bool *available_ports)
{
	unsigned int i = 0;

	if (!available_ports) {
		return 0;
	}

	for (i = 0; i < BACKLOG; i++) {
		if (available_ports[i] == false) {
			return i + STARTING_PORT;
		}
	}

	return 0;
}

static int sipc_read_data_daemon(int sockfd, struct title_list *title_list, struct orphan_list *orphan_list, bool *available_ports)
{
	int ret = OK;
	int byte_write;
	struct _packet packet;
	unsigned int old_port = 0;
	unsigned int next_port = 0;

	if (!title_list || !orphan_list || !available_ports || sockfd < 0) {
		errorf("args cannot be NULL\n");
		goto fail;
	}

	memset(&packet, 0, sizeof(struct _packet));

	errno = 0;
	if (recv(sockfd, &packet.packet_type, sizeof(packet.packet_type), 0) < 0) {
		errorf("recv error from socket %d, errno: %d\n", sockfd, errno);
		goto fail;
	}

	errno = 0;
	if (recv(sockfd, &packet.title_size, sizeof(packet.title_size), 0) < 0) {
		errorf("recv error from socket %d, errno: %d\n", sockfd, errno);
		goto fail;
	}

	if (!packet.title_size) {
		errorf("packet title size should be greater than zero\n");
		goto out;
	}
	packet.title = (char *)calloc(packet.title_size, sizeof(char));
	if (!packet.title) {
		errorf("calloc failed\n");
		goto fail;
	}
	errno = 0;
	if (recv(sockfd, packet.title, packet.title_size, 0) < 0) {
		errorf("recv error from socket %d, errno: %d\n", sockfd, errno);
		goto fail;
	}

	errno = 0;
	if (recv(sockfd, &packet.payload_size, sizeof(packet.payload_size), 0) < 0) {
		errorf("recv error from socket %d, errno: %d\n", sockfd, errno);
		goto fail;
	}

	if (!packet.payload_size) {
		goto proceed;
	}

	packet.payload = (char *)calloc(packet.payload_size + 1, sizeof(char));
	if (!packet.payload) {
		errorf("calloc failed\n");
		goto fail;
	}
	errno = 0;
	if (recv(sockfd, packet.payload, packet.payload_size, 0) < 0) {
		errorf("recv error from socket %d, errno: %d\n", sockfd, errno);
		goto fail;
	}

proceed:
	errno = 0;
	if (recv(sockfd, &old_port, sizeof(old_port), 0) < 0) {
		errorf("recv error from socket %d, errno: %d\n", sockfd, errno);
		goto fail;
	}

	if (!old_port && packet.packet_type == REGISTER) {
		next_port = next_available_port(available_ports);
		byte_write = send(sockfd, &next_port, sizeof(next_port), MSG_NOSIGNAL);
		if (byte_write != sizeof(next_port)) {
			errorf("Write error to socket %d.\n", sockfd);
			goto fail;
		}
		old_port = next_port;

		sleep(5); //give time to caller to create server thread
	}

	if (sipc_packet_handler_daemon(&packet, old_port, title_list, orphan_list, available_ports) == NOK) {
		errorf("sipc_packet_handler() failed\n");
		goto fail;
	}

	goto out;

fail:
	ret = NOK;

out:
	FREE(packet.title);
	FREE(packet.payload);

	return ret;
}

static void dump_title_list(struct title_list *title_list)
{
	struct title_list_entry *entry = NULL;
	struct port_list_entry *pentry = NULL;

	if (!title_list) {
		errorf("args cannot be NULL\n");
		return;
	}

	debugf("dump title list\n");
	TAILQ_FOREACH(entry, title_list, entries) {
		if (entry && entry->title) {
			debugf("\tdump title: %s\n", entry->title);
		}
		TAILQ_FOREACH(pentry, &(entry->port_list), entries) {
			if (pentry) {
				debugf("\t\tdump port: %d\n", pentry->port);
			}
		}
	}
}

static void dump_orphan_list(struct orphan_list *orphan_list)
{
	struct orphan_list_entry *entry = NULL;
	struct port_list_entry *pentry = NULL;

	if (!orphan_list) {
		errorf("args cannot be NULL\n");
		return;
	}

	debugf("dump orphan list\n");
	TAILQ_FOREACH(entry, orphan_list, entries) {
		if (entry && entry->title) {
			debugf("\tdump title: %s\n", entry->title);
		}
		if (entry && entry->data) {
			debugf("\tdump data: %s\n", entry->data);
		}
		TAILQ_FOREACH(pentry, &(entry->port_list), entries) {
			if (pentry) {
				debugf("\t\tdump port: %d\n", pentry->port);
			}
		}
	}
}

static int sipc_create_server_daemon(struct title_list *title_list, struct orphan_list *orphan_list, bool *available_ports)
{
	int ret = OK;
	int enable = 1;
	int listen_fd, conn_fd, max_fd = 1, ret_val, i;
	struct sockaddr_storage client_addr, server_addr;
	char c_ip_addr[INET6_ADDRSTRLEN] = {0};
	fd_set backup_set, client_set;
	struct timeval tv;

	memset(c_ip_addr, 0, sizeof(c_ip_addr));
	memset(&server_addr, 0, sizeof(server_addr));
	memset(&client_addr, 0, sizeof(client_addr));

	if (sipc_fill_wildcard_sockstorage(PORT, AF_UNSPEC, &server_addr) != 0) {
		errorf("sipc_fill_wildcard_sockstorage() failed\n");
		goto fail;
	}

	if ((listen_fd = sipc_socket_open_use_sockaddr((struct sockaddr *)&server_addr, SOCK_STREAM, 0)) == -1) {
		errorf("sipc_socket_open_use_sockaddr() failed\n");
		goto fail;
	}

	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR , &enable, sizeof(int)) < 0) {
		errorf("setsockopt reuseport fail\n");
		goto fail;
	}

	if (sipc_bind_socket(listen_fd, (struct sockaddr *)&server_addr) == -1) {
		errorf("sipc_bind_socket() failed\n");
		goto fail;
	}

	if (sipc_socket_listen(listen_fd, BACKLOG) == -1) {
		errorf("sipc_socket_listen() failed\n");
		goto fail;
	}

	FD_ZERO(&backup_set);
	max_fd = listen_fd;
	FD_SET(listen_fd, &backup_set);
	tv.tv_usec = 0;

	for (;;) {
		tv.tv_sec = RECEIVE_TIMEOUT;
		memcpy(&client_set, &backup_set, sizeof(backup_set));

		ret_val = select(max_fd + 1, &client_set, NULL, NULL, &tv);

		if (ret_val < 0) {
			errorf("select error\n");
			continue;
		} else if (ret_val == 0) {
			//dump title and orphan list
			dump_title_list(title_list);
			dump_orphan_list(orphan_list);

			for (i = 0; i <= max_fd; i++) {
				if (FD_ISSET(i, &backup_set) && i != listen_fd) {
						close(i);
						FD_CLR(i, &backup_set);
				}
			}
			max_fd = listen_fd;
			continue;
		}

		if (FD_ISSET(listen_fd, &client_set)) {
			conn_fd = sipc_socket_accept(listen_fd, &client_addr);
			if (conn_fd < 0) {
				if (errno == EINTR) {
					debugf("CRS interrupted, try to accept again\n");
					continue;
				}
				errorf("accept error.\n");
				goto fail;
			}

			FD_SET(conn_fd, &backup_set);
			if (conn_fd > max_fd) {
				max_fd = conn_fd;
			}
			continue;
		}

		for (i = 0; i <= max_fd; i++) {
			if (FD_ISSET(i, &client_set) && i != listen_fd) {
				if (sipc_read_data_daemon(i, title_list, orphan_list, available_ports) == NOK) {
					errorf("sipc_read_data_daemon() failed\n");
					goto fail;
				}
				close(i);
				FD_CLR(i, &backup_set);
			}
		}
	}

	goto out;

fail:
	ret = NOK;

out:
	for (i = 0; i <= max_fd; i++) {
		close(i);
		FD_CLR(i, &backup_set);
	}

	return ret;
}

static void title_data_structure_destroy(struct title_list *title_list)
{
	struct title_list_entry *entry1 = NULL;
	struct title_list_entry *entry2 = NULL;

	if (!title_list) {
		errorf("args cannot be NULL\n");
		return;
	}

	if (TAILQ_EMPTY(title_list)) {
		return;
	}

	entry1 = TAILQ_FIRST(title_list);
	while (entry1 != NULL) {
		entry2 = TAILQ_NEXT(entry1, entries);
		FREE(entry1->title);
		port_data_structure_destroy(&(entry1->port_list));
		FREE(entry1);
		entry1 = entry2;
	}

	TAILQ_INIT(title_list); 
}

static void orphan_data_structure_destroy(struct orphan_list *orphan_list)
{
	struct orphan_list_entry *entry1 = NULL;
	struct orphan_list_entry *entry2 = NULL;

	if (!orphan_list) {
		errorf("args cannot be NULL\n");
		return;
	}

	if (TAILQ_EMPTY(orphan_list)) {
		return;
	}

	entry1 = TAILQ_FIRST(orphan_list);
	while (entry1 != NULL) {
		entry2 = TAILQ_NEXT(entry1, entries);
		port_data_structure_destroy(&(entry1->port_list));
		FREE(entry1->title);
		FREE(entry1->data);
		FREE(entry1);
		entry1 = entry2;
	}

	TAILQ_INIT(orphan_list);
}

static void sigint_handler(__attribute__((unused)) int sig_num)
{
	title_data_structure_destroy(&title_list);
	orphan_data_structure_destroy(&orphan_list);

	exit(NOK);
}

int main(int argc, char **argv)
{
	int ret = OK;
	int c, o;

	signal(SIGINT, sigint_handler);

	while ((c = getopt_long(argc, argv, "hv", parameters, &o)) != -1) {
		switch (c) {
			case 'h':
				print_help_exit(argv[0]);
				break;
			case 'v':
				debugf("%s version %s\n", argv[0], VERSION);
				return OK;
				break;
			default:
				debugf("unknown argument\n");
				goto fail;
		}
	}

	TAILQ_INIT(&title_list);
	TAILQ_INIT(&orphan_list);
	memset(available_port_map, 0, sizeof(bool) * BACKLOG);

	if (sipc_create_server_daemon(&title_list, &orphan_list, available_port_map) == NOK) {
		errorf("sipc_create_server_daemon() failed\n");
		goto fail;
	}

	goto out;

fail:
	ret = NOK;

out:
	title_data_structure_destroy(&title_list);
	orphan_data_structure_destroy(&orphan_list);

	return ret;
}
