#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

struct clook_request {
	struct request *req;
	struct list_head queuelist;
};

struct clook_data {
	struct list_head queue;
};

static void clook_merged_requests(struct request_queue *q, struct request *rq, struct request *next)
{
	list_del_init(&next->queuelist);
}

static int clook_dispatch(struct request_queue *q, int force)
{
	struct clook_data *nd = q->elevator->elevator_data;
	char direction = 'R';
	struct clook_request *clook_rq;

	clook_rq = list_first_entry_or_null(&nd->queue, struct clook_request, queuelist);
	if (clook_rq) {
		struct request *rq = clook_rq->req;
		list_del_init(&clook_rq->queuelist);
		elv_dispatch_sort(q, rq);
		printk(KERN_EMERG "[C-LOOK] dsp %c %llu\n", direction, (unsigned long long)blk_rq_pos(rq));
		kfree(clook_rq);
		return 1;
	}
	return 0;
}

static void clook_add_request(struct request_queue *q, struct request *rq)
{
	struct clook_data *nd = q->elevator->elevator_data;
	char direction = 'R';

	struct clook_request *clook_rq = kmalloc(sizeof(*clook_rq), GFP_KERNEL);
	if (!clook_rq) {
		printk(KERN_EMERG "[C-LOOK] failed to allocate memory\n");
		return;
	}

	clook_rq->req = rq;

	struct clook_request *curr, *next;

	list_for_each_entry_safe(curr, next, &nd->queue, queuelist) {
		if (blk_rq_pos(rq) < blk_rq_pos(curr->req)) {
			list_add_tail(&clook_rq->queuelist, &curr->queuelist);
			printk(KERN_EMERG "[C-LOOK] add %c %llu\n", direction, (unsigned long long)blk_rq_pos(rq));
			return;
		}
	}

	list_add_tail(&clook_rq->queuelist, &nd->queue);
	printk(KERN_EMERG "[C-LOOK] add %c %llu\n", direction, (unsigned long long)blk_rq_pos(rq));
}

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

static int __init clook_init(void)
{
	return elv_register(&elevator_clook);
}

static void __exit clook_exit(void)
{
	elv_unregister(&elevator_clook);
}

module_init(clook_init);
module_exit(clook_exit);

MODULE_AUTHOR("Henrique Mrás");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("C-LOOK IO scheduler");
