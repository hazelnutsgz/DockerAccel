#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/limits.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/jhash.h>
#include <linux/seccomp.h>
#include <linux/spinlock.h>

#define INIT_HASH_ARGUMENT 149
#define JHASH_INIT 10000004

#define ALERT_DRACO
//#define DEBUG_DRACO
#define METRICS_DRACO

#define ASOS 4
#define PRE_ALLOCATED_SYSCALL_TABLE_COUNT 10000

#define KMALLOC_FLAG GFP_KERNEL

extern int (*draco_checker)(int, struct pt_regs*);
extern int (*draco_checker_backup)(int, struct pt_regs*);

typedef struct k {
	int syscall_id;
	struct pt_regs* regs;
	#ifdef METRICS_DRACO
		pid_t process_id;
	#endif
	unsigned long argument_list[MAX_ARGUMENT_COUNT];
} key_type;

typedef struct hash_table_per_process_per_syscall {
	unsigned long table[ASOS*INIT_HASH_ARGUMENT][MAX_ARGUMENT_COUNT];
	uint8_t flag[ASOS*INIT_HASH_ARGUMENT];

	#ifdef METRICS_DRACO
		uint32_t per_syscall_conflict_count;
		uint32_t per_syscall_argument_count;
		uint32_t per_syscall_call_count;
	#endif

} hash_table_per_process_per_syscall_type;


typedef struct per_syscall_call_pool {
	hash_table_per_process_per_syscall_type pool_internal[PRE_ALLOCATED_SYSCALL_TABLE_COUNT];
	int cursor;
} per_syscall_call_pool_type;

typedef struct hash_table_per_process {
	hash_table_per_process_per_syscall_type* syscall_table[SYSCALL_COUNT];
		
	#ifdef METRICS_DRACO
		uint32_t per_process_argument_count;
		uint32_t per_process_syscall_count;
		uint32_t per_process_call_count;
		uint32_t per_process_conflict_count;
		pid_t process_id;
	#endif
} hash_table_per_process_type;

typedef struct process_node {
	struct process_node* next;
	struct task_struct* process;
} process_node_type;

typedef struct hash_table {
	process_node_type process_head; 
	per_syscall_call_pool_type pool;

	#ifdef METRICS_DRACO
		uint32_t total_process_count;
		uint32_t total_call_count;
		uint32_t total_conflict_count;
		uint32_t total_hit_count;
		uint32_t total_argument_count;
		uint32_t total_syscall_count;
	#endif

} hash_table_type;

hash_table_type hash_table;


int init_hash_table(hash_table_type* hash_table_internal);
inline unsigned long get_argument(struct pt_regs* regs, uint8_t index);
inline void arguments_hash_function(key_type* key); 
inline void init_key(key_type* k, int syscall_id, struct pt_regs* regs);
inline hash_table_per_process_per_syscall_type* get_item_from_pool(hash_table_type* hash_table);
int insert_value(hash_table_type* hash_table, key_type* key);
void free_hash_table(hash_table_type* hash_table);

DEFINE_SPINLOCK(draco_spinlock);

#ifdef METRICS_DRACO
static int syscall_number[SYSCALL_COUNT] = {0};
#endif
