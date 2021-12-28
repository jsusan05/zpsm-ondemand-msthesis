/**
 * Name: sender_udp.c
 * Description: Test app that sends udp data, by default uniform distribution is supported
 *              The code stubs for Poisson and normal distribution is also part of it but needs
 *              to be integrated to command line friendly use
 * Usage: udp_sender <ip address> <port> <size of packet in KB-1KB by default> <no. of packets unlimited by default>
 * Author: 
 * Copyright:
 */

//SYSTEM INCLUDES
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <time.h>
#include <math.h>
#include <limits.h>

//INTERNAL INCLUDES

//CONSTANTS
#define AP_WI				999         //This is the packet arrival rate, lambda
#define GOODRAND            random()
#define GOODRANDMAX         INT_MAX
#define RANDTYPE            long

//GLOBALS
static int dummy_socket_fd = -1;
static struct sockaddr_in address;
static timer_t wfTimerID;
static char ip_address[256] = {0};
static int port = 0;
static int count_tx = 0;
static int packet_size = 1024; //order of 1K
static char* packet = NULL;
static int tot_pkt_to_send = -1;

//----- Function prototypes -------------------------------------------------
int* normal_distribution(const double dAverage, const double dStddev, const long int liSampleSize);
double   poisson(double x);       // Returns a Poisson random variable
double expon(double x);         // Returns an exponential random variable
double rand_val(int seed);      // Jain's RNG

int* normal_distribution(const double dAverage, const double dStddev, const long int liSampleSize)
{
    struct timeval time;
    int iRandom1, iRandom2;
    double dRandom1, dRandom2;
    long int i;
    int *distribution = (int*) malloc(liSampleSize*sizeof(int));
    gettimeofday(&time, NULL);
       
    //Convert seconds to a unsigned integer to init the seed
    srandom((unsigned int) time.tv_usec);

    for (i=0; i<liSampleSize;i++)
    {
        //generates a number between 0 and 4G
        iRandom1=(random());
        //offset to have unifor ditribution between -2G and +2G
        dRandom1=(double)iRandom1 /2147483647;
       
        //generates a number between 0 and 4G
        iRandom2=(random());
        //offset to have unifor ditribution between -2G and +2G
        dRandom2=(double)iRandom2 /2147483647;
       
        //Output random values. The formula is based on Box-Muller method
        distribution[i] = ceil(dAverage + dStddev * sqrt(-2.0 * log(dRandom1))*cos(6.28318531 * dRandom2));
    }
    return distribution;
}

//===== Main program ========================================================
int* poisson_distribution(const int pois_rv, const int num_values, const double lambda)
{
    int      i;                   // Loop counter
    int *distribution = (int*) malloc(num_values*sizeof(int));
    // Generate and output exponential random variables
    for (i=0; i<num_values; i++)
    {
        distribution[i] = poisson(1.0 / lambda);
    }
    return distribution;
}

//===========================================================================
//=  Function to generate Poisson distributed random variables              =
//=    - Input:  Mean value of distribution                                 =
//=    - Output: Returns with Poisson distributed random variable           =
//===========================================================================
double poisson(double x)
{
  double    poi_value;             // Computed Poisson value to be returned
  double t_sum;                 // Time sum value

  // Loop to generate Poisson values using exponential distribution
  poi_value = 0.0;
  t_sum = 0.0;
  while(1)
  {
    t_sum = t_sum + expon(x);
// printf("time: %lf \n",t_sum);
    if (t_sum >= 1.0) break;
    poi_value++;
  }

  return(poi_value);
}

//===========================================================================
//=  Function to generate exponentially distributed random variables        =
//=    - Input:  Mean value of distribution                                 =
//=    - Output: Returns with exponentially distributed random variable     =
//===========================================================================
double expon(double x)
{
  double z;                     // Uniform random number (0 < z < 1)
  double exp_value;             // Computed exponential value to be returned

  // Pull a uniform random number (0 < z < 1)
  do
  {
    z = rand_val(0);
  }
  while ((z == 0) || (z == 1));

  // Compute exponential random variable using inversion method
  exp_value = -x * log(z);

  return(exp_value);
}

//=========================================================================
//= Multiplicative LCG for generating uniform(0.0, 1.0) random numbers    =
//=   - x_n = 7^5*x_(n-1)mod(2^31 - 1)                                    =
//=   - With x seeded to 1 the 10000th x value should be 1043618065       =
//=   - From R. Jain, "The Art of Computer Systems Performance Analysis," =
//=     John Wiley & Sons, 1991. (Page 443, Figure 26.2)                  =
//=========================================================================
double rand_val(int seed)
{
  const long  a =      16807;  // Multiplier
  const long  m = 2147483647;  // Modulus
  const long  q =     127773;  // m div a
  const long  r =       2836;  // m mod a
  static long x;               // Random int value
  long        x_div_q;         // x divided by q
  long        x_mod_q;         // x modulo q
  long        x_new;           // New x value

  // Set the seed if argument is non-zero and then return zero
  if (seed > 0)
  {
    x = seed;
    return(0.0);
  }

  // RNG using integer arithmetic
  x_div_q = x / q;
  x_mod_q = x % q;
  x_new = (a * x_mod_q) - (r * x_div_q);
  if (x_new > 0)
    x = x_new;
  else
    x = x_new + m;

  // Return a random value between 0.0 and 1.0
  return((double) x / m);
}


/**
* Sends a dummy packet to a dummy port of the AP that would change radio from PS to awake mode
*/
int send_to_ap(char buf[])
{
	int err = 0;
	if (dummy_socket_fd==-1) //create socket to sent dummy pkt
	{					
		bzero(&address,sizeof(address));
		address.sin_family=AF_INET;
		address.sin_addr.s_addr=inet_addr(ip_address);
		address.sin_port=htons(port);
		dummy_socket_fd=socket(AF_INET, SOCK_DGRAM, 0);		
	}
	if(!buf)
		err = -1;
	else
	{
		if(sendto(dummy_socket_fd,buf,packet_size,0,(struct sockaddr *)&address,sizeof(address)))
			err = 0;
		else
			err = -1;
	}
	return err;
}

/**
* Cleans up the module
*/
static int cleanup()
{
	close(dummy_socket_fd);
}

/**
* Send dummy packets
*/
static void wfTimerCB( int sig, siginfo_t *si, void *uc )
{    
    struct timeval  tv;
    gettimeofday(&tv, NULL);

	sprintf(packet, "%ld,%ld,%d",tv.tv_sec,tv.tv_usec ,count_tx);		
	if(0==send_to_ap(packet))
		printf("ID:%d, packet msg :%s\n",count_tx,packet);
    count_tx++;
	if(tot_pkt_to_send+1==count_tx)
	{
		exit(0);
	}    
}


/**
* Handler for all timers
*/
static void timerHandler( int sig, siginfo_t *si, void *uc )
{
    timer_t *tidp;
    tidp = si->si_value.sival_ptr;

    if ( *tidp == wfTimerID )
        wfTimerCB(sig, si, uc);   
}

/**
* Starts a timer
*/
static int makeTimer( char *name, timer_t *timerID, int expireMS, int intervalMS )
{
    struct sigevent         te;
    struct itimerspec       its;
    struct sigaction        sa;
    int                     sigNo = SIGRTMIN;

    /* Set up signal handler. */
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timerHandler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(sigNo, &sa, NULL) == -1)
    {
        fprintf(stderr, "Failed to setup signal handling for %s.\n", name);
        return(-1);
    }

    /* Set and enable alarm */
    te.sigev_notify = SIGEV_SIGNAL;
    te.sigev_signo = sigNo;
    te.sigev_value.sival_ptr = timerID;
    timer_create(CLOCK_REALTIME, &te, timerID);

    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = intervalMS * 1000000;
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = expireMS * 1000000;
    timer_settime(*timerID, 0, &its, NULL);

    return(0);
}

/**
* Entry point
*/
int main(int argc, char **argv)
{ 
    int* dist = 0; int i=0;
	packet = (char*) malloc(packet_size*sizeof(char));		
	
	if(argc<3)
    {
    	printf("Usage: udp_sender <ip address> <port> <size of packet in KB-1KB by default> <no. of packets unlimited by default>\n");
    	exit(0);
    }
    
    strcpy(ip_address, argv[1]);
	port = atoi(argv[2]);
	
	if(argc>=4)
	{
		packet_size = atoi(argv[3])*1024;
	}
	
	if(argc==5)
	{
		tot_pkt_to_send = atoi(argv[4]);
	}
    struct timeval  tv;
    gettimeofday(&tv, NULL);

	sprintf(packet, "%ld,%ld,%d",tv.tv_sec,tv.tv_usec ,count_tx);
    //The first packet is send with sequence id 0, this is used to synchronize the sender and
    //receiver system clock		
	if(0==send_to_ap(packet))
		printf("ID:%d, packet msg :%s\n",count_tx,packet);
    count_tx++;
    sleep(10); //to synchronize the first packet
    //After synchronizing the actual packets are send every AP_WI period
	makeTimer("WF Timer", &wfTimerID, AP_WI, AP_WI);
	while (1) {}
}

