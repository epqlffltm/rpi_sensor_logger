/*
2026-02-07
db test
*/
#include <stdio.h>
#include <sqlite3.h>

int main(void)
{
    sqlite3* db; //sqlite3구조체 포인터는 db의 핸들러
    sqlite3_stmt* res; //sqlite3_stmt은 하나의 sql을 표현하는 핸들러
    
    int rc = sqlite3_open(":memory:", &db);//db를 열어 인메모리 db로 쓴다.
    
    if (rc != SQLITE_OK)
    {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        
        return 1;
    }
    
    rc = sqlite3_prepare_v2(db, "SELECT SQLITE_VERSION()", -1, &res, 0);    
    
    if (rc != SQLITE_OK) 
    {
        fprintf(stderr, "Failed to fetch data: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        
        return 1;
    }
    
    rc = sqlite3_step(res);
    
    if (rc == SQLITE_ROW)
    {
        printf("%s\n", sqlite3_column_text(res, 0));
    }
    
    sqlite3_finalize(res);
    sqlite3_close(db);
    
    return 0;
}