/* SPDX-License-Identifier: GPL-3.0-only */
/* Bryo: the memory every track's sound shares (docs/bryo-architecture.md, "Memory by usage").
 *
 * Instead of a fixed tape per track, there's one pool of chunks, and each track takes what it uses: its tape grows
 * as it records, GRAIN's live buffer takes a few bars while GRAIN is on, a dry SYNTH track takes nothing. A chunk
 * is 16 of the tape's blocks (4,096 samples, 186 ms at 22.05 kHz) with each block's decoder state and peak, so any
 * chunk holds tape-format sound and a list of chunks reads like one tape (tape.c's views carry the list).
 *
 * Chunks live in two places: most in the pool, the rest in main RAM's spare room (the pool alone can't fit them).
 * mem_at() hides that.
 *
 * Who decides: the main loop allocates and frees, and nothing else may: the audio ISR only reads and writes the
 * chunks an owner's list hands it, below the list's published length, and a WAV arriving over USB (TIMER5's
 * usb_poll, which can cut into the main loop) only takes chunks the main loop set aside for it (vdisk.c). Taking a
 * chunk away is: shorten the list (the length first), then free it. That's safe to reuse at once because the main loop never runs inside the audio ISR: a block the ISR is
 * rendering finishes before the main loop's next step, and every later block sees the shorter list. The ISR keeps
 * no pointer into a chunk from one block to the next (readers copy what they decode; the tape's writer re-checks
 * the list before it commits). Who to take from when nothing is free is tape.c's call (tape_steal): this file keeps
 * the books. Integer only. */

#define MEM_CB 16u                   /* tape blocks per chunk */
#define MEM_NC_POOL 108u             /* chunks in the pool (228 KB). 2026-10-13, from playing it: 20 s of sound shared by the
                                      * four tracks is enough, and the room it gives back is worth more: main RAM's 24
                                      * chunks (50 KB, the tightest region) back for features, and the pool's top for
                                      * the audio code in RAM (app.ld .hot). (128 until 2026-10-11, then 126, 119, and
                                      * 24 more in main RAM: 152, 150, 143 in all) */
#define MEM_NC MEM_NC_POOL           /* 108: 20.1 s of tape-format sound in all */
#define MEM_FREE 0xFFu               /* owner: nobody */
#define MEM_ZC 0xFEu                 /* in a list, for a chunk: the shared silent one (mem_zero: no memory of its own;
                                      * 2026-10-19, mem_hollow) */
/* owners: a track's tape (MEM_TAPE + t), its GRAIN buffer (MEM_GRAIN + t), a WAV arriving over USB (MEM_IMPORT),
 * its RIPPLE's strings (MEM_RESO + t), its SPACE's delay (MEM_SPACE + t), the shared rooms A and B (MEM_ROOM + r) */
/* (SPARE: kept ready for a WAV, vdisk.c) */
enum { MEM_TAPE = 0, MEM_GRAIN = 4, MEM_IMPORT = 8, MEM_SPARE = 9, MEM_RESO = 10, MEM_SPACE = 14, MEM_ROOM = 18,
       MEM_WT = 20, MEM_GFX = 24 };  /* (MEM_WT + t: SYNTH's wavetable, two chunks while WAVE is WT: synth.c;
                                      * MEM_GFX + t: GRAIN's WARBLE or VINYL line, a chunk while on: grainfx.c) */

typedef struct {
    uint8_t data[MEM_CB][128];       /* 16 blocks of 256 samples, 4 bits each */
    int16_t pred[MEM_CB];            /* each block's decoder state at its start */
    uint8_t idx[MEM_CB];
    uint8_t peak[MEM_CB];            /* each block's peak, for the screen */
} mem_chunk_t;

static mem_chunk_t mem_pool[MEM_NC_POOL] __attribute__((section(".pool")));
static uint8_t mem_owner[MEM_NC];
/* Silence shares one chunk (2026-10-19, "the quietest sounds just drop completely when cpu and ram gets overloaded":
 * of that, the memory: silence only, never quiet sound). A chunk of a tape or a GRAIN buffer that holds nothing but
 * silence (every byte 0: zero nibbles from a zero state decode to exactly 0) is given back to the pool, and its place
 * in the list says MEM_ZC: this one, all zeros, in flash. Readers read it as any chunk and hear the same silence;
 * nothing ever writes it (tape_commit checks), and before a tape is written the main loop puts real chunks there
 * again (tape_solid, when REC is armed). Who looks, and when: mem_sweep (tape.c), a few chunks a pass. Tapes only:
 * a GRAIN buffer records all the time and would want its silence back within seconds (tape.c says why) */
static const mem_chunk_t mem_zero = {{{0}}, {0}, {0}, {0}};
static uint32_t mem_hollowed;                        /* chunks given back for their silence, ever (the tests) */

static inline __attribute__((always_inline)) mem_chunk_t *mem_at(uint32_t c)
{
    return c < MEM_NC_POOL ? &mem_pool[c] : (mem_chunk_t *)(void *)(uintptr_t)&mem_zero;
}

static void mem_init(void)
{
    uint32_t c;
    for (c = 0; c < MEM_NC; c++)
        mem_owner[c] = MEM_FREE;
}

/* main loop: a free chunk for owner o, silent (zero nibbles from a zero state), or -1 when none is free */
static int32_t mem_alloc(uint32_t o)
{
    uint32_t c;
    for (c = 0; c < MEM_NC; c++)
        if (mem_owner[c] == MEM_FREE) {
            memset(mem_at(c), 0, sizeof(mem_chunk_t));
            mem_owner[c] = (uint8_t)o;
            return (int32_t)c;
        }
    return -1;
}

/* main loop: chunk c back to the pool (after its owner's list has stopped handing it to the ISR) */
static void mem_free(uint32_t c)
{
    if (c < MEM_NC)
        mem_owner[c] = MEM_FREE;
}

/* chunk c changes hands (a WAV's chunks becoming a tape's) */
static void mem_give(uint32_t c, uint32_t o)
{
    if (c < MEM_NC && mem_owner[c] != MEM_FREE)
        mem_owner[c] = (uint8_t)o;
}

/* main loop: chunk c holds only silence (every byte 0) */
static int mem_silent(uint32_t c)
{
    const uint32_t *w = (const uint32_t *)(const void *)mem_at(c);
    uint32_t i;
    if (c >= MEM_NC)
        return 0;
    for (i = 0; i < sizeof(mem_chunk_t) / 4u; i++)
        if (w[i])
            return 0;
    return 1;
}

/* main loop: list entry e's chunk back to the pool if it holds only silence, the entry saying MEM_ZC from then on
 * (first, then the chunk freed: a reader between the two reads either, the same silence). The list's owner must not
 * be writing there (mem_sweep's call). Returns 1 when a chunk came free */
static int mem_hollow(uint8_t *e)
{
    uint32_t c = *e;
    if (!mem_silent(c))
        return 0;
    *e = (uint8_t)MEM_ZC;
    RING_PUBLISH();
    mem_free(c);
    mem_hollowed++;
    return 1;
}

/* chunks owner o holds (MEM_FREE: the free ones) */
static uint32_t mem_count(uint32_t o)
{
    uint32_t c, n = 0;
    for (c = 0; c < MEM_NC; c++)
        n += mem_owner[c] == o;
    return n;
}
