#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bf.h"
#include "hp_file.h"
#include "record.h"


#define CALL_BF(call)       \
{                           \
  BF_ErrorCode code = call; \
  if (code != BF_OK) {      \
    BF_PrintError(code);    \
    exit(code);             \
  }                         \
}


int HP_CreateFile(char *fileName){
    int fd;
    if(BF_CreateFile(fileName) != BF_OK) {
        return -1; 
    }
    if(BF_OpenFile(fileName, &fd) != BF_OK){
      return -1;
    }
    BF_Block* block;
    BF_Block_Init(&block);

    if(BF_AllocateBlock(fd, block) != BF_OK){
      return -1;
    }

    void* data = BF_Block_GetData(block);
    HP_info* hpinfo; 
    hpinfo = data;
    
    hpinfo->id_last = 0;
    hpinfo->blocks_count = 1;
    hpinfo->records_per_block = ((BF_BLOCK_SIZE - sizeof(HP_block_info))/sizeof(Record));
    

    BF_Block_SetDirty(block);
    if(BF_UnpinBlock(block) != BF_OK){
      return -1;
    }

    BF_Block_Destroy(&block);


    if(BF_CloseFile(fd) != BF_OK){
      return -1;
    }
    
    return 0;
}

HP_info* HP_OpenFile(char *fileName, int *file_desc){
    CALL_BF(BF_OpenFile(fileName, file_desc));
  
    BF_Block* block = NULL;
    BF_Block_Init(&block);
    CALL_BF(BF_GetBlock(*file_desc, 0, block));
     
    void* data = BF_Block_GetData(block);
    HP_info* hpInfo = data;
    

    BF_Block_Destroy(&block);

    return hpInfo;
}


int HP_CloseFile(int file_desc,HP_info* hp_info ){
   
    BF_Block *block;
    BF_Block_Init(&block);
    CALL_BF(BF_GetBlock(file_desc, 0, block));
    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);
    CALL_BF(BF_CloseFile(file_desc));
    return 0;
}

int HP_InsertEntry(int file_desc,HP_info* hp_info, Record record){
    int block_num;
    CALL_BF(BF_GetBlockCounter(file_desc, &block_num));
    if(block_num > 1){ //if we are not in the very first block(where we only have the info of the file)
      BF_Block * block = NULL;
      BF_Block_Init(&block);
      BF_GetBlock(file_desc, hp_info->id_last, block);

      void* data = BF_Block_GetData(block);
      HP_block_info* block_info = data + hp_info->records_per_block * sizeof(Record);//store an HP_block_info struct at the end of the block which is right after its records

      if(block_info->records_num < hp_info->records_per_block){  //if there are few enough records inserted and there is space insert the record in the same block
        data = data + block_info->records_num * sizeof(Record);  //move the pointer at the end of the records inserted 
        if(memcpy(data, &record, sizeof(Record)) == NULL){       //and insert there the next record
          perror("Allocation Failed");
          return -1;
        }
        block_info->records_num++;
        BF_Block_SetDirty(block);
        CALL_BF(BF_UnpinBlock(block));
        BF_Block_Destroy(&block);
        return hp_info->id_last;
      }
      CALL_BF(BF_UnpinBlock(block));
      BF_Block_Destroy(&block);
    }
    //else if we are on the first block or if th current block is full create new block for more records
    BF_Block *block = NULL;
    BF_Block_Init(&block);
    CALL_BF(BF_AllocateBlock(file_desc, block));
    void *data = BF_Block_GetData(block);
    
    hp_info->id_last++;
    hp_info->blocks_count++;
    HP_block_info* block_info = data + hp_info->records_per_block * sizeof(Record);
    block_info->next = (void*)block_info + sizeof(block_info); //pointer to the next block points at the end of the block_info struct which is the last thing in the current block

    block_info->records_num=0;
    memcpy(data, &record, sizeof(Record));   //insert the new record
    block_info->records_num++;

    BF_Block_SetDirty(block);
    CALL_BF(BF_UnpinBlock(block));
    BF_Block_Destroy(&block);

    return hp_info->id_last;
}

int HP_GetAllEntries(int file_desc,HP_info* hp_info, int value){  
    int count = 0;
    void *data;
    Record *record;
    BF_Block *block = NULL;
    BF_Block_Init(&block);

    for(int i=1 ; i<hp_info->blocks_count ; i++){

      BF_GetBlock(file_desc, i, block);
      data = BF_Block_GetData(block);
      HP_block_info* block_info = data + hp_info->records_per_block*sizeof(Record);
      
      for(int j =0 ; j<block_info->records_num ; j++){
        record = data + j *sizeof(Record);  //move the pointer to the next record
        if(record->id == value){
          printRecord(*record);
          count++;
        }
      }
      CALL_BF(BF_UnpinBlock(block));
    }

    BF_Block_Destroy(&block);
    return count;
}

