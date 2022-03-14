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

//ˢ��������ҳ��ȡ�����仺�����غ� BufDesc ��
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

//��ʱ����ǰ�����������е���һ֡��
void BufMgr::advanceClock()
{
    clockHand = (clockHand + 1) % numBufs;
}

//ʹ��ʱ���㷨����һ������֡�� ���б�Ҫ������ҳ д�ش��̡�
//����̶������л�����֡�����׳� BufferExceededException��
//��˽�з������� ���������� readPage������ allocPage�����������á�
//ȷ���������Ļ���������о����� Чҳ����ӹ�ϣ����ɾ����Ӧ����Ŀ��
void BufMgr::allocBuf(FrameId & frame)
{
    FrameId sum = 0;//sum��pinned���Ĵ�������
    while(true)
    {
        advanceClock();
        if(sum==numBufs)//���д��ڶ���pinned�����׳��쳣
        {
            throw BufferExceededException();
        }
        if(!bufDescTable[clockHand].valid)//����Ƿ����
        {
            bufDescTable[clockHand].Clear();
            frame = clockHand;
            return;
        }else
        {
            if(bufDescTable[clockHand].refbit)
            {
                bufDescTable[clockHand].refbit = false;//��refbit��0
            }else
            {
                if(bufDescTable[clockHand].pinCnt>=1)
                {
                    sum++;
                    continue;
                }else//�����ʾҪд��
                {
                    hashTable->remove(bufDescTable[clockHand].file,bufDescTable[clockHand].pageNo);
                    if(bufDescTable[clockHand].dirty)
                    {
                        bufDescTable[clockHand].file->writePage(bufPool[clockHand]);//д��Ӳ��
                    }
                    bufDescTable[clockHand].Clear();
                    frame = clockHand;
                    return;
                }
            }
        }
    }
}

//����ͨ������ lookup �����������ҳ���Ƿ����ڻ��������У���ҳ�治�ڻ���������ʱ���÷������ܻ��׳� HashNotFoundException����ϣ���Ի�ȡ֡�š�
//���� lookup�������õĽ�������������Ҫ ����
//�C��� 1��ҳ�治�ڻ��������С����� allocBuf�������仺������ܣ�Ȼ����÷��� file-> readPage������ҳ��Ӵ��̶�ȡ���������ؿ���С�
//����������ҳ������ϣ������� ����ϵ��� Set��������ȷ�������� Set������ʹҳ��� pinCnt ����Ϊ 1��ͨ�� page ���� ����һ��ָ�����ҳ��Ŀ�ܵ�ָ�롣
//�C��� 2��ҳ��λ�ڻ��������С�����������£������ʵ��� refbit������ҳ��� pinCnt�� Ȼ��ͨ�� page ������ָ�뷵�ص�����ҳ��Ŀ�ܡ�
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

//�ݼ�������file�� PageNo����֡�� pinCnt����� dirty == true�������� dirty λ��
//����������Ѿ�Ϊ 0������ �� PAGENOTPINNED������ڹ�ϣ�������δ�ҵ�ҳ�棬��ִ���κβ�����
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


//ˢ��ҳ��
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
