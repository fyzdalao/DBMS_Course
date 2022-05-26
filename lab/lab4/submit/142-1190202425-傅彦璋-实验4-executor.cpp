/**
 * @author Zhaonian Zou <znzou@hit.edu.cn>,
 * School of Computer Science and Technology,
 * Harbin Institute of Technology, China
 */

#include "executor.h"

#include <exceptions/buffer_exceeded_exception.h>
#include <cmath>
#include <ctime>
#include <functional>
#include <iostream>
#include <string>
#include <utility>

#include "file_iterator.h"
#include "page_iterator.h"
#include "storage.h"

using namespace std;

namespace badgerdb {

void TableScanner::print() const {
  badgerdb::File file = badgerdb::File::open(tableFile.filename());
  for (badgerdb::FileIterator iter = file.begin(); iter != file.end(); ++iter) {
    badgerdb::Page page = *iter;
    badgerdb::Page* buffered_page;
    bufMgr->readPage(&file, page.page_number(), buffered_page);

    for (badgerdb::PageIterator page_iter = buffered_page->begin();
         page_iter != buffered_page->end(); ++page_iter) {
      string key = *page_iter;
      string print_key = "(";
      int current_index = 0;
      for (int i = 0; i < tableSchema.getAttrCount(); ++i) {
        switch (tableSchema.getAttrType(i)) {
          case INT: {
            int true_value = 0;
            for (int j = 0; j < 4; ++j) {
              if (std::string(key, current_index + j, 1)[0] == '\0') {
                continue;  // \0 is actually representing 0
              }
              true_value +=
                  (std::string(key, current_index + j, 1))[0] * pow(128, 3 - j);
            }
            print_key += to_string(true_value);
            current_index += 4;
            break;
          }
          case CHAR: {
            int max_len = tableSchema.getAttrMaxSize(i);
            print_key += std::string(key, current_index, max_len);
            current_index += max_len;
            current_index +=
                (4 - (max_len % 4)) % 4;  // align to the multiple of 4
            break;
          }
          case VARCHAR: {
            int actual_len = key[current_index];
            current_index++;
            print_key += std::string(key, current_index, actual_len);
            current_index += actual_len;
            current_index +=
                (4 - ((actual_len + 1) % 4)) % 4;  // align to the multiple of 4
            break;
          }
        }
        print_key += ",";
      }
      print_key[print_key.size() - 1] = ')';  // change the last ',' to ')'
      cout << print_key << endl;
    }
    bufMgr->unPinPage(&file, page.page_number(), false);
  }
  bufMgr->flushFile(&file);
}

JoinOperator::JoinOperator(File& leftTableFile,
                           File& rightTableFile,
                           const TableSchema& leftTableSchema,
                           const TableSchema& rightTableSchema,
                           const Catalog* catalog,
                           BufMgr* bufMgr)
    : leftTableFile(leftTableFile),
      rightTableFile(rightTableFile),
      leftTableSchema(leftTableSchema),
      rightTableSchema(rightTableSchema),
      resultTableSchema(
          createResultTableSchema(leftTableSchema, rightTableSchema)),
      catalog(catalog),
      bufMgr(bufMgr),
      isComplete(false) {
  // nothing
}

TableSchema JoinOperator::createResultTableSchema(
    const TableSchema& leftTableSchema,
    const TableSchema& rightTableSchema) {
  vector<Attribute> attrs;

  // first add all the left table attrs to the result table
  for (int k = 0; k < leftTableSchema.getAttrCount(); ++k) {
    Attribute new_attr = Attribute(
        leftTableSchema.getAttrName(k), leftTableSchema.getAttrType(k),
        leftTableSchema.getAttrMaxSize(k), leftTableSchema.isAttrNotNull(k),
        leftTableSchema.isAttrUnique(k));
    attrs.push_back(new_attr);
  }

  // test every right table attrs, if it doesn't have the same attr(name and
  // type) in the left table, then add it to the result table
  for (int i = 0; i < rightTableSchema.getAttrCount(); ++i) {
    bool has_same = false;
    for (int j = 0; j < leftTableSchema.getAttrCount(); ++j) {
      if ((leftTableSchema.getAttrType(j) == rightTableSchema.getAttrType(i)) &&
          (leftTableSchema.getAttrName(j) == rightTableSchema.getAttrName(i))) {
        has_same = true;
      }
    }
    if (!has_same) {
      Attribute new_attr = Attribute(
          rightTableSchema.getAttrName(i), rightTableSchema.getAttrType(i),
          rightTableSchema.getAttrMaxSize(i), rightTableSchema.isAttrNotNull(i),
          rightTableSchema.isAttrUnique(i));
      attrs.push_back(new_attr);
    }
  }
  return TableSchema("TEMP_TABLE", attrs, true);
}

void JoinOperator::printRunningStats() const {
  cout << "# Result Tuples: " << numResultTuples << endl;
  cout << "# Used Buffer Pages: " << numUsedBufPages << endl;
  cout << "# I/Os: " << numIOs << endl;
}

vector<Attribute> JoinOperator::getCommonAttributes(
    const TableSchema& leftTableSchema,
    const TableSchema& rightTableSchema) const {
  vector<Attribute> common_attrs;
  for (int i = 0; i < rightTableSchema.getAttrCount(); ++i) {
    for (int j = 0; j < leftTableSchema.getAttrCount(); ++j) {
      if ((leftTableSchema.getAttrType(j) == rightTableSchema.getAttrType(i)) &&
          (leftTableSchema.getAttrName(j) == rightTableSchema.getAttrName(i))) {
        Attribute new_attr = Attribute(rightTableSchema.getAttrName(i),
                                       rightTableSchema.getAttrType(i),
                                       rightTableSchema.getAttrMaxSize(i),
                                       rightTableSchema.isAttrNotNull(i),
                                       rightTableSchema.isAttrUnique(i));
        common_attrs.push_back(new_attr);
      }
    }
  }
  return common_attrs;
}

/**
 * use the original key to generate the search key
 * @param key
 * @param common_attrs
 * @param TableSchema
 * @return
 */
string construct_search_key(string key,
                            vector<Attribute>& common_attrs,
                            const TableSchema& TableSchema) {
  string search_key;
  int current_index = 0;
  int current_attr_index = 0;
  for (int i = 0; i < TableSchema.getAttrCount(); ++i) {
    switch (TableSchema.getAttrType(i)) {
      case INT: {
        if (TableSchema.getAttrName(i) ==
                common_attrs[current_attr_index].attrName &&
            TableSchema.getAttrType(i) ==
                common_attrs[current_attr_index].attrType) {
          search_key += std::string(key, current_index, 4);
          current_attr_index++;
        }
        current_index += 4;
        break;
      }
      case CHAR: {
        int max_len = TableSchema.getAttrMaxSize(i);
        if (TableSchema.getAttrName(i) ==
                common_attrs[current_attr_index].attrName &&
            TableSchema.getAttrType(i) ==
                common_attrs[current_attr_index].attrType) {
          search_key += std::string(key, current_index, max_len);
          current_attr_index++;
        }
        current_index += max_len;
        current_index += (4 - (max_len % 4)) % 4;
        ;  // align to the multiple of 4
        break;
      }
      case VARCHAR: {
        int actual_len = key[current_index];
        current_index++;
        if (TableSchema.getAttrName(i) ==
                common_attrs[current_attr_index].attrName &&
            TableSchema.getAttrType(i) ==
                common_attrs[current_attr_index].attrType) {
          search_key += std::string(key, current_index, actual_len);
          current_attr_index++;
        }
        current_index += actual_len;
        current_index +=
            (4 - ((actual_len + 1) % 4)) % 4;  // align to the multiple of 4
        break;
      }
    }
    if (current_attr_index >= common_attrs.size())
      break;
  }
  return search_key;
}

string JoinOperator::joinTuples(string leftTuple,
                                string rightTuple,
                                const TableSchema& leftTableSchema,
                                const TableSchema& rightTableSchema) const {
  int cur_right_index = 0;  // current substring index in the right table key
  string result_tuple = leftTuple;

  for (int i = 0; i < rightTableSchema.getAttrCount(); ++i) {
    bool has_same = false;
    for (int j = 0; j < leftTableSchema.getAttrCount(); ++j) {
      if ((leftTableSchema.getAttrType(j) == rightTableSchema.getAttrType(i)) &&
          (leftTableSchema.getAttrName(j) == rightTableSchema.getAttrName(i))) {
        has_same = true;
      }
    }
    // if the key is only owned by right table, add it to the result tuple
    switch (rightTableSchema.getAttrType(i)) {
      case INT: {
        if (!has_same) {
          result_tuple += std::string(rightTuple, cur_right_index, 4);
        }
        cur_right_index += 4;
        break;
      }
      case CHAR: {
        int max_len = rightTableSchema.getAttrMaxSize(i);
        if (!has_same) {
          result_tuple += std::string(rightTuple, cur_right_index, max_len);
        }
        cur_right_index += max_len;
        unsigned align_ = (4 - (max_len % 4)) % 4;  // align to the multiple of
                                                    // 4
        for (int k = 0; k < align_; ++k) {
          result_tuple += "0";
          cur_right_index++;
        }
        break;
      }
      case VARCHAR: {
        int actual_len = rightTuple[cur_right_index];
        result_tuple += std::string(rightTuple, cur_right_index, 1);
        cur_right_index++;
        if (!has_same) {
          result_tuple += std::string(rightTuple, cur_right_index, actual_len);
        }
        cur_right_index += actual_len;
        unsigned align_ =
            (4 - ((actual_len + 1) % 4)) % 4;  // align to the multiple of 4
        for (int k = 0; k < align_; ++k) {
          result_tuple += "0";
          cur_right_index++;
        }
        break;
      }
    }
  }
  return result_tuple;
}

bool OnePassJoinOperator::execute(int numAvailableBufPages, File& resultFile) {
  if (isComplete)
    return true;

  numResultTuples = 0;
  numUsedBufPages = 0;
  numIOs = 0;

  // TODO: Execute the join algorithm

  isComplete = true;
  return true;
}

int getTableSize(const File &tableFile){
  int ret=0;
  File f = tableFile;
  for(FileIterator it=f.begin();it!=f.end();++it)
    ret++;
  return ret;
}

void splitTuple(const TableSchema &tableSchema, const string &raw, vector<string> &ret){
  ret.clear();
  for(int i=0,pos=0;i<tableSchema.getAttrCount();++i)
  {
    switch(tableSchema.getAttrType(i))
    {
      case INT:{
        ret.push_back(string(raw,pos,4));
        pos+=4;
        break;
      }
      case CHAR:{
        int len=tableSchema.getAttrMaxSize(i);
        int oldpos=pos;
        pos+=(4-(len%4))%4+len;
        ret.push_back(string(raw,oldpos,pos-oldpos));
        break;
      }
      case VARCHAR:{
        int len=raw[pos];
        int oldpos=pos;
        pos+=(4-((len+1)%4))%4+len;
        ret.push_back(string(raw,oldpos,pos-oldpos));
        break;
      }
    }
  }
}

bool NestedLoopJoinOperator::execute(int numAvailableBufPages,
                                     File& resultFile) {
  if (isComplete)
    return true;

  numResultTuples = 0;
  numUsedBufPages = 0;
  numIOs = 0;

  File sfile=leftTableFile;
  TableSchema sschema=leftTableSchema;
  File rfile=rightTableFile;
  TableSchema rschema=rightTableSchema;
  //I/O: B(S) + B(R)B(S)/(M-1)
  //let the smaller one to be 'S'
  if(getTableSize(leftTableFile)>getTableSize(rightTableFile))
  {
    sfile=rightTableFile;
    sschema=rightTableSchema;
    rfile=leftTableFile;
    rschema=leftTableSchema;
  }
  
  int frameAmt=numAvailableBufPages-1;
  int frameUsed=0;
  
  Page* frames[frameAmt];
  Page* rframe;

  FileIterator s_it=sfile.begin();
  while(s_it!=sfile.end())
  {
    frameUsed=0;//how many frames in the buffer pool is being used
    vector<string> sTuple,rTuple;//two tuples to be joit
    string ret;//the result tuple
    for(frameUsed=0;frameUsed<frameAmt && s_it!=sfile.end();++frameUsed,++s_it)
    {
      //read m-1 pages of S to the buffer pool, place from frames[0] to frames[m-2]
      bufMgr->readPage(&sfile,(*s_it).page_number(),frames[frameUsed]);
      numIOs++;
      numUsedBufPages++;
    }
    for(FileIterator r_it=rfile.begin();r_it!=rfile.end();++r_it)
    {
      //read 1 page of R to the buffer pool, place at rframe
      bufMgr->readPage(&rfile,(*r_it).page_number(),rframe);
      numIOs++;
      numUsedBufPages++;
      for(PageIterator rframe_it=rframe->begin();rframe_it!=rframe->end();++rframe_it)
      {
        splitTuple(rschema,*rframe_it,rTuple);
        for(int i=0;i<frameUsed;++i)
        {
          for(PageIterator sframe_it=frames[i]->begin();sframe_it!=frames[i]->end();++sframe_it)
          {
            splitTuple(sschema,*sframe_it,sTuple);
            ret.clear();
            bool flag=1;//set to 0 when the join operator fails
            for(int j=0;j<resultTableSchema.getAttrCount();++j)
            {
              string attr=resultTableSchema.getAttrName(j);
              bool shas=sschema.hasAttr(attr);
              bool rhas=rschema.hasAttr(attr);
              string sraw=sTuple[sschema.getAttrNum(attr)];
              string rraw=rTuple[rschema.getAttrNum(attr)];
              if(shas && rhas)//this attr comes from both R and S, it's the junction
              {
                /*
                  Actually, we should check the attrs before this for-loop
                  so that the junction attr could be checked at the first place of the for-loop, in order to drop those failing join operator at the beginning
                  and passingly we can findout whether an attr comes from S or T.
                  But this preprocess is annoying. I'm lazy and I don't want to increase the complexity of my code.
                */
                if(sraw!=rraw)//if the values of this attr in S and R are not equal, then the join operator fails
                {
                  flag=0;//drop this tuple
                  break;
                }
                ret+=sraw;
              }
              else if(shas)//this attr comes from S
              {
                ret+=sraw;
              }
              else//this attr comes from R
              {
                ret+=rraw;
              }
            }
            if(flag)//join success
            {
              HeapFileManager::insertTuple(ret,resultFile,bufMgr);
              ++numResultTuples;
            }
          }
        }
      }
      //release that 1 page of R
      bufMgr->unPinPage(&rfile,(*r_it).page_number(),false);
    }
    //relase that m-1 pages of S
    for(int i=0;i<frameUsed;++i)
    {
      bufMgr->unPinPage(&sfile,frames[i]->page_number(),false);
    }
  }
  isComplete = true;
  return true;
}

BucketId GraceHashJoinOperator::hash(const string& key) const {
  std::hash<string> strHash;
  return strHash(key) % numBuckets;
}

bool GraceHashJoinOperator::execute(int numAvailableBufPages,
                                    File& resultFile) {
  if (isComplete)
    return true;

  numResultTuples = 0;
  numUsedBufPages = 0;
  numIOs = 0;

  // TODO: Execute the join algorithm

  isComplete = true;
  return true;
}

}  // namespace badgerdb