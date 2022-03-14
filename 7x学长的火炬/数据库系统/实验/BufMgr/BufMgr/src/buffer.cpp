/**
 * @author See Contributors.txt for code contributors and overview of BadgerDB.
 *
 * @section LICENSE
 * Copyright (c) 2012 Database Group, Computer Sciences Department, University of Wisconsin-Madison.
 */

#include <memory>
#include <iostream>
#include "buffer.h"
#include "exceptions/buffer_exceeded_exception.h"
#include "exceptions/page_not_pinned_exception.h"
#include "exceptions/page_pinned_exception.h"
#include "exceptions/bad_buffer_exception.h"
#include "exceptions/hash_not_found_exception.h"

namespace badgerdb {

BufMgr::BufMgr(std::uint32_t bufs)
	: numBufs(bufs) {
	bufDescTable = new BufDesc[bufs];

  for (FrameId i = 0; i < bufs; i++)
  {
  	bufDescTable[i].frameNo = i;
  	bufDescTable[i].valid = false;
  }

  bufPool = new Page[bufs];

	int htsize = ((((int) (bufs * 1.2))*2)/2)+1;
  hashTable = new BufHashTbl (htsize);  // allocate the buffer hash table

  clockHand = bufs - 1;
}

//刷新所有脏页并取消分配缓冲区池和 BufDesc 表。
BufMgr::~BufMgr() {
    for(FrameId i = 0;i<numBufs;++i)
    {
        if(bufDescTable[i].dirty)
        {
            flushFile(bufDescTable[i].file);
        }
    }
    delete []bufPool;
    delete []bufDescTable;
    delete hashTable;
}

//将时钟提前到缓冲区池中的下一帧。
void BufMgr::advanceClock()
{
    clockHand = (clockHand + 1) % numBufs;
}

//使用时钟算法分配一个空闲帧； 如有必要，将脏页 写回磁盘。
//如果固定了所有缓冲区帧，则抛出 BufferExceededException。
//该私有方法将由 下面描述的 readPage（）和 allocPage（）方法调用。
//确保如果分配的缓冲区框架中具有有 效页，请从哈希表中删除相应的条目。
void BufMgr::allocBuf(FrameId & frame)
{
    FrameId sum = 0;//sum是pinned过的窗口数量
    while(true)
    {
        advanceClock();
        if(sum==numBufs)//所有窗口都被pinned，泽抛出异常
        {
            throw BufferExceededException();
        }
        if(!bufDescTable[clockHand].valid)//检查是否可用
        {
            bufDescTable[clockHand].Clear();
            frame = clockHand;
            return;
        }else
        {
            if(bufDescTable[clockHand].refbit)
            {
                bufDescTable[clockHand].refbit = false;//将refbit置0
            }else
            {
                if(bufDescTable[clockHand].pinCnt>=1)
                {
                    sum++;
                    continue;
                }else//这里表示要写入
                {
                    hashTable->remove(bufDescTable[clockHand].file,bufDescTable[clockHand].pageNo);
                    if(bufDescTable[clockHand].dirty)
                    {
                        bufDescTable[clockHand].file->writePage(bufPool[clockHand]);//写进硬盘
                    }
                    bufDescTable[clockHand].Clear();
                    frame = clockHand;
                    return;
                }
            }
        }
    }
}

//首先通过调用 lookup （）方法检查页面是否已在缓冲区池中，当页面不在缓冲区池中时，该方法可能会抛出 HashNotFoundException。哈希表以获取帧号。
//根据 lookup（）调用的结果，有两种情况要 处理：
//–情况 1：页面不在缓冲区池中。调用 allocBuf（）分配缓冲区框架，然后调用方法 file-> readPage（）将页面从磁盘读取到缓冲区池框架中。
//接下来，将页面插入哈希表。最后，在 框架上调用 Set（）以正确设置它。 Set（）将使页面的 pinCnt 设置为 1。通过 page 参数 返回一个指向包含页面的框架的指针。
//–情况 2：页面位于缓冲区池中。在这种情况下，设置适当的 refbit，增加页面的 pinCnt， 然后通过 page 参数将指针返回到包含页面的框架。
void BufMgr::readPage(File* file, const PageId pageNo, Page*& page)
{
    FrameId frameNo = -1;
    try{
        hashTable->lookup(file,pageNo,frameNo);
        bufDescTable[frameNo].refbit = true;
        bufDescTable[frameNo].pinCnt++;
    }
    catch(HashNotFoundException& e)
    {
        allocBuf(frameNo);
        bufPool[frameNo] = file->readPage(pageNo);
        hashTable->insert(file,pageNo,frameNo);
        bufDescTable[frameNo].Set(file,pageNo);

    }
    page = &bufPool[frameNo];
}

//递减包含（file， PageNo）的帧的 pinCnt，如果 dirty == true，则设置 dirty 位。
//如果引脚数已经为 0，则抛 出 PAGENOTPINNED。如果在哈希表查找中未找到页面，则不执行任何操作。
void BufMgr::unPinPage(File* file, const PageId pageNo, const bool dirty)
{
     FrameId frameNo;
    try
    {
        hashTable->lookup(file,pageNo,frameNo);
        if(bufDescTable[frameNo].pinCnt==0)
        {
            throw PageNotPinnedException("PAGENOTPINNED",pageNo,frameNo);
        }
        bufDescTable[frameNo].pinCnt--;
        if(dirty)
        {
            bufDescTable[frameNo].dirty = true;
        }
    }
    catch(HashNotFoundException e)
    {

    }
}

void BufMgr::allocPage(File* file, PageId &pageNo, Page*& page)
{
    FrameId frameNo;
    Page newpage = file->allocatePage();
    allocBuf(frameNo);
    bufPool[frameNo] = newpage;
    pageNo = newpage.page_number();
    hashTable->insert(file,pageNo,frameNo);
    bufDescTable[frameNo].Set(file,pageNo);
    page = &bufPool[frameNo];
}

void BufMgr::disposePage(File* file, const PageId PageNo)
{
    try
    {
        FrameId fid;
        hashTable->lookup(file,PageNo,fid);
        bufDescTable[fid].valid = false;
        hashTable->remove(file,PageNo);
    }catch(HashNotFoundException e)
    {
    }
    file->deletePage(PageNo);
}


//刷新页面
void BufMgr::flushFile(const File* file)
{
    for(FrameId i = 0;i<numBufs;++i)
    {
        if(bufDescTable[i].file==file)
        {
            if(!bufDescTable[i].valid)
            {
                throw BadBufferException(bufDescTable[i].frameNo,bufDescTable[i].dirty,bufDescTable[i].valid,bufDescTable[i].refbit);
            }
            PageId pid = bufDescTable[i].pageNo;
            if(bufDescTable[i].pinCnt!=0)
            {
                throw PagePinnedException("PAGEPINNED",pid,bufDescTable[i].frameNo);
            }
            if(bufDescTable[i].dirty)
            {
                bufDescTable[i].file->writePage(bufPool[i]);
                bufDescTable[i].dirty = false;
            }
            hashTable->remove(file,pid);
            bufDescTable[i].Clear();
        }
    }

}


void BufMgr::printSelf(void)
{
  BufDesc* tmpbuf;
	int validFrames = 0;

  for (std::uint32_t i = 0; i < numBufs; i++)
	{
  	tmpbuf = &(bufDescTable[i]);
		std::cout << "FrameNo:" << i << " ";
		tmpbuf->Print();

  	if (tmpbuf->valid == true)
    	validFrames++;
  }

	std::cout << "Total Number of Valid Frames:" << validFrames << "\n";
}

}
