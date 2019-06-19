#ifndef _APUE_DB_H  /* ��ֻ֤������ͷ�ļ�һ�� */
#define _APUE_DB_H

/**************apue.h�ļ��Ĳ�������***************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stddef.h>
#include <error.h>
#include <errno.h>

#include <sys/uio.h>
#include <stdarg.h>


#define MAXLINE 4096//��������������õ�
#define FILE_MODE (S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH) //�ļ�Ȩ��   ���ļ������ߣ���|д| ���ļ��飩��|д

//ͨ���궨������Ĳ���
#define writew_lock(fd,offset,whence,len) 	lock_reg((fd),F_SETLKW,F_WRLCK,(offset),(whence),(len)) //F_SETLKW����Ѿ��������̹���ȴ�
#define readw_lock(fd,offset,whence,len)  	lock_reg((fd),F_SETLKW,F_RDLCK,(offset),(whence),(len))	//F_SETLK����Ѿ�����ֱ�ӷ���
#define un_lock(fd,offset,whence,len)		lock_reg((fd),F_SETLK,F_UNLCK,(offset),(whence),(len))//F_UNLCK����  F_RDLCK�Ӷ��� F_WRLCK��д��
//����һ��֪ʶ�㣺��������Ѿ������ټ����������������





//���ļ��Ӽ�¼��,���϶������������ô˺���
int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len);
/*int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len) {
	struct flock lock;
	lock.l_len = len;
	lock.l_start = offset;
	lock.l_type = type;
    lock.l_whence = whence;
	return (fcntl(fd, cmd, &lock));
}
��������ֻ�Ƿ������ǿ�ʵ����db.c*/ 



//�Բ�ͬ�Ĵ����ɲ�ͬ�ı�����err_doit�������Σ���db.c����
void err_doit(int errnoflag, int error, const char * fmt, va_list ap);
void err_dump(const char *fmt, ...) ;
void err_sys(const char * fmt,...) ;   
void err_quit(const char * fmt ,...);


/************************************************/


typedef void * DBHANDLE;  //���ݿ������

DBHANDLE db_open(const char *pathname, int oflag, ...);
void     db_close(DBHANDLE h);
char    *db_fetch(DBHANDLE h, const char *key);
int      db_store(DBHANDLE h, const char *key, const char *data, int flag);  //�û����õĺ���
int      db_delete(DBHANDLE h, const char *key);
void     db_rewind(DBHANDLE h);
char    *db_nextrec(DBHANDLE h, char *key);

/* db_store : oflag */
#define DB_INSERT  1  /* insert new record only */
#define DB_REPLACE 2  /* replace existing record */
#define DB_STORE   3  /* replace or insert */

/* ʵ�ֵĻ������ƣ�Ϊ֧�ָ�������ݿ�ɸ�����Щ���� */
#define IDXLEN_MIN 6     //key��Ϊ1ʱ����������(����С��������)
#define IDXLEN_MAX 1024  //ʵ��������󳤶�
#define DATLEN_MIN 2     //�������ٰ���һ����ĸ�ͻ��з�
#define DATLEN_MAX 1024  //�������

#endif  /* _APUE_DB_H */
