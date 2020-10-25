// Author : Zixiu Su
// Date: 10/22/2020
//
// CloudFlare assignment. Repeat testing HTTP response.

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <float.h>
#include <limits.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define MAX_HEADER_SIZE 512
#define MAX_BUFFER_SIZE 2048 // this is the buffer receiving response data
#define EXIT_ON_ERROR 1 // for error()

// set by `--verbose` option
static int verbose_flag;

// Default server port
char servport[8] = "80";

// we send exactly this HTTP request header 
const char *request_header_template = "GET / HTTP/1.1\r\n"
                                      "Host: %s\n"
				      "Connection: close\r\n\r\n'";

// A simple data struct to store url info
struct URL{
    char hostname[64];
    char pathname[256];
};    

// Print usage info
void usage(void) {
    fprintf(stderr, "Usage: test --url url_address "
                    "--profile number_of_request\n");
}

// Print help info
void help(void) {
    fprintf(stdout, "Test sending HTTP requests to a give address\n"
                    "  Required arguments:\n"
                    "    -u --url      specify web address\n"
                    "    -p --profile  specify number of tests\n"
                    "  Optional arguments:\n"
	            "    --verbose     (default)print debug info, complete response"
		                     " and test result\n"
		    "    --brief       only print error http code (NOT 200) and test result\n");
}

// Print debug message, if quit == 1 then exit failure after print
void error(const char* msg, int quit) {
    perror(msg);
    if (quit)
        exit(EXIT_FAILURE);
}

// Return time difference in milliseconds (b-a)
double timediff(const struct timeval *a, const struct timeval *b) {
    return (b->tv_sec - a->tv_sec) * 1e3 + (b->tv_usec - a->tv_usec) * 1e-3;
}

// Compare doubles for qsort()
int cmpdbl(const void *a, const void *b) {
    double *d1 = (double *)a;
    double *d2 = (double *)b;
    return (*d1 > *d2) ? 1 : (*d1 < *d2) ? -1 : 0;
}

// Test Driver
int main(int argc, char* argv[]) {
    verbose_flag = 1;
    int rc = 0; // temp return code
    // char* rs = NULL; // temp return string
    //
    // Step 1: Parse arguments and URL
    //
    static struct option longopts[] =
    {
        {"verbose",  no_argument,       &verbose_flag, 1},
        {"brief",    no_argument,       &verbose_flag, 0},
        {"url",      required_argument, 0, 'u'},
        {"profile",  required_argument, 0, 'p'},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    int opt, optid;
    char *urlarg = NULL;
    int repeat = 0; // specifies how many loops the test should run
    while ((opt = getopt_long(argc, argv, "u:p:h",
                              longopts, &optid)) != -1) {
        switch (opt)
        {
        case 0:
            if (longopts[optid].flag != 0)
                break;
            printf ("option %s", longopts[optid].name);
            if (optarg)
                printf (" with arg %s", optarg);
            printf ("\n");
            break;
        case 'u':
            urlarg = optarg;
            break;
        case 'p':
            repeat = atoi(optarg);
            break;
        case 'h':
            usage();
            help();            
            return EXIT_SUCCESS;
            break;
        case '?':
        default:
            error("Unknown parameter!", EXIT_ON_ERROR);
            break;
        }
    }
    // Check arguments validity
    if (!urlarg || repeat <= 0)
        //|| (!strncmp(url, "http://", strlen(url)) 
        //&& !strncmp(url, "https://", strlen(url))))
    {
        usage();
        error("Invalid arguments!", EXIT_ON_ERROR);
    }
    else if (strlen(urlarg) > 255)
	error("URL argument longer than 255!", EXIT_ON_ERROR);
    if (verbose_flag)
    	printf("Parsing URL argument = %s\n", urlarg);
    
    // Parse URL in a simple way
    // trim scheme (http or https)
    struct URL url;
    if (strstr(urlarg, "http://") == urlarg)
        urlarg += 7;
    else if (strstr(urlarg, "https://") == urlarg)
	urlarg += 8;
    // get path
    char *sep = strstr(urlarg, "/");
    if (sep == NULL) {
	strcpy(url.hostname, urlarg);
	strcpy(url.pathname, "/");
    }
    else {
        strcpy(url.pathname, sep);
        strncpy(url.hostname, urlarg, sep - urlarg);
    }
    if (verbose_flag)
	printf("hostname = %s, pathname = %s\n", url.hostname, url.pathname);
    
    //
    // Step 2: Initialize Test
    //
    // Allocate memory storing request header
    char *request_header = calloc(MAX_HEADER_SIZE, sizeof(char));
    // Allocate a data array because we want median
    double *data = calloc(repeat, sizeof(double));
    // Read buffer
    char *buffer = calloc(MAX_BUFFER_SIZE+1, sizeof(char));
    if (!data || !request_header || !buffer) {
        error("Failed to allocate memory!", EXIT_ON_ERROR);
    }
    // Build request header for sendeng later
    sprintf(request_header, request_header_template, url.hostname);
    // Test timer
    struct timeval start, stop;
    if (gettimeofday(&start, NULL) == -1) {
        error("Cannot get system time.", EXIT_ON_ERROR);
    }
    // Stats variables
    int success = 0; // how many times the fetch succeeds
    double fastest, slowest;
    fastest = DBL_MAX;
    slowest = DBL_MIN;
    int largest, smallest;
    largest = INT_MIN;
    smallest = INT_MAX;
    double t_total = 0;

    // Lookup and resolve url
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;
    hints.ai_protocol = 0;
    rc = getaddrinfo(url.hostname, servport, &hints, &result);
    if (rc != 0) {
        fprintf(stderr, "Cannot get address info for %s\n", url.hostname);
        error(gai_strerror(rc), EXIT_ON_ERROR);
    }
    //
    // Step 3: Start testing loop
    //
    if (verbose_flag)
        printf("Starting test... (repeat=%d)\n", repeat);
    for (int i = 0; i < repeat; i++) {
        int valid = 1; // marking OK status

	/*** Start timer ***/
        if (verbose_flag)
	    printf("Run #%d: timer started.\n", i+1);
	gettimeofday(&start, NULL);

        // Loop over address result and connect socket
        int sockfd = -1;
        for (rp = result; rp; rp = rp->ai_next) {
            // Close previous socket
	    if (sockfd != -1)
	        close(sockfd);
	    // Open new socket
	    sockfd = socket(rp->ai_family, rp->ai_socktype, 0);
            if (sockfd == -1)
                continue;
	    if (verbose_flag) { // Print IP addr
	        const char *rptr;
		char addrstr[64];
	        // Decode addr format
	        switch(rp->ai_family) {
                    case AF_INET:
			{
			    struct sockaddr_in *aptr = (struct sockaddr_in *)(rp->ai_addr); 	
			    rptr = inet_ntop(rp->ai_family, &aptr->sin_addr,
			                     addrstr, INET_ADDRSTRLEN);
                            break;
		        }
		    case AF_INET6:
			{
			    struct sockaddr_in6 *aptr = (struct sockaddr_in6 *)(rp->ai_addr); 	
			    rptr = inet_ntop(rp->ai_family, &aptr->sin6_addr,
 			                     addrstr, INET_ADDRSTRLEN);
                            break;
		        }
		    default:
		        rptr = NULL;
                }
	        if (rptr == NULL)
	            strcpy(addrstr, "Unknown");
                printf("Connecting IP address: %s\n", addrstr);
	    }
	    if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1) {
                break;
	    }
        }

        if (sockfd == -1) {
            if (verbose_flag)
                error("Failed to create socket!", 0);
            valid = 0;
            continue; /*** No connection, go next run ***/
        }
        
	// Send request
        rc = write(sockfd, request_header, strlen(request_header));
        if (rc < 0) {
            valid = 0;
            if (verbose_flag)
                error("Failed to send request!", 0);
            continue; /*** No connection, go next run ***/
        }
        
        // Get response
        int size = 0; // packet size
        while ((rc = read(sockfd, buffer, MAX_BUFFER_SIZE)) > 0){
            if (verbose_flag)
                printf("%.*s", rc, buffer);
            if (size == 0) { // first line
                // Extract and print code in brief mode
		// Code must be the second token of status line
                if (!verbose_flag) {
                    char *str = buffer;
		    char *token = strtok(str, " ");
                    if (token) {
                        token = strtok(NULL, " ");                           
                    }
		    int code = atoi(token);
                    if (code == 0) {
                        printf("Code: Unknown\n");
			valid = 0;
                    }
		    else if (code != 200) {
			printf("Code: %d\n", code);
                        valid = 0;
	            }
		    // Print nothing if 200 OK
                }
            }
            size += rc;
        }
        
	/*** Stop timer ***/
        gettimeofday(&stop, NULL);

        // Statistics
        if (valid) {
            ++success;
        }
        data[i] = timediff(&start, &stop);
        t_total += data[i];
        if (data[i] < fastest) {
            fastest = data[i];
        }
        if (data[i] > slowest) {
            slowest = data[i];
        }
        if (size > largest)
            largest = size;
        if (size < smallest)
            smallest = size;

        // Close socket
        if (verbose_flag)
            printf("Run #%d: %.3f ms (success=%d)\n", i+1, data[i], valid);
        close(sockfd);
        fflush(stdout);
    }

    //
    // Step 4: Median and Mean
    //
    qsort(data, repeat, sizeof(data[0]), cmpdbl);
    double median = (data[(repeat - 1) / 2] + data[repeat / 2]) / 2;
    double mean = t_total / repeat;
    //
    // Step 5: Output
    //
    printf("\nFinished testing url: %s.\n"
           "Total time spent: %.4g s.\n\n", urlarg, t_total / 1000);
    printf("-------------- Connection Stats (ms) ------------------\n");
    printf(" Fastest | Slowest |  Mean   | Median\n");
    printf(" %7.3f | %7.3f | %7.3f | %7.3f\n", fastest, slowest, mean, median);
            
    printf("----------------- Content Size (KB) -------------------\n");
    printf(" Largest | Smallest | Repeat | Success Rate\n");
    printf(" %7.3f | %8.3f | %6d | %8.2f%%\n",
           largest / 1024.0, smallest / 1024.0, repeat,(double)success*100 / repeat);
   
    // Step 6: Finishing up
    freeaddrinfo(result);
    free(request_header);
    free(buffer);
    free(data);
    return EXIT_SUCCESS;
}

