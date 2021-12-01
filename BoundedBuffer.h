#ifndef BoundedBuffer_h
#define BoundedBuffer_h

#include <stdio.h>
#include <utility>
#include <iostream>
#include <queue>
#include <string>
#include "Semaphore.h"


using namespace std;

class BoundedBuffer
{
private:
	int cap;
	queue<vector<char>> que;

	Semaphore empty;
	Semaphore full;
	Semaphore mut;

public:
	BoundedBuffer(int _cap):cap(_cap), empty(_cap), full(0), mut(1), que() {
	}	
	// void push(vector<char> data){does not work
	void push(char* item, int len){

		empty.P();
		mut.P();
		vector<char> data (item, item+len);
		que.push(data);
		mut.V();
		full.V();
	}
	// void push<vector<char> data){ does not work
	vector<char> pop(char* buf, int bufcap){
		//1.wait, 2.push,3.unlock, 4.return pop
		full.P();
		mut.P();

		vector<char> item = que.front();
		memcpy (buf, item.data(), item.size());

		que.pop();
		mut.V();
		empty.V();
		return item;
		}
};

#endif 
