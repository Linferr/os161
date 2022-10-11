
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>

/*
	what I want to implement is: 
	using an array rope to track each rope's state;
	array hook and stake to track which rope to change;
*/
struct rope{
	struct lock *rp_lk;
	volatile bool cut; //false is available
    // int rp_hk;
    // volatile int rp_sk;
};

struct hook{
	int rp_num; //used to track the rope
	struct lock *hk_lk;
};

struct stake{
	int rp_num; //used to track the rope
	struct lock *sk_lk;  
};


