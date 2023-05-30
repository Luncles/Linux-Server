/* ************************************************************************
> File Name:     ErrorHandling.cpp
> Author:        Luncles
> 功能：         错误输出程序
> Created Time:  Tue 23 May 2023 09:33:42 PM CST
> Description:   
 ************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "ErrorHandling.h"

void ErrorHandling(const char *message)
{
    fputs(message, stderr);
    fputc('\n', stderr);
    exit(1);
}
