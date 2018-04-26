/*
 *  Author: Joey Jacobus
 *  Date 4/3/2018
 *  To cross compile for ARM:
 * $(buildroot_home)/output/host/usr/bin/arm-linux-gcc --sysroot=$(buildroot_home)/output/staging  -c main.c -o main.o
 * $(buildroot_home)/output/host/usr/bin/arm-linux-gcc --sysroot=$(buildroot_home)/output/staging  -o hw main.o  -lcurl -uClibc -lc
 */

#include <stdio.h>
#include <curl/curl.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <malloc.h>

#define OK	0
#define INIT_ERR 1
#define REQ_ERR 2
#define CLI_ERR 3
#define METHOD_ERR 3

#define POST 0
#define GET 1
#define PUT 2
#define DEL 3

#define TMPFILENAME "/tmp/temp"
#define STATUSFILENAME "/tmp/status"

/* Function prototypes */
void show_help(void);
void read_configs(const char *);
double readTmpFromFile(void);

int send_request(const char *URL, int8_t METHOD, const char *msg);


FILE *tmpFP;
FILE *statusFP;

int main(uint32_t argc, char **argv){
	uint32_t i;

	/* Finds the index where the final message argument starts */
	int32_t startidxmsg = -1;

	/* The HTTP method to do, -1 means invalid */
	int8_t METHOD = -1;
	char *URL = NULL;

	/* msg will be a malloced string combining all of the final arguments */
	char *msg = NULL;

	/* The configuration filename */
	char *configfilename = "thermd.conf";

	double read_temp;

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
			read_configs(argv[i]);
		}

		//else if (!strcmp(arg, "--url") || !strcmp(arg, "-u")){
		//	/* Make sure url isn't already set */
		//	if (URL != NULL){
		//		printf("URL set twice - aborting\n");
		//		return CLI_ERR;
		//	}
		//	
		//	/* skip to the next argument - it must be the url */
		//	i++;
		//	if (i >= argc){
		//		printf("No url argument specified.\n");
		//		return CLI_ERR;
		//	}
		//	URL = argv[i];
		//	if (validate_url(URL)){
		//		printf("invalid url - aborting\n");
		//		return CLI_ERR;
		//	}
		//}

		//else if (!strcmp(arg, "--post") || !strcmp(arg, "-o")){
		//	/* Ensure method hasn't already been specified */
		//	if (METHOD != -1){
		//		printf("More than one method specfied - aborting\n");
		//		return CLI_ERR;
		//	}	
		//	METHOD = POST;
		//}
		//else if (!strcmp(arg, "--get") || !strcmp(arg, "-g")){
		//	/* Ensure method hasn't already been specified */
		//	if (METHOD != -1){
		//		printf("More than one method specfied - aborting\n");
		//		return CLI_ERR;
		//	}	
		//	METHOD = GET;
		//}
		//else if (!strcmp(arg, "--put") || !strcmp(arg, "-p")){
		//	/* Ensure method hasn't already been specified */
		//	if (METHOD != -1){
		//		printf("More than one method specfied - aborting\n");
		//		return CLI_ERR;
		//	}	
		//	METHOD = PUT;
		//}
		//else if (!strcmp(arg, "--delete") || !strcmp(arg, "-d")){
		//	/* Ensure method hasn't already been specified */
		//	if (METHOD != -1){
		//		printf("More than one method specfied - aborting\n");
		//		return CLI_ERR;
		//	}	
		//	METHOD = DEL;
		//}
		/* Must be the start of the final string argument */
		//else{
		//	if (startidxmsg == -1){
		//		startidxmsg = i;
		//		break;
		//	}
		//}
	}
	printf("%s\n", configfilename);

	while(1){
		read_temp = readTmpFromFile();
		printf("temperature is %lf\n", read_temp);
		sleep(1);
	}
	statusFP = fopen(STATUSFILENAME, "w");
	fclose(statusFP);
	
//	if (METHOD == -1){
//		printf("No method specified - aborting\n");
//		return CLI_ERR;
//	}

	/* combine final strings into one string */
	//msg = combinestrings(argc, argv, startidxmsg);


	//uint8_t err = send_request(URL, METHOD, msg );
		
	//free (msg);
	return OK;
}

double readTmpFromFile(void){
	double temp;	
	tmpFP = fopen(TMPFILENAME, "r");
	fscanf(tmpFP, "%lf", &temp);
	fclose(tmpFP);
}
/**
 *  Sends the HTTP request 
 *  @params 
 *  URL: the url to send the request to
 *  METHOD: the HTTP method to send i.e. GET, POST, PUT, DELETE
 *  msg: the post parameters to send 
 */
int send_request(const char *URL, int8_t METHOD, const char *msg){
	CURL 	*curl;
	CURLcode res;

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
				curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
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
		if (res != CURLE_OK){
			printf("Could not connect - double check server and URL\n");
			return REQ_ERR;
		}
		curl_easy_cleanup(curl);
	}
	else{
		return INIT_ERR;
	}
	return OK;
}


void show_help(void){
	printf("Help menu\n"
		"---------\n"
		"Usage: hw [METHOD] --url [URL] [usage string]\n"
		"                                                \n"
		"-u, --url the url to send the http request to   \n"
		"-o, --post issue a post command                 \n"
		"-g, --get issue a get command                   \n"
		"-p, --put issue a put command                   \n"
		"-d, --delete issue a delete command             \n"
		"-h, --help show this help menu                   \n"
	);
}


/** 
 * read the config file 
 */
void read_configs(const char *configfile){

}