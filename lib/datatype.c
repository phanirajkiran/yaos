/*
 * "[...] Sincerity (comprising truth-to-experience, honesty towards the self,
 * and the capacity for human empathy and compassion) is a quality which
 * resides within the laguage of literature. It isn't a fact or an intention
 * behind the work [...]"
 *
 *             - An introduction to Literary and Cultural Theory, Peter Barry
 *
 *
 *                                                   o8o
 *                                                   `"'
 *     oooo    ooo  .oooo.    .ooooo.   .oooo.o     oooo   .ooooo.
 *      `88.  .8'  `P  )88b  d88' `88b d88(  "8     `888  d88' `88b
 *       `88..8'    .oP"888  888   888 `"Y88b.       888  888   888
 *        `888'    d8(  888  888   888 o.  )88b .o.  888  888   888
 *         .8'     `Y888""8o `Y8bod8P' 8""888P' Y8P o888o `Y8bod8P'
 *     .o..P'
 *     `Y8P'                   Kyunghwan Kwon <kwon@yaos.io>
 *
 *  Welcome aboard!
 */

#include <types.h>
#include <error.h>
#include <kernel/lock.h>
#include <bitops.h>

void fifo_init(struct fifo *q, void *queue, size_t size)
{
	q->size  = size;
	q->buf   = queue;
	q->front = q->rear = 0;
}

void fifo_flush(struct fifo *q)
{
	q->front = q->rear = 0;
}

bool fifo_empty(struct fifo *q)
{
	return q->front == q->rear;
}

/* NOTE: the size of queue must be power of 2 when using byte type because it
 * takes performace advantage replacing modulo operation with bit operation. */
int fifo_getb(struct fifo *q)
{
	size_t size;
	unsigned int pos;
	char *buf;
	int val;

	if (!q || !q->buf)
		return -EINVAL;

	size = 1 << (fls(q->size) - 1); /* in case of not power of 2 */
	buf = q->buf;

	do {
		pos = __ldrex(&q->front);

		if (pos == *(volatile typeof(q->rear) *)&q->rear)
			return -ENOENT; /* empty */

		val = (typeof(val))buf[pos];
		pos = (pos + 1) & (size - 1);
	} while (__strex(pos, &q->front));

	return val;
}

int fifo_putb(struct fifo *q, int val)
{
	size_t size;
	unsigned int pos;
	char *buf;

	if (!q || !q->buf)
		return -EINVAL;

	size = 1 << (fls(q->size) - 1); /* in case of not power of 2 */
	buf = q->buf;

	do {
		pos = __ldrex(&q->rear);

		if (((pos + 1) & (size - 1)) ==
				*(volatile typeof(q->front) *)&q->front)
			return -ENOSPC; /* no more room */

		buf[pos] = (typeof(*buf))val;
		pos = (pos + 1) & (size - 1);
	} while (__strex(pos, &q->rear));

	return 0;
}

int fifo_getw(struct fifo *q)
{
	unsigned int pos, val, *buf;

	if (!q || !q->buf)
		return -EINVAL;

	buf = q->buf;

	do {
		pos = __ldrex(&q->front);

		if (pos == *(volatile typeof(q->rear) *)&q->rear)
			return -ENOENT; /* empty */

		val = buf[pos];
		pos = (pos + 1) % (q->size >> 2);
	} while (__strex(pos, &q->front));

	return (int)val;
}

int fifo_putw(struct fifo *q, int val)
{
	unsigned int mod, pos, *buf;

	if (!q || !q->buf)
		return -EINVAL;

	mod = q->size >> 2;
	buf = q->buf;

	do {
		pos = __ldrex(&q->rear);

		if (((pos + 1) % mod) == *(volatile typeof(q->front) *)&q->front)
			return -ENOSPC; /* no more room */

		buf[pos] = (typeof(*buf))val;
		pos = (pos + 1) % mod;
	} while (__strex(pos, &q->rear));

	return 0;
}

int fifo_get(struct fifo *q, int type_size)
{
	unsigned int mod, idx, pos, val;
	char *buf;
	register int i;

	if (!q || !q->buf)
		return -EINVAL;

	mod = q->size / type_size;
	buf = q->buf;
	val = 0;

	do {
		pos = __ldrex(&q->front);
		idx = pos * type_size;

		if (pos == *(volatile typeof(q->rear) *)&q->rear)
			return -ENOENT; /* empty */

		for (i = 0; i < type_size; i++)
			val = val | ((typeof(val))buf[idx + i] << (i << 3));

		pos = (pos + 1) % mod;
	} while (__strex(pos, &q->front));

	return (int)val;
}

int fifo_put(struct fifo *q, int val, int type_size)
{
	unsigned int mod, pos, idx;
	char *buf;
	register int i;

	if (!q || !q->buf)
		return -EINVAL;

	mod = q->size / type_size;
	buf = q->buf;

	do {
		pos = __ldrex(&q->rear);
		idx = pos * type_size;

		if (((pos + 1) % mod) == *(volatile typeof(q->front) *)&q->front)
			return -ENOSPC; /* no more room */

		for (i = 0; i < type_size; i++) {
			buf[idx + i] = (typeof(*buf))val;
			val = (unsigned int)val >> 8;
		}
		pos = (pos + 1) % mod;
	} while (__strex(pos, &q->rear));

	return 0;
}
