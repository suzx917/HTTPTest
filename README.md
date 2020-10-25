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
We ran the test against follwing 3 URLs with 1000 repeats each:
1. www.google.com
2. my-worker.suzx917.worker.dev (deployed worker in General Assignment)
3. httpbin.org/anything (a test website)

The results (rounded to nearest integer) are following (also see `screenshots` folder):

| Test | Fastest(ms) | Slowest(ms) | Mean(ms) | Median(ms) | Largest | Smallest(KB) | Success(%) |
|------|-------------|-------------|----------|------------|---------|--------------|------------|
| 1    | 93          | 78201       | 210      | 127        | 48      | 47           | 100        |
| 2    | 82          | 536         | 121      | 99         | 1       | 1            | 100        |
| 3    | 87          | 22204       | 131      | 102        | 10      | 19           | 100        |

The packets from my-worker are very small because they are refresh instructions (http-equiv="refresh"). Presumably some caching technology comes into play.

Most popular websites (such as YouTube, Twitter, GitHub) only support an HTTPS visit and our program gets a 301 or 302 which redirects to thier https link.
Due to limited schedule, SSL connection is not implemented for this project.

Any feedback is appreciated. szx917@gmail.com
