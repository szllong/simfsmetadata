/*************************************************************************
	> File Name: consistency.c
	> Author: szl
	> Mail: szllong@cqu.edu.cn 
	> Created Time: 2016年01月29日 星期五 14时53分33秒
 ************************************************************************/
#include "nvmm.h"
void nvmm_consistency_before_writing(struct inode *normal_i)
{
	struct nvmm_inode *normal_inode, *consistency_inode = NULL;
	struct super_block *sb;
	sb = normal_i->i_sb;
	normal_inode = nvmm_get_inode(sb, normal_i->i_ino);
	consistency_inode = NVMM_I(normal_i)->consistency_inode;
	if(consistency_inode){
		consistency_inode->i_mode = normal_inode->i_mode;
		consistency_inode->i_uid = normal_inode->i_uid;
		consistency_inode->i_gid = normal_inode->i_gid;
		consistency_inode->i_link_counts = normal_inode->i_link_counts;
		consistency_inode->i_blocks = normal_inode->i_blocks;
		consistency_inode->i_generation = normal_inode->i_generation;
		consistency_inode->i_size = normal_inode->i_size;
		consistency_inode->i_atime = normal_inode->i_atime;
		consistency_inode->i_ctime = normal_inode->i_ctime;
		consistency_inode->i_mtime = normal_inode->i_mtime;
		consistency_inode->transaction_flag = TRANSACTION_PENDING;
//		nvmm_info("consistency_inode %lx, back up metada and set flag to TRANSACTION_PENDING\n", consistency_inode->consistency_inode_ino);
	}
}


void nvmm_consistency_backup_data(struct super_block *sb, struct inode *normal_i, loff_t offset, size_t length)
{
	struct nvmm_inode *normal_inode, *consistency_inode;
	struct nvmm_inode_info *normal_i_info;
	void *normal_vaddr, *consistency_vaddr;
	loff_t size;

	normal_inode = nvmm_get_inode(sb, normal_i->i_ino);
	normal_i_info = NVMM_I(normal_i);
	normal_vaddr = normal_i_info->i_virt_addr + offset;
	consistency_inode = normal_i_info->consistency_inode;
	consistency_vaddr = (void *)consistency_inode->i_pg_addr;
	
	size = i_size_read(normal_i);
	if(offset + length > size)
		length = size - offset;
	
	consistency_inode->start_write_position = offset;
	consistency_inode->write_length = length;
	consistency_inode->origin_inode_ino = normal_i->i_ino;
	consistency_inode->transaction_flag = TRANSACTION_COMMIT;
//	nvmm_info("consistency_inode %lx, back up data and set flag to TRANSACTION_COMMIT\n", consistency_inode->consistency_inode_ino);
	memcpy(consistency_vaddr, normal_vaddr, length);
	consistency_inode->i_blocks = normal_inode->i_blocks;

}

void nvmm_consistency_end_writing(struct inode *normal_i)
{
	struct nvmm_inode *consistency_inode = NULL;
	
	consistency_inode = NVMM_I(normal_i)->consistency_inode;
	consistency_inode->i_flags = TRANSACTION_CHECKPOINTING;
//	nvmm_info("consistency_inode %lx, set flag to TRANSACTION_CHECKPOINTING", consistency_inode->consistency_inode_ino);
}
