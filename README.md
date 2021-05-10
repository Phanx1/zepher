# Zepher Operating System
A dive into ZephyrOS on a NXP MIMXRT-1060 EVK

ZephyrOS
Zephyr is a RTOS that is based off of linux. Its main objective is to be more modular than other current competing RTOS in that the architecture is meant to have the same API regardless of the board so that code can be reused between boards making it easier to use others code. As long as a board is set-up for zephyr (currently hundreds of chips are supported) but moderately easy to set up your own PCB and has the required hardware it can be reused. This is very useful because things like a UART driver can be reused across many boards for different projects instead of having to make a new one for each chip. This guide is broken into a basic how to for setting up a CLI, threading, data types, memory management, and general notes.

CLI How To
This will be set up as a cli that uses an interrupt driven UART that will print back to the console whatever is typed to it. This basic functionality can be built upon to allow for commands to made in order to effect the system as whole. 

To write the code I cloned the Zephyr Project repo https://github.com/zephyrproject-rtos/zephyr. I then took the hello world example and changed it for my purposes. To build and flash the code I used a command line interface with the commands bellow. 

	cd zephyrproject/zephyr 
	west flash -d build/hello_world

west flash builds and flashes the evaluation board. I used a segger j-link to program the NXP MIMXRT 1060-evk. Whenever the board was powered it would run whatever code what put onto it and the reset button could be used to restart the program. I connected to the UART on the arduino header to a ttly - usb cable and that back to my computer. I could send and receive by running

	 screen /dev/tty.usb<check /dev/tty for name> 115200   //depending on host computer operating system the command to read a UART will be different

The same command can read the UART that is on the USB that powers the board. The power USB can easily be printed to through code with the printk() function which helped with debugging. The west build setup also supports west debug which gives a gdb interface for debugging which was helpful with figuring out why data was incorrectly passing at one point. 

The first step is setting up the Arduino headers to have the UART configured. Zephyr allows for a bunch of ways to do this the easiest is to go to the .dts file which is located under boards, <your_board> then adding 

	arduino_serial: &lpuart3 {};

This sets up lpuart3 in the pinmux.c file to be included in the defines so that only hardware needed is defined and to have the ability to support more hardware than pins are available. In the file 

	#define MY_UART DT_NODELABEL(lpuart3)

Allows the device tree to find the UART and use it when calling device_get_binding. ZephyrOS treats all hardware as a device and has the same API for all of them. 

To create the CLI I started with a simple function that reads the UART and transmits it back on the same UART so that the character is displayed on the console you type into. The critical part of this code is the setup of the UART for TX and RX. I first disabled the IRQ for TX and RX so that I could set the callback. The callback handles both RX and TX commands. In the callback I would read from the UART FIFO and then enable TX and write to the FIFO the same character. This code is simple_cli_main.c. 

The next iteration of the CLI was one that used a thread and used a separate FIFO to pass data to the IRQ out so it could be sent back and displayed. In zephyr there are two ways to make a thread 

A. 	#define MY_STACK_SIZE 500
	#define MY_PRIORITY 5
	K_THREAD_DEFINE(my_tid, MY_STACK_SIZE, print_from_fifo, NULL, NULL, NULL,
               MY_PRIORITY, 0, 0);

B. 	K_THREAD_STACK_DEFINE(my_stack_area, MY_STACK_SIZE);
    
	k_tid_t my_tid = k_thread_create(&my_thread_data, my_stack_area,
          K_THREAD_STACK_SIZEOF(my_stack_area),
          print_kevin,
          NULL, NULL, NULL,
     	MY_PRIORITY, 0, K_NO_WAIT);

Both versions work. It is my understanding that the best practice is to us the K_THREAD_DEFINE() when the thread will need to be used in multiple places like if its declared in a header file. The k_thread_create version is best for when it will be private. With this iteration I had issues originally with getting the data to pass correctly with the FIFO, the value being pushed was different then the values being popped. In the end it was required to use memcpy to get the data ready to be put as a void pointer for the data since it can handle any length data as what is pushed. This is shown in thread_cli_main.c.

	uart_fifo_read(uart_dev, buffer, len);
	struct printk_item_t tx_data = { .data = *buffer };
     size_t size = sizeof(struct printk_item_t);
     char *mem_ptr = k_malloc(size);
     __ASSERT_NO_MSG(mem_ptr != 0);
     memcpy(mem_ptr, &tx_data, size);
     k_fifo_put(&printk_fifo, mem_ptr);

The final iteration was a three thread CLI that had one thread printing what was typed onto the console and if ’s’ was pressed it suspended the second thread if ‘r’ was typed it would resume it. A second thread printed “KEVIN” and a third printed “HELLO WORLD”. These three threads would run into hard resource issues with timing so a mutex was created that prevent multiple sources from trying to transmit at the same time and three was a flag check added to see if the UART had finished clearing the fifo before adding more. At this point the CLI is ready to have commands that can do more implemented. This file is three_thread_cli_main.c.

We have learned how to lock hardware resources, pass data between different tasks, wait for flags to indicate that hardware has finished, use the west debugger, and set pins in the device tree so that resources are set correctly for zephyr architecture.

PASSING DATA
Fifo, lifo, stack, message queue, mailbox, pipes for passing data between threads or between an ISR and a thread
For more in-depth and examples https://docs.zephyrproject.org/1.9.0/kernel/data_passing/data_passing.html

FIFO
Any number of fifos can be defined. They are referenced by their memory address. 
Implemented as a simple linked list queue of items that have been added but not yet removed.
Size wise if the data is N bytes, the pointer too next piece of data is 4 bytes. So N+4 for the total data.
Data can be added by a thread or an ISR, only can be removed by a thread (the kernel allows an ISR to remove and item from a fifo, however the ISR must not wait for fifo empty)
	A thread can wait for a fifo to have something added in which case it is given to the highest priority thread that has waited longest
Multiple items can be added at once if they are in a linked list

LIFO
Any number of lifo can be defined. Each lifo is referenced by its memory address
Implemented as a simple linked list.
Same restrictions for adding and removing as a fifo

STACK
Any number of stacks can be defined. Referenced by its memory address.
A queue of 32-bit values, implemented with an array of 32-bit integers and must be aligned on a 4-byte boundary
Stack must have maximum quantity of values that can be queued in the array
Data can be added to a stack by a thread or an ISR the value is given to a waiting thread if there is one, or added tot the lifo queue. The kernel does not detect attempts to add a data value to a stack at the maximum quantity
Same restrictions for ISR removing. It can, but it must not wait if the stack is empty

MESSAGE QUEUE
Any number of message queues can be defined. Each message queue is referenced by its memory address
A message queue has: a ring buffer of data items sent but not yet received, a data item size in bytes, and a maximum quantity of data items that can be queued in the ring buffer
A message queues ring buffer must be aligned to an N-byte boundary, where N is a power of 2
A data item can be sent by a thread or ISR, the data item pointed at by sending thread is copied to a waiting thread. If no waiting thread exists, the item is copied to the message queue's ring buffer. The size of the data area being sent must equal the message queues data item size. (Since this copies should be passing pointers)
If a thread attempt to receive a data time when the ring buffer is empty the receiving thread can wait for a data item to be sent. Multiple cane wait of the same message queue


MAILBOX
Is an improved message queue. Allows threads to send and receive messages of any size synchronously or async 
Has a send queue and a receive queue
Allows threads to exchange messages
Each message may be received by only one thread. 


PIPES
Allows a thread to send a byte stream to another thread
Has a property for size. If size is 0 a pipe with no ring buffer is defined. 
Data can be synchronously or async.
Sync sent either in whole or part. Accepted data is copied tot he pipe's ring buffer or directly to the waiting read thread
Async can be sent with a memory block. Once accepted the memory block is freed and a semaphore can be used to show completion


Memory Protection
Zephyr gears towards microcontrollers with MPU(memory protection unit) hardware. Do support architecture with paged MMU(memory management unit)but 
it is  treated similar to an MPU with and identity page table. 

CONFIG_HW_STACK_PROTECTION is a feature which detects stack buffer overflows when the system si running in supercisor mde. this catches when the entire 
stack buffer has overflowed

k_xxxx_alloc_init() sets up k_xxxx with a its storage buffer allocated out of a resouce pool instead of a buffer provided by the user. k_free() frees the memory
 
The kernel ensure any user thread only has acess to its own stack buffe,   text, and read-only data. You want to minimize the number of boot-time
MPU regions becuase that effets your ability to grant additiona blocs of memory to a user thread.

Memory partitions consit of a memory address, size and access attributes. 

link with more detail espetially for partitions https://docs.zephyrproject.org/latest/reference/usermode/memory_domain.html


General Notes
To create projects I always would just change an existing project. Typically I would use Blinky or Hello World sample. Blinky is nice because your program is running the LED if your board has it will flash.

