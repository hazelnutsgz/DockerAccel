#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int malloc_count = 0;

struct pt_regs {
	unsigned long r15;
	unsigned long r14;
	unsigned long r13;
	unsigned long r12;
	unsigned long bp;
	unsigned long bx;
/* arguments: non interrupts/non tracing syscalls only save up to here*/
	unsigned long r11;
	unsigned long r10;
	unsigned long r9;
	unsigned long r8;
	unsigned long ax;
	unsigned long cx;
	unsigned long dx;
	unsigned long si;
	unsigned long di;
	unsigned long orig_ax;
/* end of arguments */
/* cpu exception frame or undefined */
	unsigned long ip;
	unsigned long cs;
	unsigned long flags;
	unsigned long sp;
	unsigned long ss;
/* top of stack page */
};

#define INIT_HASH_ARGUMENT 1009

#define MAX_SYSCALL_ID 400
#define MAX_COUNT_ARGUMENTS 6
#define MAX_PROCESS_COUNT 100

#define KMALLOC_FLAG __GFP_REPEAT

static int sys2arguments[MAX_SYSCALL_ID][MAX_COUNT_ARGUMENTS] = {{1, 3}, {1, 3}, {3}, {1}, {}, {1}, {}, {2, 3}, {1, 2, 3}, {1, 2, 3, 4, 5, 6}, {1, 2, 3}, {1, 2}, {1}, {1, 4}, {1, 4}, {}, {1, 2, 3}, {1, 3, 4}, {1, 3, 4}, {1, 3}, {1, 3}, {2}, {}, {1}, {}, {1, 2, 3, 4, 5}, {1, 2}, {1, 2}, {1, 2, 3}, {1, 2}, {1}, {1, 2}, {1}, {1, 2}, {}, {}, {1}, {1}, {1}, {}, {1, 3}, {1, 2, 3}, {1, 3}, {1}, {1, 3, 4, 6}, {1, 3, 4}, {1}, {1, 3}, {1, 2}, {1, 3}, {1, 2}, {1}, {1}, {1, 2, 3}, {1, 2, 3, 5}, {1, 2, 3}, {1, 2, 5}, {}, {}, {}, {1}, {1, 3}, {1, 2}, {}, {1, 2}, {1, 3}, {1, 2, 3, 4}, {1}, {1}, {1, 3}, {1, 3, 4}, {1, 2}, {1, 2, 3}, {1, 2}, {1}, {1}, {2}, {1, 2}, {1, 3}, {2}, {}, {1}, {}, {2}, {}, {2}, {}, {}, {2}, {3}, {2}, {1, 2}, {2, 3}, {1, 2, 3}, {2, 3}, {1}, {}, {1}, {1}, {}, {}, {1, 2, 3, 4}, {}, {1, 3}, {}, {1}, {1}, {}, {}, {1, 2}, {}, {}, {}, {1, 2}, {1, 2}, {1}, {1}, {1, 2, 3}, {}, {1, 2, 3}, {}, {1}, {1}, {1}, {1}, {}, {}, {2}, {4}, {1, 2}, {2}, {}, {}, {2, 3}, {}, {1}, {1}, {}, {1}, {1, 2, 3}, {1, 2}, {1, 2, 3}, {1}, {1}, {1, 2}, {1}, {1}, {1}, {1}, {1, 2}, {1, 2}, {1, 2}, {}, {}, {1, 3}, {}, {}, {1, 2, 3, 4, 5}, {2}, {}, {1}, {}, {}, {}, {}, {}, {}, {}, {}, {1, 2, 3}, {2}, {2}, {1}, {1, 2, 3}, {2}, {2}, {}, {}, {2, 4}, {1, 3}, {1}, {1}, {1, 4}, {}, {}, {}, {}, {1, 2, 3}, {4}, {4}, {1, 4}, {4}, {4}, {1, 4}, {3}, {3}, {1, 3}, {}, {}, {1}, {1, 2}, {}, {2, 3, 6}, {1, 2}, {1, 2}, {}, {1}, {}, {2, 3}, {2}, {}, {}, {1, 3}, {1}, {1, 2, 3}, {1, 3, 4}, {1, 2, 3, 4}, {1, 3}, {}, {}, {1, 3}, {1, 2, 3, 4}, {1}, {1}, {1}, {1}, {1}, {1}, {1}, {1}, {1}, {1}, {1, 3, 4}, {1, 2, 3}, {1, 2, 3}, {}, {}, {1, 2, 3, 5}, {1, 3}, {3, 4}, {2, 3}, {}, {1, 3, 4}, {1, 3}, {1}, {1}, {1, 2}, {1, 2, 4}, {4, 5}, {4}, {1, 2, 3, 4, 5}, {1, 2, 3}, {1, 2}, {}, {1, 3}, {1, 2}, {1, 2}, {1, 4}, {1, 3}, {1, 3}, {1, 3, 4}, {1}, {1}, {1}, {1, 3}, {1, 3}, {2}, {1, 4}, {1, 3}, {1, 3}, {1}, {2, 5}, {}, {2}, {1}, {1, 3}, {1, 2, 3}, {1, 2, 3}, {1, 3}, {1, 2}, {1}, {1, 3, 4, 6}, {1, 3}, {1}, {1}, {1, 2, 3}, {1}, {1}, {1}, {1, 3}, {1}, {}, {1, 2, 3}, {}, {}, {1, 3, 4, 5}, {1, 3, 4, 5}, {1, 2, 3}, {2, 3, 4}, {1, 3}, {}, {1, 3, 4}, {1, 2}, {1}, {1}, {1}, {1}, {1, 3}, {1, 2}, {}, {1, 3, 5}, {1, 3, 5}, {1, 2, 3, 4, 5}, {1}, {1}, {1, 3}, {1, 3}, {1}, {2}, {}, {1, 2, 3}, {1, 3}, {}, {1}, {1, 2}, {1, 3, 5}, {1, 2, 3, 4}, {2}, {1}};

typedef struct k {
	int syscall_id;
	struct pt_regs* regs;
	int process_id;
	int sign;
	int argument_hash_code;
	int argument_count;
	unsigned long argument_list[MAX_COUNT_ARGUMENTS];
} key_type;

typedef struct argument {
	int argument_count;
	unsigned long* argument_list;
	int sign; // 0 unused, 1 used, -1 invalid
} argument_type;


typedef struct hash_table_per_process_per_syscall {
	argument_type* argument_table; 
	int sign; // 0 unused, 1 used, -1 invalid
} hash_table_per_process_per_syscall_type;


typedef struct hash_table_per_process {
	hash_table_per_process_per_syscall_type syscall_table[MAX_SYSCALL_ID];
	// int sign; // 0 unused, 1 used, -1 invalid
	struct hash_table_per_process* next;
	int distinct_arguments;
	int distinct_syscalls;
} hash_table_per_process_type;

typedef struct hash_table {
	hash_table_per_process_type* for_gc; 
	int argument_table_size;
	int process_table_size;

	int distinct_process;
	int malloc_count;
	int call_count;
	int conflict_count;
} hash_table_type;

typedef struct curr {
	void* syscall_table;
} current_type;

current_type* current;

hash_table_type* new_hash_table(void) {

	printf ("[Draco:new_hash_table()]: Enter hash_table\n");
	hash_table_type* hash_table = \
		(hash_table_type*) malloc(sizeof(hash_table_type));

	if (hash_table == NULL) {
		printf ("[Draco:new_hash_table()]: hash_table kmalloc failed\n");
		return NULL;
	}

	printf ("[Draco:new_hash_table()]: AAAAA hash_table\n");

	hash_table->malloc_count = 0; 
	hash_table->conflict_count = 0;
	hash_table->call_count = 0;

	hash_table->malloc_count += 1;

	hash_table->argument_table_size = INIT_HASH_ARGUMENT;

	// This is the sentinel node.
	hash_table->for_gc = (hash_table_per_process_type*) malloc(sizeof(hash_table_per_process_type*));

	if (hash_table->for_gc == NULL) {
		printf  ("[Draco:new_hash_table()]: for_gc kmalloc failed\n");
		return NULL;
	}

	printf ("[Draco:new_hash_table()]: BBBBB hash_table\n");

	hash_table->malloc_count += 1;
	hash_table->for_gc->next = NULL;

	int index = 0;
	while (index < MAX_SYSCALL_ID) {
		hash_table->for_gc->syscall_table[index].sign = 1;
		++index;
	}
	
	printf  ("[Draco:new_hash_table()]:Hash table is initialized....\n");
	return hash_table;
}

unsigned long get_argument(struct pt_regs* regs, int index) {
	switch(index) {	
		case 0:
			return regs->di;
		case 1:
			return regs->si;
		case 2:
			return regs->dx;
		case 3:
			return regs->r10;
		case 4:
			return regs->r8;
		case 5:
			return regs->r9;
		default:
			return 0;
	};
}

void arguments_hash_function(
	key_type* key
	) {

	int* argument_positions = sys2arguments[key->syscall_id];

	while(key->argument_count < MAX_COUNT_ARGUMENTS && argument_positions[key->argument_count] != 0) {
		key->argument_count += 1;
	}

	int capacity_for_arguments = sizeof(unsigned long)*key->argument_count;

	int index = 0;
	while (index < key->argument_count) {
		key->argument_list[index] = get_argument(key->regs, argument_positions[index]-1); // -1 because of the bias.
		++index;
	}

	// int hash_code = jhash((void* )key->argument_list, capacity_for_arguments, 0) % INIT_HASH_ARGUMENT;
	int hash_code = random() % INIT_HASH_ARGUMENT;
	if (hash_code < 0) {
		hash_code = -hash_code;
	}
	printf  ("[Draco:arguments_hash_function]:%d\n", hash_code);
	key->argument_hash_code = hash_code;
}

void init_key(
	key_type* k,\
	int process_id,\
	int syscall_id,\
	struct pt_regs* regs
	) {
	
	k->argument_count = 0;
	k->process_id = process_id;
	k->syscall_id = syscall_id;
	k->regs = regs;
}

int insert_value(
	hash_table_type* hash_table, 
	key_type* key
	) {

	hash_table->call_count += 1;
	
	if (current->syscall_table == NULL) {

		//Allocate the space for current->syscall_table
		hash_table_per_process_type* allocated_process = (hash_table_per_process_type*) 
								malloc(sizeof(hash_table_per_process_type));
	
		if (allocated_process == NULL) {
			printf  ("[Draco:insert_value()] kmalloc failed....");
			return 1;
		}
		
		hash_table->malloc_count += 1;

		int index = 0;
		while (index < MAX_SYSCALL_ID) {
			allocated_process->syscall_table[index].sign = 0;
			++index;
		}
		
		allocated_process->distinct_arguments = 0;
		allocated_process->distinct_syscalls = 0;
		current->syscall_table = allocated_process;
	
		hash_table->distinct_process += 1;

		//Put the process to linked list for GC
		hash_table_per_process_type* temp = hash_table->for_gc->next;
		hash_table->for_gc->next = (hash_table_per_process_type*) current->syscall_table;
		hash_table->for_gc->next->next = temp;
	}

	hash_table_per_process_type* per_process = (hash_table_per_process_type*) current->syscall_table;

	if (per_process->syscall_table[key->syscall_id].sign != 1) {

		argument_type* argument_table = (argument_type*) malloc(\
				sizeof(argument_type) * hash_table->argument_table_size);
		
		if (argument_table == NULL) {
			printf  ("[Draco:new_hash_table_per_process_per_syscall()] malloc agument_table failed....");
			return 1;
		}
		hash_table->malloc_count += 1;
		per_process->syscall_table[key->syscall_id].sign = 1;
		per_process->syscall_table[key->syscall_id].argument_table = argument_table;
		per_process->distinct_syscalls += 1;
	}

	arguments_hash_function(key);
	argument_type* target = \
		per_process->syscall_table[key->syscall_id].argument_table + key->argument_hash_code;
	
	printf  ("[Draco:insert_value()] AAAA\n");

	if (target->sign != 1) {
		// Fill in the target item.
		target->argument_list = malloc(sizeof(unsigned long)*key->argument_count);
		memcpy(target->argument_list, key->argument_list, sizeof(unsigned long)*key->argument_count);
		target->argument_count = key->argument_count;
		// hash_table->distinct_arguments += 1;
		printf  ("[Draco:insert_value()] Insert a new value.\n");
		target->sign = 1;
		per_process->distinct_arguments += 1;
		return 0;
	} else {
		//The slot is occupied.
		printf  ("[Draco:insert_value()], comparing %d\n", key->argument_count);

		int index = 0;
		while (index < key->argument_count) {
			printf("key=%%lu, target=%%lu \n", key->argument_list[index], target->argument_list[index]);
			++index;
		}

		if (memcmp(key->argument_list, target->argument_list, \
			sizeof(unsigned long)*key->argument_count) == 0) {

			printf  ("[Draco:insert_value()] Try to insert the same value\n");
			return 1;
		}

		//TODO: Other, solve the confliction (Currently, we do nothing)
		printf  ("[Draco:insert_value()] Hash conflicting...\n");
		hash_table->conflict_count += 1;
		return 0;
	}

	return 1;

}

void free_hash_table_per_process_per_syscall(\
	hash_table_type* hash_table,
	hash_table_per_process_per_syscall_type* hash_table_per_process_per_syscall
	) {
	
	if (hash_table_per_process_per_syscall->sign != 1)
		return;

	int index = 0;
	while(index < INIT_HASH_ARGUMENT) {
		argument_type* t = hash_table_per_process_per_syscall->argument_table+index;
		printf ("Draco:free()%d\n", index);
		free(t->argument_list);
		++index;
	}
	free(hash_table_per_process_per_syscall->argument_table);
}

void free_hash_table_per_process(
	hash_table_type* hash_table,
	hash_table_per_process_type* hash_table_per_process
	) {

	if (hash_table_per_process == NULL) return;

	int index = 0;

	while (index < MAX_SYSCALL_ID) {
		free_hash_table_per_process_per_syscall(
			hash_table, hash_table_per_process->syscall_table+index);
		++index;
	}

	printf  ("free_hash_table_per_process:%d\n", index);
}

void free_hash_table(
	hash_table_type* hash_table
	) {

	if (hash_table == NULL) return;

	//Skip the sentinel.
	hash_table_per_process_type* traverse = hash_table->for_gc->next;

	int index = 0;
	while (traverse != NULL) {
		free_hash_table_per_process(hash_table, traverse);
		traverse = traverse->next;
		printf  ("Draco:free_hash_table%d\n", index);
		++index;
	}
}

int main(int argc, char const *argv[]) {
    
    hash_table_type* hash_table = new_hash_table();
    key_type key;

	hash_table_per_process_type* process_table[MAX_PROCESS_COUNT] = {NULL};

	struct pt_regs regs;

	current = malloc(sizeof(current_type));
	current->syscall_table = NULL;

	int test_counts = 0;
	int range = 300;

	while (test_counts < 1000000) {
		++test_counts;
		
		int syscall_id = rand() % MAX_SYSCALL_ID;
		int process_id = rand() % MAX_PROCESS_COUNT;
		current->syscall_table = process_table[process_id];
		regs.di = rand() % range; 
		regs.si = rand() % range;
		regs.dx = rand() % range;
		regs.r10 = rand() % range;
		regs.r8 = rand() % range;
 		regs.r9 = rand() % range;

		init_key(&key, process_id, syscall_id, &regs);
		
		insert_value(hash_table, &key);

		if (test_counts % 1000 == 0) {
			printf("%d produced...\n", test_counts);
		}
	
	}
	
	// free_hash_table(hash_table);
    return 0;
}
