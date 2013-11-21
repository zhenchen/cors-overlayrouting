/*
 *  project:
 *     通用模块 ( 用 c++ 处理  mysql 数据库类，像ADO )
 *                 
 *  description:
 *
 *    通过DataBase,RecordSet,Record,Field类,实现对mysql数据库的操作
 *    包括连接、修改、添加、删除、查询等等，像ADO一样操作数据库
 *    用方便
 *
 *  file:cors_mysql.cpp
 *
 *  author: @ cors
 * 
 *  time:2007-06-05 
 *
 * 
 */

#include "cors_mysql.h"

namespace cors_mysql {

  /*
   * 字段操作
   */
  Field::Field () { }
  Field::~Field () { }
  /*
   * 是否是数字
   */
  bool Field::IsNum (int num) {
    if (IS_NUM (m_type[num]))
      return true;
    else
      return false;
  }
  /*
   * 是否是数字
   */
  bool Field::IsNum (string num) {
    if (IS_NUM (m_type[GetField_NO (num)]))
      return true;
    else
      return false;
  }
  /*
   * 是否是日期
   */
  bool Field::IsDate (int num) {
    if (FIELD_TYPE_DATE == m_type[num] || FIELD_TYPE_DATETIME == m_type[num])
      return true;
    else
      return false;
  }
  /* 是否是日期 */
  bool Field::IsDate (string num) {
    int temp;
    temp = GetField_NO (num);
    if (FIELD_TYPE_DATE == m_type[temp] ||
        FIELD_TYPE_DATETIME == m_type[temp])
      return true;
    else
      return false;
  }
  /*
   * 是否是字符
   */
  bool Field::IsChar (int num) {
    if (m_type[num] == FIELD_TYPE_STRING ||
        m_type[num] == FIELD_TYPE_VAR_STRING ||
        m_type[num] == FIELD_TYPE_CHAR)
      return true;
    else
      return false;

  }
  /*
   * 是否是字符
   */
  bool Field::IsChar (string num) {
    int temp;
    temp = this->GetField_NO (num);
    if (m_type[temp] == FIELD_TYPE_STRING ||
        m_type[temp] == FIELD_TYPE_VAR_STRING ||
        m_type[temp] == FIELD_TYPE_CHAR)
      return true;
    else
      return false;
  }
  /*
   * 是否为二进制数据
   */
  bool Field::IsBlob (int num) {
    if (IS_BLOB (m_type[num]))
      return true;
    else
      return false;
  }
  /*
   * 是否为二进制数据
   */
  bool Field::IsBlob (string num) {
    if (IS_BLOB (m_type[GetField_NO (num)]))
      return true;
    else
      return false;
  }
  /*
   * 得到指定字段的序号
   */
  int Field::GetField_NO (string field_name) {

    for (unsigned int i = 0; i < m_name.size (); i++)
      {
        if (!m_name[i].compare (field_name))
          return i;
      }
    return -1;
  }

  /* +++++++++++++++++++++++++++++++++++++++++++++++++++ */
  /*
   * 1 单条记录
   * 2 [int ]操作 [""]操作
   */

  Record::Record (Field * m_f) {
    m_field = m_f;
  }

  Record::~Record () {
  };

  void Record::SetData (string value) {
    m_rs.push_back (value);
  }
  /* [""]操作 */
  string Record::operator[] (string s) {
    return m_rs[m_field->GetField_NO (s)];
  }

  string Record::operator[] (int num) {
    return m_rs[num];
  }
  /* null值判断 */
  bool Record::IsNull (int num) {
    if ("" == m_rs[num].c_str ())
      return true;
    else
      return false;
  }

  bool Record::IsNull (string s) {
    if ("" == m_rs[m_field->GetField_NO (s)].c_str ())
      return true;
    else
      return false;
  }

  /* 主要-功能:用 value tab value 的形式 返回结果 */
  string Record::GetTabText () {
    string temp;
    for (unsigned int i = 0; i < m_rs.size (); i++) {
        temp += m_rs[i];
        if (i < m_rs.size () - 1)
          temp += "\t";
    }
    return temp;
  }
 /*-----------------------------------------------------*/


  /* +++++++++++++++++++++++++++++++++++++++++++++++++++ */
  /*
   * 1 记录集合
   * 2 [int ]操作 [""]操作
   * 3 表结构操作
   * 4 数据的插入修改
   */
  RecordSet::RecordSet () {
    res = NULL;
    row = NULL;
    pos = 0;
  }

  RecordSet::RecordSet (MYSQL * hSQL) {
    res = NULL;
    row = NULL;
    m_Data = hSQL;
    pos = 0;
  }

  RecordSet::~RecordSet () {
  }
  /*
   * 处理返回多行的查询，返回影响的行数
   * 成功返回行数，失败返回-1
   */
  int RecordSet::ExecuteSQL (const char *SQL) {
  cout<<SQL<<endl;
  //  printf("%s \n", *SQL);
  cout<< "enter Execute SQL" <<endl;

  if(!mysql_real_query (m_Data, SQL, strlen (SQL))) {
      cout<<"mysql_real_query()" <<endl;
      //保存查询结果
      res = mysql_store_result (m_Data);

      //需要检查返回的是否是空指针？如果SQL操作不是select，如insert返回为空
      if (res) {  
        //res非空，获得返回的记录数量
        m_recordcount = (int) mysql_num_rows (res);
        cout<<"record count"<<m_recordcount<< endl;

        //得到字段数量
        m_field_num = mysql_num_fields (res);

        for (int x = 0; fd = mysql_fetch_field (res); x++) {
          m_field.m_name.push_back (fd->name);
          m_field.m_type.push_back (fd->type);
        }
          //保存所有数据
        while(row = mysql_fetch_row (res)) {
          Record temp (&m_field);
          for(int k = 0; k < m_field_num; k++) {
            if(row[k] == NULL || (!strlen (row[k]))) {
              temp.SetData ("");
            }
            else{
                temp.SetData (row[k]);
            }
          } 
          //添加新记录
          m_s.push_back (temp);
        }
        mysql_free_result (res);
      return m_s.size ();
      }
      else {
        cout<<"res is a null, not a select operation"<<endl;
        return -1;
      }
   } 
    return -1;
  }
  /*
   * 向下移动游标
   * 返回移动后的游标位置
   */
  long RecordSet::MoveNext () {
    return (++pos);
  }
  /* 移动游标 */
  long RecordSet::Move (long length) {
    int l = pos + length;

    if (l < 0) {
      pos = 0;
      return 0;
    }
    else {
      if (l >= m_s.size ()) {
        pos = m_s.size () - 1;
        return pos;
      }
      else {
        pos = l;
        return pos;
      }
   }
  }
  /* 移动游标到开始位置 */
  bool RecordSet::MoveFirst () {
    pos = 0;
    return true;
  }
  /* 移动游标到结束位置 */
  bool RecordSet::MoveLast () {
    pos = m_s.size () - 1;
    return true;
  }
  /* 获取当前游标位置 */
  unsigned long RecordSet::GetCurrentPos () const {
    return pos;
  }
  /* 获取当前游标的对应字段数据 */
  bool RecordSet::GetCurrentFieldValue (const char *sFieldName, char *sValue) {
    strcpy (sValue, m_s[pos][sFieldName].c_str ());
    return true;
  }
  bool RecordSet::GetCurrentFieldValue (const int iFieldNum, char *sValue) {
    strcpy (sValue, m_s[pos][iFieldNum].c_str ());
    return true;
  }
  /* 获取游标的对应字段数据 */
  bool RecordSet::GetFieldValue (long index, const char *sFieldName, char *sValue) {
    strcpy (sValue, m_s[index][sFieldName].c_str ());
    return true;
  }
  bool RecordSet::GetFieldValue (long index, int iFieldNum, char *sValue) {
    strcpy (sValue, m_s[index][iFieldNum].c_str ());
    return true;
  }
  /* 是否到达游标尾部 */
  bool RecordSet::IsEof () {
    return (pos == m_s.size ())? true : false;
  }
  /*
   * 得到记录数目
   */
  int RecordSet::GetRecordCount () {
    return m_recordcount;
  }
  /*
   * 得到字段数目
   */
  int RecordSet::GetFieldNum () {
    return m_field_num;
  }
  /*
   * 返回字段
   */
  Field *RecordSet::GetField () {
    return &m_field;
  }
  /* 返回字段名 */
  const char *RecordSet::GetFieldName (int iNum) {
    return m_field.m_name.at (iNum).c_str ();
  }
  /* 返回字段类型 */
  const int RecordSet::GetFieldType (char *sName) {
    int i = m_field.GetField_NO (sName);
    return m_field.m_type.at (i);
  }
  const int RecordSet::GetFieldType (int iNum) {
    return m_field.m_type.at (iNum);
  }
  /*
   * 返回指定序号的记录
   */
  Record RecordSet::operator[] (int num) {
    return m_s[num];
  }

  /* +++++++++++++++++++++++++++++++++++++++++++++++++++ */
  /*
   * 1 负责数据库的连接关闭
   * 2 执行sql 语句(不返回结果)
   * 3 处理事务
   */
  DataBase::DataBase() {
    m_Data = NULL;
  }
  DataBase::~DataBase() {
    if (NULL != m_Data) {
      DisConnect ();
    }
  }
  /* 返回句柄 */
  MYSQL *DataBase::GetMysql() {
    return m_Data;
  }
  /*
   * 主要功能:连接数据库
   * 参数说明:
   * 1 host 主机ip地址或者时主机名称
   * 2 user 用户名
   * 3 passwd 密码
   * 4 db 欲连接的数据库名称
   * 5 port 端口号
   * 6 uinx 嵌套字
   * 7 client_flag 客户连接参数
   * 返回值: 0成功 -1 失败
   */
  int DataBase::Connect (string host, string user,
       string passwd, string db,
       unsigned int port, unsigned long client_flag) {
  if ((m_Data = mysql_init (NULL)) && mysql_real_connect (m_Data, host.c_str (),
       user.c_str (), passwd.c_str (), db.c_str (), port, NULL, client_flag)) {
  //选择制定的数据库失败
    if (mysql_select_db (m_Data, db.c_str ()) < 0) {
      mysql_close (m_Data);
      return -1;
    }
  }
  else {
    //初始化mysql结构失败
    mysql_close (m_Data);
    return -1;
  }
  //成功
  return 0;
  }
  /*
   * 关闭数据库连接
   */
  void DataBase::DisConnect() {
    mysql_close (m_Data);
  }

  /*
   * 主要功能: 执行非返回结果查询
   * 参数:sql 待执行的查询语句
   * 返回值; n为成功 表示受到影响的行数 -1 为执行失败 
   */
  int DataBase::ExecQuery(string sql) {
  if (!mysql_real_query(m_Data, sql.c_str (), (unsigned long) sql.length ())) {
    //得到受影响的行数
    return (int) mysql_affected_rows (m_Data);
  }
  else {
    //执行查询失败
    return -1;
    }
  }
  /*
   * 主要功能:测试mysql服务器是否存活
   * 返回值:0 表示成功 -1 失败
   */
  int DataBase::Ping() {
    if (!mysql_ping (m_Data))
      return 0;
    else
      return -1;
  }
  /*
   *  主要功能:关闭mysql 服务器
   * 返回值;0成功 -1 失败
   */
  int DataBase::ShutDown () {
    if (!mysql_shutdown (m_Data, SHUTDOWN_DEFAULT))
      return 0;
    else
      return -1;
  }

  /*
   * 主要功能:重新启动mysql 服务器
   * 返回值;0表示成功 -1 表示失败
   */
  int DataBase::ReBoot() {
    if (!mysql_reload (m_Data))
      return 0;
    else
      return -1;
  }

  /*
   * 说明:事务支持InnoDB or BDB表类型
   */
  /*
   * 主要功能:开始事务
   */
  int DataBase::Start_Transaction () {
    if (!mysql_real_query (m_Data, "START TRANSACTION",
         (unsigned long) strlen ("START TRANSACTION"))) {
      return 0;
    }
    else
      //执行查询失败
      return -1;
  }
  /*
   * 主要功能:提交事务
   * 返回值:0 表示成功 -1 表示失败
   */
  int DataBase::Commit () {

    if (!mysql_real_query(m_Data, "COMMIT", (unsigned long) strlen ("COMMIT"))) {
      return 0;
    }
    else
      //执行查询失败
    return -1;
  }
  /*
   * 主要功能:回滚事务
   * 返回值:0 表示成功 -1 表示失败
   */
  int DataBase::Rollback () {
    if (!mysql_real_query (m_Data, "ROLLBACK",
         (unsigned long) strlen ("ROLLBACK")))
      return 0;
    else
      //执行查询失败
      return -1;
  }
  /* 得到客户信息 */
  const char *DataBase::Get_client_info () {
    return mysql_get_client_info ();
  }
  /*主要功能:得到客户版本信息 */
  const unsigned long DataBase::Get_client_version () {
    return mysql_get_client_version ();
  }
  /* 主要功能:得到主机信息 */
  const char *DataBase::Get_host_info () {
    return mysql_get_host_info (m_Data);
  }
  /* 主要功能:得到服务器信息 */
  const char *DataBase::Get_server_info () {
    return mysql_get_server_info (m_Data);
  }
  /* 主要功能:得到服务器版本信息 */
  const unsigned long DataBase::Get_server_version () {
    return mysql_get_server_version (m_Data);
  }
  /*主要功能:得到 当前连接的默认字符集 */
  const char *DataBase::Get_character_set_name () {
    return mysql_character_set_name (m_Data);
  }
  /*
   * 主要功能返回单值查询
   */
  char *DataBase::ExecQueryGetSingValue (string sql) {
  MYSQL_RES *res;
  MYSQL_ROW row;
  char *p = NULL;
  if (!mysql_real_query(m_Data, sql.c_str (), (unsigned long) sql.length ())) {
    //保存查询结果
    res = mysql_store_result (m_Data);
    row = mysql_fetch_row (res);
  
    if((row[0] == NULL) || (!strlen (row[0])))
      p = "-1";
    else
      p = row[0];

    //p = ((row[0] == NULL) || (!strlen (row[0]))) ? "-1" : row[0];
    mysql_free_result (res);
  }
  else
    //执行查询失败
    p = "-1";

  return p;
  }
  /*
   * 得到系统时间
   */
  const char *DataBase::GetSysTime () {
    return ExecQueryGetSingValue ("select now()");
  }
  /*
   *  主要功能:建立新数据库
   * 参数:name 为新数据库的名称
   * 返回:0成功 -1 失败
   */
  int DataBase::Create_db (string name) {
    string temp;
    temp = "CREATE DATABASE ";
    temp += name;
    if (!mysql_real_query (m_Data, temp.c_str (),
         (unsigned long) temp.length ()))
      return 0;

    else
      //执行查询失败
      return -1;
  }
  /*
   * 主要功能:删除制定的数据库
   * 参数:name 为欲删除数据库的名称
   * 返回:0成功 -1 失败
   */
  int DataBase::Drop_db (string name) {
    string temp;
    temp = "DROP DATABASE ";
    temp += name;
    if (!mysql_real_query (m_Data, temp.c_str (),
         (unsigned long) temp.length ()))
      return 0;
    else
      //执行查询失败
      return -1;
  }
};


