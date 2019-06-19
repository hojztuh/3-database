//#include "apue.h" 
#include "apue_db.h"

/************************apue.c�ļ��Ĳ�������**************************/
//��¼�������������������ĺ���ô˺���������������flock
int lock_reg(int fd, int cmd, int type, off_t offset, int whence, off_t len) {
	struct flock lock;
	lock.l_len = len;
	lock.l_start = offset;
	lock.l_type = type;
    lock.l_whence = whence;
	return (fcntl(fd, cmd, &lock));
}


void err_doit(int errnoflag, int error, const char * fmt, va_list ap)
{																			//��ӡ������Ϣ�����涼������������
	char buf[MAXLINE];
	vsnprintf(buf, MAXLINE - 1, fmt, ap);                                               
	if (errnoflag)
		snprintf(buf + strlen(buf), MAXLINE - strlen(buf) - 1, ":%s", strerror(error));
	fflush(stdout);      
	fputs(buf, stderr);
	fflush(NULL);
}
                                            // ��vsnprintf()���� & vfprintf()������ https://blog.csdn.net/qq_37824129/article/details/787632
											//��VA_LIST���÷���, һ����Χ�۰� https://blog.csdn.net/pheror_abu/article/details/5340486   
											//  ����<strarg.h>   va_list���滹���õ�
void err_dump(const char *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	err_doit(1,errno,fmt,ap);                                  
	va_end(ap);										
	abort();//�쳣��ֹһ������
	exit(1);
}

void err_sys(const char * fmt,...) {
	va_list  ap;
	va_start(ap, fmt);
	err_doit(1, errno, fmt, ap);
	va_end(ap);
	exit(1);
}

void err_quit(const char * fmt ,...){
    va_list ap;
    va_start(ap,fmt);
    err_doit(0,0,fmt,ap);		
    va_end(ap);
    exit(1);
}

/********************************************************************/



#define IDXLEN_SZ   4		//����������ռ�ֽ�
#define SEP         ':'		//�ָ���
#define SPACE       ' '		//ɾ��һ������ʱ����" "���
#define NEWLINE     '\n'	//���廻�б�־��Ϊ�˷���鿴�ļ����ݣ����û�в鿴����Ҫ���Բ��ӻ��з�(����more��cat������鿴)

/* The following definitions are for hash chains and free list chain in the index file */
#define PTR_SZ      6     	//ɢ������ָ����ռ�ֽ�
#define PTR_MAX  999999   	//ָ������ָ������Χ,��Ϊָ����ռ���ֽ�Ϊ6����ascall���ʾ�����������Ҳ��6��9��999999
#define NHASH_DEF   137   	//ɢ�б�Ĵ�С���������ã���ɢ��������ɢ�г�ͻʱ��hashֵ����Ϊ�����������ĵ��ø����е����ݷֲ����ȣ�
#define FREE_OFF    0     	//���������λ��
#define HASH_OFF PTR_SZ   	//ɢ�б��λ�ã������������

typedef unsigned long DBHASH;  /* hash values */
typedef unsigned long COUNT;   /* unsigned counter */

/* ��¼�����ݵ�������Ϣ, db_open��������DB�ṹ��ָ��DBHANDLEֵ */
typedef struct
{
	int   idxfd;      //�����ļ����ļ�������
	int   datfd;      //�����ļ����ļ�������
	char  *idxbuf;    //���������ݷ���Ļ����������Ƚ�������¼д�뻺����д���ļ����õĻ�����
	char  *datbuf;    //���������ݷ���Ļ�����������������
	char  *name;      //���ݿ�����

	off_t idxoff;     //��ǰ������¼��λ�ã�key��λ��= idxoff + PTR_SZ + IDXLEN_SZ
	size_t idxlen;    // ��ǰ������¼�ĳ��ȣ�IDXLEN_SZָ������ռ�ֽ�
	off_t  datoff;	  //��Ӧ���ݵ�λ���������ļ��е�ƫ����     offset in data file of data record
	size_t datlen;    //��Ӧ���ݳ�������"\n"   length of data    record include newline at end
	off_t  ptrval;    //��ǰ������¼��ָ����һ��hash�ڵ�ָ���ֵ    contents of chain ptr in index record
	off_t  ptroff;    //ָ��ǰ������¼��ָ���ֵ    chain ptr offset pointing to this record
	off_t  chainoff;  /*��ǰ������¼����hash��ͷָ��������ļ���ʼ����ƫ����     offset of hash chain for this index record */
	off_t  hashoff;   /* hash������������ļ���ʼ����ƫ����ǰ�滹��һ�����������ͷָ��    offset in index file of hash table */
	DBHASH nhash;     //��ǰɢ�б�Ĵ�С

	/* �Գɹ��Ͳ��ɹ��Ĳ�������  */
	COUNT  cnt_delok;    /* delete OK */
	COUNT  cnt_delerr;   /* delete error */
	COUNT  cnt_fetchok;  /* fetch OK */
	COUNT  cnt_fetcherr; /* fetch error */
	COUNT  cnt_nextrec;  /* nextrec */
	COUNT  cnt_stor1;    /* store: DB_INSERT, no empty, appended */
	COUNT  cnt_stor2;    /* store: DB_INSERT, found empty, reused */
	COUNT  cnt_stor3;    /* store: DB_REPLACE, diff len, appended */
	COUNT  cnt_stor4;    /* store: DB_REPLACE, same len, overwrote */
	COUNT  cnt_storerr;  /* store error */
}DB;

/* �ڲ�˽�к���������Ϊstatic��ֻ��ͬһ�ļ��е������������ܵ��� */
static DB *_db_alloc(int);
static void _db_dodelete(DB *);
static int _db_find_and_lock(DB *, const char *, int);
static int _db_findfree(DB *, int, int);
static void _db_free(DB *);
static DBHASH _db_hash(DB *, const char *);
static char *_db_readdat(DB *);
static off_t _db_readidx(DB *, off_t);
static off_t _db_readptr(DB *, off_t);
static void _db_writedat(DB *, const char *, off_t, int);
static void _db_writeidx(DB *, const char *, off_t, int, off_t);
static void _db_writeptr(DB *, off_t, off_t);

/*
	_db_open
		�������ݿ����������ݿ�������ļ��������ļ��������ļ�����������DB�ṹ
		���oflag�а���O_CREAT,��Ҫ�����ļ�����������ѡ�������ļ���Ȩ��
*/
DBHANDLE db_open(const char *pathname, int oflag, ...)    //�пɱ������Ӧ�ã��鿴ǰ��Ĳ���
{
	DB *db;
	int len, mode;
	size_t i; 
	char asciiptr[PTR_SZ + 1], hash[(NHASH_DEF + 1) * PTR_SZ + 2];/*aciiptrĩβ��0��hashĩβ�ǡ�\n����0  ĩβ��Ҫ��0��
																	��Ϊ�ַ���������Ҫ�õ��ַ������������Դ�ӡ�ķ�ʽ�����������Լ�hash���ӡ�������ļ��ϣ�hash��ĩβ����Ҫ�ӻ��з�����+2��
																	ע������NHASH_DEF+1����Ϊ�����˿���������һ���ʼ��*/
	struct stat statbuff;                 //��ȡ�ļ�״̬�Ļ�����

	len = strlen(pathname);
	if ((db = _db_alloc(len)) == NULL)
		//err_dump������"apue.h"						//ΪDB*db����ռ�
		err_dump("db_open: _db_alloc error for DB");

	db->nhash = NHASH_DEF;  //��ǰɢ�б�Ĵ�С����Ϊ137
	db->hashoff = HASH_OFF; //�����ļ���ɢ�б��ƫ��
	strcpy(db->name, pathname);
	strcat(db->name, ".idx");  //�����ļ������޸ĺ�׺��Ϊ���ݿ��ļ���

	if (oflag & O_CREAT)
	{//ȡ����������
		va_list ap;
		va_start(ap, oflag);					//�ɱ������ʹ��
		mode = va_arg(ap, int);          		
		va_end(ap);

		db->idxfd = open(db->name, oflag, mode);
		strcpy(db->name + len, ".dat");
		db->datfd = open(db->name, oflag, mode);
	}
	else				//û��O_CREATEֱ�Ӵ������ļ���openֻ����������
	{
		db->idxfd = open(db->name, oflag);
		strcpy(db->name + len, ".dat");
		db->datfd = open(db->name, oflag);
	}

	/* ����򿪻򴴽��������ݿ��ļ�ʱ���� */
	if (db->idxfd < 0 || db->datfd < 0)
	{
		_db_free(db);  /* ���DB�ṹ */
		return (NULL);
	}

	if ((oflag & (O_CREAT | O_TRUNC)) == (O_CREAT | O_TRUNC))   	//����������ݿ�ͼ�����������ע��121-130
	{
		/*�������ļ�����*/
		if (writew_lock(db->idxfd, 0, SEEK_SET, 0) < 0)
			err_dump("db_open: writew_lock error");

		/* ��ȡ�ļ���С�����Ϊ0����ʾ�Ǳ����̴������ļ�����Ҫ��ʼ�������ļ���д��ɢ�б� */
		if (fstat(db->idxfd, &statbuff) < 0)
			err_sys("db_open: fstat error");

		if (statbuff.st_size == 0)  //�ж��ļ��ǲ��Ǹտ�ʼ�����ģ�������ע��131-137
		{
			sprintf(asciiptr, "%*d", PTR_SZ, 0);/*	��"     0"д��asciiptr�������*�൱�ڶ�ȡPTR_SZ��ֵ��Ϊ%d�Ŀ��(��������õ�)
													��scanf��sscanf�����*��ͬ�����Ǻ��Ե���ȡ�����ݺ��� 
												*/
			hash[0] = 0;//0��ʾ���ַ������ַ�����β����֤֮�����strcat�������
			for (i = 0; i < NHASH_DEF + 1; i++)   
				strcat(hash, asciiptr);
			strcat(hash, "\n");//hash��ĩβ��Ҫ��һ�����з�"
			i = strlen(hash);
			if (write(db->idxfd, hash, i) != i)
				err_dump("db_open: index file init write error");
		}
		/* ���������ļ� */
		if (un_lock(db->idxfd, 0, SEEK_SET, 0) < 0)
			err_dump("db_open: un_lock error");
	}
	//��db->idxoff��λ����һ������
	db_rewind(db);

	return (db);
}

//ΪDB�����Ա(name,idxbuf,databuf)����ռ䣬��ʼ�������ļ�������
static DB *_db_alloc(int namelen)
{
	DB *db;

	if ((db = calloc(1, sizeof(DB))) == NULL)  //������calloc������malloc����Ϊǰ�߻Ὣ��������ڴ�ռ��ֵ������Ϊ0 
		err_dump("_db_alloc: calloc error for DB");
	db->idxfd = db->datfd = -1;    //�ļ�������Ҳ����Ϊ0�����Ը�Ϊ-1  �ο�����ע��152-164

	if ((db->name = malloc(namelen + 5)) == NULL)
		err_dump("_db_alloc: malloc error for name");  //��5��ԭ������Ϊ������Ҫ���.idx��һ��0

	if ((db->idxbuf = malloc(IDXLEN_MAX + 2)) == NULL)      //+2����Ϊ������ʱ����Ҫ���һ�����з���0
		err_dump("_db_alloc: malloc error for index buffer");
	if ((db->datbuf = malloc(DATLEN_MAX + 2)) == NULL)	
		err_dump("_db_alloc: malloc error for data buffer");
	return (db);
}

/* Relinquish access to the database */
void db_close(DBHANDLE h)
{
	_db_free((DB *)h);  //��_db_free�İ�װ��ʹ�䴫��Ĳ���ΪDBHANDLE
}

//�ڲ��������������ʱ����free���ͷſռ�
static void _db_free(DB *db)
{  
	/* �ر��ļ� */
	if (db->idxfd >= 0)    //��Ϊ������ļ���������Ϊ-1�������û����ȷ���ļ��Ͳ��ùر�  
		close(db->idxfd);
	if (db->datfd >= 0)
		close(db->datfd);
	/* �ͷŶ�̬����Ļ��� */
	if (db->idxbuf != NULL)
		free(db->idxbuf);
	if (db->datbuf != NULL)
		free(db->datbuf);
	if (db->name != NULL)
		free(db->name);
	/* �ͷ�DB�ṹռ�õĴ洢�� */
	free(db);
}


/* ���ݸ����ļ���ȡһ������ */
char *db_fetch(DBHANDLE h, const char *key)
{
	DB *db = h;
	char *ptr;

	/* ���Ҹü�¼�������ҳɹ����������DB�ṹ */
	if (_db_find_and_lock(db, key, 0) < 0)//�˹��̸������ļ���Ӧ��ɢ��������
	{
		ptr = NULL;
		db->cnt_fetcherr++;
	}
	else  /* �ɹ� */
	{
		ptr = _db_readdat(db);  //�������ļ��е����� 
		db->cnt_fetchok++;
	}

	if (un_lock(db->idxfd, db->chainoff, SEEK_SET, 1) < 0)//����
		err_dump("db_fetch: un_lock error");
	return (ptr);
}

/* �ں������ڲ��������ļ����Ҽ�¼ */
/* ������������ļ��ϼ�һ��д������writelock��������Ϊ��0������Ӷ��� */
static int _db_find_and_lock(DB *db, const char *key, int writelock)
{
	off_t offset, nextoffset;

	/*���ݹؼ�������hashֵ�������ɢ������λ��*/
	db->chainoff = (_db_hash(db, key) * PTR_SZ) + db->hashoff;
	db->ptroff = db->chainoff;

	/* ֻ����ɢ������ʼ���ĵ�1���ֽ�,���������̷��������ļ��Ĳ�ͬɢ���� */
	if (writelock)//����д��
	{
		if (writew_lock(db->idxfd, db->chainoff, SEEK_SET, 1) < 0)
			err_dump("_db_find_and_lock: writew_lock error");
	}
	else
	{
		if (readw_lock(db->idxfd, db->chainoff, SEEK_SET, 1) < 0)
			err_dump("_db_find_and_lock: readw_lock error");
	}

	/* ��ɢ�����еĵ�һ��ָ��, �������0����ɢ����Ϊ�� */
	offset = _db_readptr(db, db->ptroff);//����ɢ������ͷָ�룬�����ڷ���0
	while (offset != 0)
	{
		//�����������Һ�keyƥ���������¼
		nextoffset = _db_readidx(db, offset);
		if (strcmp(db->idxbuf, key) == 0)
			break;  /* found a match */
		db->ptroff = offset;  /* offset of this (unequal) record   ���û�ҵ���ִ����һ��ʹ��ptroffֵΪǰ������¼��ַ�����Ϊ��һ��������¼��ַ*/
		offset = nextoffset;  /* next one to compare */
	}

	//�ﵽ����βʱ�����ƥ�䣬ֱ���˳���offset=������ƫ��  ��������ע��269-273  ����datlen��datoff��_db_readidx�����и�ֵ
	return (offset == 0 ? -1 : 0);
}

/* ���ݸ����ļ�����ɢ��ֵ */
static DBHASH _db_hash(DB *db, const char *key)
{
	//key��ÿһ���ַ���ascii��*(index+1)�ĺͣ���ģnhash
	//nhash������,��������ʹ��ɢ�б��е����ݷֲ�����
	DBHASH hval = 0;
	char c;
	int i;
	for (i = 1; (c = *key++) != 0; i++)                        //�����ݿ������õ�hash����
		hval += c * i;
	return (hval % db->nhash);
}

/*
	��ȡһ��ָ�룬����ȡoffsetλ�õ�6���ַ��������س�����
*/
static off_t _db_readptr(DB *db, off_t offset)  //�ο�ע��287-302
{
	char asciiptr[PTR_SZ + 1];
	if (lseek(db->idxfd, offset, SEEK_SET) == -1)
		err_dump("_db_readptr: lseek error to ptr field");
	if (read(db->idxfd, asciiptr, PTR_SZ) != PTR_SZ)
		err_dump("_db_readptr: read error of ptr field");
	asciiptr[PTR_SZ] = 0;     //null terminate �����ַ�������atol�������ַ���ת��Ϊ����������
	
	return (atol(asciiptr));
}

/*
	��ȡoffsetλ��һ��������¼������������Ϣ����DB�ṹ
idxoff����ǰ������¼����������ļ���ʼ����ƫ����
ptrval����ɢ����������һ������¼����������ļ���ʼ����ƫ����
idxlen����ǰ������¼�ĳ���
idxbuf��ʵ��������¼
datoff�������ļ��иü�¼��ƫ����
datlen�������ݼ�¼�ĳ���
*/
static off_t _db_readidx(DB *db, off_t offset)
{
	ssize_t i;													
	char *ptr1, *ptr2;
	char asciiptr[PTR_SZ + 1], asciilen[IDXLEN_SZ + 1];		//��struct iovec �ṹ�嶨����ʹ�á� https://blog.csdn.net/lixiaogang_theanswer/article/details/73385643

	struct iovec iov[2];

	/*
		�����offsetΪ0��ӵ�ǰλ�ÿ�ʼ��
		��Ϊ������¼Ӧ��ɢ�б�֮��offset������Ϊ0
	*/
	if ((db->idxoff = lseek(db->idxfd, offset, offset == 0 ?
		SEEK_CUR : SEEK_SET)) == -1)
		err_dump("_db_readidx: lseek error");

	/*
	����readv����������¼��ʼ�������������ֶΣ�ָ����һ��������¼������ָ��
	�͸�������¼���²��ֵĳ��ȣ����²����ǲ������ģ�
	*/
	iov[0].iov_base = asciiptr;
	iov[0].iov_len = PTR_SZ;
	iov[1].iov_base = asciilen;
	iov[1].iov_len = IDXLEN_SZ;
	if ((i = readv(db->idxfd, &iov[0], 2)) != PTR_SZ + IDXLEN_SZ)
	{
		if (i == 0 && offset == 0)
			return (-1);  /* EOF for db_nextrec */
		err_dump("_db_readidx: readv error of index record");
	}

	/* This is our return value; always >= 0 */
	asciiptr[PTR_SZ] = 0;         /* null terminate */
	db->ptrval = atol(asciiptr);  /* offset of next key in chain */

	asciilen[IDXLEN_SZ] = 0;      /* null terminate */
	if ((db->idxlen = atoi(asciilen)) < IDXLEN_MIN ||
		db->idxlen > IDXLEN_MAX)
		err_dump("_db_readidx: invalid length");

	/* ��������¼�Ĳ��������ֶ���DB��idxbuf�ֶ� */
	if ((i = read(db->idxfd, db->idxbuf, db->idxlen)) != db->idxlen)
		err_dump("_db_readidx: read error of index record");
	if (db->idxbuf[db->idxlen - 1] != NEWLINE)  /* �ü�¼Ӧ�Ի��з����� */
		err_dump("_db_readidx: missing newline");
	db->idxbuf[db->idxlen - 1] = 0;  /* �����з��滻ΪNULL */

	/* ������¼����Ϊ�����ֶΣ�������Ӧ���ݼ�¼��ƫ���������ݼ�¼�ĳ��� */
	if ((ptr1 = strchr(db->idxbuf, SEP)) == NULL) //strchr(char* x,char m) ����m��һ����x�г���ʱ��ָ��
		err_dump("_db_readidx: missing first separator");
	*ptr1++ = 0;  /* replace SEP with null */	//������ptr1ָ��datoffset��ʼλ��

	if ((ptr2 = strchr(ptr1, SEP)) == NULL)
		err_dump("_db_readidx: missing second separator");
	*ptr2++ = 0;								//������ptr2ָ��datlen��ʼλ��

	if (strchr(ptr2, SEP) != NULL)
		err_dump("_db_readidx: too many separators");

	/* �����ݼ�¼��ƫ�����ͳ��ȱ�Ϊ���� */
	if ((db->datoff = atol(ptr1)) < 0)
		err_dump("_db_readidx: starting offset < 0");
	if ((db->datlen = atol(ptr2)) <= 0 || db->datlen > DATLEN_MAX)
		err_dump("_db_readidx: invalid length");

	return (db->ptrval);   /* ɢ�����е���һ����¼��ƫ���� */
}

/* ����db->datoff��db->datlen,��ȡһ������ */
static char *_db_readdat(DB *db)
{
	if (lseek(db->datfd, db->datoff, SEEK_SET) == -1)
		err_dump("_db_readdat: lseek error");
	if (read(db->datfd, db->datbuf, db->datlen) != db->datlen)
		err_dump("_db_readdat: read error");
	if (db->datbuf[db->datlen - 1] != NEWLINE)
		err_dump("_db_readdat: missing newline");
	db->datbuf[db->datlen - 1] = 0;
	return (db->datbuf);
}

/* 
	ɾ���������ƥ���һ����¼ 
*/
int db_delete(DBHANDLE h, const char *key)
{
	DB *db = h;
	int rc = 0;  //״̬�룬���غ�����ִ��״̬

	//�ж��Ƿ���ڸü�ֵ
	if (_db_find_and_lock(db, key, 1) == 0)
	{//����ҵ�ִ��delete
		_db_dodelete(db);
		db->cnt_delok++;
	}
	else
	{
		rc = -1;
		db->cnt_delerr++;
	}
	//���find_and_lock�ӵ�д��
	if (un_lock(db->idxfd, db->chainoff, SEEK_SET, 1) < 0)
		err_dump("db_delete: un_lock error");
	return (rc);
}

/*
	ʵ��ɾ��������ɾ����ָ�ú�SPACE��䣬��������������
	��Ҫ�Ĳ�����ɾ����Ӧ���ݡ�ɾ����Ӧ������ֻɾ��key�����ڿ��������������ɾ������
*/
static void _db_dodelete(DB *db) //��������ע��  441-461
{
	int i;
	char *ptr;
	off_t freeptr, saveptr;

	/* Set data buffer and key to all blanks */
	for (ptr = db->datbuf, i = 0; i < db->datlen - 1; i++)
		*ptr++ = SPACE;
	*ptr = 0;
	ptr = db->idxbuf;
	while (*ptr)
		*ptr++ = SPACE;

	
	/* We have to lock the free list */
	if (writew_lock(db->idxfd, FREE_OFF, SEEK_SET, 1) < 0)
		err_dump("_db_dodelete: writew_lock error");

	/* Write the data record with all blanks */
	_db_writedat(db, db->datbuf, db->datoff, SEEK_SET);

	/* �޸Ŀ������� */
	freeptr = _db_readptr(db, FREE_OFF);

	/* ��_db_writeidx�޸�֮ǰ�ȱ���ɢ�����еĵ�ǰ��¼ */
	saveptr = db->ptrval;

	/* �ñ�ɾ����������¼��ƫ�������¿�������ָ��,
		   Ҳ��ʹ��ָ��ǰɾ����������¼, �Ӷ����ñ�ɾ����¼�ӵ��˿�������֮�� */
	_db_writeidx(db, db->idxbuf, db->idxoff, SEEK_SET, freeptr);

	/* write the new free list pointer */
	_db_writeptr(db, FREE_OFF, db->idxoff);

	/* �޸�ɢ������ǰһ����¼��ָ�룬ʹ��ָ����ɾ����¼֮���һ����¼��
	   �������ɢ�����г�����Ҫɾ���ļ�¼ */
	_db_writeptr(db, db->ptroff, saveptr);

	/* �Կ���������� */
	if (un_lock(db->idxfd, FREE_OFF, SEEK_SET, 1) < 0)
		err_dump("_db_dodelete: un_lock error");
}

/* дһ�����ݻ��ߴ�ĳһ���ݴ���ʼд�����Ǹ����� */
static void _db_writedat(DB *db, const char *data, off_t offset, int whence)
{
	struct iovec iov[2];
	static char newline = NEWLINE;

	/* If we're appending, we have to lock before dong the lseek and
	   write to make the two an atomic operation. If we're overwriting
	   an existing record, we don't have to lock */
	//����Ǵ�ĩβд˵������ĩβ������ݲ�������ʱ��Ҫ�����������������ɵ����߼���
	if (whence == SEEK_END)
		if (writew_lock(db->datfd, 0, SEEK_SET, 0) < 0)
			err_dump("_db_writedat: writew_lock error");

	if ((db->datoff = lseek(db->datfd, offset, whence)) == -1)
		err_dump("_db_writedat: lseek error");
	db->datlen = strlen(data) + 1;  /* datlen includes new line */

	/* 
		�����뵱Ȼ����Ϊ�����߻�����β���пռ���Լӻ��з���
		�����Ƚ����з�������һ�����壬Ȼ���ٴӸû���д�����ݼ�¼
		(������仰Ӧ����ָ�����ڵ����⺯��֮ǰ����Ա����ʹ��datbuf�Ŀռ䲻������Ϊ��datbuf����ռ�ʱ��malloc(DATLEN_MAX+2)��Ӧ��
		�ŵ���һ�����з��ģ���Ϊ�˱�������������������һ��const char* data,��֤���ݲ����ƻ���Ȼ��Ϊ�˱�������ϵͳ������������writev��
		��Ȼ���½�һ��char���鸴��data�ټ�һ�����з��ٵ���writeҲ�ǿ��е�)
	*/
	iov[0].iov_base = (char *)data;
	iov[0].iov_len = db->datlen - 1;
	iov[1].iov_base = &newline;
	iov[1].iov_len = 1;
	if (writev(db->datfd, &iov[0], 2) != db->datlen)
		err_dump("_db_writedat: writev error of dat record");

	if (whence == SEEK_END)
		if (un_lock(db->datfd, 0, SEEK_SET, 0) < 0)
			err_dump("_db_writedat: un_lock error");
}

/* 
	дһ��������������ĩβ��ӣ�Ҳ���Ը���
	������DB�е�datoff��datlen֮ǰ����
   _db_writedat is called before this function to set datoff and datlen
   fields in the DB structure, which we need to write the index record  
*/
static void _db_writeidx(DB *db, const char *key, off_t offset,
	int whence, off_t ptrval)
{
	struct iovec iov[2];
	char asciiptrlen[PTR_SZ + IDXLEN_SZ + 1];
	int len;
	char *fmt;

	//���ptrval�Ƿ�Ϸ�
	if ((db->ptrval = ptrval) < 0 || ptrval > PTR_MAX)
		err_quit("_dbwriteidx: invalid ptr: %d", ptrval);
	if (sizeof(off_t) == sizeof(long long))//���ݲ�ͬƽ̨ѡ��ͬ����������   �μ�����ע��502-524
		fmt = "%s%c%lld%c%d\n";                           //���з��Ѿ�������fmt�������Ͳ��ü���
	else
		fmt = "%s%c%ld%c%d\n";
	sprintf(db->idxbuf, fmt, key, SEP, db->datoff, SEP, db->datlen);//����������д��idxbuf
	if ((len = strlen(db->idxbuf)) < IDXLEN_MIN || len > IDXLEN_MAX)
		err_dump("_db_writeidx: invalid length");
	sprintf(asciiptrlen, "%*ld%*d", PTR_SZ, ptrval, IDXLEN_SZ, len);//������ͷ����Ҳ����ָ����һ����¼��ָ���Լ���ǰ������¼�ĳ��ȣ�д��asciiptrlen

	/* If we're appending, we have to lock before dong the lseek
	   and write to make the two an atomic operation */
	if (whence == SEEK_END)//�����������ݣ����������������ɵ����߼���
		if (writew_lock(db->idxfd, ((db->nhash + 1)*PTR_SZ) + 1,
			SEEK_SET, 0) < 0)
			err_dump("_db_writeidx: writew_lock error");

	/* ���������ļ�ƫ�������Ӵ˴���ʼд������¼��
		   ����ƫ��������DB�ṹ��idxoff�ֶ� */
	if ((db->idxoff = lseek(db->idxfd, offset, whence)) == -1)
		err_dump("_db_writeidx: lseek error");

	iov[0].iov_base = asciiptrlen;
	iov[0].iov_len = PTR_SZ + IDXLEN_SZ;
	iov[1].iov_base = db->idxbuf;
	iov[1].iov_len = len;
	if (writev(db->idxfd, &iov[0], 2) != PTR_SZ + IDXLEN_SZ + len)
		err_dump("_db_writeidx: writev error of index record");

	if (whence == SEEK_END)
		if (un_lock(db->idxfd, ((db->nhash + 1)*PTR_SZ) + 1,
			SEEK_SET, 0) < 0)
			err_dump("_db_writeidx: un_lock error");

}

/* �޸�offsetλ������ָ�룬ptrval��ʾҪд������� */
static void _db_writeptr(DB *db, off_t offset, off_t ptrval)
{
	char asciiptr[PTR_SZ + 1];

	/* ���ptrval�Ƿ�Ϸ���Ȼ��ת��Ϊ�ַ��� */
	if (ptrval < 0 || ptrval > PTR_MAX)
		err_quit("_db_writeptr: invalid ptr: %d", ptrval);
	sprintf(asciiptr, "%*ld", PTR_SZ, ptrval);

	/* ����offset��λ����ptrvalд�� */
	if (lseek(db->idxfd, offset, SEEK_SET) == -1)
		err_dump("_db_writeptr: lseek error to ptr field");
	if (write(db->idxfd, asciiptr, PTR_SZ) != PTR_SZ)
		err_dump("_db_writeptr: write error of ptr field");
}

/* 
	�޸����� 
		DB_INSERT��DB_REPLACE��DB_STORE
*/
int db_store(DBHANDLE h, const char *key, const char *data, int flag)
{
	DB *db = h;
	int rc, keylen, datlen;
	off_t ptrval;								//rcΪ״̬��

	/* ������֤flag�Ƿ���Ч����Ч����core�ļ��˳� */
	if (flag != DB_INSERT && flag != DB_REPLACE && flag != DB_STORE)
	{
		errno = EINVAL;
		return (-1);
	}
	keylen = strlen(key);
	datlen = strlen(data) + 1;
	if (datlen < DATLEN_MIN || datlen > DATLEN_MAX)
		err_dump("db_store: invalid data length");


	if (_db_find_and_lock(db, key, 1) < 0)  /* record not found */
	{
		if (flag == DB_REPLACE)	
		{//�޸Ĳ���������
			rc = -1;
			db->cnt_storerr++;
			errno = ENOENT;   /* error, record does not exist */
			goto doreturn;
		}

		/* _db_find_and_lock locked the hash chain for us;
		read the chain ptr to the first index record on hash chain */
		ptrval = _db_readptr(db, db->chainoff);//��ȡ��ǰҪ��ӵļ�¼���ڵ�hash����ͷָ��

		if (_db_findfree(db, keylen, datlen) < 0)//�ڿ������в����Ƿ���ڼ��������ݳ���ȵļ�
		{											//���������ڿռ�������û��ƥ����������ݳ��������ݲ����ļ�ĩβ
			_db_writedat(db, data, 0, SEEK_END); 
			_db_writeidx(db, key, 0, SEEK_END, ptrval);//ʹ������ӵ�������¼�е�ָ��ָ��δ���������¼ǰ��hash����ͷ������¼

			/*Ȼ���ٽ��¼�¼�ӵ���Ӧ��ɢ���������� */
			_db_writeptr(db, db->chainoff, db->idxoff);
			db->cnt_stor1++;//������+1
		}
		else
		{//���������ڿ���������ƥ�䵽��ȼ��������ݳ������ڽ����ݲ��룬������ӿ�������ɾ��
			_db_writedat(db, data, db->datoff, SEEK_SET);
			_db_writeidx(db, key, db->idxoff, SEEK_SET, ptrval);  //�Ҽ�¼�Ĺ�������_db_find_free���������ˣ��μ��˺���
			_db_writeptr(db, db->chainoff, db->idxoff);
			db->cnt_stor2++;
		}
	}
	else //
	{
		if (flag == DB_INSERT)
		{//����������ݣ��˳�
			rc = 1;
			db->cnt_storerr++;
			goto doreturn;
		}

		//�޸����ݣ�ԭ�����������ݳ��Ȳ���
		if (datlen != db->datlen)
		{
			_db_dodelete(db);//ɾ��

			/* Reread the chain ptr in the hash table
			   (it may change with the deletion) */
			ptrval = _db_readptr(db, db->chainoff);

			/* ����������ӵ������ļ��������ļ���ĩβ */
			_db_writedat(db, data, 0, SEEK_END);
			_db_writeidx(db, key, 0, SEEK_END, ptrval);

			/* ���¼�¼��ӵ���Ӧɢ���������� */
			_db_writeptr(db, db->chainoff, db->idxoff);
			db->cnt_stor3++; /* ��������ļ�������1 */
		}
		else//��������ԭ���ݳ������
		{
			/* ֻ����д���ݼ�¼��������������1 */
			_db_writedat(db, data, db->datoff, SEEK_SET);
			db->cnt_stor4++;
		}
	}
	rc = 0;  /* OK */

doreturn:
	//�ͷ������˳�
	if (un_lock(db->idxfd, db->chainoff, SEEK_SET, 1) < 0)
		err_dump("db_store: unclock error");
	return (rc);
}

/* �ӿ��������в����Ƿ����keylen��datlen��ȵ�������ɾ������ֻ�ǲ�����key���֣� */
static int _db_findfree(DB *db, int keylen, int datlen)
{
	int rc;
	off_t offset, nextoffset, saveoffset;

	//�����������
	if (writew_lock(db->idxfd, FREE_OFF, SEEK_SET, 1) < 0)
		err_dump("_db_findfree: writew_lock error");

	//��������ͷָ��
	saveoffset = FREE_OFF;
	offset = _db_readptr(db, saveoffset);

	//��������
	while (offset != 0)
	{
		nextoffset = _db_readidx(db, offset);
		if (strlen(db->idxbuf) == keylen && db->datlen == datlen)
			break;  /* found a match */
		//��¼��ǰλ��
		saveoffset = offset;        	//ʼ�ձ�����һ�ڵ��λ�ã�����ҵ���Ӧ��¼�������޸Ŀ����б�
		offset = nextoffset;
	}

	if (offset == 0)//δƥ�䵽��Ӧ����
	{
		rc = -1;
	}
	else
	{//ƥ�䵽��Ӧ����
		//offset��ʾ��һ����������λ�ã�ptrval��ʾ��һ��������λ��
		//����һ��������nextָ��ptrval��ɾ����ǰ�ڵ�
		//��ʱdb->idxoff���浱ǰ����λ��
		_db_writeptr(db, saveoffset, db->ptrval);
		rc = 0;
	}

	if (un_lock(db->idxfd, FREE_OFF, SEEK_SET, 1) < 0)
		err_dump("_db_findfree: un_lock error");
	return (rc);  /* ����״̬�� */
}

//��idxoff��λ����һ������
void db_rewind(DBHANDLE h)
{
	DB *db = h;
	off_t offset;

	offset = (db->nhash + 1) * PTR_SZ;
	if ((db->idxoff = lseek(db->idxfd, offset + 1, SEEK_SET)) == -1)
		err_dump("db_rewind: lseek error");
}

/* 55
	��ȡ���ݿ����һ����������������
	����db�ṹ��datbuf
*/
char *db_nextrec(DBHANDLE h, char *key)
{
	DB *db = h;
	char c;
	char *ptr;

	if (readw_lock(db->idxfd, FREE_OFF, SEEK_SET, 1) < 0)//����������Ӷ�������ʱ�������޸Ŀ�������
		err_dump("db_nextrec: readw_lock error");

	do
	{
		if (_db_readidx(db, 0) < 0) // ����һ������������0��ʾ�ӵ�ǰλ�ö� 
		{
			ptr = NULL;
			goto doreturn;
		}

		ptr = db->idxbuf;

		/* ���ܻ������ɾ����¼����������Ч��¼����������ȫ�ո��¼ */
		while ((c = *ptr++) != 0 && c == SPACE);
	} while (c == 0);//c==0��ʾ�����ռ�¼������

	if (key != NULL)  /* �ҵ�һ�� ��Ч�� */
		strcpy(key, db->idxbuf);

	/* �����ݼ�¼������ֵδָ��������ݼ�¼���ڲ������ָ��ֵ */
	ptr = _db_readdat(db);
	db->cnt_nextrec++;

doreturn:
	if (un_lock(db->idxfd, FREE_OFF, SEEK_SET, 1) < 0)
		err_dump("db_nextrec: un_lock error");
	return (ptr);
}
