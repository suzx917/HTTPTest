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
#define BUFFER_SIZE 4096
#define EXIT_ON_ERROR 1

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

// Print error message
void error(char* msg, int quit) {
    perror(msg);
    if (quit)
        exit(EXIT_FAILURE);
}

// Return b - a in milliseconds 
double timediff(const struct timeval *a, const struct timeval *b) {
    return (b->tv_sec - a->tv_sec) * 1e3 + (b->tv_usec - a->tv_usec) * 1e-3;
}

// Compare two double values (a > b)
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
        {"url",      required_argument, 0, 'u'},
        {"profile",  required_argument, 0, 'p'},
        {"help",     no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    int opt, optid;
    char *url = NULL;
    int repeat = 0;
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
            printf("url = %s", url);
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
    // Test server address
    struct hostent *host;
    if ((host=gethostbyname(url)) == 0) {
        error("Cannot get host address!", EXIT_ON_ERROR);
    }
    // Allocate a data array because we want median, unfortunately
    double *data = calloc(repeat, sizeof(double));
    if (!data) {
        error("Failed to allocate memory for data array!", EXIT_ON_ERROR);
    }
    // Read buffer
    char *buffer = calloc(BUFFER_SIZE, sizeof(char));
    if (!buffer) {
        error("Failed to allocate memory for buffer!", EXIT_ON_ERROR);
    }
    // Start timer
    struct timeval start, stop;
    if (gettimeofday(&start, NULL) == -1) {
        error("Cannot get system time.", EXIT_ON_ERROR);
    }
    // Test variables
    int success = 0; // how many times the fetch succeeds
    double fastest, slowest, smallest, largest;
    fastest = smallest = DBL_MAX;
    slowest = largest = DBL_MIN;
    double t_total = 0;
    size_t size;
    //
    // Step 3: Start testing loop
    //
    printf("Starting test... (repeat=%d)\n", repeat);
    for (int i = 0; i < repeat; i++) {
        // Open socket
        int sockfd = socket(PF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
            error("Failed to create socket!", 0);
        }
        // Fill host info
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(DEFAULT_SERVER_PORT);
        memcpy((char *)&server_addr.sin_addr.s_addr, 
                host->h_addr_list[0], host->h_length);

        /*** Start timer ***/
        gettimeofday(&start, NULL);
        int new = 1; // for catching response line
        int valid = 1; // marking OK status
        // Build connection
        int connfd;
        if ((connfd = connect(sockfd, (struct sockaddr *)&server_addr,
                sizeof(server_addr))) == -1) { // No connection
            continue;
        }

        // Send request
        int w = write(sockfd, request_header, strlen(request_header));
        if (w < 0)
            valid = 0;

        // Get response
        int r;
        size = 0;
        while (valid && (r = read(sockfd, buffer, BUFFER_SIZE)) > 0){
            if (new) {
                new = 0;
                if (!strncmp(buffer,"HTTP/1.1 200 OK", BUFFER_SIZE)
                    && !strncmp(buffer,"HTTP/2 200", BUFFER_SIZE)
                    && !strncmp(buffer,"HTTP/1 200 OK", BUFFER_SIZE) {
                    valid = 0;
                }
            }
            size += r;
            printf("%.*s", r, buffer);
        }
        if (valid) {
            ++success;
        }

        /*** Stop timer ***/
        gettimeofday(&stop, NULL);

        // Statistics
        data[i] = timediff(&start, &stop);
        t_total += data[i];
        if (size > largest)
            largest = size;
        if (size < smallest)
            smallest = size;
        if (data[i] < fastest) {
            fastest = data[i];
        }
        if (data[i] > slowest) {
            slowest = data[i];
        }

        // Close socket
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
    printf("\nFinished testing url: %s. Total time spent: %.4g s.\n\n",
            url, t_total / 1000);
    printf("-------------------------- Timer (ms) ---------------------------\n");
    printf(" Fastest | Slowest |  Mean   | Median\n");
    printf(" %7.3f | %7.3f | %7.3f | %7.3f\n", fastest, slowest, mean, median);
           
    printf("------------------------ Content Size (KB) ----------------------\n");
    printf(" Largest | Smallest | Success Rate\n");
    printf(" %7.3f | %8.3f | %6.2f%%\n",
           largest / 1024, smallest / 1024, (double)success*100 / repeat);

    // Finishing up
    free(buffer);
    free(data);
    return EXIT_SUCCESS;
}
