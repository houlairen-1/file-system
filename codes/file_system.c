
/***************************************************************************
File Name: file_manager_system.c
系统运行环境：CentOS 7
date: 2017-06-07
***************************************************************************/

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
//*********************定义的常量************************
#define BLOCKSIZE 1024 //磁盘块大小
#define SIZE 1024*1024*100 //虚拟磁盘空间大小: 100 M
#define END 65535 //FAT中的文件结束标志
#define FREE 0 //FAT中盘块空闲标志
#define ROOTBLOCKNUM 2 //根目录区所占盘块总数
#define MAXOPENFILE 10 //最多同时打开文件个数
#define MAXFILENUM 10  //当前目录下最多存放的文件
#define DATA_DISK_NUM 995  //数据区盘块数量
#define AFTERROOTPOSITION 2  //根目录之后的数据区
struct FCB //仿照FAT16设置的
{
	char filename[8]; //文件名
	char exname[3];	//文件扩展名
	unsigned char attribute; //文件属性字段：值为0时表示目录文件，值为1时表示数据文件
	struct tm date_time; //处理创建日期和时间
	unsigned short first; //文件起始盘块号
	unsigned long length; //文件长度(字节数)
	char free; //表示目录项是否为空，若值为0，表示空，值为1，表示已分配
};

struct FAT
{
	unsigned short id;
};

typedef struct USEROPEN //用户打开文件表 只用于文件
{
	char filename[8];
	//文件名
	char exname[3];
	//文件扩展名
	unsigned char attribute;//文件属性：值为0时表示文件目录，值为1时表示数据文件
	unsigned short time;
	//文件创建时间
	unsigned short data;
	//文件创建日期
	unsigned short first;
	//文件起始盘块号
	unsigned long length;
	//文件长度(对数据文件是字节数，对目录文件可以是目录项个数)
	char free;
	//表示目录项是否为空，若值为0，表示空，值为1，表示已分配
	//下么设置的dirno和diroff记录相应打开文件的目录项在父目录文件中的位置
	//这样如果该文件的fcb被修改了，则要写回父目录文件时比较方便
	int dirno;
	// 相应打开文件的目录项在父目录文件中的盘块号
	int diroff;
	//相应打开文件的目录项在父目录文件的dirno盘块中的目录项序号
	char dir[80];//相应打开文件所在的目录名，这样方便快速检查出指定文件是否已经打开
	int count;
	//读写指针在文件中的位置
	char fcbstate;
	//是否修改了文件的FCB的内容，如果修改了置为1，否则为0
	char topenfile;
	//表示该用户打开表项是否为空，若值为0，表示为空，否则表示已被某打开文件占据
}useropen;

struct BLOCK0
{
	char information[200];
	//存储一些描述信息，如磁盘块大小，磁盘块数量，最多打开文件数等
	unsigned short root;
	//根目录文件的起始盘块号
	char *startblock;//虚拟磁盘上数据区开始位置
};

//**************************用户定义的全局变量*********************
FILE *fp;
struct FAT *fat; //FAT表的起始位置
struct FCB *root; //根目录
struct FCB *cur_dir; //当前目录
struct FCB *father_dir; //父目录地址
int fd = -1; //open()函数的返回值，文件描述符
time_t timep;
char filename[10];
int flag = 0;
int block_num = 0; //所占盘块数量
				   //******************全局变量*********************************************

char *myvhard; //指向虚拟磁盘的起始地址
useropen openfilelist[MAXOPENFILE];//用户打开文件表数组
useropen *ptrcurdir; //指向用户打开文件表中的当前目录所在打开文件表项的位置
char currentdir[80]; //记录当前目录的目录名
char *startp; //记录虚拟磁盘上数据区开始位置

			  //*****************函数*********************************************
void filesys_ui();
void show();
void startsys();
void my_format();
void my_cd(char *dirname);
void my_mkdir(char *dirname);
void my_rmdir(char *dirname);
void my_ls(void);
int my_create(char *filename);
void my_rm(char *filename);
int my_open(char *filename);
void my_close(int fd);
int my_write(int fd);
int do_write(int fd, char *text, int len, char wstyle);
int myread(int fd, int len);
int do_read(int fd, int len, char *text);
int my_exitsys();

void filesys_ui()
{
	printf("列出文件系统提供的各项功能及命令调用格式\n");
	printf("****************************************************************\n");
	printf("****************welcome to filesystem***************************\n");
	printf("格式化存储器 format\n");
	printf("创建子目录 mkdir\n");
	printf("删除子目录 rmdir\n");
	printf("显示目录中的内容 ls\n");
	printf("更改当前目录 cd\n");
	printf("创建文件 create\n");
	printf("打开文件 open\n");
	printf("关闭文件 close\n");
	printf("写文件 write\n");
	printf("读文件 read\n");
	printf("删除文件 rm\n");
	printf("退出系统 exitsys\n");

}
void startsys()
{
	char data;
	//暂存数据
	int i;
	char fileflag[10] = { "10101010" };
	//文件魔数
	myvhard = (char *)malloc(SIZE * sizeof(char));
	startp = myvhard + BLOCKSIZE + BLOCKSIZE * 4;
	fp = NULL;
	fp = fopen("file_system.txt", "rb");
	if (fp != NULL)
	{
		fread(myvhard, BLOCKSIZE, 1000, fp);
		data = myvhard[8];
		myvhard[8] = '\0';
		if (strcmp(myvhard, fileflag) == 0)
		{
			myvhard[8] = data;
			fwrite(myvhard, BLOCKSIZE, 1000, fp);
		}
		else
		{
			printf("file_system 文件系统不存在，现在开始创建文件系统\n");
			my_format();
			fwrite(myvhard, BLOCKSIZE, 1000, fp);
		}
	}
	else
	{
		printf("file_system 文件系统不存在，现在开始创建文件系统\n");
		my_format();
		fp = fopen("file_system.txt", "wb");
		fwrite(myvhard, BLOCKSIZE, 1000, fp);
	}
	fclose(fp);
	fat = (struct FAT *)(myvhard + BLOCKSIZE);
	startp = myvhard + BLOCKSIZE + 4 * BLOCKSIZE;

	for (i = 1; i < MAXOPENFILE; i++)
		//初始化用户打开表项
	{
		openfilelist[i].topenfile = 0;
		//目录为空
	}
	strcpy(openfilelist[0].filename, "root");
	// 根目录文件
	strcpy(openfilelist[0].exname, "\0");
	openfilelist[0].attribute = 0;
	//
	openfilelist[0].first = 5;
	//计数从数据区开始
	openfilelist[0].length = 0;
	openfilelist[0].free = 1;
	openfilelist[0].dirno = 5;
	openfilelist[0].diroff = 0;
	strcpy(openfilelist[0].dir, ".");
	openfilelist[0].count = 0;
	openfilelist[0].fcbstate = 0;
	openfilelist[0].topenfile = 0;
	ptrcurdir = openfilelist;
	strcpy(currentdir, "\\");

	cur_dir = (struct FCB *)startp;
	//设置当前目录
}
void my_format()
{
	struct BLOCK0 *block0;
	int i;
	cur_dir = (struct FCB *)startp;
	//***************对引导块初始化******************************
	block0 = (struct BLOCK0 *)(myvhard);
	strcpy(block0->information, "10101010");
	//文件魔数
	strcat(block0->information, "2,1,1,5,1024,1000,5120");
	block0->root = 5;
	block0->startblock = startp;
	//*************建立FAT表************************************
	fat = (struct FAT *)(myvhard + BLOCKSIZE);
	for (i = 0; i < BLOCKSIZE; i++)
	{
		fat[i].id = FREE;
		//后面数据区的内容未分配 
	}
	fat[0].id = fat[1].id = 1;
	//根目录区已经被分配了
	fat = (struct FAT *)(myvhard + BLOCKSIZE + BLOCKSIZE + BLOCKSIZE);
	for (i = 0; i < BLOCKSIZE; i++)
		//备份表
	{
		fat[i].id = FREE;
		//后面数据区的内容未分配 
	}
	fat[0].id = fat[1].id = 1;
	//根目录区已经被分配了
	root = (struct FCB *)(startp);
	//根目录地址，数据区起始地址
	//****************设置根目录下面的"."文件***********************
	strcpy(root[0].filename, ".");
	root[0].exname[0] = '\0';
	root[0].attribute = 0;
	root[0].first = 0;
	root[0].length = 0;
	root[0].free = 0;
	time(&timep);
	root[0].date_time = *localtime(&timep);
	//***************设置目录下面的".."文件************************
	strcpy(root[1].filename, "..");
	root[1].exname[0] = '\0';
	root[1].attribute = 0;
	root[1].first = 0;
	root[1].length = 0;
	root[1].free = 0;
	time(&timep);
	root[1].date_time = *localtime(&timep);
	//************子目录初始化*********************************
	for (i = 2; i < MAXFILENUM + 2; i++)
	{
		strcpy(cur_dir[i].filename, "");
		strcpy(cur_dir[i].exname, "");
		cur_dir[i].attribute = -1;
		cur_dir[i].first = FREE;
		cur_dir[i].length = 0;
		cur_dir[i].free = 0;
	}
}
void init()
{
	int i = 0;
	myvhard = NULL;
	for (i = 0; i < MAXOPENFILE; i++)
		openfilelist[i].free = 0;
	//未分配
	ptrcurdir = NULL;
	strcpy(currentdir, "\0");
}

int my_exitsys()
{
	fp = fopen("file_system.txt", "wb");
	fwrite(myvhard, BLOCKSIZE, 1000, fp);
	fclose(fp);
	free(myvhard);
	return 1;
}
void show()
{
	printf("%s>:", currentdir);
}

void my_cd(char *dirname)
{
	int i;
	struct FCB *cd_dir;
	//绝对路径  目录试探指针
	int cur_num;
	char temp[10];
	//暂存子目录名
	if (strcmp(dirname, "\\") == 0)
		//到起始目录
	{
		strcpy(currentdir, "\\");
		cur_dir = (struct FCB *)startp;
		return;
	}
	if (dirname[0] == '\\')
		//绝对路径查找
	{
		if (dirname[strlen(dirname) - 1] == '\\')
			dirname[strlen(dirname) - 1] = '\0';
		cur_num = 0;

		//当前游标
		cd_dir = (struct FCB *)startp;
		for (i = 1; dirname[i]; i++)
		{
			if (dirname[i] != '\\')
				temp[cur_num++] = dirname[i];
			else
			{
				temp[cur_num] = '\0';
				for (i = 2; i < MAXFILENUM + 2; i++)
					if (strcmp(cd_dir[i].filename, temp) == 0 && cd_dir[i].attribute == 0)
						break;
				if (i >= MAXFILENUM)
				{
					printf("找不到此目录\n");
					return;
				}
				cd_dir = (struct FCB *)(startp + cd_dir[i].first*BLOCKSIZE);
				cur_num = 0;
			}
		}
		temp[cur_num] = '\0';

		for (i = 2; i < MAXFILENUM + 2; i++)
			if (strcmp(cd_dir[i].filename, temp) == 0 && cd_dir[i].attribute == 0)
				break;
		if (i >= MAXFILENUM + 2)
		{
			printf("没有此目录文件\n");
			return;
		}
		cd_dir = (struct FCB *)(startp + cd_dir[i].first*BLOCKSIZE);
		cur_num = 0;
		strcpy(currentdir, dirname);
		strcat(currentdir, "\\");
		//添加目录
		cur_dir = cd_dir;
		//把进入的目录赋给当前目录
		return;
	}
	if (strcmp(dirname, "..") == 0)
	{
		if ((char *)cur_dir == startp)
			//在根目录下操作
			return;
		cur_dir = (struct FCB *)(startp + cur_dir[1].first*BLOCKSIZE);
		//找到父目录
		currentdir[strlen(currentdir) - 1] = '\0';
		for (i = strlen(currentdir) - 2; i >= 0; i--)
			if (currentdir[i] == '\\')
				break;
		currentdir[i + 1] = '\0';
		//修改dirname内容，当前目录
		return;
	}
	if (strcmp(dirname, ".") == 0)
		return;
	for (i = 2; i < MAXFILENUM + 2; i++)
		if (strcmp(cur_dir[i].filename, dirname) == 0 && cur_dir[i].attribute == 0)
			break;
	if (i >= MAXFILENUM + 2)
		//找不到切换的目录
	{
		printf("没有此目录\n");
		return;
	}
	strcat(currentdir, cur_dir[i].filename);
	cur_dir = (struct FCB *)(startp + cur_dir[i].first*BLOCKSIZE);
	//当前目录指针变到当前
	strcat(currentdir, "\\");
	//添加目录
}

void my_mkdir(char *dirname)
{
	int i, j;  //j代表目录文件中的序号  i代表FAT中找到的空闲块号
	struct FCB *cur_mkdir;
	if (strchr(dirname, '\\'))/*如果目录名中有 '\'字符*/
	{
		printf("目录文件名中不能存在'\'\n");
		return;
	}
	if (!strcmp(dirname, "."))
	{
		printf("目录文件名中不能存在'.'\n");
		return;
	}
	if (!strcmp(dirname, ".."))
	{
		printf("目录文件名中不能存在'..'\n");
		return;
	}
	for (j = 2; j < MAXFILENUM + 2; j++)
		//记录当前目录中的未使用的号
		if (strcmp(cur_dir[j].filename, "") == 0)
			break;
	for (i = 2; i < MAXFILENUM + 2; i++)
		if (strcmp(cur_dir[i].filename, dirname) == 0)
			break;
	if (i < MAXFILENUM + 2)
		//存在重名
	{
		printf("当前创建的目录文件存在重名\n");
		return;
	}
	for (i = AFTERROOTPOSITION; i < DATA_DISK_NUM; i++)
		//从FAT中找空闲磁盘块
	{
		if (fat[i].id == FREE)
			break;
	}
	if (i >= DATA_DISK_NUM)
	{
		printf("磁盘块用完了\n");
		return;
	}

	fat[i].id = END;
	//**********************填写目录项*********************

	strcpy(cur_dir[j].filename, dirname);
	cur_dir[j].first = i;
	cur_dir[j].attribute = 0;
	cur_dir[j].length = 0;
	//当前目录项中的文件个数增加了
	cur_dir[j].free = 0;
	time(&timep);
	cur_dir[j].date_time = *localtime(&timep);
	//处理时间
	//*******************初始化所建立的目录的信息. ..*********
	cur_mkdir = (struct FCB *)(startp + cur_dir[j].first*BLOCKSIZE);
	//找到所建立的目录文件块的地址
	strcpy(cur_mkdir[0].filename, ".");
	cur_mkdir[0].attribute = 0;
	cur_mkdir[0].first = i;
	cur_mkdir[0].length = 0;
	cur_mkdir[0].free = 1;
	time(&timep);
	cur_mkdir[0].date_time = *localtime(&timep);
	//处理时间

	strcpy(cur_mkdir[1].filename, "..");
	cur_mkdir[1].attribute = 0;
	cur_mkdir[1].first = ((char *)cur_dir - startp) / BLOCKSIZE;
	//对于子目录的.. 找到父目录
	cur_mkdir[1].length = 0;
	cur_mkdir[1].free = 1;
	time(&timep);
	cur_mkdir[1].date_time = *localtime(&timep);
	//处理时间

	//********************子目录都初始化一下*********************88
	for (i = 2; i < MAXFILENUM + 2; i++)
	{
		strcpy(cur_mkdir[i].filename, "");
		strcpy(cur_mkdir[i].exname, "");
		cur_mkdir[i].attribute = -1;
		cur_mkdir[i].first = FREE;
		cur_mkdir[i].length = 0;
		cur_mkdir[i].free = 0;
	}

	father_dir = (struct FCB*)(startp + cur_dir[1].first*BLOCKSIZE);
	//当前目录的付目录 //当前目录文件数增加
	for (i = 2; i < MAXFILENUM + 2; i++)
		//父目录中对应的当前目录项的文件数增加
		if ((struct FCB*)(startp + father_dir[i].first*BLOCKSIZE) == cur_dir)
			break;
	father_dir[i].length++;
	printf("目录文件创始成功\n");
}
void my_rmdir(char *dirname)
{
	int i;
	struct FCB *rm_dir;
	//要删除的目录文件
	//***********************检查当前目录项中有无该目录****************
	for (i = 2; i < MAXFILENUM + 2; i++)
		if (strcmp(cur_dir[i].filename, dirname) == 0 && cur_dir[i].attribute == 0)
			break;
	if (i >= MAXFILENUM + 2)
	{
		printf("找不到该文件\n");
		return;
	}
	//*************找到了所要删除的文件目录*************所删除的目录文件中的文件已经打开
	rm_dir = cur_dir + i;
	if (rm_dir->length == 0)
		//所要删除的文件没子文件了
	{
		strcpy(rm_dir->filename, "");
		strcpy(rm_dir->exname, "");
		rm_dir->attribute = -1;
		fat[rm_dir->first].id = FREE;
		rm_dir->first = -1;
		rm_dir->length = 0;
		rm_dir->free = 0;
		printf("删除成功\n");
	}
	else
	{
		printf("所要删除的目录文件内存在文件或目录文件，所以不能删除此目录文件\n");
		return;
	}
}
void my_ls()
{
	int i;
	//printf("文件夹信息\n");
	for (i = 0; i < MAXFILENUM; i++)
	{
		if (strcmp(cur_dir[i].filename, "") != 0)
			if (cur_dir[i].attribute == 0)
				//目录文件
			{
				printf("%s\t\t\t\t\t", cur_dir[i].filename);
				printf("%d %d %d ", (1900 + cur_dir[i].date_time.tm_year), (1 + cur_dir[i].date_time.tm_mon), cur_dir[i].date_time.tm_mday);
				printf("%d:%d:%d\n", cur_dir[i].date_time.tm_hour, cur_dir[i].date_time.tm_min, cur_dir[i].date_time.tm_sec);
			}
			else
			{
				printf("%s\t\t\t%ld\t\t", cur_dir[i].filename, cur_dir[i].length);
				printf("%d %d %d ", (1900 + cur_dir[i].date_time.tm_year), (1 + cur_dir[i].date_time.tm_mon), cur_dir[i].date_time.tm_mday);
				printf("%d:%d:%d\n", cur_dir[i].date_time.tm_hour, cur_dir[i].date_time.tm_min, cur_dir[i].date_time.tm_sec);

			}
	}
}









int my_create(char *filename)
{
	int i, j, k;
	//i代表找到了当前目录中的目录项，k代表fat中的空闲块
	for (i = 2; i < MAXFILENUM + 2; i++)
	{
		if (cur_dir[i].first == FREE)
			break;
	}
	for (j = 2; j < MAXFILENUM + 2; j++)
		if (strcmp(cur_dir[j].filename, filename) == 0)
			break;
	if (i >= MAXFILENUM + 2)
	{
		printf("没有空的目录项了,不能创建文件\n");
		return -1;
	}
	if (j < MAXFILENUM + 2)
	{
		printf("存在重名文件，不能创建文件\n");
		return -1;
	}
	for (k = 2; k < DATA_DISK_NUM; k++)
		//找空闲的快
		if (fat[k].id == FREE)
			break;
	if (k >= DATA_DISK_NUM)
		//找不到空闲的块
		return -1;
	if (strcmp(filename, "..") == 0 || strcmp(filename, ".") == 0)
	{
		printf("创建文件失败，不能建%s的文件\n", filename);
		return -1;
	}
	fat[k].id = END;
	//历经重重磨难啊
	//*************写数据文件目录项*******************
	time(&timep);
	//获取时间
	cur_dir[i].date_time = *localtime(&timep);
	strcpy(cur_dir[i].filename, filename);
	strcpy(cur_dir[i].exname, "txt");
	cur_dir[i].attribute = 1;
	cur_dir[i].first = k;
	cur_dir[i].free = 0;
	cur_dir[i].length = 0;
	father_dir = (struct FCB*)(startp + cur_dir[1].first*BLOCKSIZE);
	//当前目录文件数增加
	for (i = 2; i < MAXFILENUM + 2; i++)
		//父目录中对应的当前目录项的文件数增加
		if ((struct FCB*)(startp + father_dir[i].first*BLOCKSIZE) == cur_dir)
			break;
	father_dir[i].length++;
	fd = my_open(filename);
}
void my_rm(char *filename)
{
	int i;
	int id, idnext;
	//FAT中 文件所对应的连续块号
	for (i = 1; i < MAXOPENFILE; i++)
		if (strcmp(openfilelist[i].filename, filename) == 0 && openfilelist[i].topenfile == 1)
		{
			printf("此文件正在被使用，请先关闭\n");
			return;
		}
	for (i = 2; i < MAXFILENUM + 2; i++)
		if (strcmp(cur_dir[i].filename, filename) == 0 && cur_dir[i].attribute == 1)
			break;
	if (i >= MAXFILENUM + 2)
	{
		printf("找不到此文件\n");
		return;
	}

	id = cur_dir[i].first;
	while (1)  //删除FAT中文件所对应的连续盘块号
	{
		if (fat[id].id == END)
		{
			fat[id].id = FREE;
			break;
		}
		else
		{
			idnext = fat[id].id;
			fat[id].id = FREE;
			id = idnext;
		}
	}
	//**************释放目录项****************************
	strcpy(cur_dir[i].filename, "");
	strcpy(cur_dir[i].exname, "");
	cur_dir[i].attribute = -1;
	cur_dir[i].first = FREE;
	cur_dir[i].length = 0;
	cur_dir[i].free = 0;
}
int my_open(char *filename)
{
	int i, j;
	for (i = 1; i < MAXOPENFILE + 2; i++)
		if (openfilelist[i].topenfile == 1 && strcmp(openfilelist[i].dir, currentdir) == 0 && strcmp(openfilelist[i].filename, filename) == 0)
		{
			//  printf("此文件已经打开了\n");
			fd = i;
			return -1;
			//此文件已经打开
		}
	for (i = 2; i < MAXFILENUM + 2; i++)
		if (strcmp(cur_dir[i].filename, filename) == 0 && cur_dir[i].attribute == 1)
			break;
	if (i >= MAXFILENUM + 2)
		//当前目录文件下的FCB序号
	{
		printf("文件不存在，文件打开失败\n");
		return -1;
	}
	for (j = 1; j < MAXFILENUM; j++)
		//打开文件表找到了项，  j标志
		if (openfilelist[j].topenfile == 0)
			break;
	//**************************把文件的信息放到打开文件表项中**********************
	strcpy(openfilelist[j].filename, cur_dir[i].filename);
	strcpy(openfilelist[j].exname, cur_dir[i].exname);
	openfilelist[j].attribute = cur_dir[i].attribute;
	openfilelist[j].first = cur_dir[i].first;
	openfilelist[j].length = cur_dir[i].length;
	openfilelist[j].free = cur_dir[i].free;
	openfilelist[j].dirno = i;
	//子文件在父目录文件中的位置
	openfilelist[j].diroff = 0;
	strcpy(openfilelist[j].dir, currentdir);
	openfilelist[j].count = 0;
	openfilelist[j].fcbstate = 0;
	openfilelist[j].topenfile = 1;
	//此文件在使用中
	fd = j;  //openfilelist 中正被打开的文件
	return j;
}
void my_close(int fd)
{
	int dirno;
	int i;
	if (fd >= MAXOPENFILE)
	{
		printf("关闭文件出错，超过文件打开表的最大数量\n");
		return;
	}
	dirno = openfilelist[fd].dirno;
	//**********************打开文件被修改把内容写回到对应FCB中***************
	if (openfilelist[fd].fcbstate == 1)
	{
		strcpy(cur_dir[dirno].filename, openfilelist[fd].filename);
		strcpy(cur_dir[dirno].exname, openfilelist[fd].exname);
		cur_dir[dirno].attribute = openfilelist[fd].attribute;
		cur_dir[dirno].first = openfilelist[fd].first;
		cur_dir[dirno].length = openfilelist[fd].length;
		cur_dir[dirno].free = openfilelist[fd].free;
		time(&timep);
		cur_dir[dirno].date_time = *localtime(&timep);
		//处理时间
	}
	openfilelist[fd].topenfile = 0;
}




int my_read(int fd, int len)
{
	char *text;
	int i, item, reallen;

	text = (char *)malloc(BLOCKSIZE * sizeof(char));
	int realbytenum;
	if (fd >= MAXOPENFILE || fd < 0)
	{
		printf("读取文件出错，不在用户文件打开表中\n");
		return -1;
	}

	printf("读出的文件内容是:      \n");
	item = openfilelist[fd].first;
	while (1)
	{
		if (len > BLOCKSIZE)
			reallen = BLOCKSIZE;
		else
			reallen = len;
		block_num++;
		realbytenum = do_read(fd, reallen, text);
		for (i = 0; i < realbytenum; i++)
			putchar(text[i]);
		if (fat[item].id == END)
			break;
		item = fat[item].id;
		len -= BLOCKSIZE;
	}
	printf("\n");
	block_num = 0;
	//标志文件所占块数量

	return realbytenum;
	//返回实际读出的字节数
}
int do_read(int fd, int len, char *text)
{
	char *cur_first;
	//文件的当前磁盘块
	char *buf = NULL;
	int blocknum, blocknum_off;
	//文件所占的磁盘块数量
	int i, j;
	int item;
	int realreadnum = 0;
	buf = (char *)malloc(BLOCKSIZE * sizeof(char));
	if (buf == NULL)
	{
		printf("申请失败\n");
		//申请失败
		return -1;
	}
	item = openfilelist[fd].first;
	//指向要读的第一块
	for (i = 1; i < block_num; i++)
		item = fat[item].id;
	//找下个文件盘块
	cur_first = startp + item*BLOCKSIZE;
	//文件的盘块位置
	for (j = 0; j < len; j++)
	{
		text[realreadnum++] = cur_first[j];
	}

	return realreadnum;
}


int my_write(int fd)
{
	char getcommand;
	int cur_first, leftbyte;
	int temp, file_position;
	char text[BLOCKSIZE * 2];
	char ch;
	int realbytewrite;
	int i, j;
	if (fd > MAXOPENFILE)
	{
		printf("不能再打开文件了\n");
		return -1;
	}

	//文件的第一块位置
	if (fd == -1)
	{
		printf("请先打开输入文件\n");
		return -1;
	}

	printf("请输入你想哪种写方式\n");
	printf("1.截断写 2.覆盖写 3.追加写\n");
	getchar();
	//接收没用的回车键
	scanf("%c", &getcommand);
	if (getcommand == ' ')
		scanf("%c", &getcommand);

	getchar();
	j = 0;
	cur_first = openfilelist[fd].first;
	if (getcommand == '1')
		if (fat[cur_first].id != END)
		{
			temp = fat[cur_first].id;
			fat[cur_first].id = END;
			cur_first = temp;
			//cur_first 指向下一块
			while (fat[cur_first].id != END)
			{
				temp = fat[cur_first].id;
				fat[cur_first].id = FREE;
				cur_first = temp;
			}
			fat[cur_first].id = FREE;
			//释放除当前文件的第一块磁盘以外的其它块的空间
		}
	//j指示所读的内容的长度
	if (getcommand == '1')
	{
		openfilelist[fd].length = 0;
		openfilelist[fd].count = 0;
		printf("请输入文件内容 按 CTRL+d结束\n");
		while (1)
		{
			while (ch = getchar())
				//输入的数不是CTRL+d
			{
				if (ch == -1 || j == BLOCKSIZE)
					break;
				else
					text[j++] = ch;
			}
			realbytewrite = do_write(fd, text, j, getcommand);

			if (realbytewrite < 0)
			{
				printf("磁盘块已用完\n");
				return -1;
			}
			j = 0;
			openfilelist[fd].count += realbytewrite;
			openfilelist[fd].length += realbytewrite;
			openfilelist[fd].fcbstate = 1;
			if (ch == -1)
				break;
		}
	}
	else if (getcommand == '2')
	{
		printf("请输入从此文件的哪几个位置开始写\n");
		scanf("%d", &file_position);
		if (file_position > openfilelist[fd].length)
		{
			printf("输入错误,此位置比文件最大长度还长\n");
			return -1;
		}
		openfilelist[fd].count = file_position;
		//指针位置
		printf("请输入文件内容 按 CTRL+d结束\n");
		getchar();
		while (1)
		{
			while (ch = getchar())
				//输入的数不是CTRL+d
			{
				if (ch == -1 || j == BLOCKSIZE)
					break;
				else
				{
					text[j++] = ch;
					leftbyte++;
					if (leftbyte == BLOCKSIZE - openfilelist[fd].count%BLOCKSIZE)
					{
						leftbyte = BLOCKSIZE + 1;
						//让leftbyte失效
						flag = 1;
						break;
					}
				}
			}
			if (ch == -1 && leftbyte < BLOCKSIZE)
				flag = 1;
			realbytewrite = do_write(fd, text, j, getcommand);
			j = 0;
			flag = 0;
			openfilelist[fd].count += realbytewrite;
			if (openfilelist[fd].length < openfilelist[fd].count)
				openfilelist[fd].length = openfilelist[fd].count;
			openfilelist[fd].count = 0;
			//文件指针
			openfilelist[fd].fcbstate = 1;
			if (ch == -1)
				break;
		}
	}
	else if (getcommand == '3')
		//追加写
	{
		flag = 0;
		leftbyte = 0;
		printf("请输入文件内容 按 CTRL+d结束\n");
		while (1)
		{
			while (ch = getchar())
				//输入的数不是CTRL+d
			{
				if (ch == -1 || j == BLOCKSIZE)
					break;
				else
				{
					text[j++] = ch;
					leftbyte++;
				}
				if (leftbyte == BLOCKSIZE - openfilelist[fd].length%BLOCKSIZE)
				{
					leftbyte = BLOCKSIZE + 1;
					//让leftbyte失效
					flag = 1;
					break;
				}
			}
			if (ch == -1 && leftbyte < BLOCKSIZE)
				//追加的第一块当前块
				flag = 1;
			realbytewrite = do_write(fd, text, j, getcommand);
			flag = 0;
			j = 0;
			if (realbytewrite < 0)
			{
				printf("磁盘块已用完\n");
				return -1;
			}
			openfilelist[fd].count += realbytewrite;
			openfilelist[fd].length += realbytewrite;
			openfilelist[fd].fcbstate = 1;
			if (ch == -1)
				break;
		}
	}
	else
	{
		printf("输入错误请重新输入\n");

	}
	return realbytewrite;
}
int do_write(int fd, char *text, int len, char wstyle)
{
	int i, j, k;
	//游标
	int textlen;
	char *file_point;
	int item, freeitem;
	int cur_block, cur_blockoff;
	int tmplen;
	//实际写入的字节数
	if (wstyle == '1')
	{
		if (openfilelist[fd].length == 0)
		{
			file_point = startp + openfilelist[fd].first*BLOCKSIZE;
			for (j = 0; j < len; j++)
				file_point[j] = text[j];
			return j;
		}
		for (i = 2; i < DATA_DISK_NUM; i++)
			//找到空的磁盘块
			if (fat[i].id == FREE)
				break;
		if (i >= DATA_DISK_NUM)
		{
			printf("磁盘块已用完\n");
			return -1;
		}
		item = openfilelist[fd].first;
		while (fat[item].id != END)
			item = fat[item].id;
		//找到此文件跨越的最后个磁盘块
		fat[item].id = i;
		//盘块号 文件起始要写的盘块号
		file_point = startp + i*BLOCKSIZE;
		//文件指针 要写的内容 
		for (j = 0; j < len; j++)
			file_point[j] = text[j];
		fat[i].id = END;
		//将找到的空闲块分配了 
	}
	else if (wstyle == '2')
	{
		if (flag == 1)
			//继续接下写
		{
			block_num = openfilelist[fd].count / BLOCKSIZE;
			cur_blockoff = openfilelist[fd].count%BLOCKSIZE;
			item = openfilelist[fd].first;
			for (i = 1; i < block_num; i++)
				item = fat[item].id;
			file_point = startp + item*BLOCKSIZE;
			for (i = 0; i < len; i++)
				file_point[cur_blockoff + i] = text[i];
		}
		else  //找新块块
		{
			for (i = 2; i < DATA_DISK_NUM; i++)
				//找到空的磁盘块
				if (fat[i].id == FREE)
					break;
			if (i >= DATA_DISK_NUM)
			{
				printf("磁盘块已用完\n");
				return -1;
			}
			item = openfilelist[fd].first;
			while (fat[item].id != END)
				item = fat[item].id;
			//找到此文件跨越的最后个磁盘块
			fat[item].id = i;
			//盘块号 文件起始要写的盘块号
			file_point = startp + i*BLOCKSIZE;
			//文件指针 要写的内容 
			for (j = 0; j < len; j++)
				file_point[j] = text[j];
			fat[i].id = END;
			//将找到的空闲块分配了
		}
	}
	else if (wstyle == '3')
	{
		if (flag == 1)
			//追加后继续写
		{
			item = openfilelist[fd].first;
			while (fat[item].id != END)
				item = fat[item].id;
			cur_blockoff = openfilelist[fd].length%BLOCKSIZE;
			file_point = startp + item*BLOCKSIZE;
			for (i = 0; i < len; i++)
				file_point[cur_blockoff + i] = text[i];
			//追加写内容

		}
		else
		{
			for (i = 2; i < DATA_DISK_NUM; i++)
				//找到空的磁盘块
				if (fat[i].id == FREE)
					break;
			if (i >= DATA_DISK_NUM)
			{
				printf("磁盘块已用完\n");
				return -1;
			}
			item = openfilelist[fd].first;
			while (fat[item].id != END)
				item = fat[item].id;
			//找到此文件跨越的最后个磁盘块
			fat[item].id = i;
			//盘块号 文件起始要写的盘块号
			file_point = startp + i*BLOCKSIZE;
			//文件指针 要写的内容 
			for (j = 0; j < len; j++)
				file_point[j] = text[j];
			fat[i].id = END;
			//将找到的空闲块分配了
		}
	}
	return len;

}


int main()
{
	int i;
	char getcommand[30];
	char dirname[10];
	char absolute_dir[80];
	///记录绝对路径的文件的路径
	int len;
	init();
	char code[12][10];
	startsys();
	filesys_ui();
	strcpy(code[0], "format");
	//把命令存在code数组中
	strcpy(code[1], "mkdir");
	strcpy(code[2], "rmdir");
	strcpy(code[3], "ls");
	strcpy(code[4], "cd");
	strcpy(code[5], "create");
	strcpy(code[6], "open");
	strcpy(code[7], "close");
	strcpy(code[8], "write");
	strcpy(code[9], "read");
	strcpy(code[10], "rm");
	strcpy(code[11], "exitsys");
	while (1)
	{
		show();
		scanf("%s", getcommand);
		for (i = 0; i < 12; i++)
			if (strcmp(getcommand, code[i]) == 0)
				break;
		switch (i)
		{
		case 0:
			my_format();
			break;
		case 1:
			scanf("%s", dirname);
			my_mkdir(dirname);

			break;
		case 2:
			scanf("%s", dirname);
			my_rmdir(dirname);
			break;
		case 3:
			my_ls();
			break;
		case 4:
			scanf("%s", dirname);
			my_cd(dirname);
			break;
		case 5:
			scanf("%s", filename);
			my_create(filename);
			break;
		case 6:
			scanf("%s", filename);
			my_open(filename);
			break;
		case 7:
			scanf("%s", filename);
			if (filename[0] == '\\')
				//打开绝对路径的文件
			{
				for (i = 1; i < MAXOPENFILE; i++)
				{
					strcpy(absolute_dir, openfilelist[i].dir);
					strcat(absolute_dir, openfilelist[i].filename);
					if (strcmp(filename, openfilelist[i].dir) == 0 && openfilelist[i].topenfile == 1)
					{
						fd = i;
						break;
					}
				}
			}
			else
			{
				for (i = 1; i < MAXOPENFILE; i++)
				{
					if (strcmp(openfilelist[i].filename, filename) == 0 && strcmp(openfilelist[i].dir, currentdir) == 0 && openfilelist[i].topenfile == 1)
						fd = i;
				}
				if (fd >= MAXOPENFILE)
				{
					printf("此文件不在文件打开表中\n");
					break;
				}
			}
			my_close(fd);
			fd = -1;
			break;
		case 8:
			scanf("%s", filename);
			if (filename[0] == '\\')
				//打开绝对路径的文件
			{
				for (i = 1; i < MAXOPENFILE; i++)
				{
					strcpy(absolute_dir, openfilelist[i].dir);
					strcat(absolute_dir, openfilelist[i].filename);
					if (strcmp(filename, openfilelist[i].dir) == 0 && openfilelist[i].topenfile == 1)
					{
						fd = i;
						break;
					}
				}
			}
			else
			{
				for (i = 1; i < MAXOPENFILE; i++)
				{
					if (strcmp(openfilelist[i].filename, filename) == 0 && strcmp(openfilelist[i].dir, currentdir) == 0 && openfilelist[i].topenfile == 1)
						fd = i;
				}
				if (fd >= MAXOPENFILE)
				{
					printf("此文件不在文件打开表中\n");
					break;
				}
			}
			my_write(fd);
			break;
		case 9:
			scanf("%s", filename);
			if (filename[0] == '\\')
				//打开绝对路径的文件
			{
				for (i = 1; i < MAXOPENFILE; i++)
				{
					strcpy(absolute_dir, openfilelist[i].dir);
					strcat(absolute_dir, openfilelist[i].filename);
					if (strcmp(filename, openfilelist[i].dir) == 0 && openfilelist[i].topenfile == 1)
					{
						fd = i;
						break;
					}
				}
			}
			else
			{
				for (i = 1; i < MAXOPENFILE; i++)
				{
					if (strcmp(openfilelist[i].filename, filename) == 0 && strcmp(openfilelist[i].dir, currentdir) == 0 && openfilelist[i].topenfile == 1)
						fd = i;
				}
				if (fd >= MAXOPENFILE)
				{
					printf("此文件不在文件打开表中\n");
					break;
				}
			}
			//printf("请输入要读的文件长度\n");
			//scanf("%d",&len);
			len = openfilelist[fd].length;
			my_read(fd, len);
			break;
		case 10:
			scanf("%s", filename);
			my_rm(filename);
			break;
		case 11:
			if (my_exitsys() == 1)
				return 0;
			break;
		default:
			printf("命令输入错误，请重新输入\n");
			break;
		}
	}
	return 0;
}
