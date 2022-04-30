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

	bufDescTable[i].refbit = false;// my code
  }

  bufPool = new Page[bufs];

	int htsize = ((((int) (bufs * 1.2))*2)/2)+1;
  hashTable = new BufHashTbl (htsize);  // allocate the buffer hash table

  clockHand = bufs - 1;
}


BufMgr::~BufMgr() {
	for(FrameId i=0;i<numBufs;++i){
		if(bufDescTable[i].valid && bufDescTable[i].dirty){
			if(File::isOpen(bufDescTable[i].file->filename())){
				bufDescTable[i].file->writePage(bufPool[i]);
			}
		}

	}
	delete [] bufDescTable;
	delete [] bufPool;
	delete hashTable;
}

void BufMgr::advanceClock()
{
	clockHand+=1;
	if(clockHand>=numBufs)clockHand=0;
	bufDescTable[clockHand].refbit=0;
}

void BufMgr::allocBuf(FrameId & frame) 
{
	bool reflag;
	int fucked=0;
	while(1)
	{
		
		advanceClock();
		reflag=bufDescTable[clockHand].refbit;

		if(reflag)continue;

		if(bufDescTable[clockHand].valid && bufDescTable[clockHand].pinCnt>0)
		{
			fucked++;
			if(fucked>=numBufs)
			{
				throw BufferExceededException();
			}
			continue;
		}

		if(!bufDescTable[clockHand].valid)
		{
			frame = clockHand;
			bufDescTable[frame].Clear();
			return;
		}


		if(bufDescTable[clockHand].valid && !bufDescTable[clockHand].dirty)
		{
			hashTable->remove(bufDescTable[clockHand].file,bufDescTable[clockHand].pageNo);
			frame = clockHand;
			bufDescTable[frame].Clear();
			return;
		}

		if(bufDescTable[clockHand].valid && bufDescTable[clockHand].dirty)
		{
			bufDescTable[clockHand].file->writePage(bufPool[clockHand]);
			hashTable->remove(bufDescTable[clockHand].file,bufDescTable[clockHand].pageNo);
			frame = clockHand;
			bufDescTable[frame].Clear();
			return;
		}
	}
}

	
void BufMgr::readPage(File* file, const PageId pageNo, Page*& page)
{
	FrameId pos;
	bufStats.accesses++;

	try{
		hashTable->lookup(file, pageNo, pos);
		//std::cout<<"read hash found, pos = "<<pos<<"pin="<<bufDescTable[pos].pinCnt<<"\n";
		bufDescTable[pos].refbit=true;
		bufDescTable[pos].pinCnt+=1;
		page = &bufPool[pos];
		//std::cout<<"pin="<<bufDescTable[pos].pinCnt<<"\n";
		return;
	}catch(HashNotFoundException e){
		allocBuf(pos);
		//std::cout<<"readPage not found "<<file<<"\n";
		bufPool[pos]=file->readPage(pageNo);
		bufStats.diskreads++;
		hashTable->insert(file, pageNo, pos);
		bufDescTable[pos].Set(file, pageNo);
		page = &bufPool[pos];
		return;
	}
	
}


void BufMgr::unPinPage(File* file, const PageId pageNo, const bool dirty) 
{
	FrameId pos;

	try{
		hashTable->lookup(file, pageNo, pos);
	}catch(HashNotFoundException e){
		//do nothing
		return;
	}
	//std::cout<<"unpin hash found, pos = "<<pos<<"pin="<<bufDescTable[pos].pinCnt<<"\n";
	if(bufDescTable[pos].pinCnt==0)
	{
		throw PageNotPinnedException(file->filename(),pageNo,pos);
		return;
	}
	bufDescTable[pos].pinCnt-=1;
	if(dirty) bufDescTable[pos].dirty=true;
	return;
}

void BufMgr::flushFile(const File* file) 
{
	for(FrameId i=0;i<numBufs;++i){
		if(bufDescTable[i].file==file){
			if(bufDescTable[i].pinCnt>0){
				throw PagePinnedException(file->filename(), bufDescTable[i].pageNo, i);
			}
			if(!bufDescTable[i].valid){
				throw BadBufferException(bufDescTable[i].frameNo, bufDescTable[i].dirty, bufDescTable[i].valid, bufDescTable[i].refbit);
			}
			if(bufDescTable[i].dirty){
				bufDescTable[i].file->writePage(bufPool[i]);
				bufStats.diskwrites++;
			}
			hashTable->remove(file,bufDescTable[i].pageNo);
			bufDescTable[i].Clear();
		}
	}
}

void BufMgr::allocPage(File* file, PageId &pageNo, Page*& page) 
{
	Page now=file->allocatePage();
	PageId nowid=now.page_number();

	FrameId pos;
	allocBuf(pos);
	//bufDescTable[pos].Clear();
	bufPool[pos]=now;
	hashTable->insert(file, nowid, pos);
	bufDescTable[pos].Set(file, nowid);
	page=&bufPool[pos];
	pageNo=nowid;
	return;
}

void BufMgr::disposePage(File* file, const PageId PageNo)
{
	FrameId pos;
	try{
		hashTable->lookup(file,PageNo,pos);
	}catch(HashNotFoundException e){
		file->deletePage(PageNo);
		return;
	}
	bufDescTable[pos].Clear();
	hashTable->remove(file, PageNo);
	file->deletePage(PageNo);
	return;
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
