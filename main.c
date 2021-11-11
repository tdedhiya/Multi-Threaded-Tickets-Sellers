#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<time.h>
#include<string.h>
#include<malloc.h>
#include<pthread.h>
#include"utility.h"
#define hp_seller_count 1
#define mp_seller_count 3
#define lp_seller_count 6
#define total_seller (hp_seller_count + mp_seller_count + lp_seller_count)
#define concert_row 10
#define concert_col 10
#define simulation_duration 60

// Seller Argument Structure
typedef struct sell_arg_struct {
	char seller_no;
	char seller_type;
	queue *seller_queue;
} sell_arg;

typedef struct customer_struct {
	char cust_no;
	int arrival_time;
	int random_wait_time;
} customer;

typedef struct seat_struct {
	pthread_mutex_t seat_mutex;
	char seller_type;
	int num;
	int response;
	int turn_time;
} seat;

//Global Variable
int sim_time = -1;
int N = 5;
seat seat_matrix[concert_row][concert_col];	

//Thread Variable
pthread_t seller_t[total_seller];
pthread_mutex_t thread_count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t reservation_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t thread_completion_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t condition_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  condition_cond  = PTHREAD_COND_INITIALIZER;
pthread_cond_t  new_thread_waiting  = PTHREAD_COND_INITIALIZER;
pthread_cond_t  thread_finished  = PTHREAD_COND_INITIALIZER;
pthread_cond_t thread_ready = PTHREAD_COND_INITIALIZER;


//Function Definition
void display_queue(queue *q);
void create_seller_threads(pthread_t *thread, char seller_type, int no_of_sellers);
void wait_for_thread_to_serve_current_time_slice();
void wakeup_all_seller_threads();
void *sell(void *);
queue * generate_customer_queue(int, char);
int compare_by_arrival_time(void * data1, void * data2);
int findAvailableSeat(char seller_type);

int thread_count = 0;
int threads_waiting_for_clock_tick = 0;
int active_thread = 0;
int verbose = 0;
int main(int argc, char** argv) {
	if(argc == 2) {
		N = atoi(argv[1]);
	}

	//Initialize Global Variables
	for(int r=0; r<concert_row; r++) {
		for(int c=0; c<concert_col; c++) {
			seat_matrix[r][c].seat_mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
			seat_matrix[r][c].seller_type = '-';
			seat_matrix[r][c].num = r*concert_col + c;
			seat_matrix[r][c].response = 0;
			seat_matrix[r][c].turn_time = 0;
		}
	}
	//Create all threads
	create_seller_threads(seller_t, 'H', hp_seller_count);
	create_seller_threads(seller_t + hp_seller_count, 'M', mp_seller_count);
	create_seller_threads(seller_t + hp_seller_count + mp_seller_count, 'L', lp_seller_count);

	//Wait for threads to finish initialization and wait for synchronized clock tick
	pthread_mutex_lock( &thread_count_mutex );
	while (thread_count != 0) {
		pthread_cond_wait( &thread_ready, &thread_count_mutex);
	}
	pthread_mutex_unlock( &thread_count_mutex );

	//Simulate each time quanta/slice as one iteration
	printf("Starting Simulation\n");
	threads_waiting_for_clock_tick = 0;
	sim_time = sim_time + 1;
	wakeup_all_seller_threads(); //For first tick
	
	do {
		//Wait for thread completion
		wait_for_thread_to_serve_current_time_slice();
		sim_time = sim_time + 1;
		//Wake up all thread
		wakeup_all_seller_threads();
	} while(sim_time < simulation_duration);

	//Wakeup all thread so that no more thread keep waiting for clock Tick in limbo
	wakeup_all_seller_threads();
	pthread_mutex_lock( &thread_count_mutex );
	while (active_thread) {
		pthread_cond_wait( &thread_finished, &thread_count_mutex);
	}
	pthread_mutex_unlock( &thread_count_mutex );

	//Display concert chart
	printf("\n\n");
	printf("Final Concert Seat Chart\n");
	printf("========================\n");

	int h_customers = 0,m_customers = 0,l_customers = 0;
	int h_total_response = 0, m_total_response = 0, l_total_response = 0;
	int h_total_turn_time = 0, m_total_turn_time = 0, l_total_turn_time = 0;

	//gathering up final stats
	for(int r=0;r<concert_row;r++) {
		for(int c=0;c<concert_col;c++) {
			if(c!=0)
				printf("\t");
			printf("%5s",&seat_matrix[r][c].seller_type);
			if(seat_matrix[r][c].seller_type=='H') {
				h_total_response += seat_matrix[r][c].response;
				h_total_turn_time += seat_matrix[r][c].turn_time;
				h_customers++;
			} 
			if(seat_matrix[r][c].seller_type=='M') {
				m_total_response += seat_matrix[r][c].response;
				m_total_turn_time += seat_matrix[r][c].turn_time;
				m_customers++;
			} 
			if(seat_matrix[r][c].seller_type=='L') {
				l_total_response += seat_matrix[r][c].response;
				l_total_turn_time += seat_matrix[r][c].turn_time;
				l_customers++;
			}
		} 
		printf("\n");
	}
	//displaying stats
	printf("\n\nStat for N = %02d\n",N);
	printf(" ======================================================================\n");
	printf("|%3c | No of Customers | Got Seat | Returned | Response | Turn Around |\n",' ');
	printf(" ======================================================================\n");
	printf("|%3c | %15d | %8d | %8d | %8f | %11f |\n",'H',hp_seller_count*N,h_customers,(hp_seller_count*N)-h_customers, (double) h_total_response/h_customers, (double) h_total_turn_time/h_customers);
	printf("|%3c | %15d | %8d | %8d | %8f | %11f |\n",'M',mp_seller_count*N,m_customers,(mp_seller_count*N)-m_customers, (double) m_total_response/m_customers, (double) m_total_turn_time/m_customers);
	printf("|%3c | %15d | %8d | %8d | %8f | %11f |\n",'L',lp_seller_count*N,l_customers,(lp_seller_count*N)-l_customers, (double) l_total_response/l_customers, (double) l_total_turn_time/l_customers);
	printf(" ======================================================================\n");

	return 0;
}
void display_queue(queue *q) {
	for(node *ptr = q->head;ptr!=NULL;ptr=ptr->next) {
		customer *cust = (customer * )ptr->data;
		printf("[%d,%d]",cust->cust_no,cust->arrival_time);
	}
}
void create_seller_threads(pthread_t *thread, char seller_type, int no_of_sellers){
	//Create all threads of given type
	for(int t_no = 0; t_no < no_of_sellers; t_no++) {
		sell_arg *seller_arg = (sell_arg *) malloc(sizeof(sell_arg));
		seller_arg->seller_no = t_no;
		seller_arg->seller_type = seller_type;
		seller_arg->seller_queue = generate_customer_queue(N, seller_type);

		//increment number of initialized but not started threads
		pthread_mutex_lock(&thread_count_mutex);
		thread_count++;
		pthread_mutex_unlock(&thread_count_mutex);

		if(verbose)
			printf("Creating thread %c%02d\n",seller_type,t_no);
		pthread_create(thread+t_no, NULL, &sell, seller_arg);
	}
}



void wait_for_thread_to_serve_current_time_slice(){
	//Check if all threads has finished their jobs for this time slice
	pthread_mutex_lock( &thread_count_mutex );
	while (threads_waiting_for_clock_tick != active_thread) {
		pthread_cond_wait( &new_thread_waiting, &thread_count_mutex);
	}
	threads_waiting_for_clock_tick = 0;	
	pthread_mutex_unlock(&thread_count_mutex);

}
void wakeup_all_seller_threads() {

	pthread_mutex_lock( &condition_mutex );
	if(verbose)
		printf("00:%02d Main Thread Broadcasting Clock Tick\n",sim_time);
	pthread_cond_broadcast( &condition_cond);
	pthread_mutex_unlock( &condition_mutex );
}

void *sell(void *t_args) {
	//Initializing thread
	sell_arg *args = (sell_arg *) t_args;
	queue * customer_queue = args->seller_queue;
	queue * seller_queue = create_queue();
	char seller_type = args->seller_type;
	int seller_no = args->seller_no + 1;
	
	pthread_mutex_lock(&thread_count_mutex);
	thread_count--;
	active_thread++;
	pthread_cond_broadcast(&thread_ready);
	pthread_mutex_unlock(&thread_count_mutex);

	customer *cust = NULL;
	int last_time;
	int seatIndex = -1;

	while(sim_time < simulation_duration) {
		//Waiting for clock tick
		if(verbose)
			printf("00:%02d %c%02d Waiting for next clock tick\n",sim_time,seller_type,seller_no);
		//store what was the last timeslice serviced, used to determine if current timeslice is new or old
		last_time = sim_time;
		//let the clock know that a seller is ready to go to next timeslice
		pthread_mutex_lock(&thread_count_mutex);
		threads_waiting_for_clock_tick++;
		pthread_cond_broadcast( &new_thread_waiting);
		pthread_mutex_unlock(&thread_count_mutex);

		pthread_mutex_lock(&condition_mutex);
		//to ensure that there wasnt a random wakeup and clock has actually ticked forward
		while (sim_time == last_time)
			pthread_cond_wait( &condition_cond, &condition_mutex);
		if(verbose)
			printf("00:%02d %c%02d Received Clock Tick\n",sim_time,seller_type,seller_no);
		pthread_mutex_unlock( &condition_mutex );

		// if endtime reached then stop
		if(sim_time == simulation_duration) break;
		//All New Customer Arrived
		while(customer_queue->size > 0 && ((customer *)customer_queue->head->data)->arrival_time <= sim_time) {
			customer *temp = (customer *) dequeue (customer_queue);
			enqueue(seller_queue,temp);
			printf("00:%02d %c%d Customer No %c%d%02d arrived\n",sim_time,seller_type,seller_no,seller_type,seller_no,temp->cust_no);
		}
		//No current customer? Then serve next customer
		if(cust == NULL && seller_queue->size>0) {
			// Check seats
			seatIndex = findAvailableSeat(seller_type);
			//if concert is sold out, tell everyone waiting to go away
			if(seatIndex == -1) {
				while (seller_queue->size>0) {
					cust = (customer *) dequeue(seller_queue);
					printf("00:%02d %c%d Customer No %c%d%02d has been told Concert Sold Out.\n",sim_time,seller_type,seller_no,seller_type,seller_no,cust->cust_no);
				}
				cust = NULL;
			} else {
				//otherwise start serving next customer
				cust = (customer *) dequeue(seller_queue);
				printf("00:%02d %c%d Serving Customer No %c%d%02d\n",sim_time,seller_type,seller_no,seller_type,seller_no,cust->cust_no);
				int row_no = seatIndex/concert_col;
				int col_no = seatIndex%concert_col;
				//storing customer stats
				seat_matrix[row_no][col_no].response = sim_time - cust->arrival_time; //calculated response time
				seat_matrix[row_no][col_no].turn_time = sim_time - cust->arrival_time + cust->random_wait_time; // calculated turnaround time
				sprintf(&seat_matrix[row_no][col_no].seller_type,"%c%d%02d",seller_type,seller_no,cust->cust_no);
				printf("00:%02d %c%d Customer No %c%d%02d assigned seat %d,%d \n",sim_time,seller_type,seller_no,seller_type,seller_no,cust->cust_no,row_no,col_no);
			}
		}
		//I have a customer I am currently serving
		if(cust != NULL) {
			//Selling Seat
			cust->random_wait_time--;
			//if customer is served by end of minute then mark myself as empty
			if(cust->random_wait_time == 0) {
				cust = NULL;
			}
		}
	}
	//once the time is up tell all unserved customers to go home
	while(cust!=NULL || seller_queue->size > 0) {
		if(cust==NULL)
			cust = (customer *) dequeue(seller_queue);
		printf("00:%02d %c%d Ticket Sale Closed. Customer No %c%d%02d Leaves\n",sim_time,seller_type,seller_no,seller_type,seller_no,cust->cust_no);
		cust = NULL;
	}
	//let main thread know that this seller is completely done
	pthread_mutex_lock(&thread_count_mutex);
	active_thread--;
	pthread_cond_broadcast( &thread_finished);
	pthread_mutex_unlock(&thread_count_mutex);
}

int findAvailableSeat(char seller_type){
	int seatIndex = -1;

	if(seller_type == 'H') {
		//start from top left
		for(int row_no = 0;row_no < concert_row; row_no ++ ){
			for(int col_no = 0;col_no < concert_col; col_no ++) {
				//if lock returns 0 then we have the seat, otherwise someone else already took it
				if(pthread_mutex_trylock(&seat_matrix[row_no][col_no].seat_mutex) == 0) {
					seatIndex = row_no * concert_col + col_no;
					return seatIndex;
				}
			}
		}
	} else if(seller_type == 'M') {
		int mid = concert_row / 2;
		int row_jump = 0;
		int next_row_no = mid;
		//row jump is to make the zigzag pattern that the middle sellers do
		for(row_jump = 0;;row_jump++) {
			//start from middle left
			int row_no = mid+row_jump;
			if(mid + row_jump < concert_row) {
				for(int col_no = 0;col_no < concert_col; col_no ++) {
					//if lock returns 0 then we have the seat, otherwise someone else already took it
					if(pthread_mutex_trylock(&seat_matrix[row_no][col_no].seat_mutex) == 0) {
						seatIndex = row_no * concert_col + col_no;
						return seatIndex;
					}
				}
			}
			row_no = mid - row_jump;
			if(row_jump != 0 && row_no >= 0) {
				for(int col_no = 0;col_no < concert_col; col_no ++) {
					//if lock returns 0 then we have the seat, otherwise someone else already took it
					if(pthread_mutex_trylock(&seat_matrix[row_no][col_no].seat_mutex) == 0) {
						seatIndex = row_no * concert_col + col_no;
						return seatIndex;
					}
				}
			}
			if(mid + row_jump >= concert_row && mid - row_jump < 0) {
				break;
			}
		}
	} else if(seller_type == 'L') {
		//start from bottom right
		for(int row_no = concert_row - 1;row_no >= 0; row_no-- ){
			for(int col_no = concert_col - 1;col_no >= 0; col_no--) {
				//if lock returns 0 then we have the seat, otherwise someone else already took it
				if(pthread_mutex_trylock(&seat_matrix[row_no][col_no].seat_mutex) == 0) {
					seatIndex = row_no * concert_col + col_no;
					return seatIndex;
				}
			}
		}
	}

	return -1;
}

queue * generate_customer_queue(int N, char seller_type){
	queue * customer_queue = create_queue();
	char cust_no = 0;
	//genereate N customers for a queue
	while(N--) {
		customer *cust = (customer *) malloc(sizeof(customer));
		cust->cust_no = cust_no;
		cust->arrival_time = rand() % simulation_duration;
		//service time depends on which seller they are going to be served by
		switch(seller_type) {
			case 'H':
			cust->random_wait_time = (rand()%2) + 1;
			break;
			case 'M':
			cust->random_wait_time = (rand()%3) + 2;
			break;
			case 'L':
			cust->random_wait_time = (rand()%4) + 4;
		}
		enqueue(customer_queue,cust);
		cust_no++;
	}
	sort(customer_queue, compare_by_arrival_time);
	node * ptr = customer_queue->head;
	cust_no = 0;
	while(ptr!=NULL) {
		cust_no ++;
		customer *cust = (customer *) ptr->data;
		cust->cust_no = cust_no;
		ptr = ptr->next;
	}
	return customer_queue;
}

int compare_by_arrival_time(void * data1, void * data2) {
	customer *c1 = (customer *)data1;
	customer *c2 = (customer *)data2;
	if(c1->arrival_time < c2->arrival_time) {
		return -1;
	} else if(c1->arrival_time == c2->arrival_time){
		return 0;
	} else {
		return 1;
	}
}
