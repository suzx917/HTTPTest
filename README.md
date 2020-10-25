# HTTP Test

A simple C client program testing HTTP connection. 

### 1. Compile

Linux + make + gcc

`$ make`

### 2. Usage

`$ ./HTTPTest --url <example.com> --profile <number_of_repeats> [--help] [--brief | --verbose]`

### 3. Options
Required:
+ `-u` or `--url`

    Specify target domain name. Only support this style: `www.example.com/index` or `http://www.example.com/index`

+ `-p` or `--profile` 

    Specify how many runs the test should repeat

Optional:
+ `--help` Show help info
+ `--verbose` (default) Print debug info and full response
+ `--brief` Brief mode only prints error coce and result stats

### 4. Testing
After parsing command line arguments and initialization, we start the test loop. 

For each regular iteration, we start the timer, open a socket and connect to the server, write a hard-coded request header to socket, receive response, then stop timer and go next run. We also record the size of response. If anything bad happened or we did not get a 200 OK response, then this run is counted as failure, otherwise it is a success.

### 5. Some Results
