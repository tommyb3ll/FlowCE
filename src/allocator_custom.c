#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/lcd.h>
#include <debug.h>

// This allocator uses the 2nd part of the VRAM as another area for heap allocations, providing 76KB more memory,
// but requires you to only use the first part of the VRAM for the LCD configured in 8bpp.

// When passed to malloc, returns how much free memory there's left
#define MAGIC_SIZE_QUERY_FREEMEM 0xFFFFFF

// minimal size (avoid blocks that are too small)
#define MALLOC_MINSIZE 6

static unsigned int freeslotpos(unsigned int n) {
    if (n == 0) {
        /**
         * this is what the original code returns, although this is probably
         * not the ideal value to return when n == 0
         */
        return 31;
    }
    return __builtin_ctz(n);
}

typedef struct char2_ {
    char c1, c2, c3, c4;
} char2_t;

typedef struct char3_ {
    char c1, c2, c3, c4, c5, c6, c7, c8, c9, c10;
} char3_t;

typedef struct char6_ {
    char c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15, c16;
} char6_t;

typedef struct __attribute__((packed)) block
{
    struct block* ptr;
    size_t size;
} __attribute__((packed)) block_t;

extern uint8_t __heapbot[];
extern uint8_t __heaptop[];

// assumes that heap2_ptrend<=lcd_Ram
static uintptr_t heap2_ptr = (uintptr_t)__heapbot;
static uintptr_t heap2_ptrend = (uintptr_t)__heaptop;

#define ALLOC2 (12 * INT24_WIDTH)
static unsigned int freeslot2[ALLOC2 / INT24_WIDTH] = {
    0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
    0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
};
#define ALLOC3 (12 * INT24_WIDTH)
static unsigned int freeslot3[ALLOC3 / INT24_WIDTH] = {
    0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
    0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
};
#define ALLOC6 (12 * INT24_WIDTH)
static unsigned int freeslot6[ALLOC6 / INT24_WIDTH] = {
    0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
    0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
};

#define LCD_SIZE_8BPP (LCD_WIDTH * LCD_HEIGHT)
#define ALLOC_START ((unsigned char*)lcd_Ram + LCD_SIZE_8BPP)
#define ALLOC_END ((unsigned char*)lcd_Ram + LCD_SIZE)

static char2_t * const tab2 = (char2_t*)(ALLOC_START);
static char3_t * const tab3 = (char3_t*)(ALLOC_START + ALLOC2 * sizeof(char2_t));
static char6_t * const tab6 = (char6_t*)(ALLOC_START + ALLOC2 * sizeof(char2_t) + ALLOC3 * sizeof(char3_t));

static uintptr_t heap_ptr = (uintptr_t)(ALLOC_START + ALLOC2 * sizeof(char2_t) + ALLOC3 * sizeof(char3_t) + ALLOC6 * sizeof(char6_t));
static uintptr_t heap_ptrend = (uintptr_t)ALLOC_END;

static block_t _alloc_base, _alloc2_base;
// these are 0 initialized, pointing to a chained list of freed pointers

// Out of memory: the reserve kept for this moment (main.cc) is given back and giac is asked to
// stop (oom_interrupt sets ctrl_c), so the calculation unwinds and frees what it built; the
// console then says "Out of memory". Without a reserve: NULL if oom_soft, else abort.
#define OOM_RESERVE 8192
void* oom_reserve;
volatile char oom_hit, oom_soft;
void oom_interrupt(void);
void _custom_free(void* ptr);

void* _custom_malloc(size_t alloc_size)
{
    // dbg_printf("[malloc] %zu bytes\n", alloc_size);

    if (alloc_size == 0)
        return NULL;

    // This allows callers to query how much free memory there is left
    if (alloc_size == MAGIC_SIZE_QUERY_FREEMEM)
        return (void*)((heap_ptrend - heap_ptr) + (heap2_ptrend - heap2_ptr));
    if (alloc_size == MAGIC_SIZE_QUERY_FREEMEM - 1) { // the freed blocks (total, low 16 bits: largest / 2)
        size_t total = 0, big = 0;
        for (block_t* b = _alloc_base.ptr; b; b = b->ptr) { total += b->size; if (b->size > big) big = b->size; }
        for (block_t* b = _alloc2_base.ptr; b; b = b->ptr) { total += b->size; if (b->size > big) big = b->size; }
        return (void*)total;
    }
    if (alloc_size == MAGIC_SIZE_QUERY_FREEMEM - 2) { // the largest freed block
        size_t big = 0;
        for (block_t* b = _alloc_base.ptr; b; b = b->ptr) if (b->size > big) big = b->size;
        for (block_t* b = _alloc2_base.ptr; b; b = b->ptr) if (b->size > big) big = b->size;
        return (void*)big;
    }

    if (alloc_size <= sizeof(char6_t))
    {
        if (tab2 && alloc_size <= sizeof(char2_t))
        {
            for (unsigned int i = 0; i < ALLOC2 / INT24_WIDTH; ++i)
            {
                if (freeslot2[i] == 0)
                {
                    continue;
                }
                const unsigned int pos = freeslotpos(freeslot2[i]);
                freeslot2[i] &= ~(1 << pos);
                // dbg_printf("allocfast2 %p %p\n", tab2, tab2 + i * INT24_WIDTH + pos);
                return (void*)(tab2 + i * INT24_WIDTH + pos);
            }
        }
        if (tab3 && alloc_size <= sizeof(char3_t))
        {
            for (unsigned int i = 0; i < ALLOC3 / INT24_WIDTH; ++i)
            {
                if (freeslot3[i] == 0)
                {
                    continue;
                }
                const unsigned int pos = freeslotpos(freeslot3[i]);
                freeslot3[i] &= ~(1 << pos);
                // dbg_printf("allocfast3 %p %p\n", tab3, tab3 + i * INT24_WIDTH + pos);
                return (void*)(tab3 + i * INT24_WIDTH + pos);
            }
        }
        if (tab6 && alloc_size <= sizeof(char6_t))
        {
            for (unsigned int i = 0; i < ALLOC6 / INT24_WIDTH; ++i)
            {
                if (freeslot6[i] == 0)
                {
                    continue;
                }
                const unsigned int pos = freeslotpos(freeslot6[i]);
                freeslot6[i] &= ~(1 << pos);
                // dbg_printf("allocfast6 %p %p\n", tab6, tab6 + i * INT24_WIDTH + pos);
                return (void*)(tab6 + i * INT24_WIDTH + pos);
            }
        }
    }

    block_t* q;
    block_t* r;

    /* add size of block header to real size */
    size_t size = alloc_size + sizeof(block_t);
    if (size < alloc_size)
        return NULL;

    // dbg_printf("alloc heap %p ptr=%p size=%zu\n",&_alloc_base,_alloc_base.ptr,_alloc_base.size);

    for (block_t* p = &_alloc_base; (q = p->ptr); p = q)
    {
        if (q->size >= size)
        {
            if (q->size <= size + sizeof(block_t) + MALLOC_MINSIZE)
            {
                // dbg_printf("heap recycle full blocsize=%zu size=%zu\n", q->size, size);
                p->ptr = q->ptr;
            }
            else
            {
                // dbg_printf("heap recycle partial blocsize=%zu size=%zu\n", q->size, size);
                q->size -= size;
                q = (block_t*)(((uint8_t*)q) + q->size);
                q->size = size;
            }

            return q + 1;
        }
    }

    /* compute next heap pointer */
    if (heap_ptr + size < heap_ptr || heap_ptr + size >= (uintptr_t)heap_ptrend)
    {
        // dbg_printf("alloc heap2 %p ptr=%p size=%zu\n",&_alloc2_base,_alloc2_base.ptr,_alloc2_base.size);
        for (block_t* p = &_alloc2_base; (q = p->ptr); p = q)
        {
            if (q->size >= size)
            {
                if (q->size <= size + sizeof(block_t))
                {
                    // dbg_printf("heap2 recycle full blocsize=%zu size=%zu\n", q->size, size);
                    p->ptr = q->ptr;
                }
                else
                {
                    // dbg_printf("heap2 recycle partial blocsize=%zu size=%zu\n", q->size, size);
                    q->size -= size;
                    q = (block_t*)(((uint8_t*)q) + q->size);
                    q->size = size;
                }

                return q + 1;
            }
        }
        if (
            (heap2_ptr + size < heap2_ptr) ||
            (heap2_ptr + size >= (uintptr_t)heap2_ptrend)
        ) {
            if (oom_reserve && alloc_size <= OOM_RESERVE - 16) {
                void* res = oom_reserve;
                oom_reserve = NULL;
                oom_hit = 1;
                _custom_free(res);
                oom_interrupt();
                return _custom_malloc(alloc_size);
            }
            if (oom_soft)
                return NULL;
            lcd_Control = 0b100100101101; // TI-OS default
            abort();
            return NULL;
        }
        r = (block_t*)heap2_ptr;
        if (size < MALLOC_MINSIZE)
            size = MALLOC_MINSIZE;
        r->size = size;
        heap2_ptr = heap2_ptr + size;
        return r + 1;
    }

    if (size < MALLOC_MINSIZE)
        size = MALLOC_MINSIZE;

    r = (block_t*)heap_ptr;
    r->size = size;
    heap_ptr = heap_ptr + size;
    return r + 1;
}

void _custom_free(void* ptr)
{
    // dbg_printf("[free] %p\n", ptr);

    if (ptr == NULL)
    {
        return;
    }

    if (
        ((size_t)ptr >= (size_t)&tab2[0]) &&
        ((size_t)ptr < (size_t)&tab2[ALLOC2])
    ) {
        const unsigned int pos = ((size_t)ptr - ((size_t)&tab2[0])) / sizeof(char2_t);
        // dbg_printf("deletefast2 %p pos=%i\n", ptr, pos);
        freeslot2[pos / INT24_WIDTH] |= (1 << (pos % INT24_WIDTH));
        return;
    }
    if (
        ((size_t)ptr >= (size_t)&tab3[0]) &&
        ((size_t)ptr < (size_t)&tab3[ALLOC3])
    ) {
        const unsigned int pos = ((size_t)ptr - ((size_t)&tab3[0])) / sizeof(char3_t);
        // dbg_printf("deletefast3 %p pos=%i\n", ptr, pos);
        freeslot3[pos / INT24_WIDTH] |= (1 << (pos % INT24_WIDTH));
        return;
    }
    if (
        ((size_t)ptr >= (size_t)&tab6[0]) &&
        ((size_t)ptr < (size_t)&tab6[ALLOC6])
    ) {
        const unsigned int pos = ((size_t)ptr - ((size_t)&tab6[0])) / sizeof(char6_t);
        // dbg_printf("deletefast6 %p pos=%i\n", ptr, pos);
        freeslot6[pos / INT24_WIDTH] |= (1 << (pos % INT24_WIDTH));
        return;
    }

    block_t* q = (block_t*)ptr - 1;

    block_t* p = ((uintptr_t)ptr <= heap2_ptrend) ? &_alloc2_base : &_alloc_base;
    // dbg_printf("free ptr=%p heap_end=%p p=%p pptr=%p psize=%zu\n",ptr,heap_ptrend,p,p->ptr,p->size);

    for (; p->ptr && p->ptr < q; p = p->ptr) {}
    // p next pointer in the free-ed chaine list, p->ptr,
    // is 0 or is the first pointer >= q
    // (this means that p is before q)
    if ((uint8_t*)p->ptr == ((uint8_t*)q) + q->size)
    {
        // concatenate q and p next pointer
        q->size += p->ptr->size;
        q->ptr = p->ptr->ptr;
        // dbg_printf("free concatenate blocsize=%zu\n", q->size);
    }
    else
    {
        // insert in chained list: get q next cell from p next cell
        q->ptr = p->ptr;
        // dbg_printf("free add block blocsize=%zu\n", q->size);
    }
    // check if we can concatenate p and q
    if (((uint8_t*)p) + p->size == (uint8_t*)q)
    {
        // yes
        p->size += q->size;
        p->ptr = q->ptr;
    }
    else
    {
        // no, update next pointer for p
        p->ptr = q;
    }
}

void* _custom_realloc(void* ptr, const size_t size)
{
    // dbg_printf("[realloc] %p for %zu bytes\n", ptr, size);

    if (ptr == NULL)
    {
        return _custom_malloc(size);
    }

    if (
        (((size_t)ptr >= (size_t)&tab2[0]) && ((size_t)ptr < (size_t)&tab2[ALLOC2])) ||
        (((size_t)ptr >= (size_t)&tab3[0]) && ((size_t)ptr < (size_t)&tab3[ALLOC3])) ||
        (((size_t)ptr >= (size_t)&tab6[0]) && ((size_t)ptr < (size_t)&tab6[ALLOC6]))
    ) {
        // ok
    }
    else
    {
        const block_t* h = (block_t*)((uint8_t*)ptr - sizeof(block_t));
        if (h->size >= (size + sizeof(block_t)))
        {
            return ptr;
        }
    }

    void* p = _custom_malloc(size);
    if (p)
    {
        memcpy(p, ptr, size);
        _custom_free(ptr);
    }

    return p;
}
