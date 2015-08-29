/*
 ============================================================================
 Name        : ddt.c
 Author      : Michael Kotson
 Version     : 0.1.5
 Copyright   : Copyright 2015
 Description : Disk Drive Test
 ============================================================================
 */


#ifdef HAVE_CONFIG_H
  #include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <ncurses.h>
#include <syslog.h>
#include <mntent.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <statgrab.h>
#include <atasmart.h>

#define copyright "Copyright 2015 Michael Kotson"
#define author "Michael Kotson"
#define email "mkotson@gmail.com"
#define version "0.1.5"
#define BUFF_SIZE (1024*1024)
#define MAX_THREADS 16

pthread_mutex_t update_scr = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t error_count_mutex = PTHREAD_MUTEX_INITIALIZER;

struct  t_type {
	int threadnum;
	int maxfilesize;
	int minfilesize;
	int filesize;
	char *filepath;
	unsigned long *rbuff;
	unsigned long *wbuff;
	time_t finish_time;
	} thread_data[MAX_THREADS];

struct stat_type {
	time_t finish_time;
	struct mntent *mount_entry;
	char *filepath;
	} system_stats;


FILE *errlogfile;
FILE *logfile;
int error_count=0;

void printhelp(void);
void update_screen(int y, int x, char *s);
void update_error_count();
extern struct mntent *find_mount_point(const char *name, const char *table);
void *stats (void *threadarg);
void *disk_thread (void *threadarg);


int main(int argc, char *argv[])
{
  int opt;
  int threads=4;
  int maxfilesize=100;
  int minfilesize=1;
  int duration=0;
  int i;
  char *filepath="";
  pthread_t thread[MAX_THREADS];
  int iret[MAX_THREADS];
  time_t start_time,end_time,current_time;
  pthread_t stat_thread;
  struct mntent *mount_entry;
  long size;
  char *buf;



  srand (time(0));
  while ((opt=getopt(argc,argv,"ht:p:m:M:d:")) !=-1) {
  	switch(opt) {
		case 't':
			threads=atoi(optarg);
			break;
		case 'p':
			filepath=optarg;
			break;
		case 'm':
			minfilesize=atoi(optarg);
			break;
		case 'M':
			maxfilesize=atoi(optarg);
			break;
		case 'd':
			duration=atof(optarg);
			break;
		case 'h':
			printhelp();
			exit (0);
			break;
		case ':':
			printf("option needs a value\n");
			break;
		case '?':
			printhelp();
			exit (0);					// Exit if missing argument
			break;

		}
	}

  if ((threads <1) || (threads>MAX_THREADS)){					// Make sure threads is MAX_THREADS or less
  	printf ("Error, maximum %d threads\n",MAX_THREADS);
	exit (1);
	}
  if (strlen(filepath)==0) {
	  size = pathconf(".", _PC_PATH_MAX);
	  if ((buf = (char *)malloc((size_t)size)) != NULL)
		  filepath = getcwd(buf, (size_t)size);
  }
  if (strcmp(&filepath[strlen(filepath)-1],"/")) strcat(&filepath[strlen(filepath)-1],"/");
  mount_entry=find_mount_point(filepath,"/etc/mtab");
  openlog ("ddt",LOG_ODELAY,LOG_USER);
  syslog (LOG_INFO,"----------------------");
  syslog (LOG_INFO,"ddt %s initiated",version);
  syslog (LOG_INFO,"threads: %d, max_file: %dM, min_file: %dM",threads,maxfilesize,minfilesize);
  syslog (LOG_INFO,"path: %s",filepath);
  syslog (LOG_INFO,"mount: %s",mount_entry->mnt_fsname);
  initscr();			/* Start curses mode 		*/
  cbreak();				/* Line buffering enabled	*/
  keypad(stdscr, TRUE);		/* We get F1, F2 etc..		*/
  noecho();			/* Don't echo() while we do getch */
  /*if (has_colors() == TRUE) {
	  start_color();
	  assume_default_colors(COLOR_WHITE,COLOR_BLUE);
  }*/
  start_time=time((time_t *)0);
  end_time=start_time+(60*duration);
  mvaddch(0,0,ACS_ULCORNER);
  mvaddch(0,COLS-1,ACS_URCORNER);
  mvaddch(6,0,ACS_LLCORNER);
  mvaddch(6,COLS-1,ACS_LRCORNER);
  curs_set(0);  //cursor invisible
  mvprintw(0,(COLS/2)-13,"Disk Drive Test (DDT) %s",version);

  syslog (LOG_INFO,"Start: %s",ctime(&start_time));

  //mvprintw(1,2,"Remaining:");
  //mvprintw(2,2,"CPU: ");
  //mvprintw(3,2,"Errors:");
  refresh();
  for (i=0;i<threads;i++) {						// Fill the thread array with needed information
  	thread_data[i].threadnum=i;
	thread_data[i].maxfilesize=maxfilesize;
	thread_data[i].minfilesize=minfilesize;
	thread_data[i].filepath=filepath;
	thread_data[i].finish_time=start_time+(60*duration);
	}
  system_stats.finish_time=start_time+(60*duration);  //Fill the stats array with needed information
  system_stats.mount_entry=mount_entry;
  system_stats.filepath=filepath;
  for (i=0;i<threads;i++) {
  		iret[i]=pthread_create(&thread[i],NULL,disk_thread,(void *) &thread_data[i]);
  }
  pthread_create(&stat_thread,NULL,stats,(void *) &system_stats);
  for (i=0;i<threads;i++) {
	  iret[i]=pthread_join(thread[i],NULL);
  }
  i=pthread_join(stat_thread,NULL);
  current_time=time((time_t *)0);
  syslog (LOG_INFO,"End Time: %s",ctime(&current_time));
  syslog (LOG_INFO,"ddt %s completed",version);
  syslog (LOG_INFO,"total error count: %d\n", error_count);
  syslog (LOG_INFO,"----------------------");
  endwin();
  closelog();
  printf ("Total error count: %d\n",error_count);
  printf ("DDT Finish Time: %s\n",ctime(&current_time));
  return EXIT_SUCCESS;
}

void printhelp(void)
{
	printf("Disk Drive Test v%s\n",version);
	printf("%s\n",copyright);
	printf("%s (%s)\n\n",author,email);
	printf("Usage:  ddt [OPTIONS]\n\n");
	printf("  -t,		Number of threads to spawn (max %d)\n",MAX_THREADS);
	printf("  -p,		Path to file system\n");
	printf("  -m,		Minimum file size in Megabytes\n");
	printf("  -M,		Maximum file size in Megabytes\n");
	printf("  -d,		Duration of test in minutes\n");
	printf("  -h,		Help\n\n");
	printf("Example: ddt -t 8 -p /tmp -m 5 -M 20 -d 10\n");
}

void update_screen(int y, int x, char *s)
{
	pthread_mutex_lock(&update_scr);
	mvprintw (y,x,s);
	refresh();
	pthread_mutex_unlock (&update_scr);
}

void update_error_count()
{
	pthread_mutex_lock (&error_count_mutex);
	error_count++;
	pthread_mutex_unlock (&error_count_mutex);
}

extern struct mntent *find_mount_point(const char *name, const char *table)
{
	struct stat s;
	dev_t mountDevice;
	FILE *mountTable;
	struct mntent *mountEntry;

	if (stat(name, &s) != 0)
		return 0;

	if ((s.st_mode & S_IFMT) == S_IFBLK)
		mountDevice = s.st_rdev;
	else
		mountDevice = s.st_dev;


	if ((mountTable = setmntent(table, "r")) == 0)
		return 0;

	while ((mountEntry = getmntent(mountTable)) != 0) {
		if (strcmp(name, mountEntry->mnt_dir) == 0
			|| strcmp(name, mountEntry->mnt_fsname) == 0)	/* String match. */
			break;
		if (stat(mountEntry->mnt_fsname, &s) == 0 && s.st_rdev == mountDevice)	/* Match the device. */
			break;
		if (stat(mountEntry->mnt_dir, &s) == 0 && s.st_dev == mountDevice)	/* Match the directory's mount point. */
			break;
	}
	endmntent(mountTable);
	return mountEntry;
}


void *stats (void *threadarg)
{
	time_t now;
	int remaining,hours,minutes,seconds,remainder;
	char str[50];
	struct stat_type *current;
	char *base_name;
	sg_disk_io_stats *diskio_stats;
	size_t num_diskio_stats;
	int x,ret,i;
	const char delimiters[]= "0123456789";
	char *disk_name;
	sg_cpu_percents *cpu_percent;
	SkDisk *d;
	uint64_t mkelvin;
	SkSmartOverall overall;
	float drive_temp;
	SkBool good;


	current = (struct stat_type *) threadarg;
	base_name = strrchr(current->mount_entry->mnt_fsname, '/') + 1;
	disk_name=strtok(base_name,delimiters);
	sg_init(1);
	ret=sk_disk_open(current->mount_entry->mnt_fsname,&d);
	do{
		now=time((time_t *)0);  //calculate time left
		remaining=(int)difftime(current->finish_time,now);
		hours=remaining/3600;
		remainder=remaining % 3600;
		minutes= remainder/60;
		seconds= remainder % 60;
		sprintf(str,"Remaining:");
		update_screen(1,2,str);
		sprintf(str,"%.2d:%.2d:%.2d",hours,minutes,seconds);
		update_screen(1,13,str);
		sprintf(str,"Errors:");
		update_screen(3,2,str);
		sprintf(str,"%d",error_count);
		update_screen(3,10,str);
		sg_snapshot();
		cpu_percent=sg_get_cpu_percents(NULL);
		sprintf(str,"CPU:");
		update_screen(2,2,str);
		sprintf (str,"%6.2f%%\n",cpu_percent->user+cpu_percent->kernel);
		update_screen(2,7,str);
		sprintf(str,"Mount: %s",current->filepath);
		update_screen(4,2,str);
		diskio_stats=sg_get_disk_io_stats_diff(&num_diskio_stats);
		sprintf(str,"[%s]",disk_name);
		update_screen (1,COLS-27,str);
		for (x=0;x<num_diskio_stats;x++) {
			if (strcmp(diskio_stats->disk_name, disk_name) ==0){
				if (diskio_stats->systime <1) diskio_stats->systime=1;
				sprintf (str,"Read: %6.2f MB/s\n",(float)(diskio_stats->read_bytes/diskio_stats->systime)/1048576);
				update_screen(1,COLS-20,str);
				sprintf (str,"Write: %6.2f MB/s\n",(float)(diskio_stats->write_bytes/diskio_stats->systime)/1048576);
				update_screen(2,COLS-20,str);

			}

			diskio_stats++;
		}
		sprintf(str,"Temp:");
		update_screen(3,COLS-20,str);
		sprintf(str,"Status:");
		update_screen(4,COLS-20,str);
		sprintf(str,"Health:");
		update_screen(5,COLS-20,str);
		if (ret>=0) {
			if ((i=sk_disk_smart_read_data(d))>=0){
				if ((i=sk_disk_smart_get_temperature(d,&mkelvin)) >=0){
					drive_temp= (((unsigned long long)mkelvin)/1000)-273.149;  //Convert to Celsius
					sprintf(str,"Temp: %4.0fC\n",drive_temp);
					update_screen(3,COLS-20,str);
				}
				if ((i=sk_disk_smart_get_overall(d,&overall)) >=0) {
					sprintf (str, "Status: %s\n",sk_smart_overall_to_string(overall));
					update_screen(4,COLS-20,str);
				}
				if ((i=sk_disk_smart_status(d,&good))>=0) {
					sprintf(str,"Health: %s\n",good ? "GOOD" : "BAD");
					update_screen(5,COLS-20,str);
				}
			}
		}
		usleep (1000000);  // Sleep for 1 second

	} while ((difftime(current->finish_time,now)>0));
	if (ret>=0) {
				if ((i=sk_disk_smart_read_data(d))>=0){
					if ((i=sk_disk_smart_get_temperature(d,&mkelvin)) >=0){
						drive_temp= (((unsigned long long)mkelvin)/1000)-273.149;  //Convert to Celsius
						syslog(LOG_INFO,"Drive Temperature: %4.0fC",drive_temp);
					}
					if ((i=sk_disk_smart_get_overall(d,&overall)) >=0) {
						syslog(LOG_INFO,"Drive Status: %s",sk_smart_overall_to_string(overall));
					}
					if ((i=sk_disk_smart_status(d,&good))>=0) {
						syslog(LOG_INFO,"Drive Health: %s",good ? "GOOD" : "BAD");
					}
				}
			}
	return (0);
}

void *disk_thread (void *threadarg)
{
  int j;
  char filename[80];
  FILE *fp;
  int sizeread=0;
  struct t_type *current;
  time_t now;
  char str[50];

  current = (struct t_type *) threadarg;
  do {
  sizeread=0;
  now=time((time_t *)0);
  current->rbuff=malloc(BUFF_SIZE);					// Allocate read and write buffer
  if (!current->rbuff) {
  	sprintf(str,"[Thread %d] Error allocating memory\n",current->threadnum);
  	update_screen(7+current->threadnum,2,str);
  	syslog(LOG_ERR,"[Thread %d] Error allocating memory for read buffer",current->threadnum);
  	update_error_count();
  	//exit (1);
	}
  current->wbuff=malloc(BUFF_SIZE);
  if (!current->wbuff) {
   	sprintf(str,"[Thread %d] Error allocating memory\n",current->threadnum);
  	update_screen(7+current->threadnum,2,str);
  	syslog(LOG_ERR,"[Thread %d] Error allocating memory for write buffer",current->threadnum);
  	update_error_count();
  	//exit(1);
	}
    for (j=0;j<(BUFF_SIZE/(sizeof(void *)));j++)
  {
  	if (sizeof(void *) == 8)
    	*(current->wbuff+j)=(unsigned long)(((double)rand()/(double)RAND_MAX)*0xffffffffffffffff);  //Bug here!!!
  	else
  		*(current->wbuff+j)=(unsigned long)(((double)rand()/(double)RAND_MAX)*0xffffffff);  //Bug here!!!
  }
  current->filesize=rand() % (current->maxfilesize-current->minfilesize+1) + current->minfilesize;
  sprintf(filename,"%s%s%d",current->filepath,"thread.",current->threadnum);
  sprintf(str,"[Thread %d] Creating file size of %dMB\n",current->threadnum,current->filesize);
  update_screen(7+current->threadnum,2,str);
  syslog(LOG_INFO,"[Thread %d] Creating file size of %dMB",current->threadnum,current->filesize);
  if ((fp=fopen(filename,"wb")) == NULL) {
	sprintf(str,"[Thread %d] Error creating file %s\n",current->threadnum,filename);
	update_screen(7+current->threadnum,2,str);
	syslog(LOG_ERR,"[Thread %d] Error creating file %s",current->threadnum,filename);
	update_error_count();
   	//exit (1);
  }
  fclose(fp);
  if ((fp=fopen(filename,"ab")) == NULL) {
  	sprintf(str,"[Thread %d] Error writing file %s\n",current->threadnum,filename);
  	update_screen(7+current->threadnum,2,str);
  	syslog(LOG_ERR,"[Thread %d] Error writing file %s",current->threadnum,filename);
  	update_error_count();
  	//exit (1);
  }
  for (j=1;j<=current->filesize;j++) {
  	if (fwrite((current->wbuff),BUFF_SIZE,1,fp) !=1) {
  	sprintf(str,"[Thread %d] Write error occurred - %s\n",current->threadnum,filename);
  	update_screen(7+current->threadnum,2,str);
  	syslog(LOG_ERR,"[Thread %d] Write error occurred - %s",current->threadnum,filename);
  	update_error_count();
  	//exit (1);
 	}
  }
  fclose (fp);
  sync();
  sprintf(str,"[Thread %d] Verifying file size of %dMB\n",current->threadnum,current->filesize);
  update_screen(7+current->threadnum,2,str);
  syslog(LOG_INFO,"[Thread %d] Verifying file size of %dMB",current->threadnum,current->filesize);
  if ((fp=fopen(filename,"rb")) == NULL) {
  	sprintf(str,"[Thread %d] Error opening file %s\n",current->threadnum,filename);
  	update_screen(7+current->threadnum,2,str);
  	syslog(LOG_ERR,"[Thread %d] Error opening file %s",current->threadnum,filename);
  	update_error_count();
   	//exit (1);
  }
  while (!feof(fp) & !ferror(fp)) {
  	if (fread((current->rbuff),BUFF_SIZE,1,fp) !=1) {
  		//sprintf(str,"Read error occurred - %s\n",filename);
  		//update_screen(7+current->threadnum,2,str);
  		//syslog(LOG_ERR,"Thread %d: Read error occurred",current->threadnum);
  		//exit (1);
  		//printf ("Read error occurred - %s\n",filename);
  		//exit (1);
  	}
  	sizeread++;
  	if (memcmp(current->wbuff,current->rbuff,BUFF_SIZE) !=0) {
  		sprintf(str,"[Thread %d] File miscompare\n",current->threadnum);
  		update_screen(7+current->threadnum,2,str);
  		syslog(LOG_ERR,"[Thread %d] File miscompare",current->threadnum);
  		update_error_count();
  	}
  }
  if (ferror(fp)) {
  	sprintf(str,"[Thread %d] File miscompare\n",current->threadnum);
  	update_screen(7+current->threadnum,2,str);
  	syslog(LOG_ERR,"[Thread %d] File miscompare",current->threadnum);
  	update_error_count();
  }
  if (current->filesize != (sizeread-1)) {
  	sprintf(str,"[Thread %d] File miscompare\n",current->threadnum);
  	update_screen(7+current->threadnum,2,str);
  	syslog(LOG_ERR,"[Thread %d] File miscompare",current->threadnum);
  	update_error_count();
   }
  fclose(fp);
  free (current->wbuff);						// Free read and write buffer
  free (current->rbuff);
 } while ((difftime(current->finish_time,now)>0));
  sprintf(str,"[Thread %d] Complete\n",current->threadnum);
  update_screen(7+current->threadnum,2,str);
  syslog(LOG_INFO,"[Thread %d] Complete",current->threadnum);
  unlink(filename);
  return(0);
}

