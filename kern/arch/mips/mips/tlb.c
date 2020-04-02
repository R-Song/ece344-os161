
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
    int spl = splhigh();

    int valid_bit;
    u_int32_t entryhi, entrylo;

    TLB_Read(&entryhi, &entrylo, index);
    valid_bit = ( (entrylo & TLBLO_VALID) >> 9);
    assert(valid_bit == 0 || valid_bit == 1);

    splx(spl);
    return valid_bit;
}

void TLB_WriteValid(u_int32_t index, int value) 
{
    int spl = splhigh();

    assert(value == 0 || value == 1);
    u_int32_t entryhi, entrylo;

    TLB_Read(&entryhi, &entrylo, index);
    entrylo = ( (entrylo & (~TLBLO_VALID)) | (value << 9) ); /* Clear the valid bit, then set it to value */
    TLB_Write(entryhi, entrylo, index);

    splx(spl);
}

int TLB_ReadDirty(u_int32_t index)
{
    int spl = splhigh();

    int dirty_bit;
    u_int32_t entryhi, entrylo;

    TLB_Read(&entryhi, &entrylo, index);
    dirty_bit = ( (entrylo & TLBLO_DIRTY) >> 10);
    assert(dirty_bit == 0 || dirty_bit == 1);

    splx(spl);
    return dirty_bit;
}

void TLB_WriteDirty(u_int32_t index, int value)
{
   int spl = splhigh();

    assert(value == 0 || value == 1);
    u_int32_t entryhi, entrylo;

    TLB_Read(&entryhi, &entrylo, index);
    entrylo = ( (entrylo & (~TLBLO_DIRTY)) | (value << 10) ); /* Clear the valid bit, then set it to value */
    TLB_Write(entryhi, entrylo, index);

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
