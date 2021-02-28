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

/* Variables ****************************************/
const struct device *uart_dev;

/* Functions ****************************************/
void add_to_buff(uint8_t* newValue) {
	uart_irq_tx_enable(uart_dev);
	uart_fifo_fill(uart_dev, newValue, 1);
	uart_irq_tx_disable(uart_dev);
}

void uart_callback() {
	while (uart_irq_update(uart_dev) && uart_irq_is_pending(uart_dev)) {
		if (uart_irq_rx_ready(uart_dev)) {
			//printk("in rx callback\n");
			int recv_len;
			size_t len = 1;
			uint8_t *buffer;
     		recv_len = uart_fifo_read(uart_dev, buffer, len);
			add_to_buff(buffer);
     		printk("%c",buffer);
		}
	}
}

void main(void)
{

	uart_dev = device_get_binding(DT_LABEL(MY_UART));

	if (!uart_dev) {
		printk("UART: Device driver not found.\n");
		return;
	}

	uart_rx_disable(uart_dev);
	uart_irq_rx_disable(uart_dev);
	uart_irq_tx_disable(uart_dev);

	uart_irq_callback_set(uart_dev, uart_callback);

	uart_irq_rx_enable(uart_dev);
	uart_irq_tx_enable(uart_dev);

	printk("Success\n");
	while(1){
		//loop forever
	}
}
