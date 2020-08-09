#include "util.h"

int linked(Node *n)
{
	int i = 0;
	for (; n != nullptr; n = n->next)
	{
		i += heavy_function(n->value);
	}
	return i;
}

int simple_1(int init_wr, int init_i)
{
	int write_read_dep = init_wr;
	int induction = init_i;

	for (; induction < 100; induction++)
	{
		auto res = heavy_function(induction);
		write_read_dep += res;
	}
	return write_read_dep + induction;
}

int simple_2(int init_wr, int init_i)
{
	int write_read_dep = init_wr;
	int induction = init_i;

	for (; induction < 100; induction++)
	{
		auto res = heavy_function(induction);
		write_read_dep += res;

		if (induction > 10 or induction < 5)
		{
			induction *= 2;
		}
	}
	return write_read_dep + induction;
}

int multiloop_1(int init_wr, int init_i, int init_wr2)
{
	int write_read_dep = init_wr;
	//int write_read_dep2 = init_wr2;
	int induction = init_i * 2;

	for (; induction < 100; induction++)
	{
		//if(induction < 10) {
		write_read_dep += heavy_function(induction);
		//}

		//rite_read_dep2 += heavy_function(write_read_dep);
	}
	return write_read_dep + induction; //+ write_read_dep2
}

int multiloop_2(int init_wr, int init_i, int init_wr2)
{
	int write_read_dep = init_wr;
	int write_read_dep2 = init_wr2;
	int induction = init_i * 2;

	for (; induction < 100; induction++)
	{
		if (induction < 10)
		{
			write_read_dep += heavy_function(induction);
		}
		else if (induction < 20)
		{
			write_read_dep2 += heavy_function(write_read_dep);
		}
		else
		{
			// this is particularly mean as it has to be moved into the prologue (inserting the required phis, but not the rest)
			induction *= 2;
		}
	}
	return write_read_dep + induction + write_read_dep2;
}