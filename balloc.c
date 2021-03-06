/*
 * linux/fs/nvmm/balloc.c
 * 
 * Copyright (C) 2013 College of Computer Science,
 * Chonqing University
 *
 */

#include <linux/io.h>
#include <linux/mm.h>
#include "nvmm.h"

#define NVMM_PAGE_MASK 0xfff

struct nvmm_inode * nvmm_new_consistency_inode(struct super_block *sb)
{
	struct nvmm_super_block *nsb = nvmm_get_super(sb);
	struct nvmm_inode *ni = NULL;
	unsigned long offset, ino_phy;
	int errval = 0;
	ino_t ino = 0;

	if(nsb->s_free_consistency_count){
		offset = le64_to_cpu(nsb->s_free_consistency_inode_hint);
		ino_phy = nvmm_offset_to_phys(sb, offset);
		ino = nvmm_get_inodenr(sb, ino_phy);
		//nvmm_info("The inode %lx\n" , ino);
		ni = nvmm_get_inode(sb, ino);
//		nvmm_info("allocating inode %lx as a consistency inode\n", (unsigned long)le64_to_cpu(ni->consistency_inode_ino));
		nsb->s_free_consistency_inode_hint = ni->i_next_inode_offset;
		ni->i_next_inode_offset = 0;
		le64_add_cpu(&nsb->s_free_consistency_count, -1);
	}else{
		errval = -ENOSPC;
		return ERR_PTR(errval);
	}

	return ni;
	
}

void nvmm_free_consistency_inode(struct super_block *sb, struct nvmm_inode *ni)
{
	struct nvmm_super_block *nsb;
	unsigned long offset, inode_phy;

	nsb = nvmm_get_super(sb);
	inode_phy = nvmm_get_inode_phy_addr(sb, ni->consistency_inode_ino);
//	nvmm_info("reclaiming consistency inode %lx\n", (unsigned long)le64_to_cpu(ni->consistency_inode_ino));
	offset = nvmm_phys_to_offset(sb, inode_phy);
	ni->i_next_inode_offset = nsb->s_free_consistency_inode_hint;
	nsb->s_free_consistency_inode_hint = offset;
	le64_add_cpu(&nsb->s_free_consistency_count, 1);
}

int nvmm_new_consistency_block(struct nvmm_super_block *ns, void *sbi_virt_addr, phys_addr_t *physaddr)
{
	unsigned long blocksize;
	char *nb;
	unsigned long *temp;
	int errval = 0, i = 0;
	phys_addr_t offset, next;

	blocksize = le32_to_cpu(ns->s_blocksize);

	offset = le64_to_cpu(ns->s_free_block_start);
	next = offset;
	for(i = 0; i < 512; ++i){
		nb = (char *) sbi_virt_addr + next;
		temp = (unsigned long *)nb;
		next = *temp;
		le64_add_cpu(&ns->s_free_block_count, -1);
	}
	ns->s_free_block_start = cpu_to_le64(next);
	*physaddr = offset + (phys_addr_t)sbi_virt_addr;
	return errval;
}

void nvmm_init_consistency_inode(struct nvmm_super_block *ns, void *sbi_virt_addr)
{
	void *start_addr = le64_to_cpu(ns->s_free_inode_start) + sbi_virt_addr;
	unsigned long consistency_num_inodes = le64_to_cpu(ns->s_consistency_count);
	unsigned long next_offset = le64_to_cpu(ns->s_free_inode_start) + NVMM_INODE_SIZE;
	struct nvmm_inode *temp = NULL;
	unsigned long inode_ino = NVMM_ROOT_INO + 1;
	ns->s_free_consistency_inode_hint = ns->s_free_inode_start;
	while(consistency_num_inodes > 0){
		phys_addr_t phys;
		temp = (struct nvmm_inode *)start_addr;
		temp->i_next_inode_offset = cpu_to_le64(next_offset);
		temp->consistency_inode_ino = cpu_to_le64(inode_ino);
		temp->transaction_flag = cpu_to_le64(TRANSACTION_CHECKPOINTING);
		temp->origin_inode_ino = cpu_to_le64(0);
		temp->write_length = cpu_to_le64(0);
		temp->start_write_position = cpu_to_le64(0);
		nvmm_new_consistency_block(ns, sbi_virt_addr, &phys);
		temp->i_pg_addr = phys;
		//nvmm_info("The address:%lx\n", (unsigned long)sbi_virt_addr);
		//nvmm_info("The inode %lx address:%lx\n", inode_ino, (unsigned long)temp->i_pg_addr);
		start_addr += NVMM_INODE_SIZE;
		next_offset += NVMM_INODE_SIZE;
		consistency_num_inodes--;
		inode_ino++;
	}
	ns->s_free_inode_start = cpu_to_le64(next_offset);
	ns->s_free_inode_count -= ns->s_consistency_count;
}


 
void nvmm_init_free_inode_list_offset(struct nvmm_super_block *ps,void *sbi_virt_addr)
{
	void *start_addr = le64_to_cpu(ps->s_free_inode_start) + sbi_virt_addr;
	//void *mytest_addr = start_addr;
	unsigned int inode_count = le64_to_cpu(ps->s_free_inode_count);
	struct nvmm_inode *temp = NULL;
	unsigned long next_offset = le64_to_cpu(ps->s_free_inode_start) + NVMM_INODE_SIZE;
	while(inode_count > 0) {
		//nvmm_info("The inode %u address:%lx,offset:%lu\n", inode_count, (unsigned long)start_addr, next_offset);
		temp = (struct nvmm_inode *)start_addr;
		temp->i_pg_addr = cpu_to_le64(next_offset);
		//nvmm_info("the inode temp offset is :%llu\n",le64_to_cpu(temp->i_pg_addr));
        start_addr += NVMM_INODE_SIZE;
        next_offset += NVMM_INODE_SIZE;
        inode_count--;
    }
	//init free consistency inodes list
	nvmm_init_consistency_inode(ps, sbi_virt_addr);
	//for(i = 0;i < ps->s_free_inode_count; i++){
	//temp = (struct nvmm_inode *)mytest_addr;
	//nvmm_info("the inode temp offset is :%llu\n",le64_to_cpu(temp->i_pg_addr));
	//mytest_addr += NVMM_INODE_SIZE;
	//}
}

void nvmm_init_free_block_list_offset(struct nvmm_super_block *ps,void *sbi_virt_addr)
{
    //use bitmap_start addr as the 1st block
    void *start_addr = le64_to_cpu(ps->s_free_block_start) + sbi_virt_addr;
//    char *mystart_addr = (char *)start_addr;
//    unsigned long *mytemp;

    unsigned int i = 0,blocks_count = le64_to_cpu(ps->s_free_block_count); //!< should + bitmap blocks
    unsigned long *temp = start_addr;
    unsigned long next_offset = le64_to_cpu(ps->s_free_block_start) + le32_to_cpu(ps->s_blocksize);
    unsigned long reread_val = 0;
    //nvmm_info("the start phy_addr of block is %llx\n", ps->s_free_block_start);
    while(blocks_count > 0) {
        i++;
       // nvmm_info("The block %d,vir_addr:%lu,next offset:%lu\n", i, (unsigned long)start_addr,(unsigned long)next_offset);
        temp = start_addr;
        *temp = (next_offset);
        reread_val = *temp;
        next_offset += le32_to_cpu(ps->s_blocksize);
        start_addr += le32_to_cpu(ps->s_blocksize);
        blocks_count--;
    }
//    for(i = 0;i < le64_to_cpu(ps->s_free_block_count); i++){
//	    mytemp = (unsigned long *)mystart_addr;
//	    nvmm_info("the next offset is %lx\n", (unsigned long)*mytemp);
//	    mystart_addr += le64_to_cpu(ps->s_blocksize);
//    }
}



int nvmm_new_block(struct super_block *sb, phys_addr_t *physaddr,
                   int zero, int num)
{
	struct nvmm_super_block *nsb;
	struct nvmm_sb_info *nsi;
	unsigned long blocksize;
	char	*nb;
	unsigned long *temp;
	int errval = 0, i;
	phys_addr_t offset, phys, next;
    	//unsigned long count_tmp;
//	spin_lock(&superblock_lock);
	//mutex_lock(&NVMM_SB(sb)->s_lock);
	spin_lock(&NVMM_SB(sb)->s_lock);
	nsi = NVMM_SB(sb);	
	nsb = nvmm_get_super(sb);
	blocksize = le32_to_cpu(nsb->s_blocksize);
	
	if(!nsb->s_free_block_count){
		errval = -ENOSPC;
		goto fail;
	}    
	offset = le64_to_cpu(nsb->s_free_block_start);
	next = offset;
	phys = nsi->phy_addr + offset;
	/*nb = (char *)nsi->virt_addr + offset;
	temp = (unsigned long *)nb;
	next = *temp;
	le64_add_cpu(&nsb->s_free_block_count, -1);
	*/
    for (i = 0; i < num; ++i){
        nb = (char *)nsi->virt_addr + next;
        temp = (unsigned long *)nb;
        next = *temp;
		le64_add_cpu(&nsb->s_free_block_count, -1);
    }
	nsb->s_free_block_start = cpu_to_le64(next);

    *physaddr = phys;

fail:
	spin_unlock(&NVMM_SB(sb)->s_lock);
//	spin_unlock(&superblock_lock);
//	mutex_unlock(&NVMM_SB(sb)->s_lock);
	return errval;
}
  
unsigned long nvmm_get_zeroed_page(struct super_block *sb)
{
	phys_addr_t physaddr= 0;
	nvmm_new_block(sb, &physaddr, 1, 1);
    	memset(__va(physaddr), 0x00, PAGE_SIZE);
	return (unsigned long)__va(physaddr);

}
struct page * nvmm_new_page(struct super_block *sb, int zero)
{
	phys_addr_t phys;

	if(nvmm_new_block(sb, &phys, zero, 1))
		return NULL;
	else
        	memset(__va(phys), 0x00, PAGE_SIZE);
		return pfn_to_page(phys >> PAGE_SHIFT);
}

void nvmm_free_block(struct super_block *sb, unsigned long pagefn)
{
	struct nvmm_sb_info *nsi;
	struct nvmm_super_block *nsb;
	unsigned long blocksize;
	unsigned long *temp;
	phys_addr_t phys, prev;
	unsigned long offset;


	phys_addr_t temp_phys;
	temp_phys = pagefn << PAGE_SHIFT;
	memset(__va(temp_phys), 0, PAGE_SIZE);

//	spin_lock(&superblock_lock);
//	mutex_lock(&NVMM_SB(sb)->s_lock);

	spin_lock(&NVMM_SB(sb)->s_lock);

	nsi = NVMM_SB(sb);
	offset = (pagefn << PAGE_SHIFT) - nsi->phy_addr;
	phys = pagefn << PAGE_SHIFT;
	temp = __va(phys);
	nsb = nvmm_get_super(sb);
	blocksize = __le32_to_cpu(nsb->s_blocksize);

	prev = __le64_to_cpu(nsb->s_free_block_start);
	nsb->s_free_block_start = cpu_to_le64(offset);
	*temp = prev;
   	le64_add_cpu(&nsb->s_free_block_count,1);

	spin_unlock(&NVMM_SB(sb)->s_lock);

//	spin_unlock(&superblock_lock);
	//mutex_unlock(&NVMM_SB(sb)->s_lock);
}
/*
 * input :
 * @sb : vfs super_block
 * returns :
 * free number of blocks
 */ 
unsigned long nvmm_count_free_blocks(struct super_block *sb)
{
	struct nvmm_super_block *ns = nvmm_get_super(sb);
	return le64_to_cpu(ns->s_free_block_count);
}
