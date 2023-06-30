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

static int clook_dispatch(struct request_queue *q, int force)
{
	struct clook_data *nd = q->elevator->elevator_data;
	char direction = 'R';
	struct request *rq, *next_rq;
	sector_t cur_sector = -1;
	struct list_head sec_queue;  // Fila secundária

	// Inicializa a fila secundária
	INIT_LIST_HEAD(&sec_queue);

	// Percorre a fila principal para encontrar as requisições com setores menores que o setor atual
	list_for_each_entry_safe(rq, next_rq, &nd->queue, queuelist) {
		if (cur_sector == -1) {
			cur_sector = blk_rq_pos(rq);
		}

		// Verifica se o setor da requisição é menor que o setor atual
		if (blk_rq_pos(rq) < cur_sector) {
			printk(KERN_EMERG "[C-LOOK] sec %c %lu\n", direction, blk_rq_pos(rq));
			list_move_tail(&rq->queuelist, &sec_queue);
		} else {
			break;  // Interrompe o laço quando encontra a primeira requisição com setor maior ou igual ao setor atual
		}
	}

	// Se a fila principal estiver vazia, adiciona a fila secundária à fila principal
	if (list_empty(&nd->queue) && !list_empty(&sec_queue)) {
		list_splice_tail_init(&sec_queue, &nd->queue);
	}

	// Despacha a primeira requisição da fila principal se ela não estiver vazia
	if (!list_empty(&nd->queue)) {
		rq = list_entry(nd->queue.next, struct request, queuelist);
		list_del_init(&rq->queuelist);
		elv_dispatch_sort(q, rq);
		cur_sector = blk_rq_pos(rq) + blk_rq_sectors(rq);
		printk(KERN_EMERG "[C-LOOK] dsp %c %lu\n", direction, blk_rq_pos(rq));
		return 1;
	}

	return 0;
}




static void clook_add_request(struct request_queue *q, struct request *rq)
{
	struct clook_data *nd = q->elevator->elevator_data;
	char direction = 'R';

	printk(KERN_EMERG "[C-LOOK] add %c %lu\n", direction, blk_rq_pos(rq));

	/* Ordena as requisições de acordo com a política C-LOOK */
	struct request *curr, *next;

	list_for_each_entry_safe(curr, next, &nd->queue, queuelist) {
		if (blk_rq_pos(rq) < blk_rq_pos(curr)) {
			list_add_tail(&rq->queuelist, &curr->queuelist);
			return;
		}
	}

	list_add_tail(&rq->queuelist, &nd->queue);
}


static int clook_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct clook_data *nd;
	struct elevator_queue *eq;

	/* Implementação da inicialização da fila (queue).
	 *
	 * Use como exemplo a inicialização da fila no driver noop-iosched.c
	 *
	 */

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

	/* Implementação da finalização da fila (queue).
	 *
	 * Use como exemplo o driver noop-iosched.c
	 *
	 */
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

MODULE_AUTHOR("Sérgio Johann Filho");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("C-LOOK IO scheduler skeleton");
