
/*
 * Machine dependant TLB operations
 */
#include <types.h>
#include <lib.h>
#include <machine/tlb.h>
#include <machine/spl.h>

/*
 * To protect against corrupting the TLB, all operations are atomic.
 */

/* Read and write to ASID field in TLBHI */
u_int32_t TLB_ReadAsid(u_int32_t index) 
{
    int spl = splhigh();

    u_int32_t asid, entryhi, entrylo;

    TLB_Read(&entryhi, &entrylo, index);
    asid = ((entryhi & TLBHI_PID) >> 6); /* Apply mask then shift the 0's out */

    splx(spl);
    return asid;
}

void TLB_WriteAsid(u_int32_t index, u_int32_t asid) 
{
    int spl = splhigh();

    u_int32_t entryhi, entrylo;
    assert(asid < NUM_ASID);

    TLB_Read(&entryhi, &entrylo, index);
    entryhi = ( (entryhi & (~TLBHI_PID)) | (asid << 6) ); /* Clear old ASID bits then set to new ones */

    TLB_Write(entryhi, entrylo, index);

    splx(spl);
}

/* Read and write to Valid and Dirty bits in TLBLO */
int TLB_ReadValid(u_int32_t index) 
{
    u_int32_t ehi, elo;

    int spl = splhigh();

    TLB_Read(&ehi, &elo, index);
    if(elo & TLBLO_VALID) {
        splx(spl);
        return 1;
    }
    splx(spl);
    return 0;
}

void TLB_WriteValid(u_int32_t index, int value) 
{
    assert(value == 0 || value == 1);
    u_int32_t ehi, elo;

    int spl = splhigh();

    TLB_Read(&ehi, &elo, index);
    if(value == 1) 
        elo |= TLBLO_VALID;
    else {
        elo &= ~TLBLO_VALID;
    }
    TLB_Write(ehi, elo, index);

    splx(spl);
}

int TLB_ReadDirty(u_int32_t index)
{
    u_int32_t ehi, elo;

    int spl = splhigh();

    TLB_Read(&ehi, &elo, index);
    if(elo & TLBLO_DIRTY) {
        splx(spl);
        return 1;
    }
    splx(spl);
    return 0;
}

void TLB_WriteDirty(u_int32_t index, int value)
{  
    assert(value == 0 || value == 1);
    u_int32_t ehi, elo;

    int spl = splhigh();

    TLB_Read(&ehi, &elo, index);
    if(value == 1) 
        elo |= TLBLO_DIRTY;
    else {
        elo &= ~TLBLO_DIRTY;
    }
    TLB_Write(ehi, elo, index);

    splx(spl);
}

/* Complete TLB flush */
void TLB_Flush()
{
    int i, spl;
    spl = splhigh();

	for(i = 0; i < NUM_TLB; i++){
		TLB_Write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}
    
    splx(spl);
}

/*
 * TLB_Replace()
 * Implements our own replacement policy for TLB.
 */
int TLB_Replace(u_int32_t entryhi, u_int32_t entrylo)
{
    int spl, idx;
    u_int32_t ehi, elo;

    spl = splhigh();

    /* Replace invalid entries first */
    for(idx=0; idx<NUM_TLB; idx++) {
        TLB_Read(&ehi, &elo, idx);
		if (elo & TLBLO_VALID) {
			continue;
		}
        TLB_Write(entryhi, entrylo, idx);
        splx(spl);
        return idx;
    }

    /* Evict randomly... */
    TLB_Random(entryhi, entrylo);
    idx = TLB_Probe(entryhi, entrylo);
    assert(idx >= 0);

    splx(spl);
    return idx;
}

int TLB_FindEntry(u_int32_t entrylo)
{
    int spl = splhigh();

    int idx;
    u_int32_t ehi, elo;

    for(idx=0; idx<NUM_TLB; idx++) {
        TLB_Read(&ehi, &elo, idx);
		if(entrylo == (elo & TLBLO_PPAGE) ) {
            return idx;
        }
    }
    return -1;

    splx(spl);
}

void TLB_Invalidate(int idx)
{
    int spl = splhigh();

	TLB_Write(TLBHI_INVALID(idx), TLBLO_INVALID(), idx);
    
    splx(spl);
}


/*
 * TLB_Stat()
 * Print debugging information for TLB
 */
void TLB_Stat()
{
    int spl = splhigh();

    int i;
    u_int32_t ehi, elo;
    kprintf("\n");
    for(i=0; i<NUM_TLB; i++) {
        TLB_Read(&ehi, &elo, i);
        if(elo & TLBLO_VALID) {
            int valid = TLB_ReadValid(i);
            int dirty = TLB_ReadDirty(i);
            kprintf("ENTRYNO: %02d  -----  EHI: 0x%08x  |  ELO: 0x%08x  |  VALID: %d  |  DIRTY: %d\n", i, ehi, elo, valid, dirty);
        }
    }
    kprintf("\n");
    splx(spl);
}




