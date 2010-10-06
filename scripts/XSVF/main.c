#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef WIN32
#include <conio.h>
 #include <windef.h>

#else
//#include <curses.h>
#endif

#include "..\framework\serial.h"
#include "..\framework\buspirate.h"
char *dumpfile;
HANDLE dumphandle;
int modem =FALSE;   // use by command line switch -m to set to true
#define FREE(x) if(x) free(x);
#ifndef WIN32
#define usleep(x) Sleep(x);
#endif
#define MAX_BUFFER 16384   //16kbytes

int print_usage(char * appname)
{
		//print usage
		printf("\n");
		printf("\n");
	    printf(" Help Menu\n");
        printf(" Usage:              \n");
		printf("   %s  -p device -f filename.xsvf -s speed -x bytes-to-send \n ",appname);
		printf("\n");
		printf("   Example Usage:   %s -d COM1 -s 115200 -f example.xsvf -x 16 \n",appname);
		printf("\n");
		printf("           Where: -p device is port e.g.  COM1  \n");
		printf("                  -s Speed is port Speed  default is 115200 \n");
		printf("                  -f Filename of XSVF file \n");
		printf("                  -x bytes to send per chunk default is 16 bytes \n");
		printf("\n");

        printf("-----------------------------------------------------------------------------\n");


		return 0;
}

int main(int argc, char** argv)
{
	   int opt;
	   char buffer[MAX_BUFFER]={0};
	   struct stat stbuf;
	   int fd,timeout_counter;
	   int res,c, nparam_bytechunks;
	   FILE *XSVF;
	   int  xsvf;
	   char *param_port = NULL;
	   char *param_speed = NULL;
	   char *param_XSVF=NULL;
	   char *param_bytechunks=NULL;

		printf("-----------------------------------------------------------------------------\n");
		printf("\n");
		printf(" XSVF Player \n");
		printf(" http://www.dangerousprototypes.com\n");
		printf("\n");
		printf("-----------------------------------------------------------------------------\n");



		if (argc <= 1)  {
			print_usage(argv[0]);
			exit(-1);
		}


		while ((opt = getopt(argc, argv, "ms:p:f:x:")) != -1) {

			switch (opt) {
				case 'p':  // device   eg. com1 com12 etc
					if ( param_port != NULL){
						printf("Device/PORT error!\n");
						exit(-1);
					}
					param_port = strdup(optarg);
					break;

				case 'f':      // clock edge
					if (param_XSVF != NULL) {
						printf("XSVF file\n");
						exit(-1);
					}
					param_XSVF = strdup(optarg);

					break;
				case 'x':
					if (param_bytechunks!= NULL) {
						printf("Bytes to send\n");
						exit(-1);
					}
					param_bytechunks = strdup(optarg);
					break;
				case 's':
					if (param_speed != NULL) {
						printf("Speed should be set: eg  115200 \n");
						exit(-1);
					}
					param_speed = strdup(optarg);

					break;
				case 'm':    //modem debugging for testing
						modem =TRUE;   // enable modem mode
					break;

				default:
					printf(" Invalid argument %c", opt);
					print_usage(argv[0]);
					exit(-1);
					break;
			}
		}

		if (param_port==NULL){
        printf(" No serial port specified\n");
		print_usage(argv[0]);
 		exit(-1);
     }

		if (param_bytechunks==NULL)
			param_bytechunks=strdup("16");  // default is 16bytes
		nparam_bytechunks=atoi(param_bytechunks);


		if (param_speed==NULL)
           param_speed=strdup("115200");  //default is 115200kbps

		if (param_XSVF !=NULL) {
			//open the XSVF file
			if(( xsvf=open(param_XSVF,O_RDONLY))==-1){
				printf("Cannot open XSVF file.\n");
				exit(-1);
			}
			else {
				XSVF = fdopen(xsvf, "rb");
				// get filesize, chunk should be smaller than or equal to
				if (fstat(xsvf, &stbuf) == -1) {
					printf("Error getting filesize\n");
					exit(-1);
				}
				if (nparam_bytechunks > stbuf.st_size) {
					nparam_bytechunks=stbuf.st_size;
					printf("chunk size exceeds filesize, using %i bytes",nparam_bytechunks);

				}
			}

		} else {
              printf("No file specified. Need an input xsvf file \n");
              exit(-1);
		}
		printf(" Opening Bus Pirate on %s at %sbps, using XSVF file %s at %i bytes per chunk.\n", param_port, param_speed,param_XSVF,nparam_bytechunks);
		fd = serial_open(param_port);
		if (fd < 0) {
			fprintf(stderr, " Error opening serial port\n");
			return -1;
		}

		//setup port and speed
		serial_setup(fd,(speed_t) param_speed);

		// Enter binary mode
		fprintf(stderr, " Configuring Bus Pirate to use binary mode\n");
		if (modem==TRUE){
			printf(" Using modem..\n");
			//send AT commands
			serial_write( fd, "ATI7\x0D\0",5 );
			Sleep(1);
			res= serial_read(fd, buffer, sizeof(buffer));
			printf("\n %s\n",buffer);
		}
		else {
			if(BP_EnableBinary(fd)!=BBIO){
				 fprintf(stderr, " Buspirate cannot switch to binary mode :( \n");
				 fprintf(stderr, " Exiting...\n");
				exit(-1);
			}
		}
		printf(" Sending some bytes \n");

		while(!feof(XSVF)) {
			if(fgets(buffer, nparam_bytechunks, XSVF)) {
				//send to bp
				printf(" sending.....");
				for (c=0; c<nparam_bytechunks; c++)
				   printf("%02X ", buffer[c]);
				printf("\n");
				serial_write( fd, buffer,nparam_bytechunks );
				//wait for reply before sending the next chunks
				timeout_counter=0;
				while(1) {
					res= serial_read(fd, buffer, sizeof(buffer));
					if(res>0){
					   //wait for 0x01 ??? for now just get whatever buspirate returns
					   printf(" receiving...");
					   for(c=0; c<res; c++){
						  printf("%02X ", buffer[c]);
					   }
					   printf("\n\n");
					   break;
					}
					else {
						printf(" waiting...");
						Sleep(1);
						timeout_counter++;
						if(timeout_counter > 10){
							printf(" No reply.... sending next chunks of bytes.\n ");
							timeout_counter=0;
							break;
						}
					}
				}
			}
		}
    printf(" Thank you for Playing! :-)\n\n");

    close(xsvf);
    fclose(XSVF);
	FREE(param_port);
 	FREE(param_speed);
    FREE(param_bytechunks);
    FREE(param_XSVF);
    return 0;
 }  //end main()
