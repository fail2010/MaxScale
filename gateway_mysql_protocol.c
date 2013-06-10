/*
This file is distributed as part of the SkySQL Gateway. It is free
software: you can redistribute it and/or modify it under the terms of the
GNU General Public License as published by the Free Software Foundation,
version 2.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 51
Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

Copyright SkySQL Ab

*/


/*
23-05-2013
Empty mysql_protocol_handling
1)send handshake in accept

2) read data
3) alway send OK

Massimiliano Pinto
*/

#include "gw.h"
#include "dcb.h"
#include "session.h"

#define MYSQL_CONN_DEBUG
#undef MYSQL_CONN_DEBUG

int mysql_send_ok(int fd, int packet_number, int in_affected_rows, const char* mysql_message) {
	int n = 0;
        uint8_t *outbuf = NULL;
        uint8_t mysql_payload_size = 0;
        uint8_t mysql_packet_header[4];
        uint8_t *mysql_payload = NULL;
        uint8_t field_count = 0;
        uint8_t affected_rows = 0;
        uint8_t insert_id = 0;
        uint8_t mysql_server_status[2];
        uint8_t mysql_warning_count[2];

        affected_rows = in_affected_rows;
	
	        mysql_payload_size = sizeof(field_count) + sizeof(affected_rows) + sizeof(insert_id) + sizeof(mysql_server_status) + sizeof(mysql_warning_count);

        if (mysql_message != NULL) {
                mysql_payload_size += strlen(mysql_message);
        }

        // allocate memory for packet header + payload
        outbuf = (uint8_t *) calloc(1, sizeof(mysql_packet_header) + mysql_payload_size);

        // write packet header with packet number
        gw_mysql_set_byte3(mysql_packet_header, mysql_payload_size);
        mysql_packet_header[3] = packet_number;

        // write header
        memcpy(outbuf, mysql_packet_header, sizeof(mysql_packet_header));

        mysql_payload = outbuf + sizeof(mysql_packet_header);

        mysql_server_status[0] = 2;
        mysql_server_status[1] = 0;
        mysql_warning_count[0] = 0;
        mysql_warning_count[1] = 0;

        // write data
        memcpy(mysql_payload, &field_count, sizeof(field_count));
        mysql_payload = mysql_payload + sizeof(field_count);

        memcpy(mysql_payload, &affected_rows, sizeof(affected_rows));
        mysql_payload = mysql_payload + sizeof(affected_rows);

        memcpy(mysql_payload, &insert_id, sizeof(insert_id));
        mysql_payload = mysql_payload + sizeof(insert_id);

        memcpy(mysql_payload, mysql_server_status, sizeof(mysql_server_status));
        mysql_payload = mysql_payload + sizeof(mysql_server_status);

        memcpy(mysql_payload, mysql_warning_count, sizeof(mysql_warning_count));
        mysql_payload = mysql_payload + sizeof(mysql_warning_count);

        if (mysql_message != NULL) {
                memcpy(mysql_payload, mysql_message, strlen(mysql_message));
        }

        // write data
	n = write(fd, outbuf, sizeof(mysql_packet_header) + mysql_payload_size);

	free(outbuf);

	return n;
}


int MySQLSendHandshake(DCB* dcb) {
	int n = 0;
        uint8_t *outbuf = NULL;
        uint8_t mysql_payload_size = 0;
        uint8_t mysql_packet_header[4];
        uint8_t mysql_packet_id = 0;
        uint8_t mysql_filler = GW_MYSQL_HANDSHAKE_FILLER;
        uint8_t mysql_protocol_version = GW_MYSQL_PROTOCOL_VERSION;
        uint8_t *mysql_handshake_payload = NULL;
        uint8_t mysql_thread_id[4];
        uint8_t mysql_scramble_buf[9] = "";
        uint8_t mysql_plugin_data[13] = "";
        uint8_t mysql_server_capabilities_one[2];
        uint8_t mysql_server_capabilities_two[2];
        uint8_t mysql_server_language = 8;
        uint8_t mysql_server_status[2];
        uint8_t mysql_scramble_len = 21;
        uint8_t mysql_filler_ten[10];
        uint8_t mysql_last_byte = 0x00;
	char server_scramble[GW_MYSQL_SCRAMBLE_SIZE + 1]="";
	MySQLProtocol *protocol = DCB_PROTOCOL(dcb, MySQLProtocol);

	gw_generate_random_str(server_scramble, GW_MYSQL_SCRAMBLE_SIZE);
	
	// copy back to the caller
	memcpy(protocol->scramble, server_scramble, GW_MYSQL_SCRAMBLE_SIZE);

	// fill the handshake packet

	memset(&mysql_filler_ten, 0x00, sizeof(mysql_filler_ten));

        // thread id, now put thePID
        gw_mysql_set_byte4(mysql_thread_id, getpid() + dcb->fd);
	
        memcpy(mysql_scramble_buf, server_scramble, 8);

        memcpy(mysql_plugin_data, server_scramble + 8, 12);

        mysql_payload_size  = sizeof(mysql_protocol_version) + (strlen(GW_MYSQL_VERSION) + 1) + sizeof(mysql_thread_id) + 8 + sizeof(mysql_filler) + sizeof(mysql_server_capabilities_one) + sizeof(mysql_server_language) + sizeof(mysql_server_status) + sizeof(mysql_server_capabilities_two) + sizeof(mysql_scramble_len) + sizeof(mysql_filler_ten) + 12 + sizeof(mysql_last_byte) + strlen("mysql_native_password") + sizeof(mysql_last_byte);

        // allocate memory for packet header + payload
        outbuf = (uint8_t *) calloc(1, sizeof(mysql_packet_header) + mysql_payload_size);

        // write packet heder with mysql_payload_size
        gw_mysql_set_byte3(mysql_packet_header, mysql_payload_size);
        //mysql_packet_header[0] = mysql_payload_size;

        // write packent number, now is 0
        mysql_packet_header[3]= mysql_packet_id;
        memcpy(outbuf, mysql_packet_header, sizeof(mysql_packet_header));

        // current buffer pointer
        mysql_handshake_payload = outbuf + sizeof(mysql_packet_header);

        // write protocol version
        memcpy(mysql_handshake_payload, &mysql_protocol_version, sizeof(mysql_protocol_version));
        mysql_handshake_payload = mysql_handshake_payload + sizeof(mysql_protocol_version);

        // write server version plus 0 filler
        strcpy(mysql_handshake_payload, GW_MYSQL_VERSION);
        mysql_handshake_payload = mysql_handshake_payload + strlen(GW_MYSQL_VERSION);
        *mysql_handshake_payload = 0x00;

       mysql_handshake_payload++;

        // write thread id
        memcpy(mysql_handshake_payload, mysql_thread_id, sizeof(mysql_thread_id));
        mysql_handshake_payload = mysql_handshake_payload + sizeof(mysql_thread_id);

        // write scramble buf
        memcpy(mysql_handshake_payload, mysql_scramble_buf, 8);
        mysql_handshake_payload = mysql_handshake_payload + 8;
        *mysql_handshake_payload = GW_MYSQL_HANDSHAKE_FILLER;
        mysql_handshake_payload++;

        // write server capabilities part one
        mysql_server_capabilities_one[0] = GW_MYSQL_SERVER_CAPABILITIES_BYTE1;
        mysql_server_capabilities_one[1] = GW_MYSQL_SERVER_CAPABILITIES_BYTE2;


        mysql_server_capabilities_one[0] &= ~GW_MYSQL_CAPABILITIES_COMPRESS;
        mysql_server_capabilities_one[0] &= ~GW_MYSQL_CAPABILITIES_SSL;

        memcpy(mysql_handshake_payload, mysql_server_capabilities_one, sizeof(mysql_server_capabilities_one));
        mysql_handshake_payload = mysql_handshake_payload + sizeof(mysql_server_capabilities_one);

        // write server language
        memcpy(mysql_handshake_payload, &mysql_server_language, sizeof(mysql_server_language));
        mysql_handshake_payload = mysql_handshake_payload + sizeof(mysql_server_language);

        //write server status
        mysql_server_status[0] = 2;
        mysql_server_status[1] = 0;
        memcpy(mysql_handshake_payload, mysql_server_status, sizeof(mysql_server_status));
        mysql_handshake_payload = mysql_handshake_payload + sizeof(mysql_server_status);

        //write server capabilities part two
        mysql_server_capabilities_two[0] = 15;
        mysql_server_capabilities_two[1] = 128;

        memcpy(mysql_handshake_payload, mysql_server_capabilities_two, sizeof(mysql_server_capabilities_two));
        mysql_handshake_payload = mysql_handshake_payload + sizeof(mysql_server_capabilities_two);

        // write scramble_len
        memcpy(mysql_handshake_payload, &mysql_scramble_len, sizeof(mysql_scramble_len));
        mysql_handshake_payload = mysql_handshake_payload + sizeof(mysql_scramble_len);

        //write 10 filler
        memcpy(mysql_handshake_payload, mysql_filler_ten, sizeof(mysql_filler_ten));
        mysql_handshake_payload = mysql_handshake_payload + sizeof(mysql_filler_ten);

        // write plugin data
        memcpy(mysql_handshake_payload, mysql_plugin_data, 12);
        mysql_handshake_payload = mysql_handshake_payload + 12;

        //write last byte, 0
        *mysql_handshake_payload = 0x00;
        mysql_handshake_payload++;

        // to be understanded ????
        memcpy(mysql_handshake_payload, "mysql_native_password", strlen("mysql_native_password"));
        mysql_handshake_payload = mysql_handshake_payload + strlen("mysql_native_password");

        //write last byte, 0
        *mysql_handshake_payload = 0x00;

        mysql_handshake_payload++;

	
	// write it to the socket
	// this not covers the EAGAIN | EWOULDBLOCK
	n = write(dcb->fd, outbuf, sizeof(mysql_packet_header) + mysql_payload_size);

	free(outbuf);

	return n;
}

int gw_mysql_do_authentication(DCB *dcb) {
	int packet_no;
	MySQLProtocol *protocol = DCB_PROTOCOL(dcb, MySQLProtocol);

	// read client auth
	packet_no = do_read_dcb(dcb);

	// if we received all data!
	protocol->state = MYSQL_AUTH_RECV;

	fprintf(stderr, "DoAuth DCB [%i], EPOLLIN Protocol next state MYSQL_AUTH_RECV [%i], Packet #%i for socket %i, scramble [%s]\n", dcb->state, protocol->state, packet_no, dcb->fd, protocol->scramble);

	//write to client mysql AUTH_OK packet, packet n. is 2
	mysql_send_ok(dcb->fd, 2, 0, NULL);

	protocol->state = MYSQL_IDLE;

	fprintf(stderr, "DCB [%i], EPOLLIN Protocol next state MYSQL_IDLE [%i], Packet #%i for socket %i, scramble [%s]\n", dcb->state, protocol->state, packet_no, dcb->fd, protocol->scramble);

	return 0;
}

int gw_mysql_read_packet_10(DCB *dcb, uint8_t *buffer) {
	int n;
	n = do_read_dcb10(dcb, buffer);

	return n;
}

int gw_mysql_read_packet(DCB *dcb, uint8_t *buffer) {
	int n;

	n = do_read_buffer(dcb, buffer);

	return n;
}

int gw_mysql_read_command(DCB *dcb) {
	int packet_no;
	MySQLProtocol *protocol = DCB_PROTOCOL(dcb, MySQLProtocol);

	packet_no = do_read_dcb(dcb);

	fprintf(stderr, "DCB [%i], EPOLLIN Protocol entering into MYSQL_IDLE [%i], Packet #%i for socket %i, scramble [%s]\n", dcb->state, protocol->state, packet_no, dcb->fd, protocol->scramble);

	if (packet_no == -2)
		return 1;

	if (packet_no < 0) {
		fprintf(stderr, "DCB [%i], EPOLLIN Protocol exiting from MYSQL_IDLE [%i], Packet #%i for socket %i, scramble [%s]\n", dcb->state, protocol->state, packet_no, dcb->fd, protocol->scramble);

		(dcb->func).error(dcb, -1);


		fprintf(stderr, "closing fd [%i], from MYSQL_IDLE\n", dcb->fd);

		if (dcb->fd) {
			if (!close (dcb->fd)) {
				if(dcb) {
					SESSION *temp = dcb->session;
					if (dcb->session) {
						if (temp->backends) {
						gw_mysql_close((MySQLProtocol **)&(temp->backends)->protocol);
						//if(dcb->backend)
							//free(dcb->protocol);
						free(dcb->session);
						}
					}
					free(dcb);
				}
			}
		}

		// client session closed, continue
		return 1;
	} else {
		packet_no++;
	}

	// send read data to backend ...
	// coming soon|
	fprintf(stderr, "DCB [%i], EPOLLIN Protocol is responding from MYSQL_IDLE [%i], Packet #%i for socket %i, scramble [%s]\n", dcb->state, protocol->state, packet_no, dcb->fd, protocol->scramble);
	
	// could be a mysql_ping() reply
	// writing the result set would come from async read from backends

	mysql_send_ok(dcb->fd, packet_no, 0, NULL);

	return 0;
}

///////////////////////////////////////
// MYSQL_conn structure setup
///////////////////////////////////////
MySQLProtocol *gw_mysql_init(MySQLProtocol *data) {
        int rv = -1;

        MySQLProtocol *input = NULL;

        if (input == NULL) {
                // structure allocation
                input = calloc(1, sizeof(MySQLProtocol));

                if (input == NULL)
                        return NULL;

        }

#ifdef MYSQL_CONN_DEBUG
	fprintf(stderr, "gw_mysql_init() called\n");
#endif

        return input;
}



/////////////////////
// MySQL connect
/////////////////////

int gw_mysql_connect(char *host, int port, char *dbname, char *user, char *passwd, MySQLProtocol *conn, int compress) {

        struct sockaddr_in serv_addr;
        socklen_t addrlen;

	int rv;
	int so = 0;

	int ciclo = 0;
	char buffer[SMALL_CHUNK];
	uint8_t packet_buffer[SMALL_CHUNK];
	char errmesg[128];
	uint8_t *payload = NULL;
	int server_protocol;
	char server_version[100]="";
	uint8_t *server_version_end = NULL;
	uint16_t mysql_server_capabilities_one;
	uint16_t mysql_server_capabilities_two;
	int fd;
	unsigned long tid =0;
	long bytes;
	uint8_t scramble_data_1[8 + 1] = "";
	uint8_t scramble_data_2[12 + 1] = "";
	uint8_t scramble_data[GW_MYSQL_SCRAMBLE_SIZE + 1] = "";
	uint8_t capab_ptr[4];
	int scramble_len;
	uint8_t scramble[GW_MYSQL_SCRAMBLE_SIZE + 1];
	uint8_t client_scramble[GW_MYSQL_SCRAMBLE_SIZE + 1];
	uint8_t client_capabilities[4];
	uint32_t server_capabilities;
	uint32_t final_capabilities;
	char dbpass[500]="";

	conn->state = MYSQL_ALLOC;
	conn->fd = -1;


        memset(&serv_addr, 0, sizeof serv_addr);
        serv_addr.sin_family = AF_INET;

	so = socket(AF_INET,SOCK_STREAM,0);
	if (so < 0) {
		fprintf(stderr, "Errore creazione socket: [%s] %i\n", strerror(errno), errno);
		return 1;
	}

	conn->fd = so;

	// set NONBLOCKING mode
	//setnonblocking(so);

	setipaddress(&serv_addr.sin_addr, host);
	serv_addr.sin_port = htons(port);

#ifdef MYSQL_CONN_DEBUG
	fprintf(stderr, "Socket initialized\n");
	fflush(stderr);
#endif

	while(1) {
		if ((rv = connect(so, (struct sockaddr *)&serv_addr, sizeof(serv_addr))) < 0) {
			fprintf(stderr, "Errore connect %i, %s: RV = [%i]\n", errno, strerror(errno), rv);
			
			if (errno == EINPROGRESS) {
				continue;
			} else {
				close(so);
				return -1;
			}
		} else {
			break;
		}
	}

#ifdef MYSQL_CONN_DEBUG
	fprintf(stderr, "CONNECT is DONE\n");
	fprintf(stderr, "Socket FD is %i\n", so);
	fflush(stderr);
#endif


	memset(&buffer, '\0', sizeof(buffer));

	bytes = 16384;

	rv = read(so, buffer, bytes);

	if ( rv >0 ) {
#ifdef MYSQL_CONN_DEBUG
		fprintf(stderr, "RESPONSE ciclo %i HO letto [%s] bytes %li\n",ciclo, buffer, bytes);
		fflush(stderr);
#endif
		ciclo++;
	} else {
		if (rv == 0 && errno == EOF) {
#ifdef MYSQL_CONN_DEBUG
			fprintf(stderr, "EOF reached. Bytes = %li\n", bytes);
			fflush(stderr);
#endif
		} else {
#ifdef MYSQL_CONN_DEBUG
			fprintf(stderr, "###### Receive error FINAL : connection not completed %i %s:  RV = [%i]\n", errno, strerror(errno), rv);
#endif
			close(so);

			return -1;
		}
	}

#ifdef MYSQL_CONN_DEBUG
	fwrite(buffer, bytes, 1, stderr);
	fflush(stderr);
#endif

	//decode mysql handshake

	payload = buffer + 4;
	server_protocol= payload[0];

#ifdef MYSQL_CONN_DEBUG
	fprintf(stderr, "Server Protocol [%i]\n", server_protocol);

#endif
	payload++;

#ifdef MYSQL_CONN_DEBUG
	fprintf(stderr, "Protocol Version [%s]\n", payload);
	fflush(stderr);
#endif

	server_version_end = gw_strend((char*) payload);
	payload = server_version_end + 1;

	// TID
	tid = gw_mysql_get_byte4(payload);
	memcpy(&conn->tid, &tid, 4);

#ifdef MYSQL_CONN_DEBUG
	fprintf(stderr, "Thread ID is %lu\n", conn->tid);
	fflush(stderr);
#endif

	payload +=4;

	// scramble_part 1
	memcpy(scramble_data_1, payload, 8);
	payload += 8;

	// 1 filler
	payload++;

	mysql_server_capabilities_one = gw_mysql_get_byte2(payload);

#ifdef MYSQL_CONN_DEBUG
	fprintf(stderr, "Capab_1[\n");
	fwrite(&mysql_server_capabilities_one, 2, 1, stderr);
	fflush(stderr);
#endif

	//2 capab_part 1 + 1 language + 2 server_status
	payload +=5;

	mysql_server_capabilities_two = gw_mysql_get_byte2(payload);

#ifdef MYSQL_CONN_DEBUG
	fprintf(stderr, "]Capab_2[\n");
	fwrite(&mysql_server_capabilities_two, 2, 1, stderr);
	fprintf(stderr, "]\n");
	fflush(stderr);
#endif

	memcpy(&capab_ptr, &mysql_server_capabilities_one, 2);

#ifdef MYSQL_CONN_DEBUG
	fprintf(stderr, "Capab_1[\n");
	fwrite(capab_ptr, 2, 1, stderr);
	fflush(stderr);
#endif

	memcpy(&(capab_ptr[2]), &mysql_server_capabilities_two, 2);

#ifdef MYSQL_CONN_DEBUG
	fprintf(stderr, "Capab_2[\n");
	fwrite(capab_ptr, 2, 1, stderr);
	fflush(stderr);
#endif

	// 2 capab_part 2
	payload+=2;

	scramble_len = payload[0] -1;

#ifdef MYSQL_CONN_DEBUG
	fprintf(stderr, "Scramble_len  [%i]\n", scramble_len);
	fflush(stderr);
#endif

	payload += 11;

	memcpy(scramble_data_2, payload, scramble_len - 8);

#ifdef MYSQL_CONN_DEBUG
	fprintf(stderr, "Scramble_buff1[");
	fwrite(scramble_data_1, 8, 1, stderr);
	fprintf(stderr, "]\nScramble_buff2  [");
	fwrite(scramble_data_2, scramble_len - 8, 1, stderr);
	fprintf(stderr, "]\n");
	fflush(stderr);
#endif

	memcpy(scramble, scramble_data_1, 8);
	memcpy(scramble + 8, scramble_data_2, scramble_len - 8);

#ifdef MYSQL_CONN_DEBUG
	fprintf(stderr, "Full Scramble 20 bytes is  [\n");
	fwrite(scramble, GW_MYSQL_SCRAMBLE_SIZE, 1, stderr);
	fprintf(stderr, "\n]\n");
	fflush(stderr);
#endif

	memcpy(conn->scramble, scramble, GW_MYSQL_SCRAMBLE_SIZE);

#ifdef MYSQL_CONN_DEBUG
	fprintf(stderr, "Scramble from MYSQL_Conn is  [\n");
	fwrite(scramble, GW_MYSQL_SCRAMBLE_SIZE, 1, stderr);
	fprintf(stderr, "\n]\n");
	fflush(stderr);
	fprintf(stderr, "Now sending user, pass & db\n[");
	fwrite(&server_capabilities, 4, 1, stderr);
	fprintf(stderr, "]\n");
#endif

	final_capabilities = gw_mysql_get_byte4((uint8_t *)&server_capabilities);

#ifdef MYSQL_CONN_DEBUG
	fprintf(stderr, "CAPABS [%u]\n", final_capabilities);
	fflush(stderr);
#endif
	memset(packet_buffer, '\0', sizeof(packet_buffer));
	//packet_header(byte3 +1 pack#)
	packet_buffer[3] = '\x01';

	//final_capabilities = 1025669;

	final_capabilities |= GW_MYSQL_CAPABILITIES_PROTOCOL_41;
	final_capabilities |= GW_MYSQL_CAPABILITIES_CLIENT;
	if (compress) {
		final_capabilities |= GW_MYSQL_CAPABILITIES_COMPRESS;
		fprintf(stderr, "Backend Connection with compression\n");
		fflush(stderr);
	}

	if (passwd != NULL) {
		uint8_t hash1[20]="";
		uint8_t hash2[20]="";
		uint8_t new_sha[20]="";


		SHA1("massi", strlen("massi"), hash1);
		//memcpy(hash1, passwd, GW_MYSQL_SCRAMBLE_SIZE);
		gw_sha1_str(hash1, GW_MYSQL_SCRAMBLE_SIZE, hash2);
		gw_bin2hex(dbpass, hash2, GW_MYSQL_SCRAMBLE_SIZE);
		gw_sha1_2_str(scramble, GW_MYSQL_SCRAMBLE_SIZE, hash2, GW_MYSQL_SCRAMBLE_SIZE, new_sha);
		gw_str_xor(client_scramble, new_sha, hash1, GW_MYSQL_SCRAMBLE_SIZE);

#ifdef MYSQL_CONN_DEBUG
		fprintf(stderr, "Hash1 [%s]\n", hash1);
		fprintf(stderr, "Hash2 [%s]\n", hash2);
		fprintf(stderr, "SHA1(SHA1(password in hex)\n");
		fprintf(stderr, "PAss [%s]\n", dbpass);
		fflush(stderr);
		fprintf(stderr, "newsha [%s]\n", new_sha);
		fprintf(stderr, "Client send scramble 20 [\n");
		fwrite(client_scramble, GW_MYSQL_SCRAMBLE_SIZE, 1, stderr);
		fprintf(stderr, "\n]\n");
		fflush(stderr);
#endif
	}

	if (dbname == NULL) {
		// now without db!!
		final_capabilities &= ~GW_MYSQL_CAPABILITIES_CONNECT_WITH_DB;
	} else {
		final_capabilities |= GW_MYSQL_CAPABILITIES_CONNECT_WITH_DB;
	}


	gw_mysql_set_byte4(client_capabilities, final_capabilities);
	memcpy(packet_buffer + 4, client_capabilities, 4);

	//packet_buffer[4] = '\x8d';
	//packet_buffer[5] = '\xa6';
	//packet_buffer[6] = '\x0f';
	//packet_buffer[7] = '\x00';

	gw_mysql_set_byte4(packet_buffer + 4 + 4, 16777216);
	packet_buffer[12] = '\x08';

#ifdef MYSQL_CONN_DEBUG
	fprintf(stderr, "User is [%s]\n", user);
	fflush(stderr);
#endif
	strcpy(packet_buffer+36, user);

	bytes = 32 + 22 + 1 + 1;

	bytes += strlen(user);

	if (dbname == NULL) {
		//strcpy(packet_buffer+36 + 5 + 2, "mysql_native_password");
	} else {
		if (passwd != NULL) {
			*(packet_buffer+36 + 5 + 1) = 20;
			memcpy(packet_buffer+36 + 5 + 1 + 1, client_scramble, GW_MYSQL_SCRAMBLE_SIZE);
			strcpy(packet_buffer+36 + 5 + 1 + 1 + 20, dbname);
			//strcpy(packet_buffer+36 + 5 + 1 + 1 + 20 + strlen(dbname) + 1, "mysql_native_password");
			//bytes += 20 + strlen(dbname) + 1;
			bytes += strlen(dbname) -1;
		} else {
			strcpy(packet_buffer+36 + 5 + 1 + 1, dbname);
			//strcpy(packet_buffer+36 + 5 + 1 + 1 + strlen(dbname) + 1, "mysql_native_password");
			bytes += strlen(dbname) -1;
		}
	}

	gw_mysql_set_byte3(packet_buffer, bytes);

	bytes += 4;

	rv = write(so, packet_buffer, bytes);

#ifdef MYSQL_CONN_DEBUG
	fprintf(stderr, "Sent [%s], [%i] bytes\n", packet_buffer, bytes);
	fflush(stderr);
#endif

	if (rv == -1) {
		fprintf(stderr, "CONNECT Error in send auth\n");
	}

	bytes = 4096;

	memset(buffer, '\0', sizeof (buffer));

	rv = read(so, buffer, 4096);


	if (rv == -1) {
		fprintf(stderr, "CONNCET Error in recv OK for auth\n");
	}

#ifdef MYSQL_CONN_DEBUG
	fprintf(stderr, "ok packet\[");
	fwrite(buffer, bytes, 1, stderr);
	fprintf(stderr, "]\n");
	fflush(stderr);
#endif
	if (buffer[4] == '\x00') {
#ifdef MYSQL_CONN_DEBUG
		fprintf(stderr, "OK packet received, packet # %i\n", buffer[3]);
		fflush(stderr);
#endif
		conn->state = MYSQL_IDLE;

		return 0;
	}

	return 1;

}

//////////////////////////////////////
// close a connection if opened
// free data scructure
//////////////////////////////////////
void gw_mysql_close(MySQLProtocol **ptr) {
        int rv;
        uint8_t packet_buffer[5];
        MySQLProtocol *conn = *ptr;

        if (conn == NULL)
                return;

        if (conn->fd > 0) {

		//write COM_QUIT
		//write

#ifdef MYSQL_CONN_DEBUG
        	fprintf(stderr, "mysqlgw_mysql_close() called for %i\n", conn->fd);
#endif
                close(conn->fd);
        } else {
#ifdef MYSQL_CONN_DEBUG
        	fprintf(stderr, "mysqlgw_mysql_close() called, no socket\n");
#endif
	}

        if (conn != NULL) {
                free(conn);
#ifdef MYSQL_CONN_DEBUG
        	fprintf(stderr, "mysqlgw_mysql_close() free(conn)\n");
#endif
        }

}