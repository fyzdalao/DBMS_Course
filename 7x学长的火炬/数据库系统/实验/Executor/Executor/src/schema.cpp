/**
 * @author Zhaonian Zou <znzou@hit.edu.cn>,
 * School of Computer Science and Technology,
 * Harbin Institute of Technology, China
 */

#include "schema.h"

#include <string>

using namespace std;

namespace badgerdb {

TableSchema TableSchema::fromSQLStatement(const string& sql) {
  string tableName;
  vector<Attribute> attrs;
  bool isTemp = false;


    size_t pos1 = sql.find_first_of("(");
    tableName = sql.substr(pos1 - 2, 1);
    string tem1 = sql.substr(pos1 + 1);
    tem1 = tem1.substr(0, tem1.length() - 2);

    
    size_t pos2 = tem1.find_first_of(",");
    string left = tem1.substr(0, pos2);
    string right = tem1.substr(pos2 + 2);

    string namel = left.substr(0, 1);
    string typel;
    string speciall;
    int maxl = 1;
    bool nul=false;
    bool unil=false;

    left = left.substr(2);
    size_t pos3 = left.find(" ");
    typel = left.substr(0,pos3);
    if (pos3 == -1)
        speciall = " ";
    else
        speciall = left.substr(pos3 + 1);

    size_t pos5 = typel.find("(");
    string numl = typel.substr(pos5 + 1, 1);
    int nl = atoi(numl.c_str());
    if (pos5 == -1)
        maxl = 1;
    else 
        maxl = nl;

    if (!speciall.compare("UNIQUE NOT NULL"))
    {
        nul = true;
        unil = true;
    }
    if (!speciall.compare("NOT NULL"))
    {
        nul = true;
    }
    if (!speciall.compare("UNIQUE"))
    {
        unil = true;
    }
        
    string namer = right.substr(0, 1);
    string typer;
    string specialr;
    int maxr = 1;
    bool nur = false;
    bool unir = false;

    right = right.substr(2);
    size_t pos4 = right.find_first_of(" ");
    typer = right.substr(0, pos4);
    if (pos4 == -1)
        specialr = " ";
    else
        specialr = right.substr(pos4 + 1);

    size_t pos6 = typer.find("(");
    string numr = typer.substr(pos6 + 1, 1);
    int nr = atoi(numr.c_str());
    if (pos6 == -1)
        maxr = 1;
    else
        maxr = nr;


    if (!specialr.compare("UNIQUE NOT NULL"))
    {
        nur = true;
        unir = true;
    }
    if (!specialr.compare("NOT NULL"))
    {
        nur = true;
    }
    if (!specialr.compare("UNIQUE"))
    {
        unir = true;
    }


	Attribute temA1 = Attribute(namel,typel,maxl);
	Attribute temA2 = Attribute(namer,typer,maxr);
	attrs.push_back(temA1);
	attrs.push_back(temA2);
    

  // TODO: create attribute definitions from sql


  return TableSchema(tableName, attrs, isTemp);

}

void TableSchema::print() const {
  // TODO
}

}  // namespace badgerdb