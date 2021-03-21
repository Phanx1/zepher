/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr.h>
#include <sys/printk.h>
#include <device.h>
#include <devicetree.h>
#include <drivers/uart.h>
//#include <posix/pthread.h>
#include <stdio.h>
#include <sys/ring_buffer.h>
#include <stdint.h>

/* Defines *******************************************/
#define MY_UART DT_NODELABEL(lpuart3)
#define TIMEOUT_TX 		1000 //ms
#define BUF_SIZE 		1024

#define MY_STACK_SIZE 500
#define MY_PRIORITY 5

/* Structs *******************************************/
const struct uart_config uart_cfg = {
		.baudrate = 115200,
		.parity = UART_CFG_PARITY_NONE,
		.stop_bits = UART_CFG_STOP_BITS_1,
		.data_bits = UART_CFG_DATA_BITS_8,
		.flow_ctrl = UART_CFG_FLOW_CTRL_NONE
	};

/* Variables ****************************************/
uint8_t in_buffer[BUF_SIZE] = {0};
uint8_t out_buffer[BUF_SIZE];

int locationWriteBuf = 0;
int locationReadBuf = 0;

bool restartCount = false;

const struct device *uart_dev;

// struct k_fifo printk_fifo;

struct k_thread my_thread_data;

// typedef struct  {
//     void *fifo_reserved;   /* 1st word reserved for use by FIFO */
//     uint8_t data;
// }data_item_t;

struct printk_item_t {
    void *fifo_reserved;   /* 1st word reserved for use by FIFO */
    uint8_t data;
};

K_FIFO_DEFINE(printk_fifo);

/* Functions ***************************************/
void print_single(uint8_t *newValue) {
	//uart_irq_tx_enable(uart_dev);
	uart_fifo_fill(uart_dev, newValue, 1);
	uart_irq_tx_disable(uart_dev);
}

// void uart_callback() {
// 	while (uart_irq_update(uart_dev) && uart_irq_is_pending(uart_dev)) {
// 		if (uart_irq_rx_ready(uart_dev)) {
// 			//printk("in rx callback\n");
// 			int recv_len;
// 			size_t len = 1;
// 			uint8_t buffer[len];
//      		recv_len = uart_fifo_read(uart_dev, buffer, len);
// 			print_single(buffer);
//      		printk("%s",buffer);
// 		}
// 	}
// }

void print_console_callback() {
	while (uart_irq_update(uart_dev) && uart_irq_is_pending(uart_dev)) {
		if (uart_irq_rx_ready(uart_dev)) {
			size_t len = 1;
			uint8_t buffer[len];
			uart_fifo_read(uart_dev, buffer, len);
			struct printk_item_t tx_data = { .data = *buffer };
			size_t size = sizeof(struct printk_item_t);
			char *mem_ptr = k_malloc(size);
			__ASSERT_NO_MSG(mem_ptr != 0);
			memcpy(mem_ptr, &tx_data, size);
			k_fifo_put(&printk_fifo, mem_ptr);
			printk("%c\n", tx_data.data);
		}
	}
}

void print_from_fifo() {
	
	printk("Hello Programmer! %s\n", CONFIG_BOARD);
	printk("thread start\n");

	k_fifo_init(&printk_fifo);

	uart_dev = device_get_binding(DT_LABEL(MY_UART));

	if (!uart_dev) {
		printk("UART: Device driver not found.\n");
		return;
	}

	//edens best practice for making sure its shutup
	uart_rx_disable(uart_dev);
	uart_irq_rx_disable(uart_dev);
	uart_irq_tx_disable(uart_dev);
	
	uart_irq_callback_set(uart_dev, print_console_callback);

	uart_irq_rx_enable(uart_dev);
	//uart_irq_tx_enable(uart_dev);

	while (1) {
		struct printk_item_t *rx_data = k_fifo_get(&printk_fifo, K_FOREVER);
		uint8_t *point_data = &rx_data->data;
		print_single(point_data);
		printk("%c\n", rx_data->data);
		k_free(rx_data);
	}
}

void print_hello_world(){
	char *tx_string = "Hello World";
	while (1) {
		for (int i = 0; i < strlen(tx_string); i++) {
			//print_single(&tx_string[i]);
			struct printk_item_t tx_data = { .data = tx_string[i] };
			size_t size = sizeof(struct printk_item_t);
			char *mem_ptr = k_malloc(size);
			__ASSERT_NO_MSG(mem_ptr != 0);
			memcpy(mem_ptr, &tx_data, size);
			k_fifo_put(&printk_fifo, mem_ptr);
		}
		k_msleep(4000);
	}
}

void print_kevin(){
	char *print_string = "KEVIN";
	while (1) {
		for (int i = 0; i < strlen(print_string); i++) {
			print_single(&print_string[i]);
			printk("%c\n", print_string[i]);
		}
		k_msleep(2000);
	}
}

//K_THREAD_DEFINE(my_tid, MY_STACK_SIZE, print_from_fifo, NULL, NULL, NULL,
               // MY_PRIORITY, 0, 0);

// void main(void)
// {
// 	printk("Hello Programmer! %s\n", CONFIG_BOARD);
// 	/* Creates a thread */
// // 	pthread_t* thread;
// // 	const pthread_attr_t attr = {
// // 		.priority = 1,
// // 		.stack = NULL,
// // 		.stacksize = 0,
// // 		.flags = PTHREAD_CANCEL_ENABLE,
// // 		.delayedstart = 0,
// // #if defined(CONFIG_PREEMPT_ENABLED)
// // 		.schedpolicy = SCHED_RR,
// // #else
// // 		.schedpolicy = SCHED_FIFO,
// // #endif
// // 		.detachstate = PTHREAD_CREATE_JOINABLE,
// // 		.initialized = true,
// // 	};
// 	// if(!pthread_create(thread, attr, print_hello_world, NULL)) {
// 	// 	printk("Thread Createion failed");
// 	// 	return
// 	// }

// 	uart_dev = device_get_binding(DT_LABEL(MY_UART));

// 	if (!uart_dev) {
// 		printk("UART: Device driver not found.\n");
// 		return;
// 	}

// 	//edens best practice for making sure its shutup
// 	uart_rx_disable(uart_dev);
// 	uart_irq_rx_disable(uart_dev);
// 	uart_irq_tx_disable(uart_dev);

// 	uart_irq_callback_set(uart_dev, print_console_callback);

// 	uart_irq_rx_enable(uart_dev);
// 	uart_irq_tx_enable(uart_dev);

// 	//start thread 
// // 	k_tid_t my_tid = k_thread_create(&my_thread_data, my_stack_area,
// //                                  K_THREAD_STACK_SIZEOF(my_stack_area),
// //                                  print_from_fifo,
// //                                  NULL, NULL, NULL,
// //                                  MY_PRIORITY, 0, K_FOREVER);

// // 	k_thread_name_set(my_tid, "print_console_thread");
// // #if CONFIG_SCHED_CPU_MASK
// // 	k_thread_cpu_mask_disable(&my_thread_data, 1);
// // 	k_thread_cpu_mask_enable(&my_thread_data, 0);
// // #endif

// // 	k_thread_start(&my_thread_data);

// 	//async api only
// 	//uart_tx(uart_dev, tx_buf, sizeof(tx_buf),1000);
// }

K_THREAD_DEFINE(print_console, MY_STACK_SIZE, print_from_fifo, NULL, NULL, NULL,
               MY_PRIORITY, 0, 0);
	   
K_THREAD_DEFINE(kevin, MY_STACK_SIZE, print_kevin, NULL, NULL, NULL,
				MY_PRIORITY, 0, 2000);

K_THREAD_DEFINE(hello_world, MY_STACK_SIZE, print_hello_world, NULL, NULL, NULL,
				MY_PRIORITY, 0, 2000);
