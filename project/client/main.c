/*
 *  Author: Joey Jacobus
 *  Date 4/29/2018
 *  To cross compile for ARM:
 *  make -f makefile-arm
 */

#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include "cJSON.h"

#define OK	0
#define INIT_ERR 1
#define REQ_ERR 2
#define CLI_ERR 3
#define METHOD_ERR 4
#define ERR_FORK 5
#define ERR_SETSID 6
#define ERR_CHDIR 7

#define ERROR_FORMAT "Error: %s"

#define DAEMON_NAME "thermd"

#define BUFFER_SIZE 100

#define POST 0
#define GET 1
#define PUT 2
#define DEL 3

#define NUM_SETPOINTS 3

#define TMPFILENAME "/tmp/temp"
#define STATUSFILENAME "/tmp/status"

#define DEFAULT_ENDPOINT "18.234.11.129:9000"
#define DEFAULT_LOGFILE "/var/log/thermd.log"

typedef struct {
	time_t times[NUM_SETPOINTS];
	time_t currenttime;
	double temps[NUM_SETPOINTS];
}config;


/* Function prototypes */
void show_help(void);
void read_configs(const char *);
double read_temp_from_file(void);
void parse_JSON(char *strJson, size_t nmemb);
void update_current_TOD(void);
double determine_set_point(void);
void update_server(double read_temp, const char *status);
void write_status_to_file(const char *status);
int send_request(const char *URL, int8_t METHOD, const char *msg);
bool string_starts_with(const char *string, const char *prefix);

static void _signal_handler(const int signal);
static void _loop(void);


char HTTP_ENDPOINT[BUFFER_SIZE];
char LOGFILE[BUFFER_SIZE];

FILE *logFP;

config configs;

int main(uint32_t argc, char **argv){
	uint32_t i;


	/* The configuration filename */
	char *configfilename = "thermd.conf";


	/* Parse command line arguments */
	for (i = 1; i < argc; i++){
		//printf("%d:  %s\n", i, argv[i]);
		char *arg = argv[i];
		
		/* Make sure the last character is a NULL before passing to strlen for security */
		arg[strlen(arg)] = '\0';
		
		/* Now compare against all known argument options */
		if (!strcmp(arg, "--help") || !strcmp(arg, "-h")){
			show_help();
			/* Always exit the program if they include the help prompt */
			return OK;
		}
		else if (!strcmp(arg, "--config") || !strcmp(arg, "-c")){
			i++;
			if (i >= argc){
				printf("no config file argument specified\n");
				return CLI_ERR;
			}	
			configfilename = argv[i];
		}
	}

	read_configs(configfilename);
	//printf("%s\n", HTTP_ENDPOINT);
	//printf("%s\n", LOGFILE);

	/* Open log files */
	openlog(DAEMON_NAME, LOG_PID | LOG_NDELAY | LOG_NOWAIT, LOG_DAEMON);
	logFP = fopen(LOGFILE, "w");

	/* Writes to /var/log/syslog on x86 */
	syslog(LOG_INFO, "started thermd!");
	
	/* We fork to prevent taking over init or syslog */
	pid_t pid = fork();
	
	/* Negative PID means an error */
	if (pid < 0){
		syslog(LOG_ERR, ERROR_FORMAT, strerror(errno));
		return ERR_FORK;
	}
	
	/* Parent */
	if (pid > 0){
		return OK;
	}

	/* check session, and be the leader */	
	if (setsid() < -1){
		syslog(LOG_ERR, ERROR_FORMAT, strerror(errno));
		return ERR_SETSID;
	}		
		

	/* Close file pointers */
	close (STDIN_FILENO);
	close (STDOUT_FILENO);
	close (STDERR_FILENO);


	/* Set the UMASK first, give us read/write and everyone else read permissions */
	umask(S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	/* Set working directory to root */
	if (chdir("/") < 0){
		syslog(LOG_ERR, ERROR_FORMAT, strerror(errno));
		return ERR_CHDIR;
	}

	/* Set up signal handler */
	signal(SIGTERM, _signal_handler);
	signal(SIGHUP, _signal_handler);


	/* main work loop */
	_loop();

	return OK;
}


/**
 *  Logs the current time to syslog every second 
 *  Time code adopted from https://stackoverflow.com/questions/7411301/how-to-introduce-date-and-time-in-log-file
 */
static void _loop(void){
	/* Either holds "on" or "off" */
	char heater_status[4];
	double read_temp;

	while(1){
		/* GET any new setpoints from the server */
		while (send_request(HTTP_ENDPOINT, GET, NULL) == REQ_ERR){
			syslog(LOG_INFO, "Server not available, re-trying\n");
			sleep(1);
		}
		
		/* read temperature and make adjustments */
		read_temp = read_temp_from_file();
		syslog(LOG_INFO, "temperature is %lf\n", read_temp);

		double set_temp = determine_set_point();
		syslog(LOG_INFO, "Set point is %lf\n", set_temp);

		if (set_temp < read_temp){
			strncpy(heater_status, "OFF", 3);
			heater_status[3] ='\0';
		}
		else if (set_temp > read_temp){
			strncpy(heater_status, "ON", 2);
			heater_status[2] ='\0';
		}

		write_status_to_file(heater_status);
		

		//printf("status is %s\n", heater_status);
		/* Post the updates to the server */
		update_server(read_temp, heater_status);


		sleep(1);
	}

}

/**
 *  Posts the last read temperature and status to the server
 */
void update_server(double read_temp, const char *status){
	char buffer[20];

	snprintf(buffer, sizeof(buffer),  "%0.2lf", read_temp);
	cJSON *root = cJSON_CreateObject();
	
	cJSON_AddItemToObject(root, "current_temp", cJSON_CreateString(buffer));
	cJSON_AddItemToObject(root, "status", cJSON_CreateString(status));

	

	send_request(HTTP_ENDPOINT, POST, cJSON_Print(root));
	cJSON_Delete(root);
}

/**
 * Writes the heater status to the file
 */
void write_status_to_file(const char *status){
	FILE *statusFP = fopen(STATUSFILENAME, "w");
	unsigned long timestamp = (unsigned long) time(NULL);	
	fprintf(statusFP, "%s : %lu", status, timestamp);
	fclose(statusFP);
}

/**
 *   Takes a time string of the format "HH:MM:SS" to a time_t struct 
 */
void stringToTimeT(char *timestr, time_t *t){
	int hh, mm, ss;
	sscanf(timestr, "%d:%d:%d", &hh, &mm, &ss);
	struct tm tm = {0};
	tm.tm_hour = hh;
	tm.tm_min = mm;
	tm.tm_sec = ss;

	*t = mktime(&tm);

}
double determine_set_point(void){
	/* first sort the set points based on time of day */
	int i, j;
	time_t tmptime;
	double tmptemp;
		
	/* bubble sort is ok since small number of setpoints */
	for (i = 0; i < NUM_SETPOINTS; i++){
		for (j = i; j < NUM_SETPOINTS; j++){
			if (difftime(configs.times[j], configs.times[i]) < 0){
				tmptime = configs.times[j];	
				configs.times[j] = configs.times[i];
				configs.times[i] = tmptime;

				tmptemp = configs.temps[j];
				configs.temps[j] = configs.temps[i];
				configs.temps[i] = tmptemp;
			}
		}
	}

	/** Debug sorting
	for (i = 0; i < NUM_SETPOINTS; i ++){
		printf("Time[%d] is %s and temp is %lf\n\n", i, ctime(&configs.times[i]), configs.temps[i]);
		
	}
	*/


	/* now find the latest time that the current time is greater than by searching backwards */
	for (i = NUM_SETPOINTS - 1; i >= 0; i--){
		/* if diff time is positive, currenttime is later */
		double diff = difftime( configs.currenttime, configs.times[i]);
		if (diff > 0){
			return configs.temps[i];
		}
	}

	/* else assume the first temp is the set point */
	return configs.temps[0];

}


/** 
 *  Parses the JSON from the server and updates the configs accordingly
 */
void parse_JSON(char *strJson, size_t nmemb){
	cJSON *root = cJSON_Parse(strJson);

	/* Update the time settings based on JSON received */
	char *time1str = cJSON_GetObjectItem(root, "time1")->valuestring;
	char *time2str = cJSON_GetObjectItem(root, "time2")->valuestring;
	char *time3str = cJSON_GetObjectItem(root, "time3")->valuestring;
	stringToTimeT(time1str, &configs.times[0]);
	stringToTimeT(time2str, &configs.times[1]);
	stringToTimeT(time3str, &configs.times[2]);
	//printf("Time1 is %s\n", ctime(&time1));
	//printf("Time1 is %s\n", ctime(&time2));
	//printf("Time1 is %s\n", ctime(&time3));


	/* Update the temp settings based on JSON */
	char *temp1str = cJSON_GetObjectItem(root, "temp1")->valuestring;
	char *temp2str = cJSON_GetObjectItem(root, "temp2")->valuestring;
	char *temp3str = cJSON_GetObjectItem(root, "temp3")->valuestring;
	configs.temps[0] = atoi(temp1str);
	configs.temps[1] = atoi(temp2str);
	configs.temps[2] = atoi(temp3str);
	//printf("temp1: %lf\n", configs.temp1);
	//printf("temp2: %lf\n", configs.temp2);
	//printf("temp3: %lf\n", configs.temp3);
	
	/* Get the current time */
	update_current_TOD();


	cJSON_Delete(root);
}


/**
 * Updates vairable currenttime with the current time of day
 */
void update_current_TOD(void){
	struct tm * mytime;

	time ( &configs.currenttime );
	mytime = localtime ( &configs.currenttime );

	/* Set to Dec 31 1899 to match our other times */
	mytime->tm_mday = 31;
	mytime->tm_mon = 11;
	mytime->tm_year = -1;
	mytime->tm_wday = 0;
	mytime->tm_yday = 365;
	mytime->tm_isdst = 0;

	configs.currenttime = mktime(mytime);
	//printf ( "Current local time and date: %s", asctime (timeinfo) );
	//printf ( "Current local time and date: %s", ctime (&currenttime) );
}

/**
 * curl GET callback
 */
size_t get_callback(void *ptr, size_t size, size_t nmemb, void *stream){
	//printf("%s\n", (char *)ptr);

	parse_JSON(ptr, nmemb); 
	/* Must return number of bytes actually taken care of, otherwise we get an error */
	return size * nmemb;
}

/**
 *  Sends an HTTP request 
 *  @params 
 *  URL: the url to send the request to
 *  METHOD: the HTTP method to send i.e. GET, POST, PUT, DELETE
 *  msg: the post parameters to send 
 */
int send_request(const char *URL, int8_t METHOD, const char *msg){
	CURL 	*curl;
	CURLcode res;
	//char *s = malloc(1);
	//*s = '\0';

	curl = curl_easy_init();
	if (curl){
		curl_easy_setopt(curl, CURLOPT_URL, URL);
		/* Setup based on the method */
		switch (METHOD){
			case DEL:
				curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
				// no break, we want delete to fall through and set post params too
			case POST:
    				curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
				    msg);	
				break;

			case GET:
				//curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, get_callback);
				//curl_easy_setopt(curl, CURLOPT_WRITEDATA, s);
				break;
			
			case PUT:
				curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    				curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
				    msg);	
				break;
			default:
				printf("Invalid Method\n");
				return METHOD_ERR;
		}

		res = curl_easy_perform(curl);
		
		//printf("res is %d\n", res);
		if (res != CURLE_OK){
			syslog(LOG_INFO, "Could not connect - double check server and URL\n");
			return REQ_ERR;
		}
		curl_easy_cleanup(curl);
	}
	else{
		return INIT_ERR;
	}
	return OK;
}




/**
 * Shows the help menu 
 */
void show_help(void){
	printf("Help menu\n"
		"---------\n"
		"Usage: thermd --config [configfile] 		 \n"
		"                                                \n"
		"-c, --config specify a config file              \n"
		"-h, --help show this help menu                   \n"
	);
}


/**
 * Adopted from https://stackoverflow.com/questions/4770985/how-to-check-if-a-string-starts-with-another-string-in-c 
 */
bool string_starts_with(const char *string, const char *prefix){
	while( *prefix){
		if ( *prefix++ != *string++){
			return 0;
		}
	}
	return 1;
}


/** 
 * read the config file 
 * populates HTTP_ENDPOINT and LOGFILE with contents of logfile
 */
void read_configs(const char *configfile){
	FILE *fp = fopen(configfile, "r");
	char *equalsIdx;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	if (fp == NULL){
		printf("Error opening config file \"%s\"\n", configfile);
		exit(1);
	}

	while((read = getline(&line, &len, fp)) != -1){
		equalsIdx = strchr(line, '=');
		equalsIdx++;
		
		if ((line + strlen(line)) - equalsIdx > BUFFER_SIZE){
			printf("Config file contents too large...\n");
			exit(1);		
		}
		
		/* Parse the configs */
		if(string_starts_with(line, "endpoint")){
			strncpy(HTTP_ENDPOINT, equalsIdx, BUFFER_SIZE);
			//printf("%s\n", HTTP_ENDPOINT);
			/* Change last character to null instead of new line */
			HTTP_ENDPOINT [ strlen(HTTP_ENDPOINT) - 1 ] = 0;
		}
		else if(string_starts_with(line, "logfile")){
			strncpy(LOGFILE, equalsIdx, BUFFER_SIZE);
			//printf("%s\n", LOGFILE);
			/* Change last character to null instead of new line */
			LOGFILE [ strlen(LOGFILE) - 1 ] = 0;
		}
	}

	/* If not set, use defaults */
	if (strlen(HTTP_ENDPOINT) == 0){
		strncpy(HTTP_ENDPOINT, DEFAULT_ENDPOINT, BUFFER_SIZE);
	}
	if (strlen(LOGFILE) == 0){
		strncpy(LOGFILE, DEFAULT_LOGFILE, BUFFER_SIZE);
	}

	free(line);
	fclose(fp);
}


/** 
 * read the latest temperature from the TMPFILENAME
 */
double read_temp_from_file(void){
	FILE *tmpFP;
	double temp;	
	tmpFP = fopen(TMPFILENAME, "r");
	if (tmpFP == NULL){
		//printf("File %s does not exist, start thermocouple service\n", TMPFILENAME);
		syslog(LOG_INFO, "File %s does not exist, start thermocouple service\n", TMPFILENAME);
		exit(1);
	}
	fscanf(tmpFP, "%lf", &temp);
	fclose(tmpFP);
	return temp;
	
}

static void _signal_handler(const int signal){
	switch (signal){
		case SIGHUP:
			break;

		case SIGTERM:
			syslog(LOG_INFO, "received SIGTERM, exiting.");
			closelog();
			fclose(logFP);
			exit(OK);
			break;
		default:
			syslog(LOG_INFO, "received unhandled signal");
	}
}
