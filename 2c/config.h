// Dominik Matijaca 0036524568
#pragma once

#define DRIVER_NAME 	"shofer"

#define AUTHOR		"Dominik Matijaca"
#define LICENSE		"Dual BSD/GPL"

#include "defs.h"

/* Circular buffer */
struct buffer {
	struct kfifo fifo;
	spinlock_t key;	/* prevent parallel access */
};

/* Device driver */
struct shofer_dev {
	dev_t dev_no;		/* device number */
	struct cdev cdev;	/* Char device structure */
	struct buffer *buffer;	/* Pointer to buffer */
	int threads_active; /* Number of active threads */
	struct wait_queue_head readers_waiting; /* Wait queue for readers */
	struct wait_queue_head writers_waiting; /* Wait queue for writers */
};
