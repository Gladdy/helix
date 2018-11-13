#include "helix.h"

/*
int example_function(int init_wr, int init_ww, int init_rr, int init_i) {
	//int example = init_i > 0 ? heavy_function(0) : heavy_function(1);
	printf("init_wr=%i init_ww=%i init_i=%i\n", init_wr, init_ww, init_i);	
	int write_read_dep = init_wr;
	//int read_write_dep = init_wr;
	//int scratch = 0;
	
	//int write_write_dep = init_ww;
	//int read_read_dep = init_rr;
	int induction = init_i;

	//int write_write_arr[1337];
	//int write_read_arr[1337];
	
	for(; induction < 100; induction++) {	
		// Real loop carried dependencies
		printf("1 i=%i write_read_dep=%i\n", induction, write_read_dep);
		write_read_dep = write_read_dep + heavy_function(induction);
		printf("2 i=%i write_read_dep=%i\n", induction, write_read_dep);
		
		// Fake dependency
		//read_write_dep = induction;
		//scratch = read_write_dep;
		
		// Fake dependency (as it's not read in the loop)
		//write_write_dep = induction;
		
		// Fake dependency (only writing)
		//write_write_arr[induction] = write_read_dep;
		
		// Real dependency, the next iteration will read this value
		//write_read_arr[induction+1] = write_read_arr[induction] + induction;
	}
	//return write_read_dep + read_write_dep + write_write_dep + read_read_dep + induction;
	return write_read_dep + induction;
}
*/

int call_simple();
int call_linked();
int call_multiloop();

int main(int argc, char **argv) {
	call_simple();
	call_linked();
	call_multiloop();
}