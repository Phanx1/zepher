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
#include <stdio.h>
#include <sys/ring_buffer.h>
#include <stdint.h>

/* Defines *******************************************/
#define MY_UART DT_NODELABEL(lpuart3)
#define TIMEOUT_TX 		1000 //ms
#define BUF_SIZE 		1024

#define MY_STACK_SIZE 500
#define MY_PRIORITY 5

void print_from_fifo();
void print_hello_world();
void print_kevin();

/* Variables ****************************************/
const struct device *uart_dev;

struct k_thread my_thread_data;

struct printk_item_t {
    void *fifo_reserved;   /* 1st word reserved for use by FIFO */
    uint8_t data;
};

K_FIFO_DEFINE(printk_fifo);

K_THREAD_DEFINE(print_console, MY_STACK_SIZE, print_from_fifo, NULL, NULL, NULL,
               MY_PRIORITY, 0, 0);
	   
K_THREAD_DEFINE(kevin, MY_STACK_SIZE, print_kevin, NULL, NULL, NULL,
				MY_PRIORITY, 0, 2000);

K_THREAD_DEFINE(hello_world, MY_STACK_SIZE, print_hello_world, NULL, NULL, NULL,
				MY_PRIORITY, 0, 2000);


/* Functions ***************************************/
void print_single(uint8_t *newValue) {
	uart_fifo_fill(uart_dev, newValue, 1);
	uart_irq_tx_disable(uart_dev);
}

void print_console_callback() {
	while (uart_irq_update(uart_dev) && uart_irq_is_pending(uart_dev)) {
		if (uart_irq_rx_ready(uart_dev)) {
			size_t len = 1;
			uint8_t buffer[len];
			uart_fifo_read(uart_dev, buffer, len);
			if (*buffer == 's') {
				k_thread_suspend(kevin);
			}
			if (*buffer == 'r') {
				k_thread_resume(kevin);
			}
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

	uart_rx_disable(uart_dev);
	uart_irq_rx_disable(uart_dev);
	uart_irq_tx_disable(uart_dev);
	uart_irq_callback_set(uart_dev, print_console_callback);
	uart_irq_rx_enable(uart_dev);

	while (1) {
		struct printk_item_t *rx_data = k_fifo_get(&printk_fifo, K_FOREVER);
		uint8_t *point_data = &rx_data->data;
		print_single(point_data);
		k_msleep(2);
		k_free(rx_data);
	}
}

void print_hello_world(){
	char *tx_string = "Hello World";
	while (1) {
		for (int i = 0; i < strlen(tx_string); i++) {
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
			k_msleep(2);
		}
		k_msleep(2000);
	}
}