#include <stdio.h>

long counter = 0;

long f(){
    return counter++;
}

long func1() {
    return f() + f() + f() + f();
}

long func2() {
    return 4*f();
}

int main()
{
    func2();
    printf("%d", counter);

    return 0;    
}
