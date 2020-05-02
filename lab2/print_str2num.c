#define __LIBRARY__

#include<unistd.h>
#include<asm/segment.h>
#include <linux/kernel.h>

char num_to_char(int i)	/* i is a single number */
{
	return (char)(i + 48);
}
long char_to_num(char c)
{
	return (long)(c - 48);
}

int sys_print_val(int a)
{
	int num_len = 0;
	char str_num[64];
	for(int num = a; num != 0; num_len++)
	{
		num /= 10;
	}
	str_num[num_len] = '\0';
	for(int i = num_len - 1, num = a; num != 0; i--)
	{
		str_num[i] = num_to_char(num % 10);
		num /= 10;
	}
	printk(str_num);
	return num_len;
}

int sys_str2num(char* str, int str_len, long* ret)
{
	long num = 0;
	int i = 0;
	for(i = 0; i < str_len; i++)
	{
		num = num * 10 + char_to_num(get_fs_byte(&str[i]));
	}
	put_fs_long(num, ret);
	return 0;
}
