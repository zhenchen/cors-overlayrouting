 /*
  *  project:
  *    通用模块 ( c++ 处理  mysql 数据库类)
  *                 
  *  description:
  *
  *    通过DataBase,RecordSet,Record,Field类,实现对mysql数据库的操作
  *    包括连接、修改、添加、删除、查询等等，像ADO一样操作数据库，使
  *    用方便
  *
  *  file: cors_mysql.h
  *
  *  author: @ cors
  * 
  *  time:2007-06-05 
  * 
  */

#ifndef CORS_MYSQL_H
#define CORS_MYSQL_H

#include <mysql/mysql.h>

#include <iostream>
#include <vector>
#include <string>

using namespace std;

namespace cors_mysql
{

  /*
   * 字段操作
   */
  class Field
  {
    public:
    /* 字段名称 */
	vector < string > m_name;
    /* 字段类型 */
	vector < enum_field_types > m_type;

    public:
	Field ();
	~Field ();

    /* 是否是数字 */
    bool IsNum (int num);
    /* 是否是数字 */
    bool IsNum (string num);
    /* 是否是日期 */
    bool IsDate (int num);
    /* 是否是日期 */
    bool IsDate (string num);
    /* 是否是字符 */
    bool IsChar (int num);
    /* 是否是字符 */
    bool IsChar (string num);
    /* 是否为二进制数据 */
    bool IsBlob (int num);
    /* 是否为二进制数据 */
    bool IsBlob (string num);
    /* 得到指定字段的序号 */
    int GetField_NO (string field_name);
  };


  /*
   * 1 单条记录
   * 2 [int ]操作 [""]操作
   */
  class Record
  {
    public:
    /* 结果集 */
    vector < string > m_rs;
    /* 字段信息 占用4字节的内存 当记录数很大是回产生性能问题 */
    Field *m_field;
    
    public:
     Record () { };

     Record (Field * m_f);
     ~Record ();

    void SetData (string value);
    /* [""]操作 */
    string operator[] (string s);
    string operator[] (int num);
    /* null值判断 */
    bool IsNull (int num);
    bool IsNull (string s);
    /* 用 value tab value 的形式 返回结果 */
    string GetTabText ();
  };


  /*
   * 1 记录集合
   * 2 [int ]操作 [""]操作
   * 3 表结构操作
   * 4 数据的插入修改
   */
  class RecordSet
  {
    private:
      	/* 记录集 */
	vector < Record > m_s;

	/* 游标位置 */
	unsigned long pos;

	/* 记录数 */
	int m_recordcount;

	/* 字段数 */
	int m_field_num;

	/* 字段信息 */
	Field m_field;

    MYSQL_RES *res;
    MYSQL_FIELD *fd;
    MYSQL_ROW row;
    MYSQL *m_Data;

    public:
	RecordSet ();
      	RecordSet (MYSQL * hSQL);
     	~RecordSet ();

    /* 处理返回多行的查询，返回影响的行数 */
    int ExecuteSQL (const char *SQL);

    /* 得到记录数目 */
    int GetRecordCount ();

    /* 得到字段数目 */
    int GetFieldNum ();

    /* 向下移动游标 */
    long MoveNext ();

    /* 移动游标 */
    long Move (long length);

    /* 移动游标到开始位置 */
    bool MoveFirst ();

    /* 移动游标到结束位置 */
    bool MoveLast ();

    /* 获取当前游标位置 */
    unsigned long GetCurrentPos () const;

    /* 获取当前游标的对应字段数据 */
    bool GetCurrentFieldValue (const char *sFieldName, char *sValue);
    bool GetCurrentFieldValue (const int iFieldNum, char *sValue);

    /* 获取游标的对应字段数据 */
    bool GetFieldValue (long index, const char *sFieldName, char *sValue);
    bool GetFieldValue (long index, int iFieldNum, char *sValue);

    /* 是否到达游标尾部 */
    bool IsEof ();

    /* 返回字段 */
    Field *GetField ();

    /* 返回字段名 */
    const char *GetFieldName (int iNum);

    /* 返回字段类型 */
    const int GetFieldType (char *sName);
    const int GetFieldType (int iNum);

    /* 返回指定序号的记录 */
    Record operator[] (int num);

  };

  /*
   * 1 负责数据库的连接关闭
   * 2 执行sql 语句(不返回结果)
   * 3 处理事务
   */
  class DataBase
  {
    public:
	DataBase ();
	~DataBase ();
 
    private:
      	/* msyql 连接句柄 */
    	MYSQL * m_Data;

    public:
      	/* 返回句柄 */
	MYSQL * GetMysql ();

	/* 连接数据库 */
	int Connect (string host, string user,
		     string passwd, string db,
		     unsigned int port, unsigned long client_flag);

	/* 关闭数据库连接 */
	void DisConnect ();

	/* 执行非返回结果查询 */
	int ExecQuery (string sql);

	/* 测试mysql服务器是否存活 */
	int Ping ();

	/* 关闭mysql 服务器 */
	int ShutDown ();

	/* 主要功能:重新启动mysql 服务器 */
	int ReBoot ();

    /*
     * 说明:事务支持InnoDB or BDB表类型
     */

	/* 主要功能:开始事务 */
	 int Start_Transaction ();

	/* 主要功能:提交事务 */
	 int Commit ();

        /* 主要功能:回滚事务 */
         int Rollback ();

       /* 得到客户信息 */
   	 const char *Get_client_info ();

       /* 主要功能:得到客户版本信息 */
	 const unsigned long Get_client_version ();

       /* 主要功能:得到主机信息 */
         const char *Get_host_info ();

       /* 主要功能:得到服务器信息 */
         const char *Get_server_info ();

       /*主要功能:得到服务器版本信息 */
         const unsigned long Get_server_version ();

       /*主要功能:得到 当前连接的默认字符集 */
         const char *Get_character_set_name ();

       /* 主要功能返回单值查询  */
         char *ExecQueryGetSingValue (string sql);

      /* 得到系统时间 */
         const char *GetSysTime ();

      /* 建立新数据库 */
         int Create_db (string name);

      /* 删除制定的数据库 */
         int Drop_db (string name);
   };

};

#endif //CORS_MYSQL_H
