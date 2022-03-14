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

  clockHand = bufs-1;
}


BufMgr::~BufMgr() {

	//遍历框架，查找所有的脏框架并调用flushFile函数将其刷新
    for(FrameId i = 0;i<numBufs;++i)
    {
        if(bufDescTable[i].dirty)
        {
            flushFile(bufDescTable[i].file);
        }
    }

	//删除bufPool、buftable及hashTable
    delete []bufPool;
    delete []bufDescTable;
    delete hashTable;
}

void BufMgr::advanceClock()
{
	//clockHand++，并考虑从numbuf到0号框的情况
    clockHand=(clockHand+1)%numBufs;
}

void BufMgr::allocBuf(FrameId & frame)
{
    FrameId numF = 0;
    while(true)
    {
		//旋转指针，到达下一个位置
        advanceClock();

		//两者相等，说明已经遍历了一整圈，没有找到合适的框，抛出异常
        if(numF==numBufs)
        {
            throw BufferExceededException();
        }

		//如果框内存储了无效数据，直接将此处数据清零，并将此处指定为要申请的框
        if(!bufDescTable[clockHand].valid)
        {
            bufDescTable[clockHand].Clear();
            frame = clockHand;
            return;
        }
		
		else
        {
			//遇到引用位为1的框，置为0
            if(bufDescTable[clockHand].refbit)
            {
                bufDescTable[clockHand].refbit = false;
            }
			else
            {
				//遇到一个pin次数大于等于1的，将计数器+1，再进行下一次扫描
                if(bufDescTable[clockHand].pinCnt>=1)
                {
                    numF++;
                    continue;
                }
				
				//遇到一个pin次数为0的且refbit为0的
				else
                {
					//移除哈希表中对应的条目
                    hashTable->remove(bufDescTable[clockHand].file,bufDescTable[clockHand].pageNo);

					//如果该框是脏的，将其写回
                    if(bufDescTable[clockHand].dirty)
                    {
                        bufDescTable[clockHand].file->writePage(bufPool[clockHand]);
                    }

					//清除状态信息
                    bufDescTable[clockHand].Clear();

					//指定该位置为所需位置，赋值给frame
                    frame = clockHand;
                    return;
                }
            }
        }
    }
}

void BufMgr::readPage(File* file, const PageId pageNo, Page*& page)
{
    //指定框号的变量Num
	FrameId Num = -1;
    try{
		//在缓冲区中寻找需要的页面，找到了，则Num存储其页框标号，则将其refbit设置为true，并将pinCnt++
        hashTable->lookup(file,pageNo,Num);
        bufDescTable[Num].refbit = true;
        bufDescTable[Num].pinCnt++;
    }
    catch(HashNotFoundException& e)
    {
		//没有找到，说明需要的页面在磁盘中

		//为其申请一个页框
        allocBuf(Num);
		//从磁盘中读取该页面并放入缓冲区
        bufPool[Num] = file->readPage(pageNo);
		//在哈希表中添加映射关系
        hashTable->insert(file,pageNo,Num);
		//调用set来规范该页面
        bufDescTable[Num].Set(file,pageNo);
    }
	//返回指向改页框的指针
    page = &bufPool[Num];
}


void BufMgr::unPinPage(File* file, const PageId pageNo, const bool dirty)
{
	//页框的标号，Num
    FrameId Num;
 
		//在缓冲区中寻找需要的页面，将其标号存储在Num中
        hashTable->lookup(file,pageNo,Num);

		//如果pinCnt已经为0，抛出PageNotPinnedException异常
        if(bufDescTable[Num].pinCnt==0)
        {
            throw PageNotPinnedException("PAGENOTPINNED",pageNo,Num);
        }
		//不为0则将其pinCnt--
		else
        bufDescTable[Num].pinCnt--;

		//如果需要设置dirty为则设置，模拟文件的更改
        if(dirty)
        {
            bufDescTable[Num].dirty = true;
        }
    
	//如果没找到则什么事也不做
   
}

void BufMgr::flushFile(const File* file)
{
	//遍历该页面中的所有页框
    for(FrameId i = 0;i<numBufs;i++)
    {
        if(bufDescTable[i].file==file&&bufDescTable[i].valid)//属于该界面且数据有效
        {
            PageId pid = bufDescTable[i].pageNo;//获取该数据磁盘中的页号
            if(bufDescTable[i].pinCnt!=0)//被固定的界面则抛出异常
            {
                throw PagePinnedException("PAGEPINNED",pid,bufDescTable[i].frameNo);
            }

            if(bufDescTable[i].dirty)//脏位则写回并将脏位置为false
            {
                bufDescTable[i].file->writePage(bufPool[i]);
                bufDescTable[i].dirty = false;
            }

            hashTable->remove(file,pid);//从哈希表中移除该条目
            bufDescTable[i].Clear();//从状态表中移除该条目
        }
		else if(!bufDescTable[i].valid)//遇到无效界面则抛出异常
        {
            throw BadBufferException(bufDescTable[i].frameNo,bufDescTable[i].dirty,bufDescTable[i].valid,bufDescTable[i].refbit);
        }
    }

}

void BufMgr::allocPage(File* file, PageId &pageNo, Page*& page)
{
	//页框的ID
    FrameId Num;
	//调用allocatePage获取空页
    Page newpage = file->allocatePage();
	//为空页申请可用的页框标号
    allocBuf(Num);
	//将该页存储入缓冲区
    bufPool[Num] = newpage;

	//获取新的空页的页号
    pageNo = newpage.page_number();
	//将文件、页号和框号的哈希映射存入哈希表
    hashTable->insert(file,pageNo,Num);
	//更新状态表
    bufDescTable[Num].Set(file,pageNo);
	//返回该页面的指针
    page = &bufPool[Num];
}
void BufMgr::disposePage(File* file, const PageId PageNo)
{
    //页框的框号
	FrameId Num;
	//寻找存储着该页的缓冲区
    hashTable->lookup(file,PageNo,Num);
	//设置此处数据为非法，等同于设置此处为空
    bufDescTable[Num].valid = false;
	//从哈希表中移除该映射
    hashTable->remove(file,PageNo);
	//从文件中删除该页面
    file->deletePage(PageNo);
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
