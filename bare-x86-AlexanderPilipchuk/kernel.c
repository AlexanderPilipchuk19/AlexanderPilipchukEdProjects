typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

enum {
    VGA_WIDTH = 80,
    VGA_HEIGHT = 25,
    VGA_ATTRIBUTE = 0x07,
    IDT_SIZE = 256,
    TIMER_VECTOR = 8,
    YIELD_VECTOR = 0x81,
    PIC1_COMMAND = 0x20,
    PIC1_DATA = 0x21,
    PIC2_DATA = 0xA1,
    PIC_EOI = 0x20,
    TASK_SYSTEM = 0,
    TASK_USER = 1,
    MAX_PROCESSES = 8,
    STACK_WORDS = 1024,
    SYSTEM_TIME_SLICE_TICKS = 5,
    TRAP_REASON_TIMER = 0,
    TRAP_REASON_YIELD = 1
};

struct context {
    int state;
};


struct idt_entry {
    u16 offset_low;
    u16 selector;
    u8 zero;
    u8 type_attributes;
    u16 offset_high;
} __attribute__((packed));


struct idtr_value {
    u16 limit;
    u32 base;
} __attribute__((packed));


struct trap_frame {
    u32 edi;
    u32 esi;
    u32 ebp;
    u32 esp_placeholder;
    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;
    u32 eip;
    u32 cs;
    u32 eflags;
};

typedef int (*process_fn)(struct context *ctx, int argc, char **args);


struct task {
    u32 *saved_esp;
    u32 kind;
    u32 slot;
    u32 is_active;
    u32 is_finished;
    process_fn entry;
    int argc;
    char **args;
    struct context ctx;
    int exit_code;
    u32 stack[STACK_WORDS];
};

static volatile u16 *const vga_buffer = (volatile u16 *)0xB8000;
static struct idt_entry idt[IDT_SIZE];
static unsigned int cursor_row;
static unsigned int cursor_col;

static struct task system_task;
static struct task user_tasks[MAX_PROCESSES];
static unsigned int user_process_count;
static struct task *current_task;
static struct task *system_return_task;

static volatile unsigned int timer_counter;
static unsigned int print_counter;
static unsigned int user_ticks_since_system;
static unsigned int active_user_count;
static u16 kernel_code_selector;

extern void timer_vector_8_stub(void);
extern void yield_vector_stub(void);
extern void restore_context(u32 *saved_esp) __attribute__((noreturn));

static char *empty_args[] = { 0 };

void yield(void);
static void process_exit(int exit_code) __attribute__((noreturn));
static int demo_process_a(struct context *ctx, int argc, char **args);
static int demo_process_b(struct context *ctx, int argc, char **args);
static int demo_process_c(struct context *ctx, int argc, char **args);


process_fn processes[] = {
    demo_process_a,
    demo_process_b,
    demo_process_c,
    0
};


static inline void outb(u16 port, u8 value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void disable_interrupts(void) {
    __asm__ volatile ("cli");
}

static inline u16 read_cs(void) {
    u16 value;

    __asm__ volatile ("mov %%cs, %0" : "=r"(value));
    return value;
}


static inline void cpu_halt(void) {
    __asm__ volatile ("hlt");
}


static u16 make_vga_cell(char ch) {
    return (u16)(((u16)VGA_ATTRIBUTE << 8) | (u8)ch);
}


static void clear_row(unsigned int row) {
    unsigned int col;

    for (col = 0; col < VGA_WIDTH; ++col) {
        vga_buffer[row * VGA_WIDTH + col] = make_vga_cell(' ');
    }
}

static void scroll_if_needed(void) {
    unsigned int row;
    unsigned int col;

    if (cursor_row < VGA_HEIGHT) {
        return;
    }

    for (row = 1; row < VGA_HEIGHT; ++row) {
        for (col = 0; col < VGA_WIDTH; ++col) {
            vga_buffer[(row - 1) * VGA_WIDTH + col] =
                vga_buffer[row * VGA_WIDTH + col];
        }
    }

    clear_row(VGA_HEIGHT - 1);
    cursor_row = VGA_HEIGHT - 1;
}

static void new_line(void) {
    cursor_col = 0;
    ++cursor_row;
    scroll_if_needed();
}


static void putc(char ch) {
    if (ch == '\n') {
        new_line();
        return;
    }

    vga_buffer[cursor_row * VGA_WIDTH + cursor_col] = make_vga_cell(ch);
    ++cursor_col;

    if (cursor_col >= VGA_WIDTH) {
        new_line();
    }
}


static void set_cursor(unsigned int row, unsigned int col) {
    if (row >= VGA_HEIGHT) {
        row = VGA_HEIGHT - 1;
    }

    if (col >= VGA_WIDTH) {
        col = VGA_WIDTH - 1;
    }

    cursor_row = row;
    cursor_col = col;
}


void reset(void) {
    unsigned int row;

    cursor_row = 0;
    cursor_col = 0;

    for (row = 0; row < VGA_HEIGHT; ++row) {
        clear_row(row);
    }
}


void puts(const char *s) {
    while (*s != '\0') {
        putc(*s);
        ++s;
    }
}


void putnum(int v) {
    char digits[12];
    unsigned int value;
    unsigned int count = 0;

    if (v < 0) {
        putc('-');
        value = (unsigned int)(-(v + 1)) + 1U;
    } else {
        value = (unsigned int)v;
    }

    if (value == 0U) {
        putc('0');
        return;
    }

    while (value != 0U) {
        digits[count] = (char)('0' + (value % 10U));
        value /= 10U;
        ++count;
    }

    while (count != 0U) {
        --count;
        putc(digits[count]);
    }
}


void set_idtr(u64 offset, unsigned int size) {
    struct idtr_value idtr;

    idtr.base = (u32)offset;
    idtr.limit = (u16)(size - 1U);

    __asm__ volatile ("lidt %0" : : "m"(idtr));
}


void set_handler(unsigned int vector, void (*handler)(void)) {
    u32 address = (u32)handler;
    struct idt_entry *entry = &idt[vector];

    entry->offset_low = (u16)(address & 0xFFFFU);
    entry->selector = kernel_code_selector;
    entry->zero = 0;
    entry->type_attributes = 0x8E;
    entry->offset_high = (u16)(address >> 16);
}

/* программная задержка из задания 1, выполняет много nop, чтобы подождать примерно заданное число условных миллисекунд */
void heavy_sleep(unsigned int millisecs) {
    volatile unsigned int ms;
    volatile unsigned int step;

    for (ms = 0; ms < millisecs; ++ms) {
        for (step = 0; step < 50000U; ++step) {
            __asm__ volatile ("nop");
        }
    }
}

/* Небольшая локальная пауза для демонстрационных процессов, чтобы пользовательские процессы не меняли state слишком быстро и их поведение было удобнее наблюдать на экране */
static void small_pause(unsigned int steps) {
    volatile unsigned int step;

    for (step = 0; step < steps; ++step) {
        __asm__ volatile ("nop");
    }
}


static void pic_enable_timer_only(void) {
    outb(PIC1_DATA, 0xFE);
    outb(PIC2_DATA, 0xFF);
}

static void clear_idt(void) {
    unsigned int vector;

    for (vector = 0; vector < IDT_SIZE; ++vector) {
        idt[vector].offset_low = 0;
        idt[vector].selector = 0;
        idt[vector].zero = 0;
        idt[vector].type_attributes = 0;
        idt[vector].offset_high = 0;
    }
}


static void prepare_task_stack(struct task *task, void (*bootstrap)(void)) {
    u32 *sp = task->stack + STACK_WORDS;

    *--sp = 0x00000202U;
    *--sp = (u32)kernel_code_selector;
    *--sp = (u32)bootstrap;
    *--sp = 0U;
    *--sp = 0U;
    *--sp = 0U;
    *--sp = 0U;
    *--sp = 0U;
    *--sp = 0U;
    *--sp = 0U;
    *--sp = 0U;

    task->saved_esp = sp;
}


static int demo_process_a(struct context *ctx, int argc, char **args) {
    (void)argc;
    (void)args;

    while (1) {
        ctx->state += 1;
        small_pause(50000U);
        yield();
    }

    return 0;
}


static int demo_process_b(struct context *ctx, int argc, char **args) {
    int value = 100;

    (void)argc;
    (void)args;

    while (1) {
        value += 3;
        ctx->state = value;
        small_pause(80000U);
        yield();
    }

    return 0;
}


static int demo_process_c(struct context *ctx, int argc, char **args) {
    (void)argc;
    (void)args;

    while (1) {
        ctx->state += 1;

        if (ctx->state >= 2000) {
            return -7;
        }

        small_pause(5000000U);
        yield();
    }

    return 0;
}

/* Переключение по yield() выполняется через отдельное программное прерывание */
void yield(void) {
    __asm__ volatile ("int $0x81" : : : "memory");
}

/* Проверка, что пользовательская задача еще участвует в планировании */
static int is_runnable_user(const struct task *task) {
    return task->kind == TASK_USER && task->is_active != 0U && task->is_finished == 0U;
}

/* Ищем первого еще не завершившегося пользовательского процесса */
static struct task *first_runnable_user(void) {
    unsigned int index;

    for (index = 0; index < user_process_count; ++index) {
        if (is_runnable_user(&user_tasks[index])) {
            return &user_tasks[index];
        }
    }

    return 0;
}

/* Выбираем следующую пользовательскую задачу среди незавершённых  */
static struct task *next_runnable_user_after_slot(unsigned int slot) {
    unsigned int offset;

    if (user_process_count == 0U) {
        return 0;
    }

    for (offset = 1; offset <= user_process_count; ++offset) {
        struct task *candidate = &user_tasks[(slot + offset) % user_process_count];

        if (is_runnable_user(candidate)) {
            return candidate;
        }
    }

    return 0;
}

/* Печать состояния всех пользовательских процессов выполняет только системный процесс */
static void draw_process_states(void) {
    unsigned int index;
    unsigned int rows_used = 0;
    unsigned int finished_count = 0;

    reset();
    puts("#");
    putnum((int)print_counter);
    puts("\n\n");

    for (index = 0; index < user_process_count; ++index) {
        set_cursor(2U + (index / 4U), (index % 4U) * 20U);
        puts("p");
        putnum((int)index);
        puts(": ");

        if (user_tasks[index].is_finished != 0U) {
            putnum(user_tasks[index].exit_code);
            ++finished_count;
        } else {
            putnum(user_tasks[index].ctx.state);
        }
    }

    if (user_process_count != 0U) {
        rows_used = (user_process_count + 3U) / 4U;
    }

    set_cursor(2U + rows_used + 1U, 0U);
    puts("Finished:");

    if (finished_count != 0U) {
        unsigned int printed = 0;

        puts(" ");

        for (index = 0; index < user_process_count; ++index) {
            if (user_tasks[index].is_finished == 0U) {
                continue;
            }

            if (printed != 0U) {
                puts(", ");
            }

            putnum((int)index);
            ++printed;
        }
    }

    ++print_counter;
}

/* Правила переключения при добровольном yield */
static struct task *schedule_after_yield(void) {
    struct task *next_task;

    if (current_task == 0) {
        if (active_user_count == 0U) {
            return &system_task;
        }

        return first_runnable_user();
    }

    if (current_task->kind == TASK_SYSTEM) {
        if (active_user_count == 0U) {
            return &system_task;
        }

        next_task = system_return_task;

        if (next_task == 0 || !is_runnable_user(next_task)) {
            next_task = first_runnable_user();
        }

        system_return_task = 0;
        return next_task;
    }

    if (active_user_count == 0U) {
        return &system_task;
    }

    if (active_user_count == 1U) {
        system_return_task = current_task;
        return &system_task;
    }

    next_task = next_runnable_user_after_slot(current_task->slot);

    if (next_task == 0) {
        return &system_task;
    }

    return next_task;
}

/* Правила переключения при прерывании */
static struct task *schedule_after_timer(void) {
    if (current_task == 0) {
        return &system_task;
    }

    if (current_task->kind != TASK_USER || active_user_count == 0U || !is_runnable_user(current_task)) {
        return current_task;
    }

    ++user_ticks_since_system;

    if (user_ticks_since_system < SYSTEM_TIME_SLICE_TICKS) {
        return current_task;
    }

    user_ticks_since_system = 0;
    system_return_task = current_task;
    return &system_task;
}

/* Общая точка принятия решения после таймера или yield */
u32 *handle_trap(struct trap_frame *frame, u32 reason) {
    struct task *next_task;

    if (current_task != 0) {
        current_task->saved_esp = (u32 *)frame;
    }

    if (reason == TRAP_REASON_TIMER) {
        ++timer_counter;
        outb(PIC1_COMMAND, PIC_EOI);
        next_task = schedule_after_timer();
    } else {
        next_task = schedule_after_yield();
    }

    current_task = next_task;
    return next_task->saved_esp;
}

/* Завершаем пользовательский процесс и исключаем его из дальнейшего планирования */
static void process_exit(int exit_code) {
    struct task *next_task;

    disable_interrupts();

    current_task->is_active = 0U;
    current_task->is_finished = 1U;
    current_task->exit_code = exit_code;

    if (active_user_count != 0U) {
        --active_user_count;
    }

    if (system_return_task == current_task) {
        system_return_task = 0;
    }

    user_ticks_since_system = 0;
    next_task = schedule_after_yield();
    current_task = next_task;
    restore_context(next_task->saved_esp);
}


static void user_task_bootstrap(void) __attribute__((noreturn));
static void user_task_bootstrap(void) {
    struct task *task = current_task;
    int exit_code;

    exit_code = task->entry(&task->ctx, task->argc, task->args);
    process_exit(exit_code);
}


static void system_task_bootstrap(void) __attribute__((noreturn));
static void system_task_bootstrap(void) {
    while (1) {
        draw_process_states();

        if (active_user_count == 0U) {
            cpu_halt();
            continue;
        }

        yield();
    }
}


static void init_tasks(void) {
    unsigned int index = 0;

    system_task.kind = TASK_SYSTEM;
    system_task.slot = 0;
    system_task.is_active = 1U;
    system_task.is_finished = 0U;
    system_task.entry = 0;
    system_task.argc = 0;
    system_task.args = empty_args;
    system_task.ctx.state = 0;
    system_task.exit_code = 0;
    prepare_task_stack(&system_task, system_task_bootstrap);

    while (index < MAX_PROCESSES && processes[index] != 0) {
        user_tasks[index].kind = TASK_USER;
        user_tasks[index].slot = index;
        user_tasks[index].is_active = 1U;
        user_tasks[index].is_finished = 0U;
        user_tasks[index].entry = processes[index];
        user_tasks[index].argc = 0;
        user_tasks[index].args = empty_args;
        user_tasks[index].ctx.state = 0;
        user_tasks[index].exit_code = 0;
        prepare_task_stack(&user_tasks[index], user_task_bootstrap);
        ++index;
    }

    user_process_count = index;
    active_user_count = index;
}


void entry(void) {
    clear_idt();
    disable_interrupts();
    reset();
    kernel_code_selector = read_cs();

    init_tasks();

    
    set_handler(TIMER_VECTOR, timer_vector_8_stub);
    set_handler(YIELD_VECTOR, yield_vector_stub);
    set_idtr((u64)(u32)&idt[0], sizeof(idt));
    pic_enable_timer_only();

    if (user_process_count == 0U) {
        current_task = &system_task;
    } else {
        current_task = &user_tasks[0];
    }

    restore_context(current_task->saved_esp);
}
