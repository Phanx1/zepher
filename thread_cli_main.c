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

#define MY_STACK_SIZE 500
#define MY_PRIORITY 5

/* Variables ****************************************/
const struct device *uart_dev;

struct k_thread my_thread_data;

struct printk_item_t {
    void *fifo_reserved;   /* 1st word reserved for use by FIFO */
    uint8_t data;
};

K_FIFO_DEFINE(printk_fifo);

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

	while(1) {
		struct printk_item_t *rx_data = k_fifo_get(&printk_fifo, K_FOREVER);
		uint8_t *point_data = &rx_data->data;
		print_single(point_data);
		printk("%c\n", rx_data->data);
		k_free(rx_data);
	}
}

K_THREAD_DEFINE(my_tid, MY_STACK_SIZE, print_from_fifo, NULL, NULL, NULL,
               MY_PRIORITY, 0, 0);