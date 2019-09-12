#include "magic-instruction.h"
#include "draco_module.h"

int init_hash_table(hash_table_type* hash_table_internal) {
	#ifdef DEBUG_DRACO
		printk (KERN_DEBUG "[Draco:init_hash_table()]: Enter hash_table\n");
	#endif

	memset(hash_table_internal, 0, sizeof(hash_table_type));
	hash_table_internal->process_head.next = NULL;
	
	#ifdef DEBUG_DRACO
		printk (KERN_DEBUG "[Draco:init_hash_table()]:Hash table is initialized....\n");
	#endif
	

	return 0;
}


inline unsigned long get_argument(struct pt_regs* regs, uint8_t index) {
	switch(index) {	
		case 1:
			return regs->di;
		case 2:
			return regs->si;
		case 3:
			return regs->dx;
		case 4:
			return regs->r10;
		case 5:
			return regs->r8;
		case 6:
			return regs->r9;
		default:
			return 0;
	};
}

inline void init_key(
	key_type* k,\
	int syscall_id,\
	struct pt_regs* regs
	) {
	
	k->syscall_id = syscall_id;
	k->regs = regs;
}

inline hash_table_per_process_per_syscall_type* get_item_from_pool(
	hash_table_type* hash_table) {
		
	per_syscall_call_pool_type* p = &(hash_table->pool);
	int bias = p->cursor;
	
	if (p->cursor == PRE_ALLOCATED_SYSCALL_TABLE_COUNT) {
		return NULL;
	}

	++(p->cursor);

	return p->pool_internal+bias;
}

int insert_value(
	hash_table_type* hash_table, 
	key_type* key
	) {
	
	int index = 0;
	hash_table_per_process_type* per_process;
	hash_table_per_process_per_syscall_type** sys_table;	
	uint8_t* argument_positions;
	int argument_count;	
	u32 hash_code;

	#ifdef DEBUG_DRACO
		int syscall;
		int j;
	#endif	

	u32 entry_position;
	unsigned long (*tb)[MAX_ARGUMENT_COUNT];
	uint8_t* flag;

	if (hash_table == NULL) {
		#ifdef ALERT_DRACO
			printk (KERN_WARNING "[Draco:insert_value()]:initialization failed, don't use it....");
		#endif
		return 0;
	}

	#ifdef DEBUG_DRACO
		printk("[Draco:insert_value()]: Begin insert_value()");
	#endif

	#ifdef METRICS_DRACO
		spin_lock(&draco_spinlock);
		hash_table->total_call_count += 1;
		spin_unlock(&draco_spinlock);
	#endif
	
	if (current->seccomp.draco_hook == NULL) {
		process_node_type* temp;
		hash_table_per_process_type* allocated_process;
		#ifdef DEBUG_DRACO
			spin_lock(&draco_spinlock);
			printk("[Draco:insert_value()]:" 
				"Begin allocating the space for the new process");
			printk("[Draco:insert_value()]:Print profile info, totally %d syscalls\n", current->seccomp.draco_count);
			
			for (index = 0; index < current->seccomp.draco_count; ++index) {
				syscall = current->seccomp.bit_map[index];
				printk("syscall=%d:", syscall);
				for (j = 0; j < current->seccomp.argument_count_table[syscall]; ++j) {
					printk("%d", current->seccomp.sys2arguments[syscall][j]);
				}
				printk("\n");
			}
			spin_unlock(&draco_spinlock);
		#endif
		//Allocate the space for current->draco_hook
		allocated_process = (hash_table_per_process_type*) 
			kmalloc(sizeof(hash_table_per_process_type), KMALLOC_FLAG);
		
		if (unlikely(allocated_process == NULL)) {

			#ifdef ALERT_DRACO
				printk (KERN_WARNING 
					"[Draco:insert_value()]:allocated_process kmalloc failed....");
			#endif

			return 0;
		}


		memset(allocated_process, 0, sizeof(hash_table_per_process_type));

		spin_lock(&draco_spinlock);
		#ifdef METRICS_DRACO
			hash_table->total_process_count += 1;
			allocated_process->per_process_argument_count = 0;
			allocated_process->per_process_call_count = 0;
			allocated_process->per_process_conflict_count = 0;
			allocated_process->per_process_syscall_count = 0;
			allocated_process->process_id = current->pid;
			spin_unlock(&draco_spinlock);
		#endif
		current->seccomp.draco_hook = allocated_process;		

		temp = hash_table->process_head.next;
		hash_table->process_head.next = kmalloc(sizeof(process_node_type), KMALLOC_FLAG);
		if (hash_table->process_head.next == NULL) {
			#ifdef KERN_WARNING
				printk("[draco:insert_value]: Failed to malloc\n");
			#endif
			spin_unlock(&draco_spinlock);
			return 0;
		}
		
		hash_table->process_head.next->process = current;
		hash_table->process_head.next->next = temp;

		spin_unlock(&draco_spinlock);
	}
	per_process = (hash_table_per_process_type*) current->seccomp.draco_hook;

	#ifdef METRICS_DRACO
		spin_lock(&draco_spinlock);
		per_process->per_process_call_count += 1;
		spin_unlock(&draco_spinlock);
	#endif
	
	sys_table = per_process->syscall_table;

	if (sys_table[key->syscall_id] == NULL) {
		#ifdef DEBUG_DRACO
			printk("[Draco:insert_value()]:allocate the space for a new syscall");
		#endif
		sys_table[key->syscall_id] = get_item_from_pool(hash_table);
		
		if (sys_table[key->syscall_id] == NULL) {
			
			#ifdef ALERT_DRACO
				printk (KERN_WARNING "malloc agument_table failed....");
			#endif
			
			return 0;
		}

		#ifdef METRICS_DRACO
			spin_lock(&draco_spinlock);
			per_process->per_process_syscall_count += 1;
			spin_unlock(&draco_spinlock);
		#endif
	}

	#ifdef METRICS_DRACO
		spin_lock(&draco_spinlock);
		sys_table[key->syscall_id]->per_syscall_call_count += 1;
		spin_unlock(&draco_spinlock);
	#endif
	
	// Fill in argument
  	argument_positions = current->seccomp.sys2arguments[key->syscall_id];
  	argument_count = current->seccomp.argument_count_table[key->syscall_id];
	
  	for (index = 0; index < argument_count; ++index) {
  		key->argument_list[index] = get_argument(
  			key->regs, argument_positions[index]);
  	}	

	hash_code = jhash((void* )key->argument_list,
		sizeof(unsigned long)*argument_count, JHASH_INIT) % INIT_HASH_ARGUMENT;

	#ifdef DEBUG_DRACO
		printk (KERN_DEBUG "[Draco:insert_value()]:hash_code=%d\n", hash_code);
	#endif

	entry_position = hash_code*ASOS;
	tb = sys_table[key->syscall_id]->table;
	flag = sys_table[key->syscall_id]->flag;
	
	for (index = 0; index < ASOS && flag[entry_position+index] == 1; ++index) {
		#ifdef DEBUG_DRACO
			printk("[Draco:insert_value()]:Begin to compare arguments.....");
		#endif

		if (memcmp(key->argument_list, 
			tb[entry_position+index], 
				argument_count*sizeof(unsigned long)) == 0) {
			// Hit it.

			#ifdef DEBUG_DRACO
				printk("[Draco:insert_value()]: hit !!");
			#endif

			#ifdef METRICS_DRACO
				spin_lock(&draco_spinlock);
				hash_table->total_hit_count += 1;
				spin_unlock(&draco_spinlock);
			#endif
			
			return 1;
		}
	}

	#ifdef DEBUG_DRACO
		printk("[Draco:insert_value()]: No hit~");
	#endif

	// No hit, insert or discard

	#ifdef METRICS_DRACO
		spin_lock(&draco_spinlock);

		per_process->per_process_argument_count += 1;
		sys_table[key->syscall_id]->per_syscall_argument_count += 1;
		hash_table->total_argument_count += 1;

		spin_unlock(&draco_spinlock);
	#endif

	/// Conflict Discard
	if (index == ASOS) {
		
		#ifdef METRICS_DRACO
			spin_lock(&draco_spinlock);

			hash_table->total_conflict_count += 1;
			per_process->per_process_conflict_count += 1;
			sys_table[key->syscall_id]->per_syscall_conflict_count += 1;
			spin_unlock(&draco_spinlock);
		#endif

		return 0;
	}
	// New entry, Insert
	flag[entry_position+index] = 1;

	memcpy(tb[entry_position+index], key->argument_list, 
		argument_count*sizeof(unsigned long));

	return 0;
}

void free_hash_table(hash_table_type* hash_table) {
	process_node_type* traverse;
	traverse = hash_table->process_head.next;
	
	#ifdef METRICS_DRACO
		printk(KERN_INFO "[Draco:free_hash_table]:\n"
			"total_hit_count = %d\n" 
			"total_call_count =%d\n"
			"total_argument_count = %d\n"
			"total_conflict_count = %d\n"
			"total_syscall_count = %d\n"
			"total_process_count = %d\n\n",
			hash_table->total_hit_count, 
			hash_table->total_call_count,
			hash_table->total_argument_count,
			hash_table->total_conflict_count,
			hash_table->total_syscall_count,
			hash_table->total_process_count
		);
	#endif

	while (traverse != NULL) {
		process_node_type* cur = NULL;
		hash_table_per_process_type* per_process;
		per_process = traverse->process->seccomp.draco_hook;
		
		kfree(traverse->process->seccomp.draco_hook);
		traverse->process->seccomp.draco_hook = NULL; // Deattach the task_struct
		cur = traverse;
		traverse = traverse->next;
		kfree(cur);
	}

	printk(KERN_INFO "Finish the draco free..............\n");
}

static int __seccomp_filter_handler(int this_syscall, struct pt_regs *regs) {

	key_type key;
	if (this_syscall < 0 || this_syscall >= SYSCALL_COUNT) {

		#ifdef ALERT_DRACO
			printk("Draco:__seccomp_filter_handler():"
				"weirld syscall=%d\n"
				"di=%lu\n"
				"si=%lu\n"
				"dx=%lu\n"
				"r10=%lu\n"
				"r8=%lu\n"
				"r9=%lu\n",
					this_syscall,
					regs->di,
					regs->si,
					regs->dx,
					regs->r10,
					regs->r8,
					regs->r9
			);
		#endif

		return 0;
	}


	#ifdef METRICS_DRACO 
		if (syscall_number[this_syscall] == 0) {
			hash_table.total_syscall_count += 1;
		}
		++syscall_number[this_syscall];
	#endif
		
	init_key(&key, this_syscall, regs);
	return insert_value(&hash_table, &key);
}

static int __init draco_init(void) {
	
	draco_checker = __seccomp_filter_handler;

	init_hash_table(&hash_table);

	return 0;
}

static void __exit draco_exit(void) {
	draco_checker = draco_checker_backup;
	free_hash_table(&hash_table);
}

module_init(draco_init)
module_exit(draco_exit)
MODULE_LICENSE("GPL");
