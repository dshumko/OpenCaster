/*
 * Copyright (C) 2010, Lorenzo Pallara l.pallara@avalpa.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>
#include <sys/io.h>

#define TS_PACKET_SIZE 188
#define BASEPORT 0x378 // lp1

unsigned char control_bit0 = 0;
unsigned char control_bit1 = 0;
unsigned char control_bit2 = 0;
unsigned char control_bit3 = 0;

int main (int argc, char *argv[]) {

	char* tsfile;
	unsigned char write_buf[TS_PACKET_SIZE];
	unsigned char control;
	
	int transport_fd;
	int len;
	int i;
	
	
	if(argc < 2 ) {
		fprintf(stderr, "Usage: %s file.ts, need to be executed root\n", argv[0]);
		fprintf(stderr, "WARNING: outputs a transport stream over a pc parallel port pins\n");
		fprintf(stderr, "WARNING: cpu goes 100 percentage usage, ts packet jitter is high so use only for data and on dual core\n");
		return 0;
	} else {
		tsfile = argv[1];
	}
	
	transport_fd = open(tsfile, O_RDONLY);
	if(transport_fd < 0) {
		fprintf(stderr, "can't open file %s\n", tsfile);
		return 0;
	}
	
	int completed = 0;
	
	if (ioperm(BASEPORT, 3, 1)) {
		fprintf(stderr, "ioperm error\n");
		close(transport_fd);
		return 0;
	}
	while (!completed) {
		len = read(transport_fd, write_buf, TS_PACKET_SIZE);
		if(len < 0) {
			fprintf(stderr, "ts file read error \n");
			completed = 1;
		} else if (len == 0) {
			fprintf(stderr, "ts sent done\n");
			completed = 1;
		} else {
			for (i = 0; i < TS_PACKET_SIZE; i++) {
				
				/*
				BASEPORT is where data bits are:
				*/
				outb(write_buf[i], BASEPORT); 
				
				/*
				BASEPORT + 1 is read only, we don't care:
				* Bit 0 is reserved
				* Bit 1 is reserved
				* Bit 2 IRQ status (not a pin, I don't know how this works)
				* Bit 3 ERROR (1=high) aka nFault > XRESET
				* Bit 4 SLCT (1=high) aka Select -> SDIN
				* Bit 5 PE (1=high) aka Perror -> SCLK
				* Bit 6 ACK (1=high) aka nAck -> SDOUT
				* Bit 7 -BUSY (0=high) aka Busy -> ASCLK
				*/
				
				/*
				BASEPORT + 2 is control and write-only (a read returns the data last written):
				* Bit 0 -STROBE (0=high) aka nStrobe -> TS-CLK
				* Bit 1 -AUTO_FD_XT (0=high) aka nAutoFd -> TS-SY
				* Bit 2 INIT (1=high) aka nInit -> TS-VL
				* Bit 3 -SLCT_IN (0=high) aka nSelectIn -> TS-EN
				* Bit 4 enables the parallel port IRQ (which occurs on the low-to-high transition of ACK) when set to 1.
				* Bit 5 controls the extended mode direction (0 = write, 1 = read), and is completely write-only (a read returns nothing useful for this bit).
				* Bit 6 reserved
				* Bit 7 reserved
				*/
				
				/* only on first byte: 0x47, the sync byte, it is necessary to set high TS-SY -> Bit 1 value is 0*/
				if (i == 0) {
					control_bit1 = 0;
				} else {
					control_bit1 = 1;
				}
				
				/* valid signal is always high because every bit is correct as no real demodulation happens -> Bit 2 value is 1*/
				control_bit2 = 1;
				
				/* enable signal is always high as every bit is data byte of the ts packet -> Bit 3 value is 0*/
				control_bit3 = 0;
				
				/* bit 0 is clock -> high at data start -> Bit 0 value is 0*/
				control_bit0 = 1;
				control = control_bit0 | (control_bit1 << 1) | (control_bit2 << 2) | (control_bit3 << 3);
				outb(control, BASEPORT + 2);
				
				/* bit 0 is clock -> low at half transmit time -> Bit 0 value is 1*/
				control_bit0 = 0;
				control = control_bit0 | (control_bit1 << 1) | (control_bit2 << 2) | (control_bit3 << 3);
				outb(control, BASEPORT + 2);
			}
		}
	}
	close(transport_fd);
	if (ioperm(BASEPORT, 3, 0)) {
		fprintf(stderr, "ioperm error on exit\n");
	}
	return 0;
}
