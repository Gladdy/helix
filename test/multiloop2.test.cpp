#include "helix.h"

int multiloop(int init_wr, int init_i, int init_wr2) {
	int write_read_dep = init_wr;
	int write_read_dep2 = init_wr2;
	int induction = init_i * 2;

	for(; induction < 100; induction++) {
		if(induction < 10) {
			write_read_dep += heavy_function(induction);
		} else if(induction < 20) {
			write_read_dep2 += heavy_function(write_read_dep);
		} else {
			// this is particularly mean as it has to be moved into the prologue (inserting the required phis, but not the rest)
			induction *= 2;
		}
	}
	return write_read_dep + induction + write_read_dep2;
}

int call_multiloop() {
	return multiloop(10, 100, 100);
}