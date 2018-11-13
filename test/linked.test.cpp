#include "helix.h"

struct Node {
	Node * next;
	int value = 0;
};

int linked(Node * n) {
	int i = 0;
	while(n != nullptr) {
		n->value = heavy_function(n->value);
		i++;
		n = n->next;
	}
	return i + 1;
}

int call_linked() {
	Node n1;
	Node n2;
	Node n3;

	n1.next = &n2;
	n2.next = &n3;
	n3.next = nullptr;
	
	Node* n = &n1;
	return linked(n);
}