#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <math.h>

/*Error codes*/
const int WRONG_ARGUMENTS_AMOUNT = 1;
const int WRONG_SIMULATION_TIME_VALUE = 2;
const int WRONG_THREADS_AMOUNT = 3;
const int IMPOSSIBILITY_OF_PARALLEL_COMPUTING = 4;
const int THREAD_CREATING_ERROR = 5;

/*For implementing parallelism*/
typedef struct {
	pthread_t tid;
	int from;
	int to;
	/*For debug purposes
	int next_iter;*/
} ThreadRecord;

ThreadRecord* threads;
pthread_barrier_t barr1, barr2;

/*For computing and storing intermediate information*/
double* g_cur;
double* g_prev;
int g_rows, g_cols; 
int g_time;

/*Initial conditions*/
double g_U = 220.0;
const double g_C = 1.0;
const double g_R = 5.0;

/*Numerical integration step*/
const double g_h = 1.0;

void* thread_func(void *arg)
{ 
	ThreadRecord* thread;
	double tmp = 0.0;
	thread = (ThreadRecord *)arg;
	for(int k = 0; k < g_time + 2; k++){
	    pthread_barrier_wait(&barr1);
        for (int i = thread->from; i <= thread->to; i++) {
            for (int j = 0; j < g_cols; j++) {
            
                /*Calculations for the four ends of the RC element mesh*/
				if (j == 0 && i == 0) {
				  g_cur[i * g_cols + j] = g_U;
				  continue;
				} else if (j == g_cols - 1 && i == 0) {
				  g_cur[i * g_cols + j] = g_U;
				  continue;
				} else if (j == 0 && i == g_rows - 1) {
				  g_cur[i * g_cols + j] = g_U;
				  continue;
				} else if (j == g_cols - 1 && i == g_rows - 1) {
				  g_cur[i * g_cols + j] = g_U;
				  continue;
				}
                
                /*Calculation of the remaining boundaries of the RC element
                mesh*/
				else if (j == 0){
					tmp = g_prev[(i - 1) * g_cols + j] +
					      g_prev[i * g_cols + j + 1] +
					      g_prev[(i + 1) * g_cols + j];
    			} else if (j == g_cols - 1){
					tmp = g_prev[i * g_cols + j - 1] +
					      g_prev[(i - 1) * g_cols + j] +
					      g_prev[(i + 1) * g_cols + j];
				} else if (i == 0){
					tmp = g_prev[i * g_cols + j - 1] +
					      g_prev[i * g_cols + j + 1] +
					      g_prev[(i + 1) * g_cols + j];
                } else if (i == g_rows - 1){
					tmp = g_prev[i * g_cols + j - 1] +
					      g_prev[(i - 1) * g_cols + j] +
					      g_prev[i * g_cols + j + 1];
                }

                /*Calculation of internal nodes of the RC element mesh*/
				else{
					tmp = g_prev[i * g_cols + j - 1] +
				          g_prev[(i - 1) * g_cols + j] +
				          g_prev[i * g_cols + j + 1] +
				          g_prev[(i + 1) * g_cols + j];
                }
                
                /*Calculation of the voltage value in a node at the current
                time*/
				g_cur[i * g_cols + j] = g_h * tmp / g_C / g_R +
				                        g_prev[i * g_cols + j] *
				                        (1 - 4 * g_h / g_C / g_R);
			}
        }
        
        /*For debug purposes
        printf("Performed %d iteration. Thread %ld\n", thread->next_iter,
                thread->tid);
        thread->next_iter += 1;*/
		pthread_barrier_wait(&barr2);
	}
    return NULL;
}

int main(int argc, char* argv[])
{
	int nthread;
	pthread_attr_t pattr;

    /*Checking input data*/
    if (argc < 5 || argc > 5){
		printf("Usage: %s <rows_amount> <cols_amount> <simulation_time>\
		        <threads_amount>\n", argv[0]);
		return(WRONG_ARGUMENTS_AMOUNT);
	}

	if (atoi(argv[3]) < 0){
		printf("Simulation time should be a natural number\n");
		return(WRONG_SIMULATION_TIME_VALUE);
	}
	
	if (atoi(argv[4]) < 0){
		printf("The number of threads should be a natural number\n");
		return(WRONG_THREADS_AMOUNT);
	}

    /*Loading input data into variables*/
    g_rows = atoi(argv[1]);
	g_cols = atoi(argv[2]);
	g_time = atoi(argv[3]);
	nthread = atoi(argv[4]);
    
    /*Checking the possibility of parallel computing*/
	if (g_rows % nthread){
	    printf("The number of rows must be divided by the number of threads\n");
		return(IMPOSSIBILITY_OF_PARALLEL_COMPUTING);
	}

    /*Initializing tools for working with parallel computing*/
	pthread_attr_init(&pattr);
	threads = (ThreadRecord *)calloc(nthread, sizeof(ThreadRecord));
	pthread_barrier_init(&barr1, NULL, nthread + 1);
	pthread_barrier_init(&barr2, NULL, nthread + 1);
	int j = g_rows / nthread;
	
	/*Allocation of memory for storing intermediate data*/
	g_cur = (double*)malloc(sizeof(double) * g_rows * g_cols);
	g_prev = (double*)malloc(sizeof(double) * g_rows * g_cols);
	
	/*Initializing and running threads*/
	for (int i = 0; i < nthread; i++) {
	    threads[i].from = j * i;
	    threads[i].to = j * (i + 1) - 1;
	    /*For debug purposes
	    threads[i].next_iter = 1;*/
	    if (pthread_create(&(threads[i].tid), &pattr, thread_func,
	                       (void*)&(threads[i]))){
            printf("An error occurred while starting the thread.\
                    Launch the program later.\n");
		    return(THREAD_CREATING_ERROR);
		}
	}

    /*Initializing tools for working with time measurements*/
	struct timeval tv1, tv2, dtv;
	struct timezone tz;
	gettimeofday(&tv1, &tz);
	
	/*Threads are starting calculations*/
	pthread_barrier_wait(&barr1);
	
    FILE *fd = fopen("output.txt", "w");
    
    /*Loop for calculating the voltage values in the mesh at each point
    in time*/
    for (int k = 1; k < g_time + 2; k++){
        
        /*Sinusoidal voltage source. Comment out the line below to get a
        constant voltage source*/
        g_U = sin(g_time);
		
		/*Wait for all threads to complete calculations before writing
		the resulting values*/
		pthread_barrier_wait(&barr2);
		
		/*Writing the obtained values ​​for the current point in time to
		the output file*/
		for (int i = 0; i < g_rows; i++){
			for (int j = 0; j < g_cols; j++){
				fprintf(fd, "%d %d %lf\n", i, j, g_prev[i * g_cols + j]);
			}
		}
		fprintf(fd, "\n\n");
		
		/*Preparing data to simulate the next point in time*/
		memcpy(g_prev, g_cur, sizeof(double) * g_rows * g_cols);
		memset(g_cur, 0, g_rows * g_cols);
		
		/*Threads are starting calculations again*/
		pthread_barrier_wait(&barr1);
	}
	fclose(fd);
	
	/*Calculating the running time of threads*/
	gettimeofday(&tv2, &tz);
	dtv.tv_sec = tv2.tv_sec - tv1.tv_sec;
	dtv.tv_usec = tv2.tv_usec - tv1.tv_usec;
	if(dtv.tv_usec < 0){
	    dtv.tv_sec -= 1;
	    dtv.tv_usec = 1000000 + dtv.tv_usec;
	}
	printf("%ld:%ld\n", dtv.tv_sec, (long)(dtv.tv_usec / 1000));
	
	/*Visualizing results as gif animation using gnuplot*/
	FILE *gnuplot = popen("gnuplot -persist 2>/dev/null", "w");
	fprintf(gnuplot, "set colorbox vertical\n");
	fprintf(gnuplot, "set pm3d at s explicit\n");
	fprintf(gnuplot, "set dgrid3d\n");
    fprintf(gnuplot, "set xrange [%d:%d]\n", 0, g_rows);
    fprintf(gnuplot, "set yrange [%d:%d]\n", 0, g_cols);
	fprintf(gnuplot, "set hidden3d\n");
	fprintf(gnuplot, "set xlabel \"Rows\"\n");
	fprintf(gnuplot, "set ylabel \"Columns\"\n");
	fprintf(gnuplot, "set zlabel \"U\"\n");
	/*Change the numerical value in the line below to change the delay
	between animation frames*/
	fprintf(gnuplot, "set term gif animate delay 40\n");
	fprintf(gnuplot, "set output \"grid.gif\"\n");
	fprintf(gnuplot, "do for [i=1:%d] { splot \"output.txt\" index i w pm3d\
	                  title \"Second \".(i) }\n", g_time);

	fflush(gnuplot);

	return 0;
}