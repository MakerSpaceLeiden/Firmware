/*
 * TFTPServer.cpp
 * Simple TFTP server
 *
 * Copyright (c) 2011 Jaap Vermaas
 *
 *   This file is part of the LaOS project (see: http://wiki.laoslaser.org)
 *
 *   LaOS is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   LaOS is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with LaOS.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "TFTPServer.h"

static Timeout timer;
static int to = 0;

// create a new tftp server, with file directory dir and
// listening on port

TFTPServer::TFTPServer(int myport) {
    port = myport;
    printf("TFTPServer(): port=%d\n", myport);
    ListenSock = new UDPSocket();
    state = listen;
    if (ListenSock->bind(port))
        state = tftperror;
    ListenSock->set_blocking(false, 1);
    filecnt = 0;
    WorkSock = NULL;
}

// destroy this instance of the tftp server
TFTPServer::~TFTPServer() {
    if (WorkSock) {
    	WorkSock->close();
	delete(WorkSock);
	WorkSock = NULL;
    };
    ListenSock->close();
    delete(ListenSock);
    strcpy(remote_ip,"");
    state = deleted;
}

void TFTPServer::reset() {
    ListenSock->close();
    delete(ListenSock);
    strcpy(remote_ip,"");
    ListenSock = new UDPSocket();
    state = listen;
    if (ListenSock->bind(port))
        state = tftperror;
    ListenSock->set_blocking(false, 1);
    strcpy(filename, "");
    filecnt = 0;
    if (WorkSock) {
    	WorkSock->close();
	delete(WorkSock);
	WorkSock = NULL;
    };
}

// get current tftp status
TFTPServerState TFTPServer::State() {
    return state;
}

// Temporarily disable incoming TFTP connections
void TFTPServer::suspend() {
    state = suspended;
}

// Resume after suspension
void TFTPServer::resume() {
    if (state == suspended)
        state = back_to_listen;
}

// During read and write, this gives you the filename
void TFTPServer::getFilename(char* name) {
    sprintf(name, "%s", filename);
}

// return number of received files
int TFTPServer::fileCnt() {
    return filecnt;
}

// create a new connection reading a file from server
void TFTPServer::ConnectRead(char* buff) {
    extern LaosFileSystem sd;
    remote_ip = client.get_address();
    remote_port = client.get_port();
    Ack(0);
    blockcnt = 0;
    dupcnt = 0;

    sprintf(filename, "%s", &buff[2]);
    
    if (modeOctet(buff))
        fp = sd.openfile(filename, "rb");
    else
        fp = sd.openfile(filename, "r");
    if (fp == NULL) {
        state  = back_to_listen;
        Err("Could not read file");
    } else {
        // file ready for reading
        blockcnt = 0;
        state = reading;
        #ifdef TFTP_DEBUG
            char debugmsg[256];
            sprintf(debugmsg, "Listen: Requested file %s from TFTP connection %d.%d.%d.%d port %d",
                filename, remote_ip[0], remote_ip[1], remote_ip[2], remote_ip[3], remote_port);
            TFTP_DEBUG(debugmsg);
        #endif
        getBlock();
        sendBlock();
    }
}

// create a new connection writing a file to the server
void TFTPServer::ConnectWrite(char* buff) {
    extern LaosFileSystem sd;
    remote_ip = client.get_address();
    remote_port = client.get_port();
    Ack(0);
    blockcnt = 0;
    dupcnt = 0;

    sprintf(filename, "%s", &buff[2]);
    sd.shorten(filename, MAXFILESIZE);
    if (modeOctet(buff))
        fp = sd.openfile(filename, "wb");
    else
        fp = sd.openfile(filename, "w");
    if (fp == NULL) {
        Err("Could not open file to write");
        state  = back_to_listen;
        strcpy(remote_ip,"");
    } else {
        // file ready for writing
        blockcnt = 0;
        state = writing;
        #ifdef TFTP_DEBUG 
            char debugmsg[256];
            sprintf(debugmsg, "Listen: Incoming file %s on TFTP connection from %d.%d.%d.%d remote_port %d",
                filename, remote_ip[0], remote_ip[1], remote_ip[2], remote_ip[3], remote_port);
            TFTP_DEBUG(debugmsg);
        #endif
    }
}

// get DATA block from file on disk into memory
void TFTPServer::getBlock() {
    blockcnt++;
    char *p;
    p = &sendbuff[4];
    int len = fread(p, 1, 512, fp);
    sendbuff[0] = 0x00;
    sendbuff[1] = 0x03;
    sendbuff[2] = blockcnt >> 8;
    sendbuff[3] = blockcnt & 255;
    blocksize = len+4;
}

// send DATA block to the client
void TFTPServer::sendBlock() {
    ListenSock->sendTo(client, sendbuff, blocksize);
}


// compare host IP and Port with connected remote machine
int TFTPServer::cmpHost() {
    char ip[17];
    strcpy(ip, client.get_address());
    int port = client.get_port();
    return ((strcmp(ip, remote_ip) == 0) && (port == remote_port));
}

// send ACK to remote
void TFTPServer::Ack(int val) {
    char ack[4];
    ack[0] = 0x00;
    ack[1] = 0x04;
    if ((val>603135) || (val<0)) val = 0;
    ack[2] = val >> 8;
    ack[3] = val & 255;
    WorkSock->sendTo(client, ack, 4);
}

// send ERR message to named client
void TFTPServer::Err(const std::string& msg) {
    char message[32];
    strncpy(message, msg.c_str(), 32);
    char err[37];
    sprintf(err, "0000%s0", message);
    err[0] = 0x00;
    err[1] = 0x05;
    err[2]=0x00;
    err[3]=0x00;
    int len = strlen(err);
    err[len-1] = 0x00;
    WorkSock->sendTo(client, err, len);

    #ifdef TFTP_DEBUG
        char debugmsg[256];
        sprintf(debugmsg, "Error: %s", message);
        TFTP_DEBUG(debugmsg);
    #endif

    state=back_to_listen;
    timer.detach();
    WorkSock->close();
    delete(WorkSock);
    WorkSock = NULL;
    to = 0;
}

// check if connection mode of client is octet/binary
int TFTPServer::modeOctet(char* buff) {
    int x = 2;
    while (buff[x++] != 0); // get beginning of mode field
    int y = x;
    while (buff[y] != 0) {
        buff[y] = tolower(buff[y]);
        y++;
    } // make mode field lowercase
    return (strcmp(&buff[x++], "octet") == 0);
}

static void timeout() {
   to = 1;
};

// event driven routines to handle incoming packets
void TFTPServer::poll() {
    static int l;
    if (l != state) printf("state change %d->%d\n", l, state);
    l = state;

    if (to) {
   	Err("Timeout; aborting connection");
	return;
    };

    if ((state == suspended) || (state == deleted) || (state == tftperror)) {
        return;
    }
    if (state == back_to_listen) {
	state = listen;
        timer.detach(); 
    };

    char buff[592]; int len;
    if (WorkSock) { 
	    WorkSock->set_blocking(false,1);
    	    len = WorkSock->receiveFrom(client, buff, sizeof(buff));
    } else {
	    ListenSock->set_blocking(false,1);
    	    len = ListenSock->receiveFrom(client, buff, sizeof(buff));
    };
    
    if (len == 0) {
        return;
    }

    bool rst = false;
    switch (state) {
        case listen: {
	    if (WorkSock) {
		printf("Shoudl nto hapepn\n");
		return;
	    };
	    // Pick a new source port - as per RFC1350, section 4-TID.
            //
            // Or see Stevens; TCP/IP illustrated, Vol 1, Ch15, s15.3, p212 for a more 
            // lucid/simpler explanation.
            //
            // Unfortunately - we cannot really pick a random port for each sequence; as
            // the TFTP clients are stateful - and expect us to honour the first port
            // we used with them. So we fake things - by using 'their' uniqueness.
            //
            WorkSock = new UDPSocket();
            WorkSock->bind(5000 + (remote_port & 0xFFF));

	    // As we switch - we're now deaf on the normal port - so essentially we can only
	    // deal with one connection at the time.  So set a time out timer.
	    timer.detach(); timer.attach(&timeout, 5); 
            to = 0;

            switch (buff[1]) {
                case 0x01: // RRQ
                    ConnectRead(buff);
                    break;
                case 0x02: // WRQ
                    ConnectWrite(buff);
                    break;
                case 0x03: // DATA before connection established
                    Err("No data expected");
                    break;
                case 0x04:  // ACK before connection established
                    Err("No ack expected");
                    break;
                case 0x05: // ERROR packet received
		    state = back_to_listen;
                    #ifdef TFTP_DEBUG
                        TFTP_DEBUG("TFTP Eror received\n\r");
                    #endif
                    break;
                default:    // unknown TFTP packet type
                    Err("Unknown TFTP packet type");
                    break;
            } // switch buff[1]
            break; // case listen
        }
        case reading: {
	    timer.detach(); timer.attach(&timeout, 5); 
            if (cmpHost())
	            switch (buff[1]) {
	                case 0x01:
	                    // if this is the receiving host, send first packet again
	                    if (blockcnt==1) {
                            Ack(0);
	                        dupcnt++;
	                    }
	                    if (dupcnt>10) { // too many dups, stop sending
	                        Err("Too many dups"); rst = true;
	                    }
	                    break;
	                case 0x02:
	                    // this should never happen, ignore
	                    Err("WRQ received on open read sochet");
 			    rst = true;
	                    break; // case 0x02
	                case 0x03:
	                    // we are the sending side, ignore
	                    Err("Received data package on sending socket");
			    rst = true;
	                    break;
	                case 0x04:
	                    // last packet received, send next if there is one
	                    dupcnt = 0;
	                    if (len == 516) {
	                        getBlock();
	                        sendBlock();
	                    } else { //EOF 
				rst = true;
	                    }
	                    break;
	                default:  // this includes 0x05 errors
	                    Err("Received 0x05 error message");
	                    rst = true;
	                    break;
	            } // switch (buff[1])
            else 
                printf("Ignoring package from other host during RRQ");
            break; // reading
        }
        case writing: {
	    timer.detach(); timer.attach(&timeout, 5); 
            if (cmpHost()) 
	            switch (buff[1]) {
	                case 0x02: {
	                    // if this is a returning host, send ack again
	                    Ack(0);
	                    #ifdef TFTP_DEBUG
	                        TFTP_DEBUG("Resending Ack on WRQ");
	                    #endif
	                    break; // case 0x02
                    }
	                case 0x03: {
	                    int block = (buff[2] << 8) + buff[3];
	                    if ((blockcnt+1) == block) {
	                        Ack(block);
	                        // new packet
	                        char *data = &buff[4];
	                        fwrite(data, 1,len-4, fp);
	                        blockcnt++;
	                        dupcnt = 0;
	                    } else { // mismatch in block nr
	                        if ((blockcnt+1) < block) { // too high
                                Err("Packet count mismatch");
	                            rst = true;
	                            remove(filename);
	                         } else { // duplicate packet, send ACK again
	                            if (dupcnt > 10) {
	                                Err("Too many dups"); rst = true;
	                                remove(filename);
	                            } else {
	                                Ack(blockcnt);
                                    dupcnt++;
	                            }
	                        }
	                    }
                        if (len<516) {
                            Ack(blockcnt); rst = true;
                            filecnt++;
                            printf("File receive finished\n");
                        }
	                    break; // case 0x03
                    }
	                default: {
	                     Err("No idea why you're sending me this!");
	                     break; // default
                    }
	            } // switch (buff[1])
            else {
                printf("Ignoring packege from other host during WRQ");
            }
            break; // writing
        }
        case tftperror: {
        }
        case suspended: {
        }
        case deleted: {
        }
        case back_to_listen: {
        }
    } // state
    if (!rst)
	return;

    fclose(fp);
    strcpy(remote_ip,""); 
    if (WorkSock) {
    WorkSock->close();
    delete(WorkSock);
    WorkSock = NULL;
    };
    state = back_to_listen;
}
