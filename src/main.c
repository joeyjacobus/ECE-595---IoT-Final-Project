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

#define OK	0
#define INIT_ERR 1
#define REQ_ERR 2
#define CLI_ERR 3

#define POST 0
#define GET 1
#define PUT 2
#define DEL 3

#define URL "http://192.168.1.32:80"

void show_help(void);
uint8_t validate_url(const char *url);

int main(uint32_t argc, char **argv){
	CURL 	*curl;
	CURLcode res;
	bool startedfinalstring = false;

	/* The HTTP operation to do, -1 means invalid */
	int8_t OP = -1;
	char *url = NULL;

	/* Parse command line arguments */
	uint32_t i;

	for (i = 1; i < argc; i++){
		//printf("%d:  %s\n", i, argv[i]);
		char *arg = argv[i];
		
		/* Make sure the last character is a NULL before passing to strlen for security */
		arg[strlen(arg)] = '\0';
		
		/* Now compare against all known argument options */
		if (!strcmp(arg, "--help") || !strcmp(arg, "-h")){
			show_help();
			return OK;
		}

		else if (!strcmp(arg, "--url") || !strcmp(arg, "-u")){
			printf("URL!!");
			
			/* Make sure url isn't already set */
			if (url != NULL){
				printf("URL set twice - aborting\n");
				return CLI_ERR;
			}
			
			/* skip to the next argument - it must be the url */
			i++;
			url = argv[i];
			printf ("Url is %s\n", url);
			if (validate_url(url)){
				printf("invalid url - aborting\n");
				return CLI_ERR;
			}
		}

		else if (!strcmp(arg, "--post") || !strcmp(arg, "-o")){
			printf("post!!");

			/* Ensure operation hasn't already been specified */
			if (OP != -1){
				printf("More than one operation specfied - aborting\n");
				return CLI_ERR;
			}	
			OP = POST;
		}
		else if (!strcmp(arg, "--get") || !strcmp(arg, "-g")){
			printf("get!!");

			/* Ensure operation hasn't already been specified */
			if (OP != -1){
				printf("More than one operation specfied - aborting\n");
				return CLI_ERR;
			}	
			OP = GET;
		}
		else if (!strcmp(arg, "--put") || !strcmp(arg, "-p")){
			printf("put!!");

			/* Ensure operation hasn't already been specified */
			if (OP != -1){
				printf("More than one operation specfied - aborting\n");
				return CLI_ERR;
			}	
			OP = PUT;
		}
		else if (!strcmp(arg, "--delete") || !strcmp(arg, "-d")){
			printf("delete!!");

			/* Ensure operation hasn't already been specified */
			if (OP != -1){
				printf("More than one operation specfied - aborting\n");
				return CLI_ERR;
			}	
			OP = DEL;
		}
		/* Must be the start of the final string argument */
		else{
			startedfinalstring = true;
			printf("%s...", arg);
			
		}

	}
	
	return 0;

	curl = curl_easy_init();
	if (curl){
		curl_easy_setopt(curl, CURLOPT_URL, URL);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		res = curl_easy_perform(curl);
		if (res != CURLE_OK){
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
		"Usage: hw [OPERATION] --url [URL] [usage string]\n"
		"                                                \n"
		"-u, --url the url to send the http request to   \n"
		"-o, --post issue a post command                 \n"
		"-g, --get issue a get command                   \n"
		"-p, --put issue a put command                   \n"
		"-d, --delete issue a delete command             \n"
		"-h, --help show this help menu                   \n"
	);
}


uint8_t validate_url(const char *url){

	return OK;
}

