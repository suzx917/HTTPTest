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
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>


#define DEFAULT_SERVER_PORT 80
#define BUFFER_SIZE 2048
#define EXIT_ON_ERROR 1

// set by `--verbose` option
static int verbose_flag

// we send exactly this HTTP request header 
const char *request_header = "GET / HTTP/1.1\r\n"
                             "Connection: close\r\n\r\n'";

// Print usage info
void usage(void) {
    fprintf(stderr, "Usage: test --url url_address "
                    "--profile number_of_request\n");
}

// Print help info
void help(void) {
    fprintf(stdout, "Test sending HTTP requests to a give address\n"
                    "Options:"
                    "  -u --url        specify web address\n"
                    "  -p --profile    specify number of tests\n");
}

// Print debug message
void error(char* msg, int quit) {
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
    //
    // Step 1: Parse arguments
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
    char *url = NULL;
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
            url = optarg;
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
    if (!url || repeat <= 0
        || (!strncmp(url, "http://", strlen(url)) 
        && !strncmp(url, "https://", strlen(url))))
    {
        usage();
        error("Invalid arguments!", EXIT_ON_ERROR);
    }

    //
    // Step 2: Initialize Test
    //

    // Allocate a data array because we want median, unfortunately
    double *data = calloc(repeat, sizeof(double));
    if (!data) {
        error("Failed to allocate memory for data array!", EXIT_ON_ERROR);
    }
    // Read buffer
    char *buffer = calloc(BUFFER_SIZE+1, sizeof(char));
    if (!buffer) {
        error("Failed to allocate memory for buffer!", EXIT_ON_ERROR);
    }
    // Test timer
    struct timeval start, stop;
    if (gettimeofday(&start, NULL) == -1) {
        error("Cannot get system time.", EXIT_ON_ERROR);
    }
    // Stats variables
    int success = 0; // how many times the fetch succeeds
    double fastest, slowest, smallest, largest;
    fastest = smallest = DBL_MAX;
    slowest = largest = DBL_MIN;
    double t_total = 0;

    // Lookup and resolve url
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    int gai = getaddrinfo(url, NULL, &hints, &result);
    if (gai != 0) {
        fprintf(stderr, "Cannot get address info for url = %s\n", url);
        error(gai_strerror(gai), EXIT_ON_ERROR);
    }
    //
    // Step 3: Start testing loop
    //
    if (verbose_flag)
        printf("Starting test... (repeat=%d)\n", repeat);
    for (int i = 0; i < repeat; i++) {
        int valid = 1; // marking OK status

        // Loop over address result and tries to open socket
        int sockfd = -1;
        for (rp = result; rp; rp = rp->ai_next) {
            sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (sockfd == -1)
                continue;
            if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) != -1)
                break;
        }

        if (sockfd == -1) {
            if (verbose_flag)
                error("Failed to create socket!", 0);
            valid = 0;
            continue; /*** No connection, go next run ***/
        }

        // Send request
        int w = write(sockfd, request_header, strlen(request_header));
        if (w < 0) {
            valid = 0;
            if (verbose_flag)
                error("Failed to send request!", 0);
            continue; /*** No connection, go next run ***/
        }
        /*** Start timer ***/
        gettimeofday(&start, NULL);

        // Get response
        size_t size; // packet size
        int new = 1; // for catching response line
        int r;
        while ((r = read(sockfd, buffer, BUFFER_SIZE)) > 0){
            size += r;
            if (verbose_flag)
                printf("%.*s", r, buffer);
            if (new) {
                new = 0;
                if (!strncmp(buffer,"HTTP/1.1 200 OK", BUFFER_SIZE)
                    && !strncmp(buffer,"HTTP/2 200", BUFFER_SIZE)
                    && !strncmp(buffer,"HTTP/1 200 OK", BUFFER_SIZE)) {
                    // Extract and print the error code in brief mode
                    // In a rough manner
                    if (!verbose_flag) {
                        char *token;
                        token = strtok_r(buffer, " ");
                        if (token) {
                            token = strtok_r(NULL, " ");                           
                        }
                        if (!token)
                            token = "?";
                        printf("Code: %s\n", token); 
                    }
                    valid = 0;
                }
            }
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
    if (success > 0) {
        qsort(data, success, sizeof(data[0]), cmpdbl);
        double median = (data[(repeat - 1) / 2] + data[repeat / 2]) / 2;
        double mean = t_total / repeat;
    //
    // Step 5: Output
    //
        printf("\nFinished testing url: %s. Total time spent: %.4g s.\n\n",
                url, t_total / 1000);
        printf("-------------- Connection Stats (ms) ------------------\n");
        printf(" Fastest | Slowest |  Mean   | Median\n");
        printf(" %7.3f | %7.3f | %7.3f | %7.3f\n", fastest, slowest, mean, median);
            
        printf("----------------- Content Size (KB) -------------------\n");
        printf(" Largest | Smallest | Success Rate\n");
        printf(" %7.3f | %8.3f | %6.2f%%\n",
            largest / 1024, smallest / 1024, (double)success*100 / repeat);
    }
    // Step 6: Finishing up
    free(buffer);
    free(data);
    return EXIT_SUCCESS;
}
