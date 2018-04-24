// myfirst.cpp --display amessage

#include <iostream>
#include <unistd.h>
#include <stdio.h>
int main()
{
    using namespace std;
    cout << "Come up and C++ me some time.\n";

    /*for(int i = 0; i < 10000 ; i++){
        cout << i ;
        usleep(3000);
    }*/
    //usleep(10*1000*1000);
    //cout << endl;
    //cout << "You won't regret it!" << endl;

    char str[10];
    cin >> str;
    cout << str << endl;
    printf("%s\n", str);
    return 0;
}
