#include <stdio.h>
#include <stdlib.h>

#include <linux/ioctl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <time.h>
#include <string.h>
#include <fcntl.h>

#define DEVICE "/dev/mycdev"

#define CDEV_IOC_MAGIC 'k'
#define ASP_CLEAR_BUFF _IO(CDEV_IOC_MAGIC, 1)

int main(int argc, char *argv[]) 
{
	char devPath[20], ch, writeBuf[10000], readBuf[10000];
	int devNo,i,fd,offset, origin, dir;
	
	
	if (argc < 2) 
	{
		fprintf(stderr, "Please specify device number equlas to (N-1) device created.\n");
		return 1;
	}
	
	devNo = atoi(argv[1]);
	printf("device number is:%d\n",devNo);
	
	sprintf(devPath, "%s%d", DEVICE, devNo);	
	
	fd = open(devPath, O_RDWR);
	if(fd == -1) 
	{
		printf("%s either locked by another process or does not exist\n", DEVICE);
		exit(-1);
	}

	printf("r: Read from the device\nc: Clear the disk\nw: Write to device\n");
	printf("\n Enter command :");
	scanf("%c", &ch);
	
	switch(ch) 
	{
		case 'r':
			printf("start reading position \n 0 = for beginning \n 1 = for current \n\n");
			printf(" enter reading position :");
			scanf("%d", &origin);
			printf(" \n enter offset :");
			scanf("%d", &offset);
			lseek(fd, offset, origin);
			if (read(fd, readBuf, sizeof(readBuf)) > 0) 
				printf("\ndata: %s\n", readBuf);
			else 
				fprintf(stderr, "Reading failed\n");
			
			break;
	
		case 'w':
			printf("Enter Data to write: ");
			scanf(" %[^\n]", writeBuf);
			write(fd, writeBuf, sizeof(writeBuf)+2);
			printf("data written to buffer is : ");
			lseek(fd, 0, 0);
			if (read(fd, readBuf, sizeof(readBuf)) > 0) 
				printf("%s\n", readBuf);
			else 
				fprintf(stderr, "Reading failed\n");
			
			break;

		case 'c':
			printf("Content before clear : ");
			lseek(fd, 0, 0);
			if (read(fd, readBuf, sizeof(readBuf)) > 0) 
				printf("%s\n", readBuf);
			else 
				fprintf(stderr, "Reading failed\n");
			
			int rc = ioctl(fd, ASP_CLEAR_BUFF, 0);
			if (rc == -1)
			{ 
				perror("\n***error in ioctl***\n");
				return -1; 
			}
			printf("Data is cleared now\n");
			lseek(fd, 0, 0);
			if (read(fd, readBuf, sizeof(readBuf)) > 0) 
				printf("\ndevice: %s\nend", readBuf);
			else 
				fprintf(stderr, "Reading failed\n");
			
			break;
	
		default:
			printf("Command not recognized\n");
			break;

	}
	close(fd);

	return 0;
}
