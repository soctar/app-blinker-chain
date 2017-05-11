#include <msp430.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <libwispbase/wisp-base.h>
#include <libchain/chain.h>
#include <libchain/thread.h>
#include <libio/log.h>

#ifdef CONFIG_LIBEDB_PRINTF
#include <libedb/edb.h>
#endif

#include "pin_assign.h"

#include <libchain/thread.h>
#include <libchain/mutex.h>

#define INIT_TASK_DURATION_ITERS  400000
#define TASK_START_DURATION_ITERS 1600000
#define BLINK_DURATION_ITERS      400000
#define WAIT_TICK_DURATION_ITERS  300000
#define NUM_BLINKS_PER_TASK       5
#define WAIT_TICKS                3

// If you link-in wisp-base, then you have to define some symbols.
uint8_t usrBank[USRBANK_SIZE];

struct msg_blinks {
    CHAN_FIELD(unsigned, blinks);
};

struct msg_tick {
    CHAN_FIELD(unsigned, tick);
};

struct msg_self_tick {
    SELF_CHAN_FIELD(unsigned, tick);
};
#define FIELD_INIT_msg_self_tick { \
    SELF_FIELD_INITIALIZER \
}

struct msg_duty_cycle {
    CHAN_FIELD(unsigned, duty_cycle);
};

struct shared_data_struct{
    CHAN_FIELD(unsigned, data1); 
    CHAN_FIELD(unsigned, data2); 
    CHAN_FIELD(mutex_t, lock); 
}; 

TASK(1, task_init)
TASK(2, task_1_r)
TASK(3, task_2_r)
TASK(4, task_3_r)

TASK(5, task_1_g)
TASK(6, task_2_g)
TASK_EXT(7, task_3_g)
TASK_EXT(16, task_shared) 

CHANNEL_WT(task_init, task_1_r, 0, msg_blinks);
CHANNEL_WT(task_init, task_3_r, 0,  msg_tick);
CHANNEL_WT(task_1_r, task_2_r, 0, msg_blinks);
CHANNEL_WT(task_2_r, task_1_r, 0, msg_blinks);

CHANNEL_WT(task_init, task_1_g, 1, msg_blinks); 
CHANNEL_WT(task_init, task_3_g, 1, msg_tick); 
CHANNEL_WT(task_1_g, task_2_g, 1, msg_blinks); 
CHANNEL_WT(task_2_g, task_1_g, 1, msg_blinks); 

SELF_CHANNEL(task_3_g, msg_self_tick); 
SELF_CHANNEL(task_3_r, msg_self_tick);
// Broken channel
CHANNEL(task_shared, task_init, shared_data_struct); 

MULTICAST_CHANNEL(msg_duty_cycle, ch_duty_cycle_r, task_init, task_1_r, task_2_r);
MULTICAST_CHANNEL(msg_duty_cycle, ch_duty_cycle_g, task_init, task_1_g, task_2_g);

volatile unsigned work_x;

static void burn(uint32_t iters)
{
    uint32_t iter = iters;
    while (iter--)
        work_x++;
}

void init()
{
    WISP_init();
/*
    GPIO(PORT_LED_1, DIR) |= BIT(PIN_LED_1);
    GPIO(PORT_LED_2, DIR) |= BIT(PIN_LED_2);
#if defined(PORT_LED_3)
    GPIO(PORT_LED_3, DIR) |= BIT(PIN_LED_3);
#endif
*/
    INIT_CONSOLE();

    __enable_interrupt();
/*
#if defined(PORT_LED_3) // when available, this LED indicates power-on
    GPIO(PORT_LED_3, OUT) |= BIT(PIN_LED_3);
#endif
*/
    //LOG("main.c Done init(): chain app booted\r\n");
}

static void blink_led1(unsigned blinks, unsigned duty_cycle) {
    unsigned i;

    for (i = 0; i < blinks; ++i) {
        GPIO(PORT_LED_1, OUT) |= BIT(PIN_LED_1);
        burn(BLINK_DURATION_ITERS * 2 * duty_cycle / 100);

        GPIO(PORT_LED_1, OUT) &= ~BIT(PIN_LED_1);
        burn(BLINK_DURATION_ITERS * 2 * (100 - duty_cycle) / 100);
    }
}

static void blink_led2(unsigned blinks, unsigned duty_cycle) {
    unsigned i;

    for (i = 0; i < blinks; ++i) {
        GPIO(PORT_LED_2, OUT) |= BIT(PIN_LED_2);
        burn(BLINK_DURATION_ITERS * 2 * duty_cycle / 100);

        GPIO(PORT_LED_2, OUT) &= ~BIT(PIN_LED_2);
        burn(BLINK_DURATION_ITERS * 2 * (100 - duty_cycle) / 100);
    }
}

// Dummy task for shared variables
// TODO hide this from user space
void task_shared(){
    return; 
}

void task_init()
{
    LOG("\r\nIN TASK INIT \r\n");
    task_prologue();
    //CHAN_OUT1(thread_t, threads[0].thread, curctx,  SELF_OUT_CH(scheduler_task));
    //Need to add scheduler init to task_init function
    thread_init();
    unsigned data_init = 0; 
    CHAN_OUT1(unsigned,data1, data_init, CH(task_shared, task_init)); 
    CHAN_OUT1(unsigned,data2, data_init, CH(task_shared, task_init)); 
    mutex_t *temp = CHAN_IN1(mutex_t, lock, CH(task_shared, task_init)); 
    mutex_init(temp); 
    CHAN_OUT1(mutex_t, lock, temp, CH(task_shared,task_init)); 
    LOG("init\r\n");

    // Solid flash signifying beginning of task
    //GPIO(PORT_LED_1, OUT) |= BIT(PIN_LED_1);
    //GPIO(PORT_LED_2, OUT) |= BIT(PIN_LED_2);
    //burn(INIT_TASK_DURATION_ITERS);
    //GPIO(PORT_LED_1, OUT) &= ~BIT(PIN_LED_1);
    //GPIO(PORT_LED_2, OUT) &= ~BIT(PIN_LED_2);
    //burn(INIT_TASK_DURATION_ITERS);

    unsigned blinks = NUM_BLINKS_PER_TASK;
    CHAN_OUT1(unsigned, blinks, blinks, CH_TH(task_init, task_1_r, 0));
    unsigned tick = 0;
    CHAN_OUT1(unsigned, tick, tick, CH_TH(task_init, task_3_r, 0));
    unsigned duty_cycle = 75;
    CHAN_OUT1(unsigned, duty_cycle, duty_cycle,
             MC_OUT_CH(ch_duty_cycle_r, task_init, task_1_r, task_2_r));
    
    CHAN_OUT1(unsigned, blinks, blinks, CH_TH(task_init, task_1_g, 1));
    CHAN_OUT1(unsigned, tick, tick, CH_TH(task_init, task_3_g, 1));
    //TODO Make sure this isn't just writing junk
    CHAN_OUT1(unsigned, duty_cycle, duty_cycle,
             MC_OUT_CH(ch_duty_cycle_g, task_init, task_1_g, task_2_g));


    LOG("LED\r\n");
    //insert thread create!!
    THREAD_CREATE(task_3_r);
  	THREAD_CREATE(task_3_g); 
    TRANSITION_TO_MT(task_3_r);
}

void task_1_r()
{
    task_prologue();

    unsigned blinks;
    unsigned duty_cycle;
    
    LOG("task 1_r\r\n");

    mutex_t lock = *CHAN_IN1(mutex_t, lock, CH(task_shared, task_init)); 
    unsigned data1 = *CHAN_IN1(unsigned, data1, CH(task_shared,task_init)); 
    unsigned data2 = *CHAN_IN1(unsigned, data2, CH(task_shared,task_init)); 
    

    blinks = *CHAN_IN2(unsigned, blinks, CH_TH(task_init, task_1_r, 0), CH_TH(task_2_r, task_1_r, 0));
    duty_cycle = *CHAN_IN1(unsigned, duty_cycle,
                           MC_IN_CH(ch_duty_cycle_r, task_init, task_1_r));

    LOG("task 1: blinks %u dc %u\r\n", blinks, duty_cycle);

    //blink_led1(blinks, duty_cycle);
    /*
    if(blinks > 8)
      blinks = 0;
    */
      
    blinks++;
    if(blinks > 50)
      mutex_unlock(&lock);  
    else
      mutex_lock(&lock);  

    CHAN_OUT1(mutex_t, lock, lock, CH(task_shared, task_init));
    CHAN_OUT1(unsigned, data1, data1, CH(task_shared, task_init));
    CHAN_OUT1(unsigned, data2, data2, CH(task_shared, task_init));
    CHAN_OUT1(unsigned, blinks, blinks, CH_TH(task_1_r, task_2_r, 0));
    
    TRANSITION_TO_MT(task_2_r);
    //TRANSITION_TO(task_2);
}

void task_2_r()
{
    task_prologue();

    unsigned blinks;
    unsigned duty_cycle;

    LOG("task 2_r\r\n");

    // Solid flash signifying beginning of task
    /*
    GPIO(PORT_LED_2, OUT) |= BIT(PIN_LED_2);
    burn(TASK_START_DURATION_ITERS);
    GPIO(PORT_LED_2, OUT) &= ~BIT(PIN_LED_2);
    burn(TASK_START_DURATION_ITERS);
    */
    blinks = *CHAN_IN1(unsigned, blinks, CH_TH(task_1_r, task_2_r, 0));
    duty_cycle = *CHAN_IN1(unsigned, duty_cycle,
                           MC_IN_CH(ch_duty_cycle_r, task_init, task_2_r));

    LOG("task 2: blinks %u dc %u\r\n", blinks, duty_cycle);

    //blink_led2(blinks, duty_cycle);
    
    if(blinks > 75)
    { PRINTF("Ending red thread!\r\n"); 
      THREAD_END(); 
    }
    blinks++;

    CHAN_OUT1(unsigned, blinks, blinks, CH_TH(task_2_r, task_1_r, 0));

	TRANSITION_TO_MT(task_3_r);

	//TRANSITION_TO(task_3);
}

void task_3_r()
{
    task_prologue();
    LOG("task 3 r prologue\r\n");

    unsigned wait_tick = *CHAN_IN2(unsigned, tick, CH_TH(task_init, task_3_r, 0),
                                                   SELF_IN_CH(task_3_r));

    LOG("task 3: wait tick %u\r\n", wait_tick);

    //GPIO(PORT_LED_1, OUT) |= BIT(PIN_LED_1);
    //GPIO(PORT_LED_2, OUT) |= BIT(PIN_LED_2);
    //burn(WAIT_TICK_DURATION_ITERS);
    //GPIO(PORT_LED_1, OUT) &= ~BIT(PIN_LED_1);
    //GPIO(PORT_LED_2, OUT) &= ~BIT(PIN_LED_2);
    //burn(WAIT_TICK_DURATION_ITERS);

    if (++wait_tick < WAIT_TICKS) {
        CHAN_OUT1(unsigned, tick, wait_tick, SELF_OUT_CH(task_3_r));

	  	TRANSITION_TO_MT(task_3_r);
        //TRANSITION_TO(task_3);
    } else {
        unsigned tick = 0;
        CHAN_OUT1(unsigned, tick, tick, SELF_OUT_CH(task_3_r));

		TRANSITION_TO_MT(task_1_r);
        //TRANSITION_TO(task_1);

    }
}

void task_1_g()
{
    task_prologue();

    unsigned blinks;
    unsigned duty_cycle;

    LOG("task 1_g\r\n");
    mutex_t lock = *CHAN_IN1(mutex_t, lock, CH(task_shared, task_init)); 
    mutex_lock(&lock); 
    // Solid flash signifying beginning of task
    //GPIO(PORT_LED_1, OUT) |= BIT(PIN_LED_1);
    //burn(TASK_START_DURATION_ITERS);
    //GPIO(PORT_LED_1, OUT) &= ~BIT(PIN_LED_1);
    //burn(TASK_START_DURATION_ITERS);


    blinks = *CHAN_IN2(unsigned, blinks, CH_TH(task_init, task_1_g, 1), CH_TH(task_2_g, task_1_g, 1));
    duty_cycle = *CHAN_IN1(unsigned, duty_cycle,
                           MC_IN_CH(ch_duty_cycle_r, task_init, task_1_g));

    LOG("task 1: blinks %u dc %u\r\n", blinks, duty_cycle);

    //blink_led1(blinks, duty_cycle);
   /* 
      if(blinks > 8)
      blinks = 0;
    */
    blinks++;

    CHAN_OUT1(unsigned, blinks, blinks, CH_TH(task_1_g, task_2_g, 1));

    //THREAD_CREATE(task_2_g);
    TRANSITION_TO_MT(task_2_g);
    //TRANSITION_TO(task_2);
}

void task_2_g()
{
    task_prologue();

    unsigned blinks;
    unsigned duty_cycle;

    LOG("task 2_g\r\n");

    // Solid flash signifying beginning of task
    //GPIO(PORT_LED_2, OUT) |= BIT(PIN_LED_2);
    //burn(TASK_START_DURATION_ITERS);
    //GPIO(PORT_LED_2, OUT) &= ~BIT(PIN_LED_2);
    //burn(TASK_START_DURATION_ITERS);

    blinks = *CHAN_IN1(unsigned, blinks, CH_TH(task_1_g, task_2_g, 1));
    duty_cycle = *CHAN_IN1(unsigned, duty_cycle,
                           MC_IN_CH(ch_duty_cycle_r, task_init, task_2_g));

    LOG("task 2_g: blinks %u dc %u\r\n", blinks, duty_cycle);

    //blink_led2(blinks, duty_cycle);
    if(blinks > 100)
    { PRINTF("Green thread end!\r\n"); 
      THREAD_END(); 
    }
    blinks++;

    CHAN_OUT1(unsigned, blinks, blinks, CH_TH(task_2_g, task_1_g, 1));

	TRANSITION_TO_MT(task_3_g);

	//TRANSITION_TO(task_3);
}

void task_3_g()
{
    task_prologue();
    LOG("task 3_g prologue\r\n");

    unsigned wait_tick = *CHAN_IN2(unsigned, tick, CH_TH(task_init, task_3_g, 1),
                                                   SELF_IN_CH(task_3_g));

    LOG("task 3: wait tick %u\r\n", wait_tick);
    /*
    GPIO(PORT_LED_1, OUT) |= BIT(PIN_LED_1);
    GPIO(PORT_LED_2, OUT) |= BIT(PIN_LED_2);
    burn(WAIT_TICK_DURATION_ITERS);
    GPIO(PORT_LED_1, OUT) &= ~BIT(PIN_LED_1);
    GPIO(PORT_LED_2, OUT) &= ~BIT(PIN_LED_2);
    burn(WAIT_TICK_DURATION_ITERS);
    */
    if (++wait_tick < WAIT_TICKS) {
        CHAN_OUT1(unsigned, tick, wait_tick, SELF_OUT_CH(task_3_g));

	  	TRANSITION_TO_MT(task_3_g);
        //TRANSITION_TO(task_3);
    } else {
        unsigned tick = 0;
        CHAN_OUT1(unsigned, tick, tick, SELF_OUT_CH(task_3_g));

		TRANSITION_TO_MT(task_1_g);
        //TRANSITION_TO(task_1);

    }
}

ENTRY_TASK(task_init)
INIT_FUNC(init)
