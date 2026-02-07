/*
2026-02-07
db test
*/
#include<stdio.h>
#include<sqlite3.h>

int main(void)
{
  printf("%s\n",sqlite3_libversion());

  return 0;
}