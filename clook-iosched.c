/*
 * C-LOOK IO Scheduler
 *
 * For Kernel 4.13.9
 */

#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>


/* C-LOOK data structure. */
struct clook_data {
	struct list_head queue;
};

static void clook_merged_requests(struct request_queue *q, struct request *rq, struct request *next)
{
	list_del_init(&next->queuelist);
}

/* Esta função despacha o próximo bloco a ser lido. */
static int clook_dispatch(struct request_queue *q, int force)
{
	struct clook_data *nd = q->elevator->elevator_data;
	char direction = 'R';
	struct request *rq;

	rq = list_first_entry_or_null(&nd->queue, struct request, queuelist);
	if (rq) {
		list_del_init(&rq->queuelist);
		elv_dispatch_sort(q, rq);
		printk(KERN_EMERG "[C-LOOK] dsp %c %llu\n", direction, (unsigned long long)blk_rq_pos(rq));
		return 1;
	}

	return 0;
}

/* Esta função adiciona uma requisição ao disco em uma fila */
static void clook_add_request(struct request_queue *q, struct request *rq)
{
	struct clook_data *nd = q->elevator->elevator_data;
	char direction = 'R';

	list_add_tail(&rq->queuelist, &nd->queue);
	printk(KERN_EMERG "[C-LOOK] add %c %llu\n", direction, (unsigned long long)blk_rq_pos(rq));
}


/* Esta função inicializa as estruturas de dados necessárias para o escalonador */
static int clook_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct clook_data *nd;
	struct elevator_queue *eq;

	eq = elevator_alloc(q, e);
	if (!eq)
		return -ENOMEM;

	nd = kmalloc_node(sizeof(*nd), GFP_KERNEL, q->node);
	if (!nd) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}
	eq->elevator_data = nd;

	INIT_LIST_HEAD(&nd->queue);

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);

	return 0;
}


static void clook_exit_queue(struct elevator_queue *e)
{
	struct clook_data *nd = e->elevator_data;

	BUG_ON(!list_empty(&nd->queue));
	kfree(nd);
}


/* Estrutura de dados para os drivers de escalonamento de IO */
static struct elevator_type elevator_clook = {
	.ops.sq = {
		.elevator_merge_req_fn		= clook_merged_requests,
		.elevator_dispatch_fn		= clook_dispatch,
		.elevator_add_req_fn		= clook_add_request,
		.elevator_init_fn		= clook_init_queue,
		.elevator_exit_fn		= clook_exit_queue,
	},
	.elevator_name = "c-look",
	.elevator_owner = THIS_MODULE,
};

/* Inicialização do driver. */
static int __init clook_init(void)
{
	return elv_register(&elevator_clook);
}

/* Finalização do driver. */
static void __exit clook_exit(void)
{
	elv_unregister(&elevator_clook);
}

module_init(clook_init);
module_exit(clook_exit);

MODULE_AUTHOR("Henrique Mrás");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("C-LOOK IO scheduler skeleton");
