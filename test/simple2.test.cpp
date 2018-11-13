#include "helix.h"

int simple(int init_wr, int init_i) {
	int write_read_dep = init_wr;
	int induction = init_i;

	for(; induction < 100; induction++) {
		auto res = heavy_function(induction);		
		write_read_dep += res;
		
		if(induction > 10 or induction < 5) {
			induction *= 2;
		}	
	}
	return write_read_dep + induction;
}

int call_simple() {
	return simple(100, 10);
}



