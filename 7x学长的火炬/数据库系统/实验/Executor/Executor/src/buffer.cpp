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

	//������ܣ��������е����ܲ�����flushFile��������ˢ��
    for(FrameId i = 0;i<numBufs;++i)
    {
        if(bufDescTable[i].dirty)
        {
            flushFile(bufDescTable[i].file);
        }
    }

	//ɾ��bufPool��buftable��hashTable
    delete []bufPool;
    delete []bufDescTable;
    delete hashTable;
}

void BufMgr::advanceClock()
{
	//clockHand++�������Ǵ�numbuf��0�ſ�����
    clockHand=(clockHand+1)%numBufs;
}

void BufMgr::allocBuf(FrameId & frame)
{
    FrameId numF = 0;
    while(true)
    {
		//��תָ�룬������һ��λ��
        advanceClock();

		//������ȣ�˵���Ѿ�������һ��Ȧ��û���ҵ����ʵĿ��׳��쳣
        if(numF==numBufs)
        {
            throw BufferExceededException();
        }

		//������ڴ洢����Ч���ݣ�ֱ�ӽ��˴��������㣬�����˴�ָ��ΪҪ����Ŀ�
        if(!bufDescTable[clockHand].valid)
        {
            bufDescTable[clockHand].Clear();
            frame = clockHand;
            return;
        }
		
		else
        {
			//��������λΪ1�Ŀ���Ϊ0
            if(bufDescTable[clockHand].refbit)
            {
                bufDescTable[clockHand].refbit = false;
            }
			else
            {
				//����һ��pin�������ڵ���1�ģ���������+1���ٽ�����һ��ɨ��
                if(bufDescTable[clockHand].pinCnt>=1)
                {
                    numF++;
                    continue;
                }
				
				//����һ��pin����Ϊ0����refbitΪ0��
				else
                {
					//�Ƴ���ϣ���ж�Ӧ����Ŀ
                    hashTable->remove(bufDescTable[clockHand].file,bufDescTable[clockHand].pageNo);

					//����ÿ�����ģ�����д��
                    if(bufDescTable[clockHand].dirty)
                    {
                        bufDescTable[clockHand].file->writePage(bufPool[clockHand]);
                    }

					//���״̬��Ϣ
                    bufDescTable[clockHand].Clear();

					//ָ����λ��Ϊ����λ�ã���ֵ��frame
                    frame = clockHand;
                    return;
                }
            }
        }
    }
}

void BufMgr::readPage(File* file, const PageId pageNo, Page*& page)
{
    //ָ����ŵı���Num
	FrameId Num = -1;
    try{
		//�ڻ�������Ѱ����Ҫ��ҳ�棬�ҵ��ˣ���Num�洢��ҳ���ţ�����refbit����Ϊtrue������pinCnt++
        hashTable->lookup(file,pageNo,Num);
        bufDescTable[Num].refbit = true;
        bufDescTable[Num].pinCnt++;
    }
    catch(HashNotFoundException& e)
    {
		//û���ҵ���˵����Ҫ��ҳ���ڴ�����

		//Ϊ������һ��ҳ��
        allocBuf(Num);
		//�Ӵ����ж�ȡ��ҳ�沢���뻺����
        bufPool[Num] = file->readPage(pageNo);
		//�ڹ�ϣ�������ӳ���ϵ
        hashTable->insert(file,pageNo,Num);
		//����set���淶��ҳ��
        bufDescTable[Num].Set(file,pageNo);
    }
	//����ָ���ҳ���ָ��
    page = &bufPool[Num];
}


void BufMgr::unPinPage(File* file, const PageId pageNo, const bool dirty)
{
	//ҳ��ı�ţ�Num
    FrameId Num;
 
		//�ڻ�������Ѱ����Ҫ��ҳ�棬�����Ŵ洢��Num��
        hashTable->lookup(file,pageNo,Num);

		//���pinCnt�Ѿ�Ϊ0���׳�PageNotPinnedException�쳣
        if(bufDescTable[Num].pinCnt==0)
        {
            throw PageNotPinnedException("PAGENOTPINNED",pageNo,Num);
        }
		//��Ϊ0����pinCnt--
		else
        bufDescTable[Num].pinCnt--;

		//�����Ҫ����dirtyΪ�����ã�ģ���ļ��ĸ���
        if(dirty)
        {
            bufDescTable[Num].dirty = true;
        }
    
	//���û�ҵ���ʲô��Ҳ����
   
}

void BufMgr::flushFile(const File* file)
{
	//������ҳ���е�����ҳ��
    for(FrameId i = 0;i<numBufs;i++)
    {
        if(bufDescTable[i].file==file&&bufDescTable[i].valid)//���ڸý�����������Ч
        {
            PageId pid = bufDescTable[i].pageNo;//��ȡ�����ݴ����е�ҳ��
            if(bufDescTable[i].pinCnt!=0)//���̶��Ľ������׳��쳣
            {
                throw PagePinnedException("PAGEPINNED",pid,bufDescTable[i].frameNo);
            }

            if(bufDescTable[i].dirty)//��λ��д�ز�����λ��Ϊfalse
            {
                bufDescTable[i].file->writePage(bufPool[i]);
                bufDescTable[i].dirty = false;
            }

            hashTable->remove(file,pid);//�ӹ�ϣ�����Ƴ�����Ŀ
            bufDescTable[i].Clear();//��״̬�����Ƴ�����Ŀ
        }
		else if(!bufDescTable[i].valid)//������Ч�������׳��쳣
        {
            throw BadBufferException(bufDescTable[i].frameNo,bufDescTable[i].dirty,bufDescTable[i].valid,bufDescTable[i].refbit);
        }
    }

}

void BufMgr::allocPage(File* file, PageId &pageNo, Page*& page)
{
	//ҳ���ID
    FrameId Num;
	//����allocatePage��ȡ��ҳ
    Page newpage = file->allocatePage();
	//Ϊ��ҳ������õ�ҳ����
    allocBuf(Num);
	//����ҳ�洢�뻺����
    bufPool[Num] = newpage;

	//��ȡ�µĿ�ҳ��ҳ��
    pageNo = newpage.page_number();
	//���ļ���ҳ�źͿ�ŵĹ�ϣӳ������ϣ��
    hashTable->insert(file,pageNo,Num);
	//����״̬��
    bufDescTable[Num].Set(file,pageNo);
	//���ظ�ҳ���ָ��
    page = &bufPool[Num];
}
void BufMgr::disposePage(File* file, const PageId PageNo)
{
    //ҳ��Ŀ��
	FrameId Num;
	//Ѱ�Ҵ洢�Ÿ�ҳ�Ļ�����
    hashTable->lookup(file,PageNo,Num);
	//���ô˴�����Ϊ�Ƿ�����ͬ�����ô˴�Ϊ��
    bufDescTable[Num].valid = false;
	//�ӹ�ϣ�����Ƴ���ӳ��
    hashTable->remove(file,PageNo);
	//���ļ���ɾ����ҳ��
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
